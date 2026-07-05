#pragma once

struct CameraSettings
{
	float kbmPanSpeed    =  10.0f;
	float kbmRotSpeed    =   1.5f;
	float kbmRollSpeed   =   1.5f;
	float kbmFovSpeed    =  30.0f;
	float mouseRotMult   =   0.015f;

	float gpPanSpeed     = 100.0f;
	float gpZoomSpeed    = 100.0f;
	float gpRotSpeed     =   2.0f;
	float gpRollSpeed    =   2.0f;
	float gpFovSpeed     =  30.0f;
	float gpCharRotSpeed =   3.0f;
	float gpDeadzone     =   0.25f;

	float quickZoomOffset = 50.0f;
};

class RaceCamera : public REX::Singleton<RaceCamera>
{
public:
	void HandleInput(float a_interval, bool a_isFrozen);
	void ApplyTransform(RE::NiNode* a_cameraRoot);
	void RevertCameraTransform(RE::NiNode* a_cameraRoot);
	void ResetOffsets();

	void LoadSettings(CSimpleIniA& a_ini);
	void SaveSettings(CSimpleIniA& a_ini);

	void ToggleQuickZoom();

	bool  HasAnyCamera() const { return _camOffset.x != 0.0f || _camOffset.y != 0.0f || _camOffset.z != 0.0f || _camRotZ != 0.0f || _camRoll != 0.0f || _fovOffset != 0.0f || std::abs(_currentZoomOffset) > 0.01f; }
	float GetCameraRate() const { return _cameraRate; }

	CameraSettings& GetSettings() { return _settings; }

private:
	CameraSettings _settings;

	RE::NiPoint3 _camOffset{ 0.0f, 0.0f, 0.0f };
	float        _camRotZ    = 0.0f;
	float        _camRoll    = 0.0f;
	float        _fovOffset  = 0.0f;
	float        _baseFov    = 0.0f;

	float _currentZoomOffset = 0.0f;
	float _targetZoomOffset  = 0.0f;
	bool  _isQuickZoomed     = false;

	float _kbmAcceleration   = 1.0f;
	float _cameraRate        = 5.0f;

	RE::NiPoint3  _originalTranslate;
	RE::NiMatrix3 _originalRotate;
	bool          _wasModified = false;
};
