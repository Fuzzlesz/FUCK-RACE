#include "RACE-Camera.h"
#include "RACE-Compat.h"
#include "RACE-Inputs.h"
#include "RACE-Widget.h"

void RaceCamera::LoadSettings(CSimpleIniA& a_ini)
{
	_settings.kbmPanSpeed         = FUCK::INI::LoadFloat(a_ini, "Camera", "KBMPanSpeed",          10.0f);
	_settings.kbmRotSpeed         = FUCK::INI::LoadFloat(a_ini, "Camera", "KBMRotSpeed",           1.5f);
	_settings.kbmRollSpeed        = FUCK::INI::LoadFloat(a_ini, "Camera", "KBMRollSpeed",          1.5f);
	_settings.kbmFovSpeed         = FUCK::INI::LoadFloat(a_ini, "Camera", "KBMFovSpeed",          30.0f);
	_settings.mouseRotMult        = FUCK::INI::LoadFloat(a_ini, "Camera", "MouseRotMult",          0.015f);

	_settings.gpPanSpeed          = FUCK::INI::LoadFloat(a_ini, "Camera", "GPPanSpeed",          100.0f);
	_settings.gpZoomSpeed         = FUCK::INI::LoadFloat(a_ini, "Camera", "GPZoomSpeed",         100.0f);
	_settings.gpRotSpeed          = FUCK::INI::LoadFloat(a_ini, "Camera", "GPRotSpeed",            2.0f);
	_settings.gpRollSpeed         = FUCK::INI::LoadFloat(a_ini, "Camera", "GPRollSpeed",           2.0f);
	_settings.gpFovSpeed          = FUCK::INI::LoadFloat(a_ini, "Camera", "GPFovSpeed",           30.0f);
	_settings.gpCharRotSpeed      = FUCK::INI::LoadFloat(a_ini, "Camera", "GPCharRotSpeed",        3.0f);
	_settings.gpDeadzone          = FUCK::INI::LoadFloat(a_ini, "Camera", "GPDeadzone",            0.25f);

	_settings.quickZoomOffset     = FUCK::INI::LoadFloat(a_ini, "Camera", "QuickZoomOffset",     100.0f);
	_settings.quickZoomDownOffset = FUCK::INI::LoadFloat(a_ini, "Camera", "QuickZoomDownOffset",  45.0f);
	_settings.quickZoomSideOffset = FUCK::INI::LoadFloat(a_ini, "Camera", "QuickZoomSideOffset", -30.0f);
}

void RaceCamera::SaveSettings(CSimpleIniA& a_ini)
{
	FUCK::INI::SaveDouble(a_ini, "Camera", "KBMPanSpeed",         _settings.kbmPanSpeed,          10.0f);
	FUCK::INI::SaveDouble(a_ini, "Camera", "KBMRotSpeed",         _settings.kbmRotSpeed,           1.5f);
	FUCK::INI::SaveDouble(a_ini, "Camera", "KBMRollSpeed",        _settings.kbmRollSpeed,          1.5f);
	FUCK::INI::SaveDouble(a_ini, "Camera", "KBMFovSpeed",         _settings.kbmFovSpeed,          30.0f);
	FUCK::INI::SaveDouble(a_ini, "Camera", "MouseRotMult",        _settings.mouseRotMult,          0.015f);

	FUCK::INI::SaveDouble(a_ini, "Camera", "GPPanSpeed",          _settings.gpPanSpeed,          100.0f);
	FUCK::INI::SaveDouble(a_ini, "Camera", "GPZoomSpeed",         _settings.gpZoomSpeed,         100.0f);
	FUCK::INI::SaveDouble(a_ini, "Camera", "GPRotSpeed",          _settings.gpRotSpeed,            2.0f);
	FUCK::INI::SaveDouble(a_ini, "Camera", "GPRollSpeed",         _settings.gpRollSpeed,           2.0f);
	FUCK::INI::SaveDouble(a_ini, "Camera", "GPFovSpeed",          _settings.gpFovSpeed,           30.0f);
	FUCK::INI::SaveDouble(a_ini, "Camera", "GPCharRotSpeed",      _settings.gpCharRotSpeed,        3.0f);
	FUCK::INI::SaveDouble(a_ini, "Camera", "GPDeadzone",          _settings.gpDeadzone,            0.25f);

	FUCK::INI::SaveDouble(a_ini, "Camera", "QuickZoomOffset",     _settings.quickZoomOffset,     100.0f);
	FUCK::INI::SaveDouble(a_ini, "Camera", "QuickZoomDownOffset", _settings.quickZoomDownOffset,  45.0f);
	FUCK::INI::SaveDouble(a_ini, "Camera", "QuickZoomSideOffset", _settings.quickZoomSideOffset, -30.0f);
}

void RaceCamera::RevertCameraTransform(RE::NiNode* a_cameraRoot)
{
	if (a_cameraRoot && _wasModified) {
		a_cameraRoot->local.translate = _originalTranslate;
		a_cameraRoot->local.rotate    = _originalRotate;
		_wasModified                  = false;
	}
}

