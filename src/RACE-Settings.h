#pragma once

#include "API/FUCK_ColorPalette.h"

struct WidgetSettings
{
	bool startFrozen       = false;
	bool hideIdles         = false;
	bool disableMirror     = false;
	bool hideRaceStats     = false;
	bool showInFittingRoom = true;
	bool showAdvanced      = false;
	bool showFov           = false;
};

struct ColorSettings
{
	ColorSettings()
	{
		recent.acceptDrops   = false;
		swatches.showAddSlot = true;
	}

	FUCK::ColorPalette recent{ 8, FUCK::ColorPalette::Mode::kRecent };
	FUCK::ColorPalette swatches{ 16, FUCK::ColorPalette::Mode::kFixed };
};

class RaceSettings : public REX::Singleton<RaceSettings>
{
public:
	void Load();
	void Save() const;
	void SaveColors() const;

	WidgetSettings widget;
	ColorSettings  color;
};
