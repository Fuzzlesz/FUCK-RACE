#include "FUCK_Register.h"

#include "IconsFontAwesome6.h"

#include "RACE-Anims.h"
#include "RACE-Camera.h"
#include "RACE-Compat.h"
#include "RACE-Equip.h"
#include "RACE-Inputs.h"
#include "RACE-Reference.h"
#include "RACE-Widget.h"

inline FUCK::PluginSettings& GetSettings()
{
	static FUCK::PluginSettings s;
	return s;
}

void RaceWidget::Initialize()
{
	_anchorPos  = { -1.0f, -1.0f };
	_currentPos = _anchorPos;
	LoadSettings();

	RaceReferenceManager::GetSingleton()->ScanReferences();

	static FUCK::MenuEventListener listener([](const char* menuName, bool opening) {
		if (opening && std::string_view(menuName) == RE::JournalMenu::MENU_NAME) {
			if (auto ui = RE::UI::GetSingleton(); ui) {
				if (auto menu = ui->GetMenu(RE::JournalMenu::MENU_NAME); menu && menu->uiMovie) {
					if (RE::GFxValue jMenu; menu->uiMovie->GetVariable(&jMenu, "_root.QuestJournalFader.Menu_mc")) {
						RE::GFxValue args[2]{ RE::GFxValue(2), RE::GFxValue(false) };
						jMenu.Invoke("RestoreSavedSettings", nullptr, args, 2);
					}
				}
			}
		}
	});
}

bool RaceWidget::GetMenuInstance(RE::GFxMovieView* a_movie, RE::GFxValue& a_outInstance) const
{
	if (!a_movie)
		return false;

	static const char* s_menuPath = nullptr;

	if (!s_menuPath) {
		if (a_movie->GetVariable(&a_outInstance, "_root.RaceSexMenuBaseInstance.RaceSexPanelsInstance")) {
			s_menuPath = "_root.RaceSexMenuBaseInstance.RaceSexPanelsInstance";
			return true;
		}
		if (a_movie->GetVariable(&a_outInstance, "root1.Menu_mc")) {
			s_menuPath = "root1.Menu_mc";
			return true;
		}
		return false;
	}

	return a_movie->GetVariable(&a_outInstance, s_menuPath);
}

void RaceWidget::OnAdvanceMovie(RE::RaceSexMenu* a_menu)
{
	RaceAnimManager::GetSingleton()->OnAdvanceMovie();

	if (a_menu && a_menu->uiMovie) {
		RE::GFxValue menuInstance;
		if (GetMenuInstance(a_menu->uiMovie.get(), menuInstance)) {
			if (SKEE64Compat::IsPresent()) {
				if (!menuInstance.HasMember("FUCK_HooksApplied")) {
					// Hijack Quick Zoom
					class ZoomHandler : public RE::GFxFunctionHandler
					{
					public:
						void Call(Params& a_params) override
						{
							RaceCamera::GetSingleton()->ToggleQuickZoom();
							if (a_params.thisPtr) {
								RE::GFxValue bPlayerZoom;
								if (a_params.thisPtr->GetMember("bPlayerZoom", &bPlayerZoom)) {
									a_params.thisPtr->SetMember("bPlayerZoom", RE::GFxValue(!bPlayerZoom.GetBool()));
								}
								a_params.thisPtr->Invoke("updateBottomBar", nullptr, nullptr, 0);
							}
						}
					};
					RE::GFxValue zoomFunc;
					a_menu->uiMovie->CreateFunction(&zoomFunc, new ZoomHandler());
					menuInstance.SetMember("onZoomClicked", zoomFunc);

					// Neuter Camera Editor Input
					class CameraEditorInputHandler : public RE::GFxFunctionHandler
					{
					public:
						void Call(Params& a_params) override
						{
							if (a_params.retVal) {
								*a_params.retVal = RE::GFxValue(false);
							}
						}
					};
					RE::GFxValue cameraEditor;
					if (menuInstance.GetMember("cameraEditor", &cameraEditor) && cameraEditor.IsObject()) {
						RE::GFxValue dummyFunc;
						a_menu->uiMovie->CreateFunction(&dummyFunc, new CameraEditorInputHandler());
						cameraEditor.SetMember("handleInput", dummyFunc);

						// Sync initial movement rate to FUCK framework on load
						cameraEditor.SetMember("movementRate", RE::GFxValue(RaceCamera::GetSingleton()->GetCameraRate()));
					}

					menuInstance.SetMember("FUCK_HooksApplied", RE::GFxValue(true));
				}
			}

			// Hide Light Control using the textField clearing trick so layout collapses properly
			if (GetCurrentMode() == 0) {
				bool isGamepad = (FUCK::GetInputDevice() == FUCK::InputDevice::kGamepad);
				if (!isGamepad) {
					RE::GFxValue bottomBar, buttonPanel;
					if (menuInstance.GetMember("bottomBar", &bottomBar) && bottomBar.GetMember("buttonPanel", &buttonPanel)) {
						int lightBtnIdx = isGamepad ? 2 : 3;

						auto killButton = [&](int idx) {
							std::string  btnName = "button" + std::to_string(idx);
							RE::GFxValue btn;
							if (buttonPanel.GetMember(btnName.c_str(), &btn)) {
								RE::GFxValue isVisible;
								if (btn.GetMember("_visible", &isVisible) && isVisible.GetBool()) {
									btn.SetMember("_visible", RE::GFxValue(false));

									RE::GFxValue textField;
									if (btn.GetMember("textField", &textField)) {
										textField.SetMember("text", RE::GFxValue(""));
										textField.SetMember("htmlText", RE::GFxValue(""));
									}

									RE::GFxValue arg(true);
									buttonPanel.Invoke("updateButtons", nullptr, &arg, 1);
								}
							}
						};
						killButton(lightBtnIdx);
					}
				}
			}
		}
	}
}

