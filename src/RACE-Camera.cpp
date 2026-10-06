#include "RACE-Camera.h"
#include "RACE-CameraMaths.h"
#include "RACE-Compat.h"
#include "RACE-Inputs.h"
#include "RACE-Settings.h"
#include "RACE-Widget.h"

using namespace RACE::CameraMath;
namespace Keys = RACE::Keys;

namespace
{
	// Multipliers & Magic Numbers
	constexpr float MOUSE_WHEEL_MULT         =  0.1f;
	constexpr float MOUSE_MOVE_MULT          =  0.002f;
	constexpr float MOUSE_ROT_MULT           =  0.010f;
	constexpr float GP_DPAD_MULT             =  0.5f;

	constexpr float PITCH_CLAMP_LIMIT        =  1.55f;
	constexpr float SCROLL_SMOOTH_SPEED      = 15.0f;
	constexpr float QUICKZOOM_DURATION       =  0.8f;
	constexpr float CINEMATIC_SPEED          = 10.0f;
	constexpr float EPSILON                  =  0.001f;

	constexpr float DEFAULT_CAMERA_RATE      =  5.0f;
	constexpr float MIN_CAMERA_RATE          =  1.0f;
	constexpr float MAX_CAMERA_RATE          = 10.0f;

	float InputAxis(std::uint32_t a_positive, std::uint32_t a_negative)
	{
		return (FUCK::IsInputDown(a_positive) ? 1.0f : 0.0f) - (FUCK::IsInputDown(a_negative) ? 1.0f : 0.0f);
	}

	bool GetMenuMember(const char* a_name, RE::GFxValue& a_out)
	{
		auto widget = RaceWidget::GetSingleton();
		auto movie  = widget->GetCachedMenuMovie();

		RE::GFxValue menuInstance;
		return movie && widget->GetMenuInstance(movie, menuInstance) && menuInstance.GetMember(a_name, &a_out);
	}

	bool GetCameraEditor(RE::GFxValue& a_out) { return GetMenuMember("cameraEditor", a_out); }

	// Find where RaceMenu's slider list ends on screen, to use as the left bound for framing Quick Zoom.
	// Falls back to the default layout if a UI theme leaves it unreadable
	float GetSliderPanelEdge()
	{
		constexpr float DEFAULT_EDGE = 439.0f;  // slider list's right edge in default RaceMenu, in stage units

		auto         movie = RaceWidget::GetSingleton()->GetCachedMenuMovie();
		RE::GFxValue itemList, root, bounds, xMax;

		// Use the slider list itself, as racePanel's bounds also include the hidden category tabs and come out far too wide
		if (GetMenuMember("itemList", itemList) && itemList.IsDisplayObject() && movie->GetVariable(&root, "_root")) {
			itemList.Invoke("getBounds", &bounds, &root, 1);
			if (bounds.IsObject() && bounds.GetMember("xMax", &xMax) && xMax.IsNumber()) {
				float edge = FUCK::TranslateScaleformToScreen(static_cast<float>(xMax.GetNumber()), 0.0f).x;

				// Ignore anything past mid-screen, as a theme has laid it out unexpectedly
				if (edge > 0.0f && edge < FUCK::GetDisplaySize().x * 0.5f)
					return edge;
			}
		}

		// Translate the width rather than the position, as the panel stays on the screen's left edge while stage x 0 moves inward on ultrawide
		logger::warn("Quick Zoom: couldn't read RaceMenu's slider list edge, using the default layout");
		return FUCK::TranslateScaleformToScreen(DEFAULT_EDGE, 0.0f).x - FUCK::TranslateScaleformToScreen(0.0f, 0.0f).x;
	}

	void RotatePlayer(float a_amount)
	{
		auto player = RE::PlayerCharacter::GetSingleton();
		if (!player || !player->Is3DLoaded())
			return;

		if (auto root = player->Get3D(false)) {
			RE::NiMatrix3 deltaMat;
			deltaMat.SetEulerAnglesXYZ(0.0f, 0.0f, -a_amount);

			root->local.rotate = deltaMat * root->local.rotate;

			RE::NiUpdateData ctx;
			root->UpdateWorldData(&ctx);
		}
	}
}

