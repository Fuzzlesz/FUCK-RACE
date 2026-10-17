#include "RACE-Settings.h"

namespace
{
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

	// The widget settings and their INI keys. Defaults come from WidgetSettings, and only values that differ are written
	const FUCK::INI::Fields<WidgetSettings> kWidgetFields{ kWidgetSection, {
		{ "StartFrozen",       &WidgetSettings::startFrozen       },
		{ "HideIdles",         &WidgetSettings::hideIdles         },
		{ "DisableMirror",     &WidgetSettings::disableMirror     },
		{ "HideRaceStats",     &WidgetSettings::hideRaceStats     },
		{ "ShowInFittingRoom", &WidgetSettings::showInFittingRoom },
		{ "ShowAdvanced",      &WidgetSettings::showAdvanced      },
		{ "ShowFov",           &WidgetSettings::showFov           },
	} };
}

void RaceSettings::Load()
{
	GetINI().Load(kWidgetFields, widget);

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
	GetINI().Save(kWidgetFields, widget);
}
