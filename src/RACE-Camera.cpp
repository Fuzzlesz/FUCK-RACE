#include "RACE-Camera.h"
#include "RACE-Compat.h"
#include "RACE-Inputs.h"
#include "RACE-Widget.h"

void RaceCamera::LoadSettings(CSimpleIniA& a_ini)
{
	CameraSettings def;

	_settings.kbmPanSpeed         = FUCK::INI::LoadFloat(a_ini, "Camera", "KBMPanSpeed",          def.kbmPanSpeed);
	_settings.kbmRotSpeed         = FUCK::INI::LoadFloat(a_ini, "Camera", "KBMRotSpeed",          def.kbmRotSpeed);
	_settings.kbmRollSpeed        = FUCK::INI::LoadFloat(a_ini, "Camera", "KBMRollSpeed",         def.kbmRollSpeed);
	_settings.kbmFovSpeed         = FUCK::INI::LoadFloat(a_ini, "Camera", "KBMFovSpeed",          def.kbmFovSpeed);
	_settings.mouseRotMult        = FUCK::INI::LoadFloat(a_ini, "Camera", "MouseRotMult",         def.mouseRotMult);
	_settings.mousePanMult        = FUCK::INI::LoadFloat(a_ini, "Camera", "MousePanMult",         def.mousePanMult);

	_settings.gpPanSpeed          = FUCK::INI::LoadFloat(a_ini, "Camera", "GPPanSpeed",           def.gpPanSpeed);
	_settings.gpZoomSpeed         = FUCK::INI::LoadFloat(a_ini, "Camera", "GPZoomSpeed",          def.gpZoomSpeed);
	_settings.gpRotSpeed          = FUCK::INI::LoadFloat(a_ini, "Camera", "GPRotSpeed",           def.gpRotSpeed);
	_settings.gpRollSpeed         = FUCK::INI::LoadFloat(a_ini, "Camera", "GPRollSpeed",          def.gpRollSpeed);
	_settings.gpFovSpeed          = FUCK::INI::LoadFloat(a_ini, "Camera", "GPFovSpeed",           def.gpFovSpeed);
	_settings.gpCharRotSpeed      = FUCK::INI::LoadFloat(a_ini, "Camera", "GPCharRotSpeed",       def.gpCharRotSpeed);
	_settings.gpDeadzone          = FUCK::INI::LoadFloat(a_ini, "Camera", "GPDeadzone",           def.gpDeadzone);

	_settings.quickZoomOffset     = FUCK::INI::LoadFloat(a_ini, "Camera", "QuickZoomOffset",      def.quickZoomOffset);
	_settings.quickZoomDownOffset = FUCK::INI::LoadFloat(a_ini, "Camera", "QuickZoomDownOffset",  def.quickZoomDownOffset);
	_settings.quickZoomSideOffset = FUCK::INI::LoadFloat(a_ini, "Camera", "QuickZoomSideOffset",  def.quickZoomSideOffset);
}

void RaceCamera::SaveSettings(CSimpleIniA& a_ini)
{
	CameraSettings def;

	FUCK::INI::SaveDouble(a_ini, "Camera", "KBMPanSpeed",         _settings.kbmPanSpeed,          def.kbmPanSpeed);
	FUCK::INI::SaveDouble(a_ini, "Camera", "KBMRotSpeed",         _settings.kbmRotSpeed,          def.kbmRotSpeed);
	FUCK::INI::SaveDouble(a_ini, "Camera", "KBMRollSpeed",        _settings.kbmRollSpeed,         def.kbmRollSpeed);
	FUCK::INI::SaveDouble(a_ini, "Camera", "KBMFovSpeed",         _settings.kbmFovSpeed,          def.kbmFovSpeed);
	FUCK::INI::SaveDouble(a_ini, "Camera", "MouseRotMult",        _settings.mouseRotMult,         def.mouseRotMult);
	FUCK::INI::SaveDouble(a_ini, "Camera", "MousePanMult",        _settings.mousePanMult,         def.mousePanMult);

	FUCK::INI::SaveDouble(a_ini, "Camera", "GPPanSpeed",          _settings.gpPanSpeed,           def.gpPanSpeed);
	FUCK::INI::SaveDouble(a_ini, "Camera", "GPZoomSpeed",         _settings.gpZoomSpeed,          def.gpZoomSpeed);
	FUCK::INI::SaveDouble(a_ini, "Camera", "GPRotSpeed",          _settings.gpRotSpeed,           def.gpRotSpeed);
	FUCK::INI::SaveDouble(a_ini, "Camera", "GPRollSpeed",         _settings.gpRollSpeed,          def.gpRollSpeed);
	FUCK::INI::SaveDouble(a_ini, "Camera", "GPFovSpeed",          _settings.gpFovSpeed,           def.gpFovSpeed);
	FUCK::INI::SaveDouble(a_ini, "Camera", "GPCharRotSpeed",      _settings.gpCharRotSpeed,       def.gpCharRotSpeed);
	FUCK::INI::SaveDouble(a_ini, "Camera", "GPDeadzone",          _settings.gpDeadzone,           def.gpDeadzone);

	FUCK::INI::SaveDouble(a_ini, "Camera", "QuickZoomOffset",     _settings.quickZoomOffset,      def.quickZoomOffset);
	FUCK::INI::SaveDouble(a_ini, "Camera", "QuickZoomDownOffset", _settings.quickZoomDownOffset,  def.quickZoomDownOffset);
	FUCK::INI::SaveDouble(a_ini, "Camera", "QuickZoomSideOffset", _settings.quickZoomSideOffset,  def.quickZoomSideOffset);
}

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
		a_cameraRoot->local.translate = _originalTranslate;
		a_cameraRoot->local.rotate    = _originalRotate;
		_wasModified                  = false;
	}
}

