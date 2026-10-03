#include "RACE-Camera.h"
#include "RACE-CameraMaths.h"
#include "RACE-Compat.h"
#include "RACE-Inputs.h"
#include "RACE-Settings.h"
#include "RACE-Widget.h"

using namespace RACE::CameraMath;

bool RaceCamera::IsMouseOverWireframe() const
{
	auto widget = RaceWidget::GetSingleton();
	if (!widget->IsOnSculptTab()) {
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

float RaceCamera::GetFovScale() const
{
	if (_baseFov <= 0.0f)
		return 1.0f;

	// Visible width grows with tan(fov / 2), so this is how much more (or less)
	// of the scene is on screen compared to RaceMenu's default FOV
	return FovScale(_baseFov, _baseFov + _fovOffset);
}

float RaceCamera::GetRaceMenuFov(float a_gameFov)
{
	constexpr float RACEMENU_VFOV = 60.0f;  // vertical FOV in RaceMenu, same at any aspect or game FOV

	// worldFOV isn't the vertical FOV, so measure how it maps to the frustum once per aspect
	ImVec2 display = FUCK::GetDisplaySize();
	float  aspect  = display.y > 0.0f ? display.x / display.y : 0.0f;
	if (_fovToTanV <= 0.0f || std::abs(aspect - _fovAspect) > 0.01f) {
		auto  worldCam = RE::Main::WorldRootCamera();
		float top      = worldCam ? worldCam->GetRuntimeData2().viewFrustum.fTop : 0.0f;
		float tanGame  = TanHalf(a_gameFov);
		if (top <= 0.01f || tanGame <= 0.0f)
			return a_gameFov;

		_fovToTanV = top / tanGame;
		_fovAspect = aspect;
	}

	return FovFromTanHalf(TanHalf(RACEMENU_VFOV) / _fovToTanV);
}

bool RaceCamera::GetCurrentFov(float& a_horizontal, float& a_vertical) const
{
	if (_baseFov <= 0.0f || _fovToTanV <= 0.0f)
		return false;

	float tanV = _fovToTanV * TanHalf(ClampFov(_baseFov + _fovOffset));

	a_vertical   = FovFromTanHalf(tanV);
	a_horizontal = FovFromTanHalf(tanV * _fovAspect);
	return true;
}

float RaceCamera::GetPanScale() const
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

	scale *= GetFovScale();

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

	// Multipliers & Magic Numbers
	constexpr float MOUSE_WHEEL_MULT         =  0.1f;
	constexpr float MOUSE_MOVE_MULT          =  0.002f;
	constexpr float MOUSE_ROT_MULT           =  0.010f;
	constexpr float GP_DPAD_MULT             =  0.5f;

	constexpr float QUICKZOOM_SNAP_TOLERANCE =  0.001f;
	constexpr float PITCH_CLAMP_LIMIT        =  1.55f;
	constexpr float SCROLL_SMOOTH_SPEED      = 15.0f;
	constexpr float QUICKZOOM_SPEED          =  5.0f;
	constexpr float CINEMATIC_SPEED          = 10.0f;
	constexpr float EPSILON                  =  0.001f;

	// Interpolate Quick Zoom swoop
	const float quickZoomTarget = _isQuickZoomed ? 1.0f : 0.0f;
	_quickZoomBlend             = std::lerp(_quickZoomBlend, quickZoomTarget, SmoothFactor(QUICKZOOM_SPEED, a_interval));

	// Snap to target to prevent floating point drift keeping the reset button active
	if (std::abs(_quickZoomBlend - quickZoomTarget) < QUICKZOOM_SNAP_TOLERANCE)
		_quickZoomBlend = quickZoomTarget;

	bool hasSkee         = SKEE64Compat::IsPresent();
	int  mode            = hasSkee ? widget->GetCurrentMode() : 0;
	bool isCameraTab     = (mode == 2);

	bool ctrlDown        = FUCK::IsModifierPressed(FUCK::Modifier::kCtrl);
	bool shiftDown       = FUCK::IsModifierPressed(FUCK::Modifier::kShift);
	bool altDown         = FUCK::IsModifierPressed(FUCK::Modifier::kAlt);

	bool lbDown          = FUCK::IsInputDown(RACE::Keys::kGP_LB);
	bool rbDown          = FUCK::IsInputDown(RACE::Keys::kGP_RB);

	bool mmbDown         = FUCK::IsMouseDown(2);
	bool rmbDown         = FUCK::IsMouseDown(1);
	bool isPopupOpen     = FUCK::IsPopupOpen(nullptr, FUCK::PopupFlags::kAnyPopup);

	bool isAnyUIHovered  = FUCK::IsWindowHovered(1) || FUCK::IsAnyItemHovered();

	// Lock out camera interaction if a mouse button was clicked down inside the sculpt wireframe
	static bool s_wasInteractDown       = false;
	static bool s_wireframeInteractLock = false;

	bool isInteractDown = FUCK::IsInputDown(RACE::Keys::kMouse_Right) || mmbDown;
	if (!isInteractDown) {
		s_wireframeInteractLock = false;
	} else if (isInteractDown && !s_wasInteractDown) {
		if (IsMouseOverWireframe()) {
			s_wireframeInteractLock = true;
		}
	}
	s_wasInteractDown = isInteractDown;

	bool isGamepad = FUCK::GetInputDevice() == FUCK::InputDevice::kGamepad;
	bool disableGP = isGamepad && widget->IsOnSculptTab();

	bool isGlobalKBM   = ctrlDown && !FUCK::IsAnyItemActive();
	bool isGlobalGP    = rbDown && !FUCK::IsAnyItemActive() && !disableGP;
	bool isGlobalMouse = mmbDown && !FUCK::IsAnyItemActive() && !s_wireframeInteractLock && !isAnyUIHovered;

	bool isCameraMode  = isGlobalKBM || isGlobalGP || isCameraTab || isGlobalMouse;

	if (isCameraMode) {
		if (isCameraTab && hasSkee) {
			auto movie = widget->GetCachedMenuMovie();

			// Inject ActionScript sync to update the Camera Editor's bottom bar toggles natively
			if (movie) {
				RE::GFxValue menuInstance;
				if (widget->GetMenuInstance(movie, menuInstance)) {
					RE::GFxValue cameraEditor;
					if (menuInstance.GetMember("cameraEditor", &cameraEditor)) {
						RE::GFxValue secVal;
						cameraEditor.GetMember("_secondary", &secVal);

						// UI toggle reflects the Camera Tab's specific secondary modifiers (LB for GP, Alt for KBM)
						bool desiredSecondary = isGamepad ? lbDown : altDown;

						if (secVal.GetBool() != desiredSecondary) {
							cameraEditor.SetMember("_secondary", RE::GFxValue(desiredSecondary));
							cameraEditor.Invoke("updateBottomBar", nullptr, nullptr, 0);
						}
					}
				}
			}

			// Handle Rate Multiplier (for Racemenu Camera Tab only)
			static bool s_rateUpPressed   = false;
			static bool s_rateDownPressed = false;

			bool rateUp   = FUCK::IsInputDown(RACE::Keys::kGP_A) || FUCK::IsKeyDown(ImGuiKey_Enter);
			bool rateDown = FUCK::IsInputDown(RACE::Keys::kGP_B) || FUCK::IsKeyDown(ImGuiKey_Tab) || FUCK::IsKeyDown(ImGuiKey_Escape);

			if (rateUp && !s_rateUpPressed) {
				_cameraRate = std::min(10.0f, _cameraRate + 1.0f);
				if (movie) {
					RE::GFxValue menuInst;
					if (widget->GetMenuInstance(movie, menuInst)) {
						RE::GFxValue camEd;
						if (menuInst.GetMember("cameraEditor", &camEd)) {
							camEd.SetMember("movementRate", RE::GFxValue(_cameraRate));
							camEd.Invoke("updateBottomBar", nullptr, nullptr, 0);
						}
					}
				}
			}
			if (rateDown && !s_rateDownPressed) {
				_cameraRate = std::max(1.0f, _cameraRate - 1.0f);
				if (movie) {
					RE::GFxValue menuInst;
					if (widget->GetMenuInstance(movie, menuInst)) {
						RE::GFxValue camEd;
						if (menuInst.GetMember("cameraEditor", &camEd)) {
							camEd.SetMember("movementRate", RE::GFxValue(_cameraRate));
							camEd.Invoke("updateBottomBar", nullptr, nullptr, 0);
						}
					}
				}
			}
			s_rateUpPressed   = rateUp;
			s_rateDownPressed = rateDown;
		}

		// Calculate scaled speeds based on the multiplier (UI default is 5.0f, which normalises back to 1.0f)
		float rateMult = isCameraTab ? (_cameraRate / 5.0f) : 1.0f;

		// Pan faster when zoomed out / wide FOV so the subject crosses the screen at a consistent rate
		float panScale = GetPanScale();

		// Orbit & pitch only follow FOV: at a narrow FOV the same angular step swings the
		// subject much further across the screen, so slow rotation down to keep precision
		float rotScale = GetFovScale();

		// --- Mouse Controls ---
		if (ctrlDown && !isAnyUIHovered && !isPopupOpen) {
			if (_pendingScroll != 0.0f && !IsMouseOverWireframe()) {
				if (shiftDown) {
					// FOV (CTRL + SHIFT + Scroll)
					_smoothScrollFov += _pendingScroll * (settings.kbmZoomFovSpeed * rateMult * MOUSE_WHEEL_MULT);
				} else {
					// Zoom (CTRL + Scroll)
					_smoothScrollZoom += _pendingScroll * (settings.kbmZoomFovSpeed * rateMult * MOUSE_WHEEL_MULT);
				}
			}
		}
		_pendingScroll = 0.0f;

		// Apply smoothed scrolling
		float scrollFactor = SmoothFactor(SCROLL_SMOOTH_SPEED, a_interval);
		_targetCamOffset.y += DrainSmoothed(_smoothScrollZoom, scrollFactor, EPSILON);
		_targetFovOffset   -= DrainSmoothed(_smoothScrollFov,  scrollFactor, EPSILON);

		if (mmbDown && !isAnyUIHovered && !isPopupOpen && !s_wireframeInteractLock) {
			ImVec2 mouseDelta = FUCK::GetMouseDelta();

			float mPan   = mouseDelta.x * settings.kbmPanSpeed  * rateMult * panScale * MOUSE_MOVE_MULT;
			float mZ     = mouseDelta.y * settings.kbmPanSpeed  * rateMult * panScale * MOUSE_MOVE_MULT;
			float mRoll  = mouseDelta.x * settings.kbmRollSpeed * rateMult * MOUSE_MOVE_MULT;
			float mOrb   = mouseDelta.x * settings.kbmRotSpeed  * rateMult * rotScale * MOUSE_MOVE_MULT;
			float mPitch = mouseDelta.y * settings.kbmRotSpeed  * rateMult * rotScale * MOUSE_MOVE_MULT;

			if (ctrlDown && shiftDown) {
				// Roll (Middle + Ctrl + Shift)
				_targetCamRoll += mRoll;
			} else if (ctrlDown) {
				// Pitch (Middle + Ctrl)
				_targetCamRotX += mPitch;
			} else if (rmbDown) {
				// Orbit (Middle + Right)
				_targetCamRotZ += mOrb;
			} else {
				// Pan (Middle)
				_targetCamOffset.x -= mPan;
				_targetCamOffset.z += mZ;
			}
		}

		// --- KBM Logic ---
		if (isGlobalKBM || (isCameraTab && FUCK::GetInputDevice() != FUCK::InputDevice::kGamepad)) {
			bool isTranslating = FUCK::IsInputDown(RACE::Keys::kKB_W) || FUCK::IsInputDown(RACE::Keys::kKB_S) || FUCK::IsInputDown(RACE::Keys::kKB_A) || FUCK::IsInputDown(RACE::Keys::kKB_D);

			if (isTranslating) {
				_kbmAcceleration += 1.5f * a_interval;
				if (_kbmAcceleration > 15.0f)
					_kbmAcceleration = 15.0f;
			} else {
				_kbmAcceleration = 1.0f;
			}

			float speed        = settings.kbmPanSpeed     * _kbmAcceleration * rateMult * panScale * a_interval;
			float rotSpeed     = settings.kbmRotSpeed     * rateMult * rotScale * a_interval;
			float rollSpeed    = settings.kbmRollSpeed    * rateMult * a_interval;
			float zoomFovSpeed = settings.kbmZoomFovSpeed * rateMult * a_interval;

			// Support both Global modifiers (Ctrl+Shift) and Camera Tab specific modifiers (Alt)
			bool isCameraTabOnly = isCameraTab && !isGlobalKBM;
			bool isSecondaryKBM  = (isGlobalKBM && shiftDown) || (isCameraTabOnly && altDown);

			if (isSecondaryKBM) {
				if (isCameraTabOnly) {
					// Camera Tab Secondary (Alt): Move Camera X/Z
					if (FUCK::IsInputDown(RACE::Keys::kKB_W))
						_targetCamOffset.z += speed;
					if (FUCK::IsInputDown(RACE::Keys::kKB_S))
						_targetCamOffset.z -= speed;
					if (FUCK::IsInputDown(RACE::Keys::kKB_A))
						_targetCamOffset.x -= speed;
					if (FUCK::IsInputDown(RACE::Keys::kKB_D))
						_targetCamOffset.x += speed;
				} else {
					// Global Secondary (Ctrl+Shift): Zoom, FOV, Roll & Tilt
					if (FUCK::IsInputDown(RACE::Keys::kKB_W))
						_targetCamOffset.y += zoomFovSpeed;
					if (FUCK::IsInputDown(RACE::Keys::kKB_S))
						_targetCamOffset.y -= zoomFovSpeed;
					if (FUCK::IsInputDown(RACE::Keys::kKB_A))
						_targetFovOffset -= zoomFovSpeed;
					if (FUCK::IsInputDown(RACE::Keys::kKB_D))
						_targetFovOffset += zoomFovSpeed;
					if (FUCK::IsInputDown(RACE::Keys::kKB_Q))
						_targetCamRoll -= rollSpeed;
					if (FUCK::IsInputDown(RACE::Keys::kKB_E))
						_targetCamRoll += rollSpeed;
					if (FUCK::IsInputDown(RACE::Keys::kKB_R))
						_targetCamRotX += rotSpeed;
					if (FUCK::IsInputDown(RACE::Keys::kKB_F))
						_targetCamRotX -= rotSpeed;
				}
			} else {
				if (isCameraTabOnly) {
					// Camera Tab Primary: Zoom, Orbit & Tilt
					if (FUCK::IsInputDown(RACE::Keys::kKB_W))
						_targetCamOffset.y += zoomFovSpeed;
					if (FUCK::IsInputDown(RACE::Keys::kKB_S))
						_targetCamOffset.y -= zoomFovSpeed;
					if (FUCK::IsInputDown(RACE::Keys::kKB_A))
						_targetCamRotZ -= rotSpeed;
					if (FUCK::IsInputDown(RACE::Keys::kKB_D))
						_targetCamRotZ += rotSpeed;
					if (FUCK::IsInputDown(RACE::Keys::kKB_Q))
						_targetCamRotX -= rotSpeed;
					if (FUCK::IsInputDown(RACE::Keys::kKB_E))
						_targetCamRotX += rotSpeed;
				} else {
					// Global Primary (Ctrl): Move Camera X/Z, Orbit & Tilt
					if (FUCK::IsInputDown(RACE::Keys::kKB_W))
						_targetCamOffset.z += speed;
					if (FUCK::IsInputDown(RACE::Keys::kKB_S))
						_targetCamOffset.z -= speed;
					if (FUCK::IsInputDown(RACE::Keys::kKB_A))
						_targetCamOffset.x -= speed;
					if (FUCK::IsInputDown(RACE::Keys::kKB_D))
						_targetCamOffset.x += speed;
					if (FUCK::IsInputDown(RACE::Keys::kKB_Q))
						_targetCamRotZ -= rotSpeed;
					if (FUCK::IsInputDown(RACE::Keys::kKB_E))
						_targetCamRotZ += rotSpeed;
					if (FUCK::IsInputDown(RACE::Keys::kKB_R))
						_targetCamRotX += rotSpeed;
					if (FUCK::IsInputDown(RACE::Keys::kKB_F))
						_targetCamRotX -= rotSpeed;
				}
			}
		}

		// --- Gamepad Logic ---
		if (isGlobalGP || (isCameraTab && FUCK::GetInputDevice() == FUCK::InputDevice::kGamepad)) {
			float gpPanSpeed  = settings.gpPanSpeed  * rateMult * panScale * a_interval;
			float gpZoomSpeed = settings.gpZoomSpeed * rateMult * a_interval;
			float gpRotSpeed  = settings.gpRotSpeed  * rateMult * rotScale * a_interval;
			float gpRollSpeed = settings.gpRollSpeed * rateMult * a_interval;
			float gpFovSpeed  = settings.gpFovSpeed  * rateMult * a_interval;
			float deadzone    = settings.gpDeadzone;

			auto ProcessAxis = [deadzone](float val) {
				return ApplyDeadzone(val, deadzone);
			};

			// D-Pad (For Racemenu Camera Tab only)
			// Ignore D-pad if RB is held so it doesn't move the camera when navigating the widget
			if (isCameraTab && !rbDown) {
				bool up    = FUCK::IsInputDown(RACE::Keys::kGP_Up);
				bool down  = FUCK::IsInputDown(RACE::Keys::kGP_Down);
				bool left  = FUCK::IsInputDown(RACE::Keys::kGP_Left);
				bool right = FUCK::IsInputDown(RACE::Keys::kGP_Right);

				if (lbDown) {
					if (up)
						_targetCamOffset.y += gpZoomSpeed * GP_DPAD_MULT;
					if (down)
						_targetCamOffset.y -= gpZoomSpeed * GP_DPAD_MULT;
					if (left)
						_targetCamRotZ     -= gpRotSpeed  * GP_DPAD_MULT;
					if (right)
						_targetCamRotZ     += gpRotSpeed  * GP_DPAD_MULT;
				} else {
					if (up)
						_targetCamOffset.z += gpPanSpeed * GP_DPAD_MULT;
					if (down)
						_targetCamOffset.z -= gpPanSpeed * GP_DPAD_MULT;
					if (left)
						_targetCamOffset.x -= gpPanSpeed * GP_DPAD_MULT;
					if (right)
						_targetCamOffset.x += gpPanSpeed * GP_DPAD_MULT;
				}
			}

			// Thumbsticks (Always requires RB)
			if (isGlobalGP) {
				float leftStickX  = ProcessAxis(FUCK::GetAnalogInput(RACE::Keys::kGP_LSX));
				float leftStickY  = ProcessAxis(FUCK::GetAnalogInput(RACE::Keys::kGP_LSY));
				float rightStickX = ProcessAxis(FUCK::GetAnalogInput(RACE::Keys::kGP_RSX));
				float rightStickY = ProcessAxis(FUCK::GetAnalogInput(RACE::Keys::kGP_RSY));
				float lt          = ProcessAxis(FUCK::GetAnalogInput(RACE::Keys::kGP_LT));
				float rt          = ProcessAxis(FUCK::GetAnalogInput(RACE::Keys::kGP_RT));

				_targetCamOffset.z += (leftStickY * gpPanSpeed);
				_targetCamOffset.x += (leftStickX * gpPanSpeed);
				_targetFovOffset   += (rt - lt) * gpFovSpeed;

				// Roll & Pitch activate if LB + RB is held
				if (lbDown) {
					_targetCamRoll += (rightStickX * gpRollSpeed);
					_targetCamRotX -= (rightStickY * gpRotSpeed);
				} else {
					_targetCamRotZ     -= (rightStickX * gpRotSpeed);
					_targetCamOffset.y += (rightStickY * gpZoomSpeed);
				}
			}
		}

		// Clamp Pitch to prevent gimble flips when viewing directly from top/bottom
		_targetCamRotX = std::clamp(_targetCamRotX, -PITCH_CLAMP_LIMIT, PITCH_CLAMP_LIMIT);
	}

	// Right-Click or Gamepad Character Rotation
	bool isGP_Y      = FUCK::IsInputDown(RACE::Keys::kGP_Y);
	bool isGP_X      = FUCK::IsInputDown(RACE::Keys::kGP_X);
	bool isAnyActive = FUCK::IsAnyItemActive();

	// Prevent input "bleed" when selecting UI items with the gamepad.
	static bool s_lockoutGPRot = false;
	if (isPopupOpen || isAnyActive) {
		s_lockoutGPRot = true;
	} else if (!isGP_Y && !isGP_X) {
		s_lockoutGPRot = false;
	}

	// Block mouse rotation if hovering ImGui windows, sculpt wireframe
	bool allowMouseRot = FUCK::IsInputDown(RACE::Keys::kMouse_Right) && !mmbDown && !isAnyUIHovered && !isPopupOpen && !s_wireframeInteractLock;
	bool allowGPRot    = rbDown && (isGP_Y || isGP_X) && !s_lockoutGPRot;

	bool isRotatingNow = (allowMouseRot || allowGPRot);

	static bool s_wasMouseRot = false;

	if (isRotatingNow) {
		float rotAmount = 0.0f;

		if (allowMouseRot) {
			if (s_wasMouseRot) { // Ignore the very first frame to prevent delta spikes
				rotAmount = FUCK::GetMouseDelta().x * MOUSE_ROT_MULT;
			}
		} else if (allowGPRot) {
			if (isGP_Y)
				rotAmount -= settings.gpCharRotSpeed * a_interval;
			if (isGP_X)
				rotAmount += settings.gpCharRotSpeed * a_interval;
		}

		if (rotAmount != 0.0f) {
			auto player = RE::PlayerCharacter::GetSingleton();
			if (player && player->Is3DLoaded()) {
				if (auto root = player->Get3D(false)) {
					RE::NiMatrix3 deltaMat;
					deltaMat.SetEulerAnglesXYZ(0.0f, 0.0f, -rotAmount);
					
					root->local.rotate = deltaMat * root->local.rotate;

					RE::NiUpdateData ctx;
					root->UpdateWorldData(&ctx);
				}
			}
		}
	}
	s_wasMouseRot = allowMouseRot;

	if (settings.smoothCamera) {
		float lerpSpeed = SmoothFactor(CINEMATIC_SPEED, a_interval);
		_camOffset.x    = std::lerp(_camOffset.x, _targetCamOffset.x, lerpSpeed);
		_camOffset.y    = std::lerp(_camOffset.y, _targetCamOffset.y, lerpSpeed);
		_camOffset.z    = std::lerp(_camOffset.z, _targetCamOffset.z, lerpSpeed);
		_camRotZ        = std::lerp(_camRotZ,     _targetCamRotZ,     lerpSpeed);
		_camRotX        = std::lerp(_camRotX,     _targetCamRotX,     lerpSpeed);
		_camRoll        = std::lerp(_camRoll,     _targetCamRoll,     lerpSpeed);
		_fovOffset      = std::lerp(_fovOffset,   _targetFovOffset,  lerpSpeed);
	} else {
		_camOffset = _targetCamOffset;
		_camRotZ   = _targetCamRotZ;
		_camRotX   = _targetCamRotX;
		_camRoll   = _targetCamRoll;
		_fovOffset = _targetFovOffset;
	}

	if (auto camera = RE::PlayerCamera::GetSingleton()) {
		auto& worldFOV = camera->GetRuntimeData2().worldFOV;
		if (_baseFov == 0.0f && worldFOV != 0.0f) {
			_gameFov = worldFOV;
			_baseFov = GetRaceMenuFov(worldFOV);
		}
		if (_baseFov != 0.0f)
			worldFOV = _baseFov + _fovOffset;
	}
}

void RaceCamera::UpdateQuickZoomFraming(const RE::NiPoint3& a_headPos, const RE::NiPoint3& a_pelvisPos, const RE::NiPoint3& a_rootPos)
{
	// Quick Zoom framing
	constexpr float QUICKZOOM_MARGIN       = 1.25f;        // 1.0 = feet touch screen edge
	constexpr float QUICKZOOM_SCREEN_X     = 1.0f / 3.0f;  // -1 = left edge, 1 = right edge (1/3 = right thirds line)
	constexpr float FALLBACK_TAN_HALF_VFOV = 0.364f;       // ~40 deg vertical FOV

	// tan(half FOV) at the current FOV, horizontal from screen aspect
	float tanV = FALLBACK_TAN_HALF_VFOV * GetFovScale();
	if (auto worldCam = RE::Main::WorldRootCamera()) {
		float top = worldCam->GetRuntimeData2().viewFrustum.fTop;
		if (top > 0.01f)
			tanV = top;
	}

	ImVec2 display = FUCK::GetDisplaySize();
	float  aspect  = (display.x > 0.0f && display.y > 0.0f) ? display.x / display.y : 16.0f / 9.0f;

	// Fit full body height, then shift sideways so the pelvis lands on QUICKZOOM_SCREEN_X
	auto framing = FrameFullBody(a_pelvisPos, a_rootPos, _originalRotate, tanV, aspect, QUICKZOOM_MARGIN, QUICKZOOM_SCREEN_X);
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

void RaceCamera::ToggleQuickZoom()
{
	_isQuickZoomed = !_isQuickZoomed;
	if (_isQuickZoomed)
		_hasQuickZoomFraming = false;

	// Anchor to the face/body framing by easing out manual moves
	_targetCamOffset  = { 0.0f, 0.0f, 0.0f };
	_targetCamRotZ    = 0.0f;
	_targetCamRotX    = 0.0f;
	_smoothScrollZoom = 0.0f;
}

void RaceCamera::ResetOffsets()
{
	if (auto camera = RE::PlayerCamera::GetSingleton()) {
		if (_gameFov != 0.0f)
			camera->GetRuntimeData2().worldFOV = _gameFov;
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
	_gameFov             = 0.0f;
	_cameraRate          = 5.0f;
	_isQuickZoomed       = false;
	_quickZoomBlend      = 0.0f;
	_quickZoomDistance   = 0.0f;
	_hasQuickZoomFraming = false;
	_smoothScrollZoom    = 0.0f;
	_smoothScrollFov     = 0.0f;
}
