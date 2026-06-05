#include "RACE-Camera.h"
#include "RACE-Compat.h"
#include "RACE-Inputs.h"
#include "RACE-Widget.h"

void RaceCamera::LoadSettings(CSimpleIniA& a_ini)
{
	_settings.kbmPanSpeed  = FUCK::INI::LoadFloat(a_ini, "Camera", "KBMPanSpeed", 10.0f);
	_settings.kbmRotSpeed  = FUCK::INI::LoadFloat(a_ini, "Camera", "KBMRotSpeed",  1.5f);
	_settings.kbmFovSpeed  = FUCK::INI::LoadFloat(a_ini, "Camera", "KBMFovSpeed", 30.0f);
	_settings.mouseRotMult = FUCK::INI::LoadFloat(a_ini, "Camera", "MouseRotMult", 0.015f);

	_settings.gpPanSpeed  = FUCK::INI::LoadFloat(a_ini, "Camera", "GPPanSpeed",  100.0f);
	_settings.gpZoomSpeed = FUCK::INI::LoadFloat(a_ini, "Camera", "GPZoomSpeed", 100.0f);
	_settings.gpRotSpeed  = FUCK::INI::LoadFloat(a_ini, "Camera", "GPRotSpeed",    2.0f);
	_settings.gpFovSpeed  = FUCK::INI::LoadFloat(a_ini, "Camera", "GPFovSpeed",   30.0f);
	_settings.gpDeadzone  = FUCK::INI::LoadFloat(a_ini, "Camera", "GPDeadzone",    0.25f);
}