void RaceCamera::HandleInput(float a_interval)
{
	auto widget = RaceWidget::GetSingleton();
	if (widget->IsJournalOpen()) {
		return;
	}

	a_interval = std::min(a_interval, 0.1f);

	// Interpolate Quick Zoom swoop
	_currentZoomOffset     = std::lerp(_currentZoomOffset,     _targetZoomOffset,     5.0f * a_interval);
	_currentZoomDownOffset = std::lerp(_currentZoomDownOffset, _targetZoomDownOffset, 5.0f * a_interval);
	_currentZoomSideOffset = std::lerp(_currentZoomSideOffset, _targetZoomSideOffset, 5.0f * a_interval);

	// Snap to target to prevent floating point drift keeping the reset button active
	if (std::abs(_currentZoomOffset - _targetZoomOffset) < 0.05f)
		_currentZoomOffset = _targetZoomOffset;
	if (std::abs(_currentZoomDownOffset - _targetZoomDownOffset) < 0.05f)
		_currentZoomDownOffset = _targetZoomDownOffset;
	if (std::abs(_currentZoomSideOffset - _targetZoomSideOffset) < 0.05f)
		_currentZoomSideOffset = _targetZoomSideOffset;

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

	bool isGlobalKBM   = ctrlDown && !FUCK::IsAnyItemActive();
	bool isGlobalGP    = rbDown && !FUCK::IsAnyItemActive();
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
						bool isGamepad = FUCK::GetInputDevice() == FUCK::InputDevice::kGamepad;

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

		// --- Mouse Controls ---
		if (ctrlDown && !isAnyUIHovered && !isPopupOpen) {
			if (_pendingScroll != 0.0f && !IsMouseOverWireframe()) {
				if (shiftDown) {
					// FOV (CTRL + SHIFT + Scroll)
					_fovOffset -= _pendingScroll * (_settings.kbmFovSpeed * 0.2f);
				} else {
					// Zoom (CTRL + Scroll)
					_camOffset.y += _pendingScroll * (_settings.kbmPanSpeed * rateMult * 0.5f);
				}
			}
		}
		_pendingScroll = 0.0f;

		if (mmbDown && !isAnyUIHovered && !isPopupOpen && !s_wireframeInteractLock) {
			ImVec2 mouseDelta = FUCK::GetMouseDelta();

			float mPan   = mouseDelta.x * _settings.kbmPanSpeed  * rateMult * _settings.mousePanMult;
			float mZ     = mouseDelta.y * _settings.kbmPanSpeed  * rateMult * _settings.mousePanMult;
			float mRoll  = mouseDelta.x * _settings.kbmRollSpeed * rateMult * 0.015f;
			float mOrb   = mouseDelta.x * _settings.kbmRotSpeed  * rateMult * 0.015f;
			float mPitch = mouseDelta.y * _settings.kbmRotSpeed  * rateMult * 0.015f;

			if (rmbDown) {
				// Orbit & Pitch (Middle + Right)
				_camRotZ += mOrb;
				_camRotX += mPitch;
			} else if (shiftDown) {
				// Roll (Middle + Shift)
				_camRoll += mRoll;
			} else {
				// Pan (Middle)
				_camOffset.x -= mPan;
				_camOffset.z += mZ;
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

			float speed     = _settings.kbmPanSpeed  * _kbmAcceleration * rateMult * a_interval;
			float rotSpeed  = _settings.kbmRotSpeed  * rateMult * a_interval;
			float rollSpeed = _settings.kbmRollSpeed * rateMult * a_interval;
			float fovSpeed  = _settings.kbmFovSpeed  * rateMult * a_interval;

			// Support both Global modifiers (Ctrl+Shift) and Camera Tab specific modifiers (Alt)
			bool isCameraTabOnly = isCameraTab && !isGlobalKBM;
			bool isSecondaryKBM  = (isGlobalKBM && shiftDown) || (isCameraTabOnly && altDown);

			if (isSecondaryKBM) {
				if (isCameraTabOnly) {
					// Camera Tab Secondary (Alt): Move Camera X/Z
					if (FUCK::IsInputDown(RACE::Keys::kKB_W))
						_camOffset.z += speed;
					if (FUCK::IsInputDown(RACE::Keys::kKB_S))
						_camOffset.z -= speed;
					if (FUCK::IsInputDown(RACE::Keys::kKB_A))
						_camOffset.x -= speed;
					if (FUCK::IsInputDown(RACE::Keys::kKB_D))
						_camOffset.x += speed;
				} else {
					// Global Secondary (Ctrl+Shift): Zoom, FOV, Roll & Tilt
					if (FUCK::IsInputDown(RACE::Keys::kKB_W))
						_camOffset.y += speed;
					if (FUCK::IsInputDown(RACE::Keys::kKB_S))
						_camOffset.y -= speed;
					if (FUCK::IsInputDown(RACE::Keys::kKB_A))
						_fovOffset -= fovSpeed;
					if (FUCK::IsInputDown(RACE::Keys::kKB_D))
						_fovOffset += fovSpeed;
					if (FUCK::IsInputDown(RACE::Keys::kKB_Q))
						_camRoll -= rollSpeed;
					if (FUCK::IsInputDown(RACE::Keys::kKB_E))
						_camRoll += rollSpeed;
					if (FUCK::IsInputDown(RACE::Keys::kKB_R))
						_camRotX += rotSpeed;
					if (FUCK::IsInputDown(RACE::Keys::kKB_F))
						_camRotX -= rotSpeed;
				}
			} else {
				if (isCameraTabOnly) {
					// Camera Tab Primary: Zoom, Orbit & Tilt
					if (FUCK::IsInputDown(RACE::Keys::kKB_W))
						_camOffset.y += speed;
					if (FUCK::IsInputDown(RACE::Keys::kKB_S))
						_camOffset.y -= speed;
					if (FUCK::IsInputDown(RACE::Keys::kKB_A))
						_camRotZ -= rotSpeed;
					if (FUCK::IsInputDown(RACE::Keys::kKB_D))
						_camRotZ += rotSpeed;
					if (FUCK::IsInputDown(RACE::Keys::kKB_Q))
						_camRotX -= rotSpeed;
					if (FUCK::IsInputDown(RACE::Keys::kKB_E))
						_camRotX += rotSpeed;
				} else {
					// Global Primary (Ctrl): Move Camera X/Z, Orbit & Tilt
					if (FUCK::IsInputDown(RACE::Keys::kKB_W))
						_camOffset.z += speed;
					if (FUCK::IsInputDown(RACE::Keys::kKB_S))
						_camOffset.z -= speed;
					if (FUCK::IsInputDown(RACE::Keys::kKB_A))
						_camOffset.x -= speed;
					if (FUCK::IsInputDown(RACE::Keys::kKB_D))
						_camOffset.x += speed;
					if (FUCK::IsInputDown(RACE::Keys::kKB_Q))
						_camRotZ -= rotSpeed;
					if (FUCK::IsInputDown(RACE::Keys::kKB_E))
						_camRotZ += rotSpeed;
					if (FUCK::IsInputDown(RACE::Keys::kKB_R))
						_camRotX += rotSpeed;
					if (FUCK::IsInputDown(RACE::Keys::kKB_F))
						_camRotX -= rotSpeed;
				}
			}
		}

		// --- Gamepad Logic ---
		if (isGlobalGP || (isCameraTab && FUCK::GetInputDevice() == FUCK::InputDevice::kGamepad)) {
			float gpPanSpeed  = _settings.gpPanSpeed  * rateMult * a_interval;
			float gpZoomSpeed = _settings.gpZoomSpeed * rateMult * a_interval;
			float gpRotSpeed  = _settings.gpRotSpeed  * rateMult * a_interval;
			float gpRollSpeed = _settings.gpRollSpeed * rateMult * a_interval;
			float gpFovSpeed  = _settings.gpFovSpeed  * rateMult * a_interval;
			float deadzone    = _settings.gpDeadzone;

			auto ProcessAxis = [deadzone](float val) {
				if (std::abs(val) < deadzone)
					return 0.0f;
				float normalized = (std::abs(val) - deadzone) / (1.0f - deadzone);
				float curved     = normalized * normalized;
				return val < 0.0f ? -curved : curved;
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
						_camOffset.y += gpZoomSpeed * 0.5f;
					if (down)
						_camOffset.y -= gpZoomSpeed * 0.5f;
					if (left)
						_camRotZ -= gpRotSpeed * 0.5f;
					if (right)
						_camRotZ += gpRotSpeed * 0.5f;
				} else {
					if (up)
						_camOffset.z += gpPanSpeed * 0.5f;
					if (down)
						_camOffset.z -= gpPanSpeed * 0.5f;
					if (left)
						_camOffset.x -= gpPanSpeed * 0.5f;
					if (right)
						_camOffset.x += gpPanSpeed * 0.5f;
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

				_camOffset.z += (leftStickY * gpPanSpeed);
				_camOffset.x += (leftStickX * gpPanSpeed);
				_fovOffset   += (rt - lt) * gpFovSpeed;

				// Roll & Pitch activate if LB + RB is held
				if (lbDown) {
					_camRoll += (rightStickX * gpRollSpeed);
					_camRotX -= (rightStickY * gpRotSpeed);
				} else {
					_camRotZ     -= (rightStickX * gpRotSpeed);
					_camOffset.y += (rightStickY * gpZoomSpeed);
				}
			}
		}

		// Clamp Pitch to prevent gimble flips when viewing directly from top/bottom
		_camRotX = std::clamp(_camRotX, -1.55f, 1.55f);
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

	if (isRotatingNow) {
		float rotAmount = 0.0f;

		if (allowMouseRot) {
			rotAmount = FUCK::GetMouseDelta().x * _settings.mouseRotMult;
		} else if (allowGPRot) {
			if (isGP_Y)
				rotAmount -= _settings.gpCharRotSpeed * a_interval;
			if (isGP_X)
				rotAmount += _settings.gpCharRotSpeed * a_interval;
		}

		if (rotAmount != 0.0f) {
			auto player = RE::PlayerCharacter::GetSingleton();
			if (player && player->Is3DLoaded()) {
				if (auto root = player->Get3D(false)) {
					if (!_isCharAngleCaptured) {
						root->local.rotate.ToEulerAnglesXYZ(_charAngle);
						_isCharAngleCaptured = true;
					}

					_charAngle.z -= rotAmount;
					root->local.rotate.SetEulerAnglesXYZ(_charAngle);

					RE::NiUpdateData ctx;
					root->UpdateWorldData(&ctx);
				}
			}
		}
	} else {
		_isCharAngleCaptured = false;
	}

	if (auto camera = RE::PlayerCamera::GetSingleton()) {
		if (_baseFov == 0.0f && camera->worldFOV != 0.0f)
			_baseFov = camera->worldFOV;
		if (_baseFov != 0.0f)
			camera->worldFOV = _baseFov + _fovOffset;
	}
}

void RaceCamera::ApplyTransform(RE::NiNode* a_cameraRoot)
{
	if (!a_cameraRoot || !HasAnyCamera())
		return;

	_originalTranslate = a_cameraRoot->local.translate;
	_originalRotate    = a_cameraRoot->local.rotate;
	_wasModified       = true;

	RE::NiPoint3 pivot  = _originalTranslate;
	auto         player = RE::PlayerCharacter::GetSingleton();
	if (player) {
		if (auto headNode = player->GetNodeByName("NPC Head [Head]")) {
			pivot = headNode->world.translate;
		} else {
			pivot = player->GetPosition();
		}
	}

	RE::NiPoint3 radiusVec = _originalTranslate - pivot;
	radiusVec.z            = 0.0f;

	float cosZ = std::cos(_camRotZ);
	float sinZ = std::sin(_camRotZ);

	RE::NiPoint3 orbitedTranslate;
	orbitedTranslate.x = pivot.x + (radiusVec.x * cosZ - radiusVec.y * sinZ);
	orbitedTranslate.y = pivot.y + (radiusVec.x * sinZ + radiusVec.y * cosZ);
	orbitedTranslate.z = _originalTranslate.z;

	RE::NiMatrix3 orbitedRotate;
	for (int i = 0; i < 3; ++i) {
		orbitedRotate.entry[0][i] = cosZ * _originalRotate.entry[0][i] - sinZ * _originalRotate.entry[1][i];
		orbitedRotate.entry[1][i] = sinZ * _originalRotate.entry[0][i] + cosZ * _originalRotate.entry[1][i];
		orbitedRotate.entry[2][i] = _originalRotate.entry[2][i];
	}

	// Pitch Matrix (Rotation around local X-axis / Right vector)
	RE::NiMatrix3 pitchMat;
	float         cosP = std::cos(_camRotX);
	float         sinP = std::sin(_camRotX);

	pitchMat.entry[0][0] = 1.0f;
	pitchMat.entry[0][1] = 0.0f;
	pitchMat.entry[0][2] = 0.0f;
	pitchMat.entry[1][0] = 0.0f;
	pitchMat.entry[1][1] = cosP;
	pitchMat.entry[1][2] = -sinP;
	pitchMat.entry[2][0] = 0.0f;
	pitchMat.entry[2][1] = sinP;
	pitchMat.entry[2][2] = cosP;

	// Roll Matrix (Rotation around local Y-axis / Forward vector)
	RE::NiMatrix3 rollMat;
	float         cosR = std::cos(_camRoll);
	float         sinR = std::sin(_camRoll);

	rollMat.entry[0][0] = cosR;
	rollMat.entry[0][1] = 0.0f;
	rollMat.entry[0][2] = -sinR;
	rollMat.entry[1][0] = 0.0f;
	rollMat.entry[1][1] = 1.0f;
	rollMat.entry[1][2] = 0.0f;
	rollMat.entry[2][0] = sinR;
	rollMat.entry[2][1] = 0.0f;
	rollMat.entry[2][2] = cosR;

	RE::NiPoint3 localOffset = _camOffset;
	localOffset.y += _currentZoomOffset;
	localOffset.z -= _currentZoomDownOffset;
	localOffset.x += _currentZoomSideOffset;

	RE::NiPoint3 finalOffset      = (orbitedRotate * localOffset);
	a_cameraRoot->local.translate = orbitedTranslate + finalOffset;
	a_cameraRoot->local.rotate    = orbitedRotate * pitchMat * rollMat;

	RE::NiUpdateData ctx;
	a_cameraRoot->UpdateWorldData(&ctx);
}

void RaceCamera::ToggleQuickZoom()
{
	_isQuickZoomed = !_isQuickZoomed;
	if (_isQuickZoomed) {
		_targetZoomOffset     = -_settings.quickZoomOffset;
		_targetZoomDownOffset = _settings.quickZoomDownOffset;
		_targetZoomSideOffset = _settings.quickZoomSideOffset;
	} else {
		_targetZoomOffset     = 0.0f;
		_targetZoomDownOffset = 0.0f;
		_targetZoomSideOffset = 0.0f;
	}
}

void RaceCamera::ResetOffsets()
{
	if (auto camera = RE::PlayerCamera::GetSingleton()) {
		if (_baseFov != 0.0f)
			camera->worldFOV = _baseFov;
		RevertCameraTransform(camera->cameraRoot.get());
	}
	_camOffset             = { 0.0f, 0.0f, 0.0f };
	_camRotZ               = 0.0f;
	_camRotX               = 0.0f;
	_camRoll               = 0.0f;
	_fovOffset             = 0.0f;
	_baseFov               = 0.0f;
	_cameraRate            = 5.0f;
	_isQuickZoomed         = false;
	_targetZoomOffset      = 0.0f;
	_currentZoomOffset     = 0.0f;
	_targetZoomDownOffset  = 0.0f;
	_currentZoomDownOffset = 0.0f;
	_targetZoomSideOffset  = 0.0f;
	_currentZoomSideOffset = 0.0f;
	_isCharAngleCaptured   = false;
}