void RaceCamera::HandleInput(float a_interval, bool a_isFrozen)
{
	auto ui = RE::UI::GetSingleton();
	if (ui && ui->IsMenuOpen(RE::JournalMenu::MENU_NAME)) {
		return;
	}

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

	bool hasSkee       = SKEE64Compat::IsPresent();
	int  mode          = hasSkee ? RaceWidget::GetSingleton()->GetCurrentMode() : 0;
	bool isCameraTab   = (mode == 2);

	bool ctrlDown      = FUCK::IsModifierPressed(FUCK::Modifier::kCtrl);
	bool shiftDown     = FUCK::IsModifierPressed(FUCK::Modifier::kShift);
	bool altDown       = FUCK::IsModifierPressed(FUCK::Modifier::kAlt);

	bool lbDown        = FUCK::IsInputDown(RACE::Keys::kGP_LB);
	bool rbDown        = FUCK::IsInputDown(RACE::Keys::kGP_RB);

	bool mmbDown       = FUCK::IsMouseDown(2);
	bool rmbDown       = FUCK::IsMouseDown(1);
	bool isPopupOpen   = FUCK::IsPopupOpen(nullptr, FUCK::PopupFlags::kAnyPopup);

	bool isGlobalKBM   = ctrlDown && !FUCK::IsAnyItemActive();
	bool isGlobalGP    = rbDown && !FUCK::IsAnyItemActive();
	bool isGlobalMouse = mmbDown && !FUCK::IsAnyItemActive();

	bool isCameraMode  = isGlobalKBM || isGlobalGP || isCameraTab || isGlobalMouse;

	if (isCameraMode) {
		if (isCameraTab && hasSkee) {
			auto menu = ui ? ui->GetMenu(RE::RaceSexMenu::MENU_NAME) : nullptr;

			// Inject ActionScript sync to update the Camera Editor's bottom bar toggles natively
			if (menu && menu->uiMovie) {
				RE::GFxValue menuInstance;
				if (RaceWidget::GetSingleton()->GetMenuInstance(menu->uiMovie.get(), menuInstance)) {
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
				if (menu && menu->uiMovie) {
					RE::GFxValue menuInst;
					if (RaceWidget::GetSingleton()->GetMenuInstance(menu->uiMovie.get(), menuInst)) {
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
				if (menu && menu->uiMovie) {
					RE::GFxValue menuInst;
					if (RaceWidget::GetSingleton()->GetMenuInstance(menu->uiMovie.get(), menuInst)) {
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
		if (ctrlDown && !FUCK::IsWindowHovered(0) && !isPopupOpen) {
			if (_pendingScroll != 0.0f) {
				_fovOffset -= _pendingScroll * (_settings.kbmFovSpeed * 0.2f);
			}
		}
		_pendingScroll = 0.0f;

		if (mmbDown && !FUCK::IsWindowHovered(0) && !isPopupOpen) {
			ImVec2 mouseDelta = FUCK::GetMouseDelta();

			float mPan  = mouseDelta.x * _settings.kbmPanSpeed  * rateMult * 0.015f;
			float mZ    = mouseDelta.y * _settings.kbmPanSpeed  * rateMult * 0.015f;
			float mZoom = mouseDelta.y * _settings.kbmPanSpeed  * rateMult * 0.015f;
			float mRoll = mouseDelta.x * _settings.kbmRollSpeed * rateMult * 0.015f;
			float mOrb  = mouseDelta.x * _settings.kbmRotSpeed  * rateMult * 0.015f;

			if (rmbDown) {
				// Orbit (Middle + Right)
				_camRotZ += mOrb;
			} else if (shiftDown) {
				// Zoom & Roll (Middle + Shift)
				_camOffset.y -= mZoom;
				_camRoll += mRoll;
			} else {
				// Pan (Middle)
				_camOffset.x -= mPan;
				_camOffset.z -= mZ;
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
					// Global Secondary (Ctrl+Shift): Zoom, FOV & Roll
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
				}
			} else {
				if (isCameraTabOnly) {
					// Camera Tab Primary: Zoom & Rotate
					if (FUCK::IsInputDown(RACE::Keys::kKB_W))
						_camOffset.y += speed;
					if (FUCK::IsInputDown(RACE::Keys::kKB_S))
						_camOffset.y -= speed;
					if (FUCK::IsInputDown(RACE::Keys::kKB_A))
						_camRotZ -= rotSpeed;
					if (FUCK::IsInputDown(RACE::Keys::kKB_D))
						_camRotZ += rotSpeed;
				} else {
					// Global Primary (Ctrl): Move Camera X/Z & Rotate
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
				_camOffset.y += (rightStickY * gpZoomSpeed);
				_fovOffset += (rt - lt) * gpFovSpeed;

				// Roll only activates if LB + RB is held
				if (lbDown) {
					_camRoll += (rightStickX * gpRollSpeed);
				} else {
					_camRotZ -= (rightStickX * gpRotSpeed);
				}
			}
		}
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

	bool allowMouseRot = FUCK::IsInputDown(RACE::Keys::kMouse_Right) && !mmbDown && !FUCK::IsWindowHovered(0) && !isPopupOpen;
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
				RE::NiPoint3 newAngle = player->data.angle;
				newAngle.z -= rotAmount;
				player->SetAngle(newAngle);

				if (a_isFrozen) {
					player->EnableAI(true);
					player->Update3DPosition(true);
					player->EnableAI(false);
				} else {
					player->Update3DPosition(true);
				}
			}
		}
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
	a_cameraRoot->local.rotate    = orbitedRotate * rollMat;

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
	}
	_camOffset             = { 0.0f, 0.0f, 0.0f };
	_camRotZ               = 0.0f;
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
}