void RaceWidget::LoadSettings()
{
	GetSettings().Load([this](CSimpleIniA& ini) {
		float savedX = FUCK::INI::LoadFloat(ini, "Widget", "X", -1.0f);
		float savedY = FUCK::INI::LoadFloat(ini, "Widget", "Y", -1.0f);

		_anchorPos = FUCK::Scale({ savedX, savedY });

		_startFrozen   = FUCK::INI::LoadBool(ini, "Widget", "StartFrozen",   false);
		_hideIdles     = FUCK::INI::LoadBool(ini, "Widget", "HideIdles",     false);
		_disableMirror = FUCK::INI::LoadBool(ini, "Widget", "DisableMirror", false);  

		RaceCamera::GetSingleton()->LoadSettings(ini);
	});

	_currentPos = _anchorPos;
}

void RaceWidget::SaveSettings()
{
	GetSettings().Save([this](CSimpleIniA& ini) {
		float  resScale   = FUCK::GetResolutionScale();
		ImVec2 defaultPos = RaceWidgetWindow::GetSingleton()->GetDefaultPos();

		FUCK::INI::SaveDouble(ini, "Widget", "X", _anchorPos.x / resScale, defaultPos.x / resScale);
		FUCK::INI::SaveDouble(ini, "Widget", "Y", _anchorPos.y / resScale, defaultPos.y / resScale);

		FUCK::INI::SaveBool(ini, "Widget", "StartFrozen",   _startFrozen,   false);
		FUCK::INI::SaveBool(ini, "Widget", "HideIdles",     _hideIdles,     false);
		FUCK::INI::SaveBool(ini, "Widget", "DisableMirror", _disableMirror, false); 

		RaceCamera::GetSingleton()->SaveSettings(ini);
	});
}

bool RaceWidget::IsOpen() const
{
	auto ui   = RE::UI::GetSingleton();
	bool open = ui && ui->IsMenuOpen(RE::RaceSexMenu::MENU_NAME);

	if (!open && _lastMode != -1) {
		auto* self          = const_cast<RaceWidget*>(this);
		auto  animManager   = RaceAnimManager::GetSingleton();
		
		animManager->InvalidateIdles();
		self->_lastMode     = -1;
		self->_uiHidden     = false;
		self->_showSettings = false;

		if (animManager->IsFrozen()) {
			animManager->SetPlayerFrozen(false);
		}

		RaceCamera::GetSingleton()->ResetOffsets();

		RaceEquipManager::GetSingleton()->RestoreEquipped();
		RaceEquipManager::GetSingleton()->Clear();

		RaceReferenceManager::GetSingleton()->SetWindowOpen(false);
		RaceReferenceManager::GetSingleton()->ClearImage();
	}

	bool journalOpen = ui && ui->IsMenuOpen(RE::JournalMenu::MENU_NAME);
	return open && !journalOpen;
}

void RaceWidget::HandlePositioning(ImVec2& expectedPos)
{
	bool isHovered = FUCK::IsWindowHovered(0);

	float nx = 0.0f, ny = 0.0f;
	if (FUCK::WASDNudge(nx, ny, isHovered)) {
		_anchorPos.x += FUCK::Scale(nx);
		_anchorPos.y += FUCK::Scale(ny);
		expectedPos = _anchorPos;
		SaveSettings();
	}

	if (isHovered && FUCK::IsMouseClicked(0)) {
		_isDragging = true;
	}

	if (_isDragging && FUCK::IsMouseReleased(0)) {
		_isDragging = false;
		SaveSettings();
	}

	if (!FUCK::IsMouseDown(0)) {
		_isDragging = false;
	}
}

