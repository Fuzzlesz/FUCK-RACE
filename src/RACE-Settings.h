#pragma once

struct CameraSettings
{
	float kbmPanSpeed     = 15.0f;
	float kbmRotSpeed     = 1.5f;
	float kbmRollSpeed    = 1.5f;
	float kbmZoomFovSpeed = 30.0f;

	float gpPanSpeed     = 100.0f;
	float gpZoomSpeed    = 100.0f;
	float gpRotSpeed     = 2.0f;
	float gpRollSpeed    = 2.0f;
	float gpFovSpeed     = 30.0f;
	float gpCharRotSpeed = 3.0f;
	float gpDeadzone     = 0.25f;

	bool HasCustomSpeeds() const;
};

struct WidgetSettings
{
	ImVec2 pos{ -1.0f, -1.0f };  // unscaled, -1 = default

	bool startFrozen       = false;
	bool hideIdles         = false;
	bool disableMirror     = false;
	bool hideRaceStats     = false;
	bool showInFittingRoom = true;
	bool showAdvanced      = false;
	bool showFov           = false;
};

class RaceSettings : public REX::Singleton<RaceSettings>
{
public:
	void Load();
	void Save() const;

	CameraSettings camera;
	WidgetSettings widget;

	bool smoothCamera = true;
};
