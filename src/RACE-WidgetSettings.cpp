#include "FUCK_Register.h"

#include "RACE-Camera.h"
#include "RACE-Compat.h"
#include "RACE-Settings.h"
#include "RACE-Widget.h"

void RaceWidget::SaveSettings()
{
	RaceSettings::GetSingleton()->Save();
}

void RaceWidget::CloseSettings()
{
	_showSettings      = false;
	_showLightSettings = false;
}

void RaceWidget::DrawSettingsPanel(bool a_includeCamera)
{
	auto& prefs = RaceSettings::GetSingleton()->widget;

	// Pad the top of the Fitting Room panel, the only one drawn without camera settings
	if (!a_includeCamera) {
		FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(5.0f)));
	}

	if (a_includeCamera) {
		FUCK::SeparatorText("$RACE_Settings"_T);
	} else {
		FUCK::Separator();
	}

	float panelScale    = 0.9f;
	float clusterScale  = GetClusterScale() * panelScale;
	float settingsWidth = FUCK::Scale(440.0f * clusterScale);

	FUCK::PushScale(clusterScale);
	FUCK::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0.0f, FUCK::Scale(4.0f) * panelScale));

	// RB is held to keep gamepad focus on the widget, so stop it also acting as ImGui's
	// x10 "tweak fast" modifier, which would make every d-pad press skip ten steps
	FUCK::PushGamepadTweakFastDisabled();

	if (FUCK::BeginTable("SettingsWidthLocker", 1, FUCK::TableFlags::kSizingFixedFit, ImVec2(settingsWidth, 0.0f))) {
		FUCK::TableSetupColumn("SettingsCol", FUCK::TableColumnFlags::kWidthFixed, settingsWidth);
		FUCK::TableNextRow();
		FUCK::TableNextColumn();

		FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(2.0f)));

		bool changed = false;

		if (a_includeCamera) {
			changed |= DrawCameraSettings();
		}

		// Consume the flag even when the camera tabs aren't drawn, so it can't go stale
		_settingsJustOpened = false;

		changed |= DrawGeneralSettings();

		if (a_includeCamera) {
			changed |= FUCK::Checkbox("$RACE_ShowAdvanced"_T, &prefs.showAdvanced, true, true);
			FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(4.0f)));
		}

		if (changed) {
			SaveSettings();
		}

		FUCK::EndTable();
	}

	FUCK::PopGamepadTweakFastDisabled();

	FUCK::PopStyleVar(1);
	FUCK::PopScale();
}

bool RaceWidget::DrawCameraSettings()
{
	auto& prefs   = RaceSettings::GetSingleton()->widget;
	bool  changed = false;

	// FUCK saves its own camera settings, so only our own changes are reported
	auto flags = FUCK::CameraSettingsFlags::kNone;
	if (prefs.showAdvanced) {
		flags = flags | FUCK::CameraSettingsFlags::kSpeeds;
	}
	if (_settingsJustOpened) {
		flags = flags | FUCK::CameraSettingsFlags::kSelectDeviceTab;
	}
	FUCK::DrawCameraSettings(flags);

	if (prefs.showAdvanced) {
		changed |= FUCK::Checkbox("$RACE_ShowFov"_T, &prefs.showFov, true, true);
	}

	return changed;
}

bool RaceWidget::DrawGeneralSettings()
{
	auto& prefs   = RaceSettings::GetSingleton()->widget;
	bool  changed = false;

	changed |= FUCK::Checkbox("$RACE_HideIdles"_T, &prefs.hideIdles, true, true);
	changed |= FUCK::Checkbox("$RACE_StartFrozen"_T, &prefs.startFrozen, true, true);

	if (FittingRoomCompat::IsPresent()) {
		changed |= FUCK::Checkbox("$RACE_ShowInFittingRoom"_T, &prefs.showInFittingRoom, true, true);
	}

	FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(4.0f)));

	return changed;
}