int RaceWidget::GetCurrentMode() const
{
	auto ui   = RE::UI::GetSingleton();
	auto menu = ui ? ui->GetMenu(RE::RaceSexMenu::MENU_NAME) : nullptr;

	if (!menu || !menu->uiMovie) {
		return -1;
	}

	RE::GFxValue menuInstance;
	if (!GetMenuInstance(menu->uiMovie.get(), menuInstance)) {
		return -1;
	}

	RE::GFxValue modeSelect;
	if (!menuInstance.GetMember("modeSelect", &modeSelect)) {
		return -1;
	}

	RE::GFxValue mode;
	modeSelect.Invoke("getMode", &mode, nullptr, 0);

	if (!mode.IsNumber()) {
		return -1;
	}

	return static_cast<int>(mode.GetNumber());
}

void RaceWidget::Draw()
{
	static bool s_clamped = false;
	auto        initRes   = FUCK::InitializeCustomPosition(
        _anchorPos,
        RaceWidgetWindow::GetSingleton()->GetDefaultPos(),
        s_clamped);

	if (initRes == FUCK::PosInitResult::kNotReady) {
		return;
	}
	if (initRes == FUCK::PosInitResult::kChanged) {
		_currentPos = _anchorPos;
		SaveSettings();
	}

	int currentMode = GetCurrentMode();

	auto animManager = RaceAnimManager::GetSingleton();
	if (!animManager->AreIdlesValid()) {
		animManager->UpdateValidIdles();
	}

	auto ui   = RE::UI::GetSingleton();
	auto menu = ui ? ui->GetMenu(RE::RaceSexMenu::MENU_NAME) : nullptr;

	RE::GFxValue menuInstance;
	bool         hasMenu = menu && menu->uiMovie && GetMenuInstance(menu->uiMovie.get(), menuInstance);

	if (hasMenu) {
		// Sync to modeSelect's alpha safely using the IsFlagSet bitmask check.
		RE::GFxValue modeSelect;
		if (menuInstance.GetMember("modeSelect", &modeSelect)) {
			RE::GFxValue::DisplayInfo dinfo;
			if (modeSelect.GetDisplayInfo(&dinfo)) {
				// If the Alpha flag is set, evaluate the fade. Otherwise, it is 100% visible.
				if (dinfo.IsFlagSet(RE::GFxValue::DisplayInfo::Flag::kAlpha) && dinfo.GetAlpha() < 50.0) {
					return;
				}
			}
		}
	}

	if (currentMode != _lastMode) {
		if (hasMenu) {
			SKEE64Compat::OnModeChanged(currentMode, _lastMode, menuInstance);
		}
		if (_lastMode == -1 && _startFrozen) {
			animManager->SetPlayerFrozen(true);
		}

		if (currentMode == 3) {
			ApplyMirrorLock();
		}

		_lastMode = currentMode;
	}

	static bool s_f11Pressed = false;
	static bool s_l3Pressed  = false;
	static bool s_r3Pressed  = false;

	bool f11Down = FUCK::IsInputDown(RACE::Keys::kKB_F11);
	bool l3Down  = FUCK::IsInputDown(RACE::Keys::kGP_L3);
	bool r3Down  = FUCK::IsInputDown(RACE::Keys::kGP_R3);
	bool rbDown  = FUCK::IsInputDown(RACE::Keys::kGP_RB);

	bool toggleTriggered = (f11Down && !s_f11Pressed && FUCK::IsModifierPressed(FUCK::Modifier::kShift)) ||
	                       (rbDown && l3Down && !s_l3Pressed);

	if (rbDown && r3Down && !s_r3Pressed) {
		if (auto menuControls = RE::MenuControls::GetSingleton()) {
			menuControls->QueueScreenshot();
		}
	}

	if (toggleTriggered) {
		_uiHidden = !_uiHidden;
		if (hasMenu) {
			RE::GFxValue root;
			if (menu->uiMovie->GetVariable(&root, "_root")) {
				root.SetMember("_visible", RE::GFxValue(!_uiHidden));
			}
		}
	}
	s_f11Pressed = f11Down;
	s_l3Pressed  = l3Down;
	s_r3Pressed  = r3Down;

	if (_uiHidden) {
		return;
	}

	if (animManager->IsFrozen()) {
		auto player = RE::PlayerCharacter::GetSingleton();
		if (player && player->boolFlags.all(RE::Actor::BOOL_FLAGS::kShouldAnimGraphUpdate)) {
			animManager->SetPlayerFrozen(false);
		}
	}

	bool isGamepad = FUCK::GetInputDevice() == FUCK::InputDevice::kGamepad;
	auto eqManager = RaceEquipManager::GetSingleton();
	eqManager->Update();

	float clusterScale = 0.8f;
	float alignOffset  = 0.0f;

	float baseWidth = FUCK::UIScale(360.0f * clusterScale);

	// Calculate alignment shift so the right edge stays firmly anchored to the screen edge
	if (isGamepad && eqManager->HasItems()) {
		float equipWidth = baseWidth * 1.5f;
		alignOffset      = equipWidth - baseWidth;
	}

	bool   isEditing   = FUCK::IsMenuOpen();
	ImVec2 expectedPos = _anchorPos;

	if (isEditing) {
		HandlePositioning(expectedPos);
	}

	ImVec2 renderPos = expectedPos;
	renderPos.x -= alignOffset;

	if (!isEditing || (!_isDragging && !FUCK::IsMouseDown(0))) {
		_currentPos = renderPos;
		FUCK::SetWindowPos(_currentPos, ImGuiCond_Always);
	} else if (_isDragging) {
		_currentPos  = FUCK::GetWindowPos();
		_anchorPos.x = _currentPos.x + alignOffset;
		_anchorPos.y = _currentPos.y;
	}

	FUCK::PushFontScaled(nullptr, clusterScale);

	ImVec2 spacing = FUCK::GetStyleVarVec(ImGuiStyleVar_ItemSpacing);
	FUCK::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spacing.x * clusterScale, spacing.y * clusterScale));

	ImVec2 framePadding = FUCK::GetStyleVarVec(ImGuiStyleVar_FramePadding);
	FUCK::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(framePadding.x * clusterScale, framePadding.y * clusterScale));

	FUCK::BeginGroup();

	float pad = FUCK::Scale(15.0f);

	FUCK::Dummy(ImVec2(0.0f, pad));

	FUCK::Indent(pad);
	FUCK::BeginGroup();
	if (currentMode == 3) {
		DrawSculptPanel();
	} else {
		DrawMainPanel();
	}
	FUCK::EndGroup();
	FUCK::Unindent(pad);

	FUCK::SameLine(0.0f, 0.0f);
	FUCK::Dummy(ImVec2(pad, 0.0f));

	FUCK::Dummy(ImVec2(0.0f, pad));

	FUCK::EndGroup();

	FUCK::PopStyleVar(2);
	FUCK::PopFont();

	if (isEditing) {
		ImVec2 winMin = FUCK::GetItemRectMin();
		ImVec2 winMax = FUCK::GetItemRectMax();

		float padding = FUCK::UIScale(4.0f);
		winMin.x -= padding;
		winMin.y -= padding;
		winMax.x += padding;
		winMax.y += padding;

		FUCK::DrawEditorBounds(winMin, winMax, FUCK::IsWindowHovered(0) ? FUCK::EditorBoundsState::kHovered : FUCK::EditorBoundsState::kNormal, 2.0f, true);
	}
}

