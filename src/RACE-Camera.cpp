#include "RACE-Camera.h"
#include "RACE-Compat.h"
#include "RACE-Inputs.h"
#include "RACE-Widget.h"

namespace Keys = RACE::Keys;

namespace
{
	constexpr float RIGHT_INSET         = 334.0f;  // from the screen's right edge at 1080p, quickzoom framing.

	constexpr float GP_DPAD_MULT        =  0.5f;

	constexpr float DEFAULT_CAMERA_RATE =  5.0f;
	constexpr float MIN_CAMERA_RATE     =  1.0f;
	constexpr float MAX_CAMERA_RATE     = 10.0f;

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

void RaceCamera::BeginSessionIfNeeded()
{
	// Paused while another plugin has the camera, e.g. Photo Mode, and FUCK resumes it
	if (_isTakenOver || FUCK::IsCameraSessionActive(this) || FUCK::IsCameraSessionPaused(this))
		return;

	FUCK::CameraSessionOptions opts;
	opts.client  = this;
	opts.flags   = FUCK::CameraSessionFlags::kPresetHotkeys | FUCK::CameraSessionFlags::kResumable;
	FUCK::BeginCameraSession(opts);
}

bool RaceCamera::GetFramingRect(FUCK::CameraScreenRect& a_out) const
{
	// Use a fixed bottom rather than RaceMenu's bottom bar, as UI themes may remove it
	constexpr float FRAME_BOTTOM = -0.8f;  // -1 is the screen bottom, 1 the top

	ImVec2 display = FUCK::GetDisplaySize();
	if (display.x <= 0.0f)
		return false;

	// Centre between the slider list and a fixed right inset
	float left = 0.0f, right = display.x;
	if (!RaceWidget::GetSingleton()->IsUIHidden()) {
		left  = GetSliderPanelEdge();
		right = std::max(display.x - FUCK::Scale(RIGHT_INSET), left + 1.0f);
	}

	a_out.centreX    = (left + right) / display.x - 1.0f;
	a_out.centreY    = (1.0f + FRAME_BOTTOM) * 0.5f;
	a_out.halfHeight = (1.0f - FRAME_BOTTOM) * 0.5f;
	return true;
}

void RaceCamera::Revert()
{
	BeginSessionIfNeeded();
	FUCK::RevertCamera(this);
}

void RaceCamera::Update(float a_interval)
{
	auto widget = RaceWidget::GetSingleton();
	if (widget->IsJournalOpen() || widget->IsFittingRoomOpen()) {
		return;
	}

	FUCK::UpdateCamera(this, a_interval);
}

void RaceCamera::OnCameraUpdate(FUCK::CameraControlState& a_state, float)
{
	auto widget = RaceWidget::GetSingleton();

	bool isLocked    = widget->IsCameraInputLocked();
	bool isCameraTab = SKEE64Compat::IsPresent() && widget->IsOnCameraTab();

	if (isLocked) {
		// Wait for the buttons used on the Done prompt or name entry (A/B, E/Tab) to be released, so they don't step the rate afterwards
		_wasRateUp   = true;
		_wasRateDown = true;
	} else {
		UpdateWireframeLock();
		if (isCameraTab) {
			SyncCameraTab();
			DriveCameraTab();
		}
	}

	a_state.inputAllowed   = !isLocked;
	a_state.mouseLocked    = _wireframeLock;
	a_state.gamepadBlocked = widget->IsOnSculptTab();  // Sculpt tab uses the gamepad for its own controls
	a_state.uiShown        = widget->IsVisible();

	SyncQuickZoomButton();
}

void RaceCamera::SyncQuickZoomButton()
{
	bool framed = FUCK::IsCameraBodyFramed(this);
	if (framed == _wasFramed)
		return;

	_wasFramed = framed;

	// Presets can change the framing too, so keep RaceMenu's zoom toggle in step
	auto         widget = RaceWidget::GetSingleton();
	auto         movie  = widget->GetCachedMenuMovie();
	RE::GFxValue menuInstance, bPlayerZoom;
	if (movie && widget->GetMenuInstance(movie, menuInstance) && menuInstance.GetMember("bPlayerZoom", &bPlayerZoom) && bPlayerZoom.GetBool() != framed) {
		menuInstance.SetMember("bPlayerZoom", RE::GFxValue(framed));
		menuInstance.Invoke("updateBottomBar", nullptr, nullptr, 0);
	}
}

void RaceCamera::UpdateWireframeLock()
{
	// Lock out camera interaction if a mouse button was clicked down inside the sculpt wireframe
	bool isInteractDown = FUCK::IsInputDown(Keys::kMouse_Right) || FUCK::IsMouseDown(2);

	if (!isInteractDown) {
		_wireframeLock = false;
	} else if (!_wasInteractDown && IsMouseOverWireframe()) {
		_wireframeLock = true;
	}

	_wasInteractDown = isInteractDown;
}

void RaceCamera::SyncCameraTab()
{
	bool isGamepad = FUCK::GetInputDevice() == FUCK::InputDevice::kGamepad;
	bool ctrlDown  = FUCK::IsModifierPressed(FUCK::Modifier::kCtrl);
	bool altDown   = FUCK::IsModifierPressed(FUCK::Modifier::kAlt);
	bool lbDown    = FUCK::IsInputDown(Keys::kGP_LB);

	// Mirror our modifier state onto the Camera Editor's bottom bar toggle
	// (LB on gamepad, Alt on keyboard), so its prompts match what the keys do
	RE::GFxValue cameraEditor;
	if (GetCameraEditor(cameraEditor)) {
		RE::GFxValue secVal;
		cameraEditor.GetMember("_secondary", &secVal);

		bool desiredSecondary = isGamepad ? lbDown : altDown;
		if (secVal.GetBool() != desiredSecondary) {
			cameraEditor.SetMember("_secondary", RE::GFxValue(desiredSecondary));
			cameraEditor.Invoke("updateBottomBar", nullptr, nullptr, 0);
		}
	}

	// Rate multiplier, stepped on key press
	// KBM keys are skipped while Ctrl is held, as E orbits/rolls there, and while typing in the widget
	bool kbmRate  = !ctrlDown && !FUCK::IsAnyItemActive();
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

void RaceCamera::DriveCameraTab()
{
	float rateMult = _cameraRate / DEFAULT_CAMERA_RATE;

	FUCK::CameraMove move;

	if (FUCK::GetInputDevice() == FUCK::InputDevice::kGamepad) {
		// D-Pad, ignored while RB is held so navigating the widget doesn't move the camera
		if (FUCK::IsInputDown(Keys::kGP_RB))
			return;

		float upDown    = InputAxis(Keys::kGP_Up,    Keys::kGP_Down);
		float rightLeft = InputAxis(Keys::kGP_Right, Keys::kGP_Left);

		if (FUCK::IsInputDown(Keys::kGP_LB)) {
			move.zoom  = upDown;
			move.orbit = rightLeft;
		} else {
			move.panZ = upDown;
			move.panX = rightLeft;
		}

		FUCK::MoveCamera(move, FUCK::InputDevice::kGamepad, rateMult * GP_DPAD_MULT);
		return;
	}

	// WASD without modifiers, left to the shared controls while Ctrl is held, and ignored while typing in the widget
	if (FUCK::IsModifierPressed(FUCK::Modifier::kCtrl) || FUCK::IsAnyItemActive())
		return;

	float ws = InputAxis(Keys::kKB_W, Keys::kKB_S);
	float da = InputAxis(Keys::kKB_D, Keys::kKB_A);

	if (FUCK::IsModifierPressed(FUCK::Modifier::kAlt)) {
		move.zoom  = ws;
		move.orbit = da;
	} else {
		move.panZ = ws;
		move.panX = da;
	}

	FUCK::MoveCamera(move, FUCK::InputDevice::kMouseKeyboard, rateMult);
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

void RaceCamera::ToggleQuickZoom()
{
	FUCK::CameraScreenRect rect;
	if (!GetFramingRect(rect))
		return;

	FUCK::SetCameraBodyFraming(this, !FUCK::IsCameraBodyFramed(this), rect);
}

void RaceCamera::ResetOffsets()
{
	FUCK::ResetCamera(this);
	_cameraRate = DEFAULT_CAMERA_RATE;
}

void RaceCamera::EndSession()
{
	FUCK::EndCameraSession(this);
	_isTakenOver = false;
	_wasFramed   = false;
	_cameraRate  = DEFAULT_CAMERA_RATE;
}