bool RaceCamera::IsMouseOverWireframe() const
{
	auto widget = RaceWidget::GetSingleton();
	if (!widget->IsVisible() || !widget->IsOnSculptTab()) {
		return false;
	}

	auto movie = widget->GetCachedMenuMovie();
	if (!movie) {
		return false;
	}

	RE::GFxValue menuInstance;
	if (!widget->GetMenuInstance(movie, menuInstance)) {
		return false;
	}

	RE::GFxValue vertexEditor;
	if (menuInstance.GetMember("vertexEditor", &vertexEditor) && vertexEditor.IsObject()) {
		RE::GFxValue wireframeDisplay;
		if (vertexEditor.GetMember("wireframeDisplay", &wireframeDisplay) && wireframeDisplay.IsObject()) {
			RE::GFxValue isVisible;
			if (wireframeDisplay.GetMember("_visible", &isVisible) && isVisible.GetBool()) {
				RE::GFxValue root;
				if (movie->GetVariable(&root, "_root")) {
					RE::GFxValue x, y;
					root.GetMember("_xmouse", &x);
					root.GetMember("_ymouse", &y);
					RE::GFxValue args[3] = { x, y, RE::GFxValue(true) };
					RE::GFxValue hit;
					wireframeDisplay.Invoke("hitTest", &hit, args, 3);
					if (hit.IsBool() && hit.GetBool()) {
						return true;
					}
				}
			}
		}
	}
	return false;
}

bool RaceCamera::GetCurrentFov(float& a_horizontal, float& a_vertical) const
{
	auto worldCam = RE::Main::WorldRootCamera();
	if (!worldCam)
		return false;

	const auto& frustum = worldCam->GetRuntimeData2().viewFrustum;
	if (frustum.fTop <= 0.0f || frustum.fRight <= 0.0f)
		return false;

	a_vertical   = FovFromTanHalf(frustum.fTop);
	a_horizontal = FovFromTanHalf(frustum.fRight);
	return true;
}

float RaceCamera::GetFovScale() const
{
	if (_baseFov <= 0.0f)
		return 1.0f;

	// Scale by how much more (or less) of the scene is on screen than at the player's FOV, as visible width grows with tan(fov / 2)
	return FovScale(_baseFov, _baseFov + _fovOffset);
}

float RaceCamera::GetDistanceScale() const
{
	float scale = 1.0f;

	// Distance: current camera-to-head distance relative to RaceMenu's default.
	// cameraRoot has already been reverted to its original transform at this point.
	auto camera = RE::PlayerCamera::GetSingleton();
	auto player = RE::PlayerCharacter::GetSingleton();
	if (camera && camera->cameraRoot && player) {
		if (auto head = player->GetNodeByName("NPC Head [Head]")) {
			float baseDist = camera->cameraRoot->local.translate.GetDistance(head->world.translate);
			if (baseDist > 1.0f) {
				float zoomIn = _camOffset.y - _quickZoomDistance * _quickZoomBlend;  // +Y moves toward the subject
				scale *= std::max(baseDist - zoomIn, 1.0f) / baseDist;
			}
		}
	}

	return std::clamp(scale, 0.1f, 10.0f);
}

void RaceCamera::HandleInput(float a_interval)
{
	auto widget = RaceWidget::GetSingleton();
	if (widget->IsJournalOpen() || widget->IsFittingRoomOpen()) {
		return;
	}

	a_interval = std::min(a_interval, 0.1f);

	const auto& settings = RaceSettings::GetSingleton()->camera;

	UpdateQuickZoomBlend(a_interval);

	if (widget->IsCameraInputLocked()) {
		ClearInputState();
	} else {
		HandleControls(settings, a_interval);
	}

	UpdateCinematic(RaceSettings::GetSingleton()->cinematicCamera, a_interval);
	ApplyFov();
}