void RaceWidget::DrawMainPanel()
{
	static bool s_wasRbDown  = false;
	static bool s_queueFocus = false;

	bool rbDown = FUCK::IsInputDown(RACE::Keys::kGP_RB);

	if (!rbDown && s_wasRbDown) {
		if (FUCK::IsPopupOpen(nullptr, FUCK::PopupFlags::kAnyPopup)) {
			FUCK::CloseCurrentPopup();
		}
	}

	if (rbDown && !s_wasRbDown) {
		FUCK::SetWindowFocus();
		s_queueFocus = true;
	}
	s_wasRbDown = rbDown;

	bool requestFocus = false;
	if (s_queueFocus && FUCK::IsWindowFocused(0)) {
		requestFocus = true;
		if (FUCK::GetInputDevice() == FUCK::InputDevice::kGamepad) {
			FUCK::SetNavCursorVisible(true);
		}
		s_queueFocus = false;
	}

	bool  isGamepad    = FUCK::GetInputDevice() == FUCK::InputDevice::kGamepad;
	auto  eqManager    = RaceEquipManager::GetSingleton();
	float clusterScale = 0.8f;
	float alignOffset  = 0.0f;

	float comboWidth = FUCK::UIScale(360.0f * clusterScale);

	if (isGamepad && eqManager->HasItems()) {
		alignOffset = (comboWidth * 1.5f) - comboWidth;

		if (requestFocus) {
			FUCK::SetKeyboardFocusHere(0);
			requestFocus = false;
		}

		FUCK::SetNextItemWidth(comboWidth * 1.5f);
		int eqIndex = 0;
		if (FUCK::ComboWithFilter("##RACE_Equip", &eqIndex, eqManager->GetComboStrings().data(), static_cast<int>(eqManager->GetComboStrings().size()))) {
			if (eqIndex > 0)
				eqManager->ToggleItem(eqIndex - 1);
		}
		FUCK::Dummy(ImVec2(0.0f, FUCK::UIScale(2.0f)));
		FUCK::Indent(alignOffset);
	}

	auto animManager = RaceAnimManager::GetSingleton();

	if (!_hideIdles) {
		if (requestFocus) {
			FUCK::SetKeyboardFocusHere(0);
			requestFocus = false;
		}

		FUCK::SetNextItemWidth(comboWidth);
		if (FUCK::ComboWithFilter("##RACE_PluginFilter", &animManager->GetSelectedPluginIndex(), animManager->GetPluginNamesCStr().data(), static_cast<int>(animManager->GetPluginNamesCStr().size())))
			animManager->InvalidateIdles();
		FUCK::Dummy(ImVec2(0.0f, FUCK::UIScale(2.0f)));

		FUCK::SetNextItemWidth(comboWidth);
		if (FUCK::ComboWithFilter("##RACE_Idles", &animManager->GetSelectedIndex(), animManager->GetIdleNames().data(), static_cast<int>(animManager->GetIdleNames().size()))) {
			animManager->PlaySelectedIdle();
		}
		FUCK::Dummy(ImVec2(0.0f, FUCK::UIScale(2.0f)));
	}

	auto refManager = RaceReferenceManager::GetSingleton();

	if (!refManager->IsWindowOpen() && refManager->GetSelectedIndex() > 0) {
		refManager->ClearImage();
	}

	if (!IsOnCameraTab() && !isGamepad && refManager->HasReferences()) {
		if (requestFocus) {
			FUCK::SetKeyboardFocusHere(0);
			requestFocus = false;
		}

		FUCK::SetNextItemWidth(comboWidth);
		int refIndex = refManager->GetSelectedIndex();
		if (FUCK::Combo("##RACE_RefImage", &refIndex, refManager->GetComboStrings().data(), static_cast<int>(refManager->GetComboStrings().size()))) {
			refManager->SelectReference(refIndex);
		}
		FUCK::Dummy(ImVec2(0.0f, FUCK::UIScale(2.0f)));
	}

	if (requestFocus) {
		FUCK::SetKeyboardFocusHere(0);
		requestFocus = false;
	}

	if (FUCK::Button("$RACE_Freeze"_T)) {
		if (!animManager->IsFrozen())
			animManager->SetPlayerFrozen(true);
	}

	FUCK::SameLine();
	if (FUCK::Button("$RACE_Play"_T)) {
		animManager->TogglePlay();
	}
	FUCK::SameLine();
	if (FUCK::Button("$RACE_Default"_T)) {
		animManager->StopCurrentIdle();
	}

	if (SKEE64Compat::IsPresent() && !isGamepad) {
		FUCK::SameLine();
		RE::GFxValue menuInstance;
		bool         isLightOn = false;
		auto         ui        = RE::UI::GetSingleton();
		auto         menu      = ui ? ui->GetMenu(RE::RaceSexMenu::MENU_NAME) : nullptr;
		if (menu && GetMenuInstance(menu->uiMovie.get(), menuInstance)) {
			RE::GFxValue bShowLight;
			if (menuInstance.GetMember("bShowLight", &bShowLight) && bShowLight.IsBool())
				isLightOn = bShowLight.GetBool();
		}

		if (isLightOn) {
			FUCK::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.9f, 0.2f, 1.0f));
			FUCK::PushStyleColor(ImGuiCol_TextDisabled, ImVec4(0.8f, 0.7f, 0.15f, 1.0f));
		}
		FUCK::PushID("RACE_LightToggle");
		if (FUCK::Button(" " ICON_FA_LIGHTBULB " "))
			menuInstance.Invoke("onLightClicked", nullptr, nullptr, 0);
		FUCK::PopID();
		if (isLightOn)
			FUCK::PopStyleColor(2);
	}

	FUCK::SameLine();
	if (FUCK::Button(ICON_FA_GEAR)) {
		_showSettings = !_showSettings;
		if (_showSettings) {
			_settingsJustOpened = true;
			eqManager->SetWindowOpen(false);
		}
	}

	if (isGamepad) {
		FUCK::SameLine();
		FUCK::HelpMarker("$RACE_CamTooltip_GP"_T);
	}

	bool hasCam      = RaceCamera::GetSingleton()->IsCameraModified();
	bool hasEquipBtn = (!isGamepad && eqManager->HasItems());

	if (hasCam || hasEquipBtn) {
		FUCK::Dummy(ImVec2(0.0f, FUCK::UIScale(2.0f)));
	}

	FUCK::Indent();
	bool itemsOnBottomLine = false;

	if (hasEquipBtn) {
		if (FUCK::Button("$RACE_EquipBtn"_T)) {
			eqManager->ToggleWindow();
			if (eqManager->IsWindowOpen()) {
				ImVec2 minPos = FUCK::GetItemRectMin();
				ImVec2 maxPos = FUCK::GetItemRectMax();
				eqManager->SetSpawnPos(ImVec2(minPos.x, maxPos.y + FUCK::Scale(4.0f)));
				_showSettings = false;
			}
		}
		itemsOnBottomLine = true;
	}

	if (!isGamepad) {
		if (itemsOnBottomLine)
			FUCK::SameLine();
		FUCK::HelpMarker("$RACE_CamTooltip_KBM"_T);
		itemsOnBottomLine = true;
	}

	if (hasCam) {
		if (itemsOnBottomLine)
			FUCK::SameLine();
		if (FUCK::Button("$RACE_ResetCam"_T)) {
			RaceCamera::GetSingleton()->ResetOffsets();

			// Sync the ActionScript UI toggle so Quick Zoom doesn't require a double-press to re-engage
			auto         ui   = RE::UI::GetSingleton();
			auto         menu = ui ? ui->GetMenu(RE::RaceSexMenu::MENU_NAME) : nullptr;
			RE::GFxValue menuInst;
			if (menu && GetMenuInstance(menu->uiMovie.get(), menuInst)) {
				RE::GFxValue bPlayerZoom;
				if (menuInst.GetMember("bPlayerZoom", &bPlayerZoom)) {
					menuInst.SetMember("bPlayerZoom", RE::GFxValue(false));
					menuInst.Invoke("updateBottomBar", nullptr, nullptr, 0);
				}
			}
		}
	}

	FUCK::Unindent();

	if (_showSettings) {
		if (alignOffset > 0.0f) {
			FUCK::Unindent(alignOffset);
		}
		DrawSettingsPanel();
	} else if (alignOffset > 0.0f) {
		FUCK::Unindent(alignOffset);
	}
}