void RaceCamera::SaveSettings(CSimpleIniA& a_ini)
{
	FUCK::INI::SaveDouble(a_ini, "Camera", "KBMPanSpeed",  _settings.kbmPanSpeed, 10.0f);
	FUCK::INI::SaveDouble(a_ini, "Camera", "KBMRotSpeed",  _settings.kbmRotSpeed,  1.5f);
	FUCK::INI::SaveDouble(a_ini, "Camera", "KBMFovSpeed",  _settings.kbmFovSpeed, 30.0f);
	FUCK::INI::SaveDouble(a_ini, "Camera", "MouseRotMult", _settings.mouseRotMult, 0.015f);

	FUCK::INI::SaveDouble(a_ini, "Camera", "GPPanSpeed",  _settings.gpPanSpeed,  100.0f);
	FUCK::INI::SaveDouble(a_ini, "Camera", "GPZoomSpeed", _settings.gpZoomSpeed, 100.0f);
	FUCK::INI::SaveDouble(a_ini, "Camera", "GPRotSpeed",  _settings.gpRotSpeed,    2.0f);
	FUCK::INI::SaveDouble(a_ini, "Camera", "GPFovSpeed",  _settings.gpFovSpeed,   30.0f);
	FUCK::INI::SaveDouble(a_ini, "Camera", "GPDeadzone",  _settings.gpDeadzone,    0.25f);
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

	bool ctrlDown  = FUCK::IsModifierPressed(FUCK::Modifier::kCtrl);
	bool shiftDown = FUCK::IsModifierPressed(FUCK::Modifier::kShift);

	bool rbDown       = FUCK::IsInputDown(RACE::Keys::kGP_RB);
	bool isCameraMode = (ctrlDown && !FUCK::IsAnyItemActive()) || (rbDown && !FUCK::IsAnyItemActive());

	if (isCameraMode) {
		// --- KBM Logic ---
		if (ctrlDown) {
			bool isTranslating = FUCK::IsInputDown(RACE::Keys::kKB_W) || FUCK::IsInputDown(RACE::Keys::kKB_S) || FUCK::IsInputDown(RACE::Keys::kKB_A) || FUCK::IsInputDown(RACE::Keys::kKB_D);

			if (isTranslating) {
				_kbmAcceleration += 1.5f * a_interval;
				if (_kbmAcceleration > 15.0f)
					_kbmAcceleration = 15.0f;
			} else {
				_kbmAcceleration = 1.0f;
			}

			float speed    = _settings.kbmPanSpeed * _kbmAcceleration * a_interval;
			float rotSpeed = _settings.kbmRotSpeed * a_interval;
			float fovSpeed = _settings.kbmFovSpeed * a_interval;

			if (shiftDown) {
				if (FUCK::IsInputDown(RACE::Keys::kKB_W))
					_camOffset.y += speed;
				if (FUCK::IsInputDown(RACE::Keys::kKB_S))
					_camOffset.y -= speed;
				if (FUCK::IsInputDown(RACE::Keys::kKB_A))
					_fovOffset -= fovSpeed;
				if (FUCK::IsInputDown(RACE::Keys::kKB_D))
					_fovOffset += fovSpeed;
			} else {
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

		// --- Gamepad Logic ---
		if (rbDown) {
			float gpPanSpeed  = _settings.gpPanSpeed  * a_interval;
			float gpZoomSpeed = _settings.gpZoomSpeed * a_interval;
			float gpRotSpeed  = _settings.gpRotSpeed  * a_interval;
			float gpFovSpeed  = _settings.gpFovSpeed  * a_interval;
			float deadzone    = _settings.gpDeadzone;

			auto ProcessAxis = [deadzone](float val) {
				if (std::abs(val) < deadzone)
					return 0.0f;
				float normalized = (std::abs(val) - deadzone) / (1.0f - deadzone);
				float curved     = normalized * normalized;
				return val < 0.0f ? -curved : curved;
			};

			float leftStickX  = ProcessAxis(FUCK::GetAnalogInput(RACE::Keys::kGP_LSX));
			float leftStickY  = ProcessAxis(FUCK::GetAnalogInput(RACE::Keys::kGP_LSY));
			float rightStickX = ProcessAxis(FUCK::GetAnalogInput(RACE::Keys::kGP_RSX));
			float rightStickY = ProcessAxis(FUCK::GetAnalogInput(RACE::Keys::kGP_RSY));
			float lt          = ProcessAxis(FUCK::GetAnalogInput(RACE::Keys::kGP_LT));
			float rt          = ProcessAxis(FUCK::GetAnalogInput(RACE::Keys::kGP_RT));

			_camOffset.z += (leftStickY * gpPanSpeed);
			_camOffset.x += (leftStickX * gpPanSpeed);
			_camOffset.y += (rightStickY * gpZoomSpeed);
			_camRotZ += (rightStickX * gpRotSpeed);
			_fovOffset += (rt - lt) * gpFovSpeed;
		}
	}

	// Right-Click Character Rotation
	if (FUCK::IsInputDown(RACE::Keys::kMouse_Right) && !FUCK::IsWindowHovered(0)) {
		ImVec2 delta = FUCK::GetMouseDelta();
		if (delta.x != 0.0f) {
			auto player = RE::PlayerCharacter::GetSingleton();
			if (player && player->Is3DLoaded()) {
				RE::NiPoint3 newAngle = player->data.angle;
				newAngle.z -= (delta.x * _settings.mouseRotMult);
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

	RE::NiPoint3 finalOffset      = (orbitedRotate * _camOffset);
	a_cameraRoot->local.translate = orbitedTranslate + finalOffset;
	a_cameraRoot->local.rotate    = orbitedRotate;

	RE::NiUpdateData ctx;
	a_cameraRoot->UpdateWorldData(&ctx);
}

void RaceCamera::ResetOffsets()
{
	if (auto camera = RE::PlayerCamera::GetSingleton()) {
		if (_baseFov != 0.0f)
			camera->worldFOV = _baseFov;
	}
	_camOffset = { 0.0f, 0.0f, 0.0f };
	_camRotZ   = 0.0f;
	_fovOffset = 0.0f;
	_baseFov   = 0.0f;
}