void RaceCamera::HandleControls(const CameraSettings& a_settings, float a_interval)
{
	FrameInput in = GatherInput();

	if (in.isCameraMode) {
		if (in.isCameraTab) {
			SyncCameraTab(in);
		}

		// Read after SyncCameraTab, which may have just changed the rate
		in.rateMult = in.isCameraTab ? (_cameraRate / DEFAULT_CAMERA_RATE) : 1.0f;

		// Pan faster when zoomed out / wide FOV so the subject crosses the screen at a consistent rate
		in.panScale = std::clamp(GetDistanceScale() * GetFovScale(), 0.1f, 10.0f);

		// Zoom by distance only, so each step covers the same share of the way to the subject at any range
		in.zoomScale = GetDistanceScale();

		// Orbit & pitch only follow FOV: at a narrow FOV the same angular step swings the
		// subject much further across the screen, so slow rotation down to keep precision
		in.rotScale = GetFovScale();

		HandleMouse   (in, a_settings, a_interval);
		HandleKeyboard(in, a_settings, a_interval);
		HandleGamepad (in, a_settings, a_interval);

		// Clamp Pitch to prevent gimbal flips when viewing directly from top/bottom
		_targetCamRotX = std::clamp(_targetCamRotX, -PITCH_CLAMP_LIMIT, PITCH_CLAMP_LIMIT);
	}

	HandleCharacterRotation(in, a_settings, a_interval);
}

void RaceCamera::UpdateQuickZoomBlend(float a_interval)
{
	// Ease the Quick Zoom swoop in and out, landing exactly on face or body so no drift keeps the camera active
	float step         = a_interval / QUICKZOOM_DURATION;
	_quickZoomProgress = std::clamp(_quickZoomProgress + (_isQuickZoomed ? step : -step), 0.0f, 1.0f);
	_quickZoomBlend    = _quickZoomProgress * _quickZoomProgress * (3.0f - 2.0f * _quickZoomProgress);
}

void RaceCamera::ClearInputState()
{
	// Drop anything queued up so it doesn't fire the moment the lock lifts
	_pendingScroll    = 0.0f;
	_smoothScrollZoom = 0.0f;
	_smoothScrollFov  = 0.0f;
	_kbmAcceleration  = 1.0f;
	_wasMouseCharRot  = false;

	// Wait for the buttons used on the Done prompt or name entry (A/B, E/Tab, X/Y) to be released,
	// so they don't step the camera rate or turn the player afterwards
	_wasRateUp        = true;
	_wasRateDown      = true;
	_lockoutGPCharRot = true;
}

RaceCamera::FrameInput RaceCamera::GatherInput()
{
	auto widget = RaceWidget::GetSingleton();

	FrameInput in;

	in.isCameraTab  = SKEE64Compat::IsPresent() && widget->GetCurrentMode() == 2;
	in.isGamepad    = FUCK::GetInputDevice() == FUCK::InputDevice::kGamepad;

	in.ctrlDown     = FUCK::IsModifierPressed(FUCK::Modifier::kCtrl);
	in.shiftDown    = FUCK::IsModifierPressed(FUCK::Modifier::kShift);
	in.altDown      = FUCK::IsModifierPressed(FUCK::Modifier::kAlt);

	in.lbDown       = FUCK::IsInputDown(Keys::kGP_LB);
	in.rbDown       = FUCK::IsInputDown(Keys::kGP_RB);

	in.mmbDown      = FUCK::IsMouseDown(2);
	in.rmbDown      = FUCK::IsMouseDown(1);

	in.isPopupOpen  = FUCK::IsPopupOpen(nullptr, FUCK::PopupFlags::kAnyPopup);

	bool isHidden   = !widget->IsVisible();
	in.isUIHovered  = !isHidden && (FUCK::IsWindowHovered(1) || FUCK::IsAnyItemHovered());
	in.isItemActive = !isHidden && FUCK::IsAnyItemActive();

	UpdateWireframeLock(in.mmbDown);

	// Sculpt tab uses the gamepad for its own controls
	bool disableGP = in.isGamepad && widget->IsOnSculptTab();

	bool isGlobalMouse = in.mmbDown && !in.isItemActive && !_wireframeLock && !in.isUIHovered;

	in.isGlobalKBM  = in.ctrlDown && !in.isItemActive;
	in.isGlobalGP   = in.rbDown && !in.isItemActive && !disableGP;
	in.isCameraMode = in.isGlobalKBM || in.isGlobalGP || in.isCameraTab || isGlobalMouse;

	return in;
}