void RaceWidget::DrawSettingsPanel()
{
	FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(5.0f)));
	FUCK::SeparatorText("$RACE_Settings"_T);

	float panelScale    = 0.9f;
	float clusterScale  = 0.8f * panelScale; 
	float settingsWidth = FUCK::UIScale(440.0f * clusterScale);

	FUCK::PushFontScaled(nullptr, clusterScale);

	ImVec2 currentSpacing = FUCK::GetStyleVarVec(ImGuiStyleVar_ItemSpacing);
	FUCK::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(currentSpacing.x * panelScale, currentSpacing.y * panelScale));

	ImVec2 currentFramePadding = FUCK::GetStyleVarVec(ImGuiStyleVar_FramePadding);
	FUCK::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(currentFramePadding.x * panelScale, currentFramePadding.y * panelScale));

	FUCK::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0.0f, FUCK::Scale(4.0f) * panelScale));

	if (FUCK::BeginTable("SettingsWidthLocker", 1, FUCK::TableFlags::kSizingFixedFit, ImVec2(settingsWidth, 0.0f))) {
		FUCK::TableSetupColumn("SettingsCol", FUCK::TableColumnFlags::kWidthFixed, settingsWidth);
		FUCK::TableNextRow();
		FUCK::TableNextColumn();

		auto& camSettings = RaceCamera::GetSingleton()->GetSettings();
		bool  changed     = false;

		int flagsKBM = 0;
		int flagsGP  = 0;

		if (_settingsJustOpened) {
			if (FUCK::GetInputDevice() == FUCK::InputDevice::kGamepad) {
				flagsGP = 2;  // ImGuiTabItemFlags_SetSelected
			} else {
				flagsKBM = 2;  // ImGuiTabItemFlags_SetSelected
			}
			_settingsJustOpened = false;
		}

		FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(2.0f)));

		// --- Helper to normalise displayed settings to whole numbers ---
		auto ScaledSlider = [](const char* label, float& value, float min, float max, float scale) {
			float display = value * scale;
			if (FUCK::SliderFloat(label, &display, min * scale, max * scale, "%.0f")) {
				value = display / scale;
				return true;
			}
			return false;
		};

		if (FUCK::BeginTabBar("RaceSettingsTabs", 0)) {
			// Keyboard & Mouse Tab
			if (FUCK::BeginTabItem("$RACE_CamSettings_KBM"_T, flagsKBM)) {
				FUCK::PushID("KBM");
				FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(4.0f)));

				changed |= ScaledSlider("$RACE_SpeedPan"_T,      camSettings.kbmPanSpeed,         1.0f,  50.0f,    1.0f);
				changed |= ScaledSlider("$RACE_SpeedOrbit"_T,    camSettings.kbmRotSpeed,         0.1f,   5.0f,   10.0f);
				changed |= ScaledSlider("$RACE_SpeedRoll"_T,     camSettings.kbmRollSpeed,        0.1f,   5.0f,   10.0f);
				changed |= ScaledSlider("$RACE_SpeedFOV"_T,      camSettings.kbmFovSpeed,         5.0f, 100.0f,    1.0f);
				changed |= ScaledSlider("$RACE_MouseOrbit"_T,    camSettings.mouseRotMult,      0.001f,  0.05f, 1000.0f);
				changed |= ScaledSlider("$RACE_QuickZoomDist"_T, camSettings.quickZoomOffset,    10.0f, 300.0f,    1.0f);
				changed |= ScaledSlider("$RACE_QuickZoomDown"_T, camSettings.quickZoomDownOffset,  0.0f, 150.0f,    1.0f);
				changed |= ScaledSlider("$RACE_QuickZoomPan"_T,  camSettings.quickZoomSideOffset,-150.0f, 150.0f,    1.0f);

				FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(4.0f)));
				FUCK::PopID();
				FUCK::EndTabItem();
			}

			// Gamepad Tab
			if (FUCK::BeginTabItem("$RACE_CamSettings_GP"_T, flagsGP)) {
				FUCK::PushID("GP");
				FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(4.0f)));

				changed |= ScaledSlider("$RACE_SpeedPan"_T,      camSettings.gpPanSpeed,          10.0f, 300.0f,   1.0f);
				changed |= ScaledSlider("$RACE_SpeedZoom"_T,     camSettings.gpZoomSpeed,         10.0f, 300.0f,   1.0f);
				changed |= ScaledSlider("$RACE_SpeedOrbit"_T,    camSettings.gpRotSpeed,           0.1f,  10.0f,  10.0f);
				changed |= ScaledSlider("$RACE_SpeedRoll"_T,     camSettings.gpRollSpeed,          0.1f,  10.0f,  10.0f);
				changed |= ScaledSlider("$RACE_SpeedCharRot"_T,  camSettings.gpCharRotSpeed,       0.1f,  10.0f,  10.0f);
				changed |= ScaledSlider("$RACE_Deadzone"_T,      camSettings.gpDeadzone,           0.0f,   0.5f, 100.0f);
				changed |= ScaledSlider("$RACE_QuickZoomDist"_T, camSettings.quickZoomOffset,     10.0f, 300.0f,   1.0f);
				changed |= ScaledSlider("$RACE_QuickZoomDown"_T, camSettings.quickZoomDownOffset,  0.0f, 150.0f,   1.0f);
				changed |= ScaledSlider("$RACE_QuickZoomPan"_T,  camSettings.quickZoomSideOffset,-150.0f, 150.0f,   1.0f);

				FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(4.0f)));
				FUCK::PopID();
				FUCK::EndTabItem();
			}
			FUCK::EndTabBar();
		}

		FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(4.0f)));

		if (FUCK::Checkbox("$RACE_HideIdles"_T, &_hideIdles, true, true)) {
			SaveSettings();
		}

		if (FUCK::Checkbox("$RACE_StartFrozen"_T, &_startFrozen, true, true)) {
			SaveSettings();
		}

		FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(4.0f)));

		CameraSettings def;
		bool           isModified =
			std::abs(camSettings.kbmPanSpeed         - def.kbmPanSpeed)         > 0.001f  ||
			std::abs(camSettings.kbmRotSpeed         - def.kbmRotSpeed)         > 0.001f  ||
			std::abs(camSettings.kbmFovSpeed         - def.kbmFovSpeed)         > 0.001f  ||
			std::abs(camSettings.mouseRotMult        - def.mouseRotMult)        > 0.0001f ||
			std::abs(camSettings.gpPanSpeed          - def.gpPanSpeed)          > 0.001f  ||
			std::abs(camSettings.gpZoomSpeed         - def.gpZoomSpeed)         > 0.001f  ||
			std::abs(camSettings.gpRotSpeed          - def.gpRotSpeed)          > 0.001f  ||
			std::abs(camSettings.gpFovSpeed          - def.gpFovSpeed)          > 0.001f  ||
			std::abs(camSettings.gpCharRotSpeed      - def.gpCharRotSpeed)      > 0.001f  ||
			std::abs(camSettings.quickZoomOffset     - def.quickZoomOffset)     > 0.001f  ||
			std::abs(camSettings.quickZoomDownOffset - def.quickZoomDownOffset) > 0.001f  ||
			std::abs(camSettings.quickZoomSideOffset - def.quickZoomSideOffset) > 0.001f  ||
			std::abs(camSettings.gpDeadzone          - def.gpDeadzone)          > 0.001f   ;

		if (isModified) {
			if (FUCK::Button("$RACE_RestoreDefaults"_T)) {
				camSettings = def;
				changed     = true;
			}
		}

		if (changed) {
			SaveSettings();
		}

		FUCK::EndTable();
	}
	FUCK::PopStyleVar(3);
	FUCK::PopFont();
}

