#pragma once

struct CameraSettings;

class RaceCamera : public REX::Singleton<RaceCamera>
{
public:
	void HandleInput(float a_interval);
	void ApplyTransform(RE::NiNode* a_cameraRoot);
	void RevertCameraTransform(RE::NiNode* a_cameraRoot);
	void ResetOffsets();

	void ToggleQuickZoom();
	void AccumulateScroll(float a_delta) { _pendingScroll += a_delta; }

	bool HasAnyCamera() const
	{
		return IsCameraModified() || _quickZoomBlend > 0.001f;
	}

	bool IsCameraModified() const
	{
		return std::abs(_camOffset.x) > 0.01f || std::abs(_camOffset.y) > 0.01f || std::abs(_camOffset.z) > 0.01f ||
		       std::abs(_camRotZ) > 0.001f    || std::abs(_camRotX) > 0.001f || std::abs(_camRoll) > 0.001f       ||
		       std::abs(_fovOffset) > 0.01f   || _isQuickZoomed;
	}

	float GetCameraRate() const { return _cameraRate; }
	bool  GetCurrentFov(float& a_horizontal, float& a_vertical) const;

	bool IsMouseOverWireframe() const;

private:
	struct FrameInput
	{
		bool isCameraTab   = false;
		bool isGamepad     = false;
		bool isGlobalKBM   = false;
		bool isGlobalGP    = false;
		bool isCameraMode  = false;

		bool ctrlDown      = false;
		bool shiftDown     = false;
		bool altDown       = false;
		bool lbDown        = false;
		bool rbDown        = false;
		bool mmbDown       = false;
		bool rmbDown       = false;

		bool isPopupOpen   = false;
		bool isUIHovered   = false;
		bool isItemActive  = false;

		float rateMult  = 1.0f;
		float panScale  = 1.0f;
		float zoomScale = 1.0f;
		float rotScale  = 1.0f;
	};

	void       UpdateQuickZoomBlend(float a_interval);
	void       ClearInputState();
	void       HandleControls(const CameraSettings& a_settings, float a_interval);
	FrameInput GatherInput();
	void       UpdateWireframeLock(bool a_mmbDown);
	void       SyncCameraTab(const FrameInput& a_in);
	void       HandleMouse(const FrameInput& a_in, const CameraSettings& a_settings, float a_interval);
	void       HandleKeyboard(const FrameInput& a_in, const CameraSettings& a_settings, float a_interval);
	void       HandleGamepad(const FrameInput& a_in, const CameraSettings& a_settings, float a_interval);
	void       HandleCharacterRotation(const FrameInput& a_in, const CameraSettings& a_settings, float a_interval);
	void       UpdateCinematic(bool a_enabled, float a_interval);
	void       ApplyFov();

	void SetCameraTabRate(float a_rate);

	float GetFovScale() const;
	float GetDistanceScale() const;
	void  UpdateQuickZoomFraming(const RE::NiPoint3& a_headPos, const RE::NiPoint3& a_pelvisPos, const RE::NiPoint3& a_rootPos);

	RE::NiPoint3 _camOffset{ 0.0f, 0.0f, 0.0f };
	float        _camRotZ   = 0.0f;
	float        _camRotX   = 0.0f;
	float        _camRoll   = 0.0f;
	float        _fovOffset = 0.0f;

	RE::NiPoint3 _targetCamOffset{ 0.0f, 0.0f, 0.0f };
	float        _targetCamRotZ   = 0.0f;
	float        _targetCamRotX   = 0.0f;
	float        _targetCamRoll   = 0.0f;
	float        _targetFovOffset = 0.0f;

	float _baseFov = 0.0f;  // player's FOV, restored on close

	float        _quickZoomProgress   = 0.0f;  // linear swoop progress, eased into _quickZoomBlend
	float        _quickZoomBlend      = 0.0f;  // 0 = face, 1 = body
	float        _quickZoomDistance   = 0.0f;
	bool         _isQuickZoomed       = false;
	bool         _hasQuickZoomFraming = false;
	RE::NiPoint3 _quickZoomCamDelta  { 0.0f, 0.0f, 0.0f };
	RE::NiPoint3 _quickZoomPivotDelta{ 0.0f, 0.0f, 0.0f };

	float _pendingScroll    = 0.0f;
	float _smoothScrollZoom = 0.0f;
	float _smoothScrollFov  = 0.0f;

	float _kbmAcceleration  = 1.0f;
	float _cameraRate       = 5.0f;
	bool  _wasInteractDown  = false;
	bool  _wireframeLock    = false;
	bool  _wasRateUp        = false;
	bool  _wasRateDown      = false;
	bool  _lockoutGPCharRot = false;
	bool  _wasMouseCharRot  = false;

	RE::NiPoint3  _originalTranslate;
	RE::NiMatrix3 _originalRotate;
	RE::NiPoint3  _appliedTranslate;
	RE::NiMatrix3 _appliedRotate;
	bool          _wasModified = false;
};