void RaceCamera::UpdateWireframeLock(bool a_mmbDown)
{
	// Lock out camera interaction if a mouse button was clicked down inside the sculpt wireframe
	bool isInteractDown = FUCK::IsInputDown(Keys::kMouse_Right) || a_mmbDown;

	if (!isInteractDown) {
		_wireframeLock = false;
	} else if (!_wasInteractDown && IsMouseOverWireframe()) {
		_wireframeLock = true;
	}

	_wasInteractDown = isInteractDown;
}

void RaceCamera::SyncCameraTab(const FrameInput& a_in)
{
	// Mirror our modifier state onto the Camera Editor's bottom bar toggle
	// (LB on gamepad, Alt on keyboard), so its prompts match what the keys do
	RE::GFxValue cameraEditor;
	if (GetCameraEditor(cameraEditor)) {
		RE::GFxValue secVal;
		cameraEditor.GetMember("_secondary", &secVal);

		bool desiredSecondary = a_in.isGamepad ? a_in.lbDown : a_in.altDown;
		if (secVal.GetBool() != desiredSecondary) {
			cameraEditor.SetMember("_secondary", RE::GFxValue(desiredSecondary));
			cameraEditor.Invoke("updateBottomBar", nullptr, nullptr, 0);
		}
	}

	// Rate multiplier, stepped on key press
	// KBM keys are skipped while Ctrl is held, as E orbits/rolls there
	bool kbmRate  = !a_in.ctrlDown;
	bool rateUp   = FUCK::IsInputDown(Keys::kGP_A) || (kbmRate && FUCK::IsInputDown(Keys::kKB_E));
	bool rateDown = FUCK::IsInputDown(Keys::kGP_B) || (kbmRate && FUCK::IsInputDown(Keys::kKB_Tab));

	if (rateUp && !_wasRateUp) {
		SetCameraTabRate(std::min(MAX_CAMERA_RATE, _cameraRate + 1.0f));
	}
	if (rateDown && !_wasRateDown) {
		SetCameraTabRate(std::max(MIN_CAMERA_RATE, _cameraRate - 1.0f));
	}

	_wasRateUp   = rateUp;
	_wasRateDown = rateDown;
}

void RaceCamera::SetCameraTabRate(float a_rate)
{
	_cameraRate = a_rate;

	RE::GFxValue cameraEditor;
	if (GetCameraEditor(cameraEditor)) {
		cameraEditor.SetMember("movementRate", RE::GFxValue(_cameraRate));
		cameraEditor.Invoke("updateBottomBar", nullptr, nullptr, 0);
	}
}

void RaceCamera::HandleMouse(const FrameInput& a_in, const CameraSettings& a_settings, float a_interval)
{
	if (a_in.ctrlDown && !a_in.isUIHovered && !a_in.isPopupOpen) {
		if (_pendingScroll != 0.0f && !IsMouseOverWireframe()) {
			float amount = _pendingScroll * (a_settings.kbmZoomFovSpeed * a_in.rateMult * MOUSE_WHEEL_MULT);
			if (a_in.shiftDown) {
				// FOV (CTRL + SHIFT + Scroll)
				_smoothScrollFov += amount;
			} else {
				// Zoom (CTRL + Scroll)
				_smoothScrollZoom += amount * a_in.zoomScale;
			}
		}
	}
	_pendingScroll = 0.0f;

	float scrollFactor = SmoothFactor(SCROLL_SMOOTH_SPEED, a_interval);
	_targetCamOffset.y += DrainSmoothed(_smoothScrollZoom, scrollFactor, EPSILON);
	_targetFovOffset   -= DrainSmoothed(_smoothScrollFov,  scrollFactor, EPSILON);

	if (!a_in.mmbDown || a_in.isUIHovered || a_in.isPopupOpen || _wireframeLock)
		return;

	ImVec2 mouseDelta = FUCK::GetMouseDelta();
	float  mult       = a_in.rateMult * MOUSE_MOVE_MULT;

	if (a_in.ctrlDown && a_in.shiftDown) {
		// Roll (Middle + Ctrl + Shift)
		_targetCamRoll += mouseDelta.x * a_settings.kbmRollSpeed * mult;
	} else if (a_in.ctrlDown) {
		// Pitch (Middle + Ctrl)
		_targetCamRotX += mouseDelta.y * a_settings.kbmRotSpeed * mult * a_in.rotScale;
	} else if (a_in.rmbDown) {
		// Orbit (Middle + Right)
		_targetCamRotZ += mouseDelta.x * a_settings.kbmRotSpeed * mult * a_in.rotScale;
	} else {
		// Pan (Middle)
		_targetCamOffset.x -= mouseDelta.x * a_settings.kbmPanSpeed * mult * a_in.panScale;
		_targetCamOffset.z += mouseDelta.y * a_settings.kbmPanSpeed * mult * a_in.panScale;
	}
}