void RaceWidget::ApplyMirrorLock()
{
	if (!SKEE64Compat::IsPresent())
		return;

	auto ui   = RE::UI::GetSingleton();
	auto menu = ui ? ui->GetMenu(RE::RaceSexMenu::MENU_NAME) : nullptr;
	if (!menu || !menu->uiMovie)
		return;

	double targetValue = _disableMirror ? 0.0 : 1.0;

	RE::GFxValue charGen;
	bool         hasCharGen = menu->uiMovie->GetVariable(&charGen, "_global.skse.plugins.CharGen");

	RE::GFxValue menuInstance;
	if (GetMenuInstance(menu->uiMovie.get(), menuInstance)) {
		RE::GFxValue vertexEditor, brushWindow, brushList, entryList, brushesArr;

		if (menuInstance.GetMember("vertexEditor", &vertexEditor) &&
			vertexEditor.GetMember("brushWindow", &brushWindow)) {
			if (brushWindow.GetMember("brushes", &brushesArr) && brushesArr.IsArray()) {
				std::uint32_t count = brushesArr.GetArraySize();
				for (std::uint32_t i = 0; i < count; ++i) {
					RE::GFxValue brush;
					if (brushesArr.GetElement(i, &brush) && brush.IsObject()) {
						brush.SetMember("mirror", RE::GFxValue(targetValue));

						if (hasCharGen && charGen.IsObject()) {
							RE::GFxValue typeVal;
							brush.GetMember("type", &typeVal);

							RE::GFxValue args[2] = { typeVal, brush };
							charGen.Invoke("SetBrushData", nullptr, args, 2);
						}
					}
				}
			}

			if (brushWindow.GetMember("brushList", &brushList) &&
				brushList.GetMember("entryList", &entryList) &&
				entryList.IsArray()) {
				std::uint32_t entryCount = entryList.GetArraySize();
				bool          changedUI  = false;

				for (std::uint32_t i = 0; i < entryCount; ++i) {
					RE::GFxValue entry;
					if (entryList.GetElement(i, &entry) && entry.IsObject()) {
						RE::GFxValue textVal;
						if (entry.GetMember("text", &textVal) && textVal.IsString()) {
							if (std::string_view(textVal.GetString()) == "$Mirror") {
								entry.SetMember("position", RE::GFxValue(targetValue));

								RE::GFxValue brushRef;
								if (entry.GetMember("brush", &brushRef) && brushRef.IsObject()) {
									brushRef.SetMember("mirror", RE::GFxValue(targetValue));
								}
								changedUI = true;
							}
						}
					}
				}

				if (changedUI) {
					brushList.Invoke("InvalidateData", nullptr, nullptr, 0);
				}
			}
		}
	}
}

