#include "FUCK_Register.h"

#include "RACE-Compat.h"
#include "RACE-Settings.h"
#include "RACE-Widget.h"

namespace
{
	// KBM/GP speeds use different units and have shipped in user INIs, so rather than
	// migrate them, map each to a shared 1-20 display with the default at 10
	bool SpeedSlider(const char* a_label, float* a_value, float a_default)
	{
		return FUCK::ScaledSliderFloat(a_label, a_value, a_default / 10.0f, a_default * 2.0f, 10.0f / a_default);
	}
}

void RaceWidget::LoadPosition()
{
	_anchorPos  = FUCK::Scale(RaceSettings::GetSingleton()->widget.pos);
	_currentPos = _anchorPos;
}

void RaceWidget::SaveSettings()
{
	float  resScale   = FUCK::GetResolutionScale();
	ImVec2 defaultPos = RaceWidgetWindow::GetSingleton()->GetDefaultPos();

	auto toStored = [resScale](float a_pos, float a_default) {
		float pos = a_pos / resScale;
		return std::abs(pos - a_default / resScale) < 0.00001f ? -1.0f : pos;
	};

	auto settings        = RaceSettings::GetSingleton();
	settings->widget.pos = { toStored(_anchorPos.x, defaultPos.x), toStored(_anchorPos.y, defaultPos.y) };
	settings->Save();
}

void RaceWidget::CloseSettings()
{
	_showSettings      = false;
	_showLightSettings = false;
}

void RaceWidget::DrawSettingsPanel(bool a_includeCamera)
{
	auto& prefs = RaceSettings::GetSingleton()->widget;

	// !a_includeCamera is pretty much Fitting Room only right now
	if (!a_includeCamera) {
		FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(5.0f)));
	}

	if (a_includeCamera) {
		FUCK::SeparatorText("$RACE_Settings"_T);
	} else {
		FUCK::Separator();
	}

	float panelScale    = 0.9f;
	float clusterScale  = 0.8f * panelScale;
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
	constexpr int TAB_SET_SELECTED = 2;  // ImGuiTabItemFlags_SetSelected

	auto& prefs       = RaceSettings::GetSingleton()->widget;
	auto& camSettings = RaceSettings::GetSingleton()->camera;
	bool  changed     = false;
	bool  isGamepad   = FUCK::GetInputDevice() == FUCK::InputDevice::kGamepad;

	const CameraSettings def;

	// Open on the tab for whichever device is in use
	int flagsKBM = (_settingsJustOpened && !isGamepad) ? TAB_SET_SELECTED : 0;
	int flagsGP  = (_settingsJustOpened && isGamepad) ? TAB_SET_SELECTED : 0;

	if (prefs.showAdvanced) {
		ImVec2 tabRowPos   = FUCK::GetCursorPos();
		float  tabRowWidth = FUCK::GetContentRegionAvail().x;

		if (FUCK::BeginTabBar("RaceSettingsTabs", 0)) {
			// Keyboard & Mouse Tab
			if (!isGamepad) {
				if (FUCK::BeginTabItem("$RACE_CamSettings_KBM"_T, flagsKBM)) {
					FUCK::PushID("KBM");
					FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(4.0f)));

					changed |= SpeedSlider("$RACE_SpeedPan"_T,   &camSettings.kbmPanSpeed,     def.kbmPanSpeed);
					changed |= SpeedSlider("$RACE_SpeedOrbit"_T, &camSettings.kbmRotSpeed,     def.kbmRotSpeed);
					changed |= SpeedSlider("$RACE_SpeedRoll"_T,  &camSettings.kbmRollSpeed,    def.kbmRollSpeed);
					changed |= SpeedSlider("$RACE_SpeedFOV"_T,   &camSettings.kbmZoomFovSpeed, def.kbmZoomFovSpeed);

					FUCK::PopID();
					FUCK::EndTabItem();
				}
			}

			// Gamepad Tab
			if (FUCK::BeginTabItem("$RACE_CamSettings_GP"_T, flagsGP)) {
				FUCK::PushID("GP");
				FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(4.0f)));

				changed |= SpeedSlider("$RACE_SpeedPan"_T,     &camSettings.gpPanSpeed,     def.gpPanSpeed);
				changed |= SpeedSlider("$RACE_SpeedZoom"_T,    &camSettings.gpZoomSpeed,    def.gpZoomSpeed);
				changed |= SpeedSlider("$RACE_SpeedOrbit"_T,   &camSettings.gpRotSpeed,     def.gpRotSpeed);
				changed |= SpeedSlider("$RACE_SpeedRoll"_T,    &camSettings.gpRollSpeed,    def.gpRollSpeed);
				changed |= SpeedSlider("$RACE_SpeedFOV_GP"_T,  &camSettings.gpFovSpeed,     def.gpFovSpeed);
				changed |= SpeedSlider("$RACE_SpeedCharRot"_T, &camSettings.gpCharRotSpeed, def.gpCharRotSpeed);

				changed |= FUCK::ScaledSliderFloat("$RACE_Deadzone"_T, &camSettings.gpDeadzone, 0.0f, 0.5f, 40.0f);

				FUCK::PopID();
				FUCK::EndTabItem();
			}
			FUCK::EndTabBar();
		}

		if (camSettings.HasCustomSpeeds()) {
			static float s_resetWidth = 0.0f;

			ImVec2 afterTabs  = FUCK::GetCursorPos();
			float  resetWidth = s_resetWidth > 0.0f ? s_resetWidth : FUCK::CalcTextSize("$RACE_RestoreDefaults"_T).x + FUCK::GetStyleVarVec(ImGuiStyleVar_FramePadding).x * 2.0f;

			FUCK::SetCursorPos(ImVec2(tabRowPos.x + tabRowWidth - resetWidth - FUCK::Scale(30.0f), tabRowPos.y - FUCK::Scale(3.0f)));

			bool resetPressed = FUCK::Button("$RACE_RestoreDefaults"_T);
			s_resetWidth      = FUCK::GetItemRectSize().x;

			if (resetPressed) {
				camSettings = def;
				changed     = true;
			}

			FUCK::SetCursorPos(afterTabs);
		}

		FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(4.0f)));

		changed |= FUCK::Checkbox("$RACE_ShowFov"_T, &prefs.showFov, true, true);
	}

	changed |= FUCK::Checkbox("$RACE_SmoothCamera"_T, &RaceSettings::GetSingleton()->smoothCamera, true, true);

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