void RaceCamera::HandleKeyboard(const FrameInput& a_in, const CameraSettings& a_settings, float a_interval)
{
	if (!a_in.isGlobalKBM && !(a_in.isCameraTab && !a_in.isGamepad))
		return;

	// Translation accelerates the longer WASD is held
	bool isTranslating = FUCK::IsInputDown(Keys::kKB_W) || FUCK::IsInputDown(Keys::kKB_S) ||
	                     FUCK::IsInputDown(Keys::kKB_A) || FUCK::IsInputDown(Keys::kKB_D);

	if (isTranslating) {
		_kbmAcceleration = std::min(_kbmAcceleration + 1.5f * a_interval, 15.0f);
	} else {
		_kbmAcceleration = 1.0f;
	}

	float speed        = a_settings.kbmPanSpeed     * _kbmAcceleration * a_in.rateMult * a_in.panScale * a_interval;
	float rotSpeed     = a_settings.kbmRotSpeed     * a_in.rateMult * a_in.rotScale * a_interval;
	float rollSpeed    = a_settings.kbmRollSpeed    * a_in.rateMult * a_interval;
	float fovSpeed     = a_settings.kbmZoomFovSpeed * a_in.rateMult * a_interval;
	float zoomSpeed    = fovSpeed * a_in.zoomScale;

	float ws = InputAxis(Keys::kKB_W, Keys::kKB_S);
	float da = InputAxis(Keys::kKB_D, Keys::kKB_A);
	float eq = InputAxis(Keys::kKB_E, Keys::kKB_Q);
	float rf = InputAxis(Keys::kKB_R, Keys::kKB_F);

	// Global modifiers (Ctrl / Ctrl+Shift) and Camera Tab modifiers (none / Alt) map differently
	bool isCameraTabOnly = a_in.isCameraTab && !a_in.isGlobalKBM;
	bool isSecondary     = (a_in.isGlobalKBM && a_in.shiftDown) || (isCameraTabOnly && a_in.altDown);

	if (isCameraTabOnly && isSecondary) {
		// Camera Tab Secondary (Alt): Zoom & Orbit
		_targetCamOffset.y += ws * zoomSpeed;
		_targetCamRotZ     += da * rotSpeed;
	} else if (isSecondary) {
		// Global Secondary (Ctrl+Shift): Zoom, FOV, Roll & Tilt
		_targetCamOffset.y += ws * zoomSpeed;
		_targetFovOffset   += da * fovSpeed;
		_targetCamRoll     += eq * rollSpeed;
		_targetCamRotX     += rf * rotSpeed;
	} else if (isCameraTabOnly) {
		// Camera Tab Primary: Move Camera X/Z
		_targetCamOffset.z += ws * speed;
		_targetCamOffset.x += da * speed;
	} else {
		// Global Primary (Ctrl): Move Camera X/Z, Orbit & Tilt
		_targetCamOffset.z += ws * speed;
		_targetCamOffset.x += da * speed;
		_targetCamRotZ     += eq * rotSpeed;
		_targetCamRotX     += rf * rotSpeed;
	}
}