void RaceWidget::DrawSculptPanel()
{
	if (!SKEE64Compat::IsPresent())
		return;

	auto animManager = RaceAnimManager::GetSingleton();
	auto refManager  = RaceReferenceManager::GetSingleton();

	if (FUCK::Checkbox("$RACE_DisableMirror"_T, &_disableMirror, false)) {
		SaveSettings();
		ApplyMirrorLock();
	}

	FUCK::Dummy(ImVec2(0.0f, FUCK::UIScale(2.0f)));

	if (!refManager->IsWindowOpen() && refManager->GetSelectedIndex() > 0) {
		refManager->ClearImage();
	}

	bool isGamepad = FUCK::GetInputDevice() == FUCK::InputDevice::kGamepad;

	if (!isGamepad && refManager->HasReferences()) {
		float clusterScale = 0.8f;
		float comboWidth   = FUCK::UIScale(360.0f * clusterScale);

		FUCK::SetNextItemWidth(comboWidth);
		int refIndex = refManager->GetSelectedIndex();

		if (FUCK::Combo("##RACE_RefImage_Sculpt", &refIndex, refManager->GetComboStrings().data(), static_cast<int>(refManager->GetComboStrings().size()))) {
			refManager->SelectReference(refIndex);
		}
		FUCK::Dummy(ImVec2(0.0f, FUCK::UIScale(2.0f)));
	}

	FUCK::Indent();
	FUCK::Indent();

	if (FUCK::Button("$RACE_Freeze"_T)) {
		if (!animManager->IsFrozen())
			animManager->SetPlayerFrozen(true);
	}

	FUCK::SameLine();
	if (FUCK::Button("$RACE_Play"_T)) {
		animManager->TogglePlay();
	}

	FUCK::Unindent();
	FUCK::Unindent();
}
