#include "RACE-Settings.h"

namespace
{
	constexpr const char* kCameraSection = "Camera";
	constexpr const char* kWidgetSection = "Widget";
	constexpr const char* kColorSection  = "Colour";

	FUCK::PluginSettings& GetINI()
	{
		static FUCK::PluginSettings s;
		return s;
	}

	std::string GetColorsPath()
	{
		static std::string path = FUCK::PluginSettings().GetConfigDirectory() + "\\colours.ini";
		return path;
	}

	void LoadCamera(const CSimpleIniA& a_ini, CameraSettings& a_out)
	{
		const CameraSettings def;
		const char*          sec = kCameraSection;

		a_out.kbmPanSpeed     = FUCK::INI::LoadFloat(a_ini, sec, "KBMPanSpeed",    def.kbmPanSpeed);
		a_out.kbmRotSpeed     = FUCK::INI::LoadFloat(a_ini, sec, "KBMRotSpeed",    def.kbmRotSpeed);
		a_out.kbmRollSpeed    = FUCK::INI::LoadFloat(a_ini, sec, "KBMRollSpeed",   def.kbmRollSpeed);
		a_out.kbmZoomFovSpeed = FUCK::INI::LoadFloat(a_ini, sec, "KBMFovSpeed",    def.kbmZoomFovSpeed);

		a_out.gpPanSpeed      = FUCK::INI::LoadFloat(a_ini, sec, "GPPanSpeed",     def.gpPanSpeed);
		a_out.gpZoomSpeed     = FUCK::INI::LoadFloat(a_ini, sec, "GPZoomSpeed",    def.gpZoomSpeed);
		a_out.gpRotSpeed      = FUCK::INI::LoadFloat(a_ini, sec, "GPRotSpeed",     def.gpRotSpeed);
		a_out.gpRollSpeed     = FUCK::INI::LoadFloat(a_ini, sec, "GPRollSpeed",    def.gpRollSpeed);
		a_out.gpFovSpeed      = FUCK::INI::LoadFloat(a_ini, sec, "GPFovSpeed",     def.gpFovSpeed);
		a_out.gpCharRotSpeed  = FUCK::INI::LoadFloat(a_ini, sec, "GPCharRotSpeed", def.gpCharRotSpeed);
		a_out.gpDeadzone      = FUCK::INI::LoadFloat(a_ini, sec, "GPDeadzone",     def.gpDeadzone);
	}

	void SaveCamera(CSimpleIniA& a_ini, const CameraSettings& a_in)
	{
		const CameraSettings def;
		const char*          sec = kCameraSection;

		FUCK::INI::SaveDouble(a_ini, sec, "KBMPanSpeed",    a_in.kbmPanSpeed,     def.kbmPanSpeed);
		FUCK::INI::SaveDouble(a_ini, sec, "KBMRotSpeed",    a_in.kbmRotSpeed,     def.kbmRotSpeed);
		FUCK::INI::SaveDouble(a_ini, sec, "KBMRollSpeed",   a_in.kbmRollSpeed,    def.kbmRollSpeed);
		FUCK::INI::SaveDouble(a_ini, sec, "KBMFovSpeed",    a_in.kbmZoomFovSpeed, def.kbmZoomFovSpeed);

		FUCK::INI::SaveDouble(a_ini, sec, "GPPanSpeed",     a_in.gpPanSpeed,      def.gpPanSpeed);
		FUCK::INI::SaveDouble(a_ini, sec, "GPZoomSpeed",    a_in.gpZoomSpeed,     def.gpZoomSpeed);
		FUCK::INI::SaveDouble(a_ini, sec, "GPRotSpeed",     a_in.gpRotSpeed,      def.gpRotSpeed);
		FUCK::INI::SaveDouble(a_ini, sec, "GPRollSpeed",    a_in.gpRollSpeed,     def.gpRollSpeed);
		FUCK::INI::SaveDouble(a_ini, sec, "GPFovSpeed",     a_in.gpFovSpeed,      def.gpFovSpeed);
		FUCK::INI::SaveDouble(a_ini, sec, "GPCharRotSpeed", a_in.gpCharRotSpeed,  def.gpCharRotSpeed);
		FUCK::INI::SaveDouble(a_ini, sec, "GPDeadzone",     a_in.gpDeadzone,      def.gpDeadzone);
	}