void RaceCamera::HandleGamepad(const FrameInput& a_in, const CameraSettings& a_settings, float a_interval)
{
	if (!a_in.isGlobalGP && !(a_in.isCameraTab && a_in.isGamepad))
		return;

	float panSpeed  = a_settings.gpPanSpeed  * a_in.rateMult * a_in.panScale * a_interval;
	float zoomSpeed = a_settings.gpZoomSpeed * a_in.rateMult * a_in.zoomScale * a_interval;
	float rotSpeed  = a_settings.gpRotSpeed  * a_in.rateMult * a_in.rotScale * a_interval;
	float rollSpeed = a_settings.gpRollSpeed * a_in.rateMult * a_interval;
	float fovSpeed  = a_settings.gpFovSpeed  * a_in.rateMult * a_interval;

	// D-Pad (RaceMenu Camera Tab only)
	// Ignored while RB is held so navigating the widget doesn't move the camera
	if (a_in.isCameraTab && !a_in.rbDown) {
		float upDown    = InputAxis(Keys::kGP_Up,    Keys::kGP_Down);
		float rightLeft = InputAxis(Keys::kGP_Right, Keys::kGP_Left);

		if (a_in.lbDown) {
			_targetCamOffset.y += upDown    * zoomSpeed * GP_DPAD_MULT;
			_targetCamRotZ     += rightLeft * rotSpeed  * GP_DPAD_MULT;
		} else {
			_targetCamOffset.z += upDown    * panSpeed * GP_DPAD_MULT;
			_targetCamOffset.x += rightLeft * panSpeed * GP_DPAD_MULT;
		}
	}

	// Thumbsticks (Always requires RB)
	if (a_in.isGlobalGP) {
		auto axis = [&](std::uint32_t a_key) {
			return ApplyDeadzone(FUCK::GetAnalogInput(a_key), a_settings.gpDeadzone);
		};

		float leftX  = axis(Keys::kGP_LSX);
		float leftY  = axis(Keys::kGP_LSY);
		float rightX = axis(Keys::kGP_RSX);
		float rightY = axis(Keys::kGP_RSY);
		float lt     = axis(Keys::kGP_LT);
		float rt     = axis(Keys::kGP_RT);

		_targetCamOffset.z += leftY * panSpeed;
		_targetCamOffset.x += leftX * panSpeed;
		_targetFovOffset   += (rt - lt) * fovSpeed;

		if (a_in.lbDown) {
			// LB + RB: Roll & Pitch
			_targetCamRoll += rightX * rollSpeed;
			_targetCamRotX -= rightY * rotSpeed;
		} else {
			// RB: Orbit & Zoom
			_targetCamRotZ     -= rightX * rotSpeed;
			_targetCamOffset.y += rightY * zoomSpeed;
		}
	}
}

void RaceCamera::HandleCharacterRotation(const FrameInput& a_in, const CameraSettings& a_settings, float a_interval)
{
	// Right-drag, or RB + X/Y on gamepad
	bool gpY = FUCK::IsInputDown(Keys::kGP_Y);
	bool gpX = FUCK::IsInputDown(Keys::kGP_X);

	// Prevent input "bleed" when selecting UI items with the gamepad.
	if (a_in.isPopupOpen || a_in.isItemActive) {
		_lockoutGPCharRot = true;
	} else if (!gpY && !gpX) {
		_lockoutGPCharRot = false;
	}

	// Block mouse rotation if hovering ImGui windows, sculpt wireframe
	bool allowMouseRot = FUCK::IsInputDown(Keys::kMouse_Right) && !a_in.mmbDown && !a_in.isUIHovered && !a_in.isPopupOpen && !_wireframeLock;
	bool allowGPRot    = a_in.rbDown && (gpY || gpX) && !_lockoutGPCharRot;

	float rotAmount = 0.0f;

	if (allowMouseRot) {
		if (_wasMouseCharRot) { // Ignore the very first frame to prevent delta spikes
			rotAmount = FUCK::GetMouseDelta().x * MOUSE_ROT_MULT;
		}
	} else if (allowGPRot) {
		if (gpY)
			rotAmount -= a_settings.gpCharRotSpeed * a_interval;
		if (gpX)
			rotAmount += a_settings.gpCharRotSpeed * a_interval;
	}

	_wasMouseCharRot = allowMouseRot;

	if (rotAmount != 0.0f) {
		RotatePlayer(rotAmount);
	}
}

