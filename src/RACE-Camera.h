#pragma once

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
	float GetRaceMenuFov(float a_gameFov);
	float GetFovScale() const;
	float GetPanScale() const;
	void  UpdateQuickZoomFraming(const RE::NiPoint3& a_headPos, const RE::NiPoint3& a_pelvisPos, const RE::NiPoint3& a_rootPos);

	float _pendingScroll = 0.0f;

	RE::NiPoint3 _camOffset{ 0.0f, 0.0f, 0.0f };
	float        _camRotZ   = 0.0f;
	float        _camRotX   = 0.0f;
	float        _camRoll   = 0.0f;
	float        _fovOffset = 0.0f;
	float        _baseFov   = 0.0f;
	float        _gameFov   = 0.0f;  // user's FOV, restored on close
	float        _fovToTanV = 0.0f;  // tan(half vertical FOV) per tan(half worldFOV)
	float        _fovAspect = 0.0f;

	RE::NiPoint3 _targetCamOffset{ 0.0f, 0.0f, 0.0f };
	float        _targetCamRotZ   = 0.0f;
	float        _targetCamRotX   = 0.0f;
	float        _targetCamRoll   = 0.0f;
	float        _targetFovOffset = 0.0f;

	float        _quickZoomBlend      = 0.0f;  // 0 = face, 1 = body
	float        _quickZoomDistance   = 0.0f;
	bool         _isQuickZoomed       = false;
	bool         _hasQuickZoomFraming = false;
	RE::NiPoint3 _quickZoomCamDelta  { 0.0f, 0.0f, 0.0f };
	RE::NiPoint3 _quickZoomPivotDelta{ 0.0f, 0.0f, 0.0f };

	float _smoothScrollZoom = 0.0f;
	float _smoothScrollFov  = 0.0f;

	float _kbmAcceleration = 1.0f;
	float _cameraRate      = 5.0f;

	RE::NiPoint3  _originalTranslate;
	RE::NiMatrix3 _originalRotate;
	RE::NiPoint3  _appliedTranslate;
	RE::NiMatrix3 _appliedRotate;
	bool          _wasModified = false;
};
