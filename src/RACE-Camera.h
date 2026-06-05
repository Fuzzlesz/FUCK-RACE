#pragma once

struct CameraSettings
{
	float kbmPanSpeed  = 10.0f;
	float kbmRotSpeed  = 1.5f;
	float kbmFovSpeed  = 30.0f;
	float mouseRotMult = 0.015f;

	float gpPanSpeed  = 100.0f;
	float gpZoomSpeed = 100.0f;
	float gpRotSpeed  = 2.0f;
	float gpFovSpeed  = 30.0f;
	float gpDeadzone  = 0.25f;
};

class RaceCamera
{
public:
	static RaceCamera* GetSingleton()
	{
		static RaceCamera s;
		return &s;
	}

	void HandleInput(float a_interval, bool a_isFrozen);
	void ApplyTransform(RE::NiNode* a_cameraRoot);
	void RevertCameraTransform(RE::NiNode* a_cameraRoot);
	void ResetOffsets();

	void LoadSettings(CSimpleIniA& a_ini);
	void SaveSettings(CSimpleIniA& a_ini);

	bool HasAnyCamera() const { return _camOffset.x != 0.0f || _camOffset.y != 0.0f || _camOffset.z != 0.0f || _camRotZ != 0.0f || _fovOffset != 0.0f; }

	CameraSettings& GetSettings() { return _settings; }

private:
	RaceCamera() = default;

	CameraSettings _settings;

	RE::NiPoint3 _camOffset{ 0.0f, 0.0f, 0.0f };
	float        _camRotZ   = 0.0f;
	float        _fovOffset = 0.0f;
	float        _baseFov   = 0.0f;

	float _kbmAcceleration = 1.0f;
	bool  _isRotating      = false;

	RE::NiPoint3  _originalTranslate;
	RE::NiMatrix3 _originalRotate;
	bool          _wasModified = false;
};