	void LoadWidget(const CSimpleIniA& a_ini, WidgetSettings& a_out)
	{
		const WidgetSettings def;
		const char*          sec = kWidgetSection;

		a_out.startFrozen       = FUCK::INI::LoadBool (a_ini, sec, "StartFrozen",       def.startFrozen);
		a_out.hideIdles         = FUCK::INI::LoadBool (a_ini, sec, "HideIdles",         def.hideIdles);
		a_out.disableMirror     = FUCK::INI::LoadBool (a_ini, sec, "DisableMirror",     def.disableMirror);
		a_out.hideRaceStats     = FUCK::INI::LoadBool (a_ini, sec, "HideRaceStats",     def.hideRaceStats);
		a_out.showInFittingRoom = FUCK::INI::LoadBool (a_ini, sec, "ShowInFittingRoom", def.showInFittingRoom);
		a_out.showAdvanced      = FUCK::INI::LoadBool (a_ini, sec, "ShowAdvanced",      def.showAdvanced);
		a_out.showFov           = FUCK::INI::LoadBool (a_ini, sec, "ShowFov",           def.showFov);
	}

	void SaveWidget(CSimpleIniA& a_ini, const WidgetSettings& a_in)
	{
		const WidgetSettings def;
		const char*          sec = kWidgetSection;

		FUCK::INI::SaveBool  (a_ini, sec, "StartFrozen",       a_in.startFrozen,       def.startFrozen);
		FUCK::INI::SaveBool  (a_ini, sec, "HideIdles",         a_in.hideIdles,         def.hideIdles);
		FUCK::INI::SaveBool  (a_ini, sec, "DisableMirror",     a_in.disableMirror,     def.disableMirror);
		FUCK::INI::SaveBool  (a_ini, sec, "HideRaceStats",     a_in.hideRaceStats,     def.hideRaceStats);
		FUCK::INI::SaveBool  (a_ini, sec, "ShowInFittingRoom", a_in.showInFittingRoom, def.showInFittingRoom);
		FUCK::INI::SaveBool  (a_ini, sec, "ShowAdvanced",      a_in.showAdvanced,      def.showAdvanced);
		FUCK::INI::SaveBool  (a_ini, sec, "ShowFov",           a_in.showFov,           def.showFov);
	}
}

bool CameraSettings::HasCustomSpeeds() const
{
	constexpr float EPSILON = 0.001f;

	const CameraSettings def;
	auto differs = [](float a, float b) { return std::abs(a - b) > EPSILON; };

	return differs(kbmPanSpeed,     def.kbmPanSpeed)     ||
	       differs(kbmRotSpeed,     def.kbmRotSpeed)     ||
	       differs(kbmRollSpeed,    def.kbmRollSpeed)    ||
	       differs(kbmZoomFovSpeed, def.kbmZoomFovSpeed) ||
	       differs(gpPanSpeed,      def.gpPanSpeed)      ||
	       differs(gpZoomSpeed,     def.gpZoomSpeed)     ||
	       differs(gpRotSpeed,      def.gpRotSpeed)      ||
	       differs(gpRollSpeed,     def.gpRollSpeed)     ||
	       differs(gpFovSpeed,      def.gpFovSpeed)      ||
	       differs(gpCharRotSpeed,  def.gpCharRotSpeed)  ||
	       differs(gpDeadzone,      def.gpDeadzone);
}

void RaceSettings::Load()
{
	GetINI().Load([this](CSimpleIniA& ini) {
		LoadWidget(ini, widget);
		LoadCamera(ini, camera);

		cinematicCamera = FUCK::INI::LoadBool(ini, kCameraSection, "CinematicCamera", true);
	});

	CSimpleIniA colorsIni;
	colorsIni.SetUnicode();
	if (colorsIni.LoadFile(GetColorsPath().c_str()) >= 0) {
		color.recent.Load(colorsIni, kColorSection, "Recent");
		color.swatches.Load(colorsIni, kColorSection, "Swatches");
	}
}

void RaceSettings::SaveColors() const
{
	std::string path = GetColorsPath();
	
	std::error_code ec;
	if (!fs::exists(path, ec) && color.recent.Empty() && color.swatches.Empty()) {
		return;
	}

	CSimpleIniA colorsIni;
	colorsIni.SetUnicode();
	
	colorsIni.LoadFile(path.c_str());
	
	color.recent.Save(colorsIni, kColorSection, "Recent");
	color.swatches.Save(colorsIni, kColorSection, "Swatches");
	
	colorsIni.SaveFile(path.c_str());
}

void RaceSettings::Save() const
{
	GetINI().Save([this](CSimpleIniA& ini) {
		SaveWidget(ini, widget);
		SaveCamera(ini, camera);

		FUCK::INI::SaveBool(ini, kCameraSection, "CinematicCamera", cinematicCamera, true);
	});
}
