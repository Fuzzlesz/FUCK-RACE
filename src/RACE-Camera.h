#pragma once

#include "API/FUCK_Camera.h"

// RaceMenu's side of FUCK's camera: its camera tab and rate, Quick Zoom framing and input locks
class RaceCamera :
	public REX::Singleton<RaceCamera>,
	public FUCK::ICameraClient
{
public:
	// Around RaceMenu's movie update, which moves its camera
	void Revert();
	void Update(float a_interval);

	void EndSession();
	void ResetOffsets();
	void ToggleQuickZoom();

	bool IsCameraModified() { return FUCK::IsCameraModified(this); }

	float GetCameraRate() const { return _cameraRate; }
	bool  GetCurrentFov(float& a_horizontal, float& a_vertical) const { return FUCK::GetCameraFov(a_horizontal, a_vertical); }

	bool IsMouseOverWireframe() const;

private:
	// ICameraClient
	void OnCameraUpdate(FUCK::CameraControlState& a_state, float a_interval) override;
	bool IsCameraScrollBlocked() override { return IsMouseOverWireframe(); }
	void OnCameraTakenOver() override { _isTakenOver = true; }

	void BeginSessionIfNeeded();
	void ApplyWideScreen();
	void SyncQuickZoomButton();
	bool GetFramingRect(FUCK::CameraScreenRect& a_out) const;

	void UpdateWireframeLock();
	void SyncCameraTab();
	void DriveCameraTab();
	void SetCameraTabRate(float a_rate);

	float _cameraRate      = 5.0f;
	bool  _wasRateUp       = false;
	bool  _wasRateDown     = false;
	bool  _wasInteractDown = false;
	bool  _wireframeLock   = false;
	bool  _isTakenOver     = false;  // another plugin took the camera, so don't take it back until RaceMenu reopens
	bool  _wasFramed       = false;

	// Wider than 16:9: RaceMenu's camera as RaceMenu placed it, and as moved back to the 16:9 distance (see ApplyWideScreen)
	RE::NiPoint3 _wideRaceMenuPos;
	RE::NiPoint3 _wideShownPos;
	RE::NiPoint3 _wideCachedRaceMenuPos;
	RE::NiPoint3 _wideCachedPos;
	bool         _wideShown  = false;
	bool         _wideCached = false;
};