void RaceCamera::UpdateCinematic(bool a_enabled, float a_interval)
{
	if (!a_enabled) {
		_camOffset = _targetCamOffset;
		_camRotZ   = _targetCamRotZ;
		_camRotX   = _targetCamRotX;
		_camRoll   = _targetCamRoll;
		_fovOffset = _targetFovOffset;
		return;
	}

	float t      = SmoothFactor(CINEMATIC_SPEED, a_interval);
	_camOffset.x = std::lerp(_camOffset.x, _targetCamOffset.x, t);
	_camOffset.y = std::lerp(_camOffset.y, _targetCamOffset.y, t);
	_camOffset.z = std::lerp(_camOffset.z, _targetCamOffset.z, t);
	_camRotZ     = std::lerp(_camRotZ,     _targetCamRotZ,     t);
	_camRotX     = std::lerp(_camRotX,     _targetCamRotX,     t);
	_camRoll     = std::lerp(_camRoll,     _targetCamRoll,     t);
	_fovOffset   = std::lerp(_fovOffset,   _targetFovOffset,   t);
}

void RaceCamera::ApplyFov()
{
	auto camera = RE::PlayerCamera::GetSingleton();
	if (!camera)
		return;

	auto& worldFOV = camera->GetRuntimeData2().worldFOV;

	// Grab the player's FOV on the first frame (restored on close), so RaceMenu matches vanilla until it's adjusted
	if (_baseFov == 0.0f)
		_baseFov = worldFOV;

	if (_baseFov != 0.0f)
		worldFOV = _baseFov + _fovOffset;
}

void RaceCamera::UpdateQuickZoomFraming(const RE::NiPoint3& a_headPos, const RE::NiPoint3& a_pelvisPos, const RE::NiPoint3& a_rootPos)
{
	// Use a fixed bottom rather than RaceMenu's bottom bar, as UI themes may remove it
	constexpr float QUICKZOOM_MARGIN = 1.25f;  // 1.0 = feet touch FRAME_BOTTOM
	constexpr float FRAME_BOTTOM     = -0.8f;  // -1 is the screen bottom, 1 the top

	// Bail if the camera isn't ready yet, it's retried next frame
	auto   worldCam = RE::Main::WorldRootCamera();
	ImVec2 display  = FUCK::GetDisplaySize();
	if (!worldCam || display.x <= 0.0f)
		return;

	const auto& frustum = worldCam->GetRuntimeData2().viewFrustum;
	if (frustum.fTop <= 0.01f || frustum.fRight <= 0.01f)
		return;

	// Centre between the slider list and the widget's default x, so dragging the widget doesn't move the framing
	float left = 0.0f, right = display.x;
	if (!RaceWidget::GetSingleton()->IsUIHidden()) {
		left  = GetSliderPanelEdge();
		right = std::max(RaceWidget::GetDefaultPos().x, left + 1.0f);
	}

	ScreenRect rect{
		(left + right) / display.x - 1.0f,
		(1.0f + FRAME_BOTTOM) * 0.5f,
		(1.0f - FRAME_BOTTOM) * 0.5f
	};

	auto framing = FrameFullBody(a_pelvisPos, a_rootPos, _originalRotate, frustum.fTop, frustum.fRight, QUICKZOOM_MARGIN, rect);
	if (!framing)
		return;

	float headDepth = (a_headPos - _originalTranslate).Dot(ForwardAxis(_originalRotate));

	// Stored relative to RaceMenu's camera so idle animation can't drag the framing around
	_quickZoomCamDelta   = framing->camPos - _originalTranslate;
	_quickZoomPivotDelta = a_pelvisPos - _originalTranslate;
	_quickZoomDistance   = std::max(framing->depth - headDepth, 0.0f);
	_hasQuickZoomFraming = true;
}

void RaceCamera::ApplyTransform(RE::NiNode* a_cameraRoot)
{
	if (!a_cameraRoot || !HasAnyCamera() || RaceWidget::GetSingleton()->IsFittingRoomOpen())
		return;

	_originalTranslate = a_cameraRoot->local.translate;
	_originalRotate    = a_cameraRoot->local.rotate;
	_wasModified       = true;

	auto player = RE::PlayerCharacter::GetSingleton();

	RE::NiPoint3 headPos = _originalTranslate;
	RE::NiPoint3 rootPos = _originalTranslate;
	if (player) {
		rootPos = player->GetPosition();
		if (auto headNode = player->GetNodeByName("NPC Head [Head]")) {
			headPos = headNode->world.translate;
		} else {
			headPos = rootPos;
		}
	}

	if (_isQuickZoomed && !_hasQuickZoomFraming) {
		RE::NiPoint3 pelvisPos = headPos + (rootPos - headPos) * 0.5f;
		if (player) {
			if (auto pelvisNode = player->GetNodeByName("NPC Pelvis [Pelv]")) {
				pelvisPos = pelvisNode->world.translate;
			}
		}
		UpdateQuickZoomFraming(headPos, pelvisPos, rootPos);
	}

	RE::NiPoint3 pivot   = headPos;
	RE::NiPoint3 camBase = _originalTranslate;
	if (_hasQuickZoomFraming && _quickZoomBlend > 0.0f) {
		RE::NiPoint3 bodyPivot = _originalTranslate + _quickZoomPivotDelta;
		pivot                  = headPos + (bodyPivot - headPos) * _quickZoomBlend;
		camBase                = _originalTranslate + _quickZoomCamDelta * _quickZoomBlend;
	}

	RE::NiPoint3  orbitedTranslate = OrbitAroundZ(pivot, camBase, _camRotZ);
	RE::NiMatrix3 orbitedRotate    = MakeYaw(_camRotZ) * _originalRotate;
	RE::NiMatrix3 pitchMat         = MakePitch(_camRotX);
	RE::NiMatrix3 rollMat          = MakeRoll(_camRoll);

	RE::NiPoint3 finalOffset      = orbitedRotate * (pitchMat * _camOffset);
	a_cameraRoot->local.translate = orbitedTranslate + finalOffset;
	a_cameraRoot->local.rotate    = orbitedRotate * pitchMat * rollMat;
	_appliedTranslate             = a_cameraRoot->local.translate;
	_appliedRotate                = a_cameraRoot->local.rotate;

	RE::NiUpdateData ctx;
	a_cameraRoot->UpdateWorldData(&ctx);
}

void RaceCamera::RevertCameraTransform(RE::NiNode* a_cameraRoot)
{
	if (a_cameraRoot && _wasModified) {
		if (a_cameraRoot->local.translate == _appliedTranslate && a_cameraRoot->local.rotate == _appliedRotate) {
			a_cameraRoot->local.translate = _originalTranslate;
			a_cameraRoot->local.rotate    = _originalRotate;
		}
		_wasModified = false;
	}
}

void RaceCamera::ToggleQuickZoom()
{
	_isQuickZoomed = !_isQuickZoomed;
	if (_isQuickZoomed)
		_hasQuickZoomFraming = false;

	// Ease out manual moves so the camera settles on the new face or body framing
	_targetCamOffset  = { 0.0f, 0.0f, 0.0f };
	_targetCamRotZ    = 0.0f;
	_targetCamRotX    = 0.0f;
	_smoothScrollZoom = 0.0f;
}

void RaceCamera::ResetOffsets()
{
	if (auto camera = RE::PlayerCamera::GetSingleton()) {
		if (_baseFov != 0.0f)
			camera->GetRuntimeData2().worldFOV = _baseFov;
		RevertCameraTransform(camera->cameraRoot.get());
	}
	_camOffset           = { 0.0f, 0.0f, 0.0f };
	_camRotZ             = 0.0f;
	_camRotX             = 0.0f;
	_camRoll             = 0.0f;
	_fovOffset           = 0.0f;
	_targetCamOffset     = { 0.0f, 0.0f, 0.0f };
	_targetCamRotZ       = 0.0f;
	_targetCamRotX       = 0.0f;
	_targetCamRoll       = 0.0f;
	_targetFovOffset     = 0.0f;
	_baseFov             = 0.0f;
	_cameraRate          = DEFAULT_CAMERA_RATE;
	_isQuickZoomed       = false;
	_quickZoomProgress   = 0.0f;
	_quickZoomBlend      = 0.0f;
	_quickZoomDistance   = 0.0f;
	_hasQuickZoomFraming = false;
	_smoothScrollZoom    = 0.0f;
	_smoothScrollFov     = 0.0f;
}
