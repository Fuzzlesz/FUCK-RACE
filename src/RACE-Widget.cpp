#include "FUCK_Register.h"

#include "IconsFontAwesome6.h"

#include "RACE-Camera.h"
#include "RACE-Compat.h"
#include "RACE-Equip.h"
#include "RACE-Inputs.h"
#include "RACE-Widget.h"

inline FUCK::PluginSettings& GetSettings()
{
	static FUCK::PluginSettings s;
	return s;
}

void RaceWidget::Initialize()
{
	_anchorPos  = RaceWidgetWindow::GetSingleton()->GetDefaultPos();
	_currentPos = _anchorPos;
	LoadSettings();

	std::sort(_allIdles.begin(), _allIdles.end(), [](const auto& a, const auto& b) {
		return _stricmp(a.first.c_str(), b.first.c_str()) < 0;
	});

	_pluginNames.clear();
	_pluginNames.push_back(std::string("$RACE_AllPlugins"_T) + "##NOFILTER");
	_pluginNamesCStr.clear();
	_pluginNamesCStr.push_back(_pluginNames[0].c_str());
}

void RaceWidget::AddIdle(const std::string& a_name, RE::TESIdleForm* a_idle)
{
	auto ptrIt = std::find_if(_allIdles.begin(), _allIdles.end(), [&](const auto& pair) {
		return pair.second == a_idle;
	});

	if (ptrIt != _allIdles.end()) {
		ptrIt->first = a_name;
		return;
	}

	auto nameIt = std::find_if(_allIdles.begin(), _allIdles.end(), [&](const auto& pair) {
		return _stricmp(pair.first.c_str(), a_name.c_str()) == 0;
	});

	if (nameIt != _allIdles.end()) {
		nameIt->second = a_idle;
	} else {
		_allIdles.emplace_back(a_name, a_idle);
	}
}

void RaceWidget::UpdateValidIdles()
{
	auto player = RE::PlayerCharacter::GetSingleton();
	if (!player || !player->Is3DLoaded())
		return;

	std::sort(_allIdles.begin(), _allIdles.end(), [](const auto& a, const auto& b) {
		return _stricmp(a.first.c_str(), b.first.c_str()) < 0;
	});

	std::string targetPlugin = _selectedPluginIndex > 0 && _selectedPluginIndex < static_cast<int>(_pluginNames.size()) ? _pluginNames[_selectedPluginIndex] : "";

	std::set<std::string> activePlugins;
	for (const auto& pair : _allIdles) {
		if (player->CanUseIdle(pair.second) && pair.second->CheckConditions(player, nullptr, false)) {
			if (auto file = pair.second->GetFile(0)) {
				activePlugins.insert(std::string(file->GetFilename()));
			} else {
				activePlugins.insert("Unknown");
			}
		}
	}

	_pluginNames.clear();
	_pluginNames.push_back("$RACE_AllPlugins"_T);

	if (auto dataHandler = RE::TESDataHandler::GetSingleton()) {
		for (auto* file : dataHandler->files) {
			if (file) {
				std::string fileName(file->GetFilename());
				if (activePlugins.contains(fileName)) {
					_pluginNames.push_back(fileName);
					activePlugins.erase(fileName);
				}
			}
		}
	}

	for (const auto& p : activePlugins) {
		_pluginNames.push_back(p);
	}

	_pluginNamesCStr.clear();
	for (const auto& p : _pluginNames) {
		_pluginNamesCStr.push_back(p.c_str());
	}

	_selectedPluginIndex = 0;
	if (!targetPlugin.empty()) {
		for (size_t i = 1; i < _pluginNames.size(); ++i) {
			if (_pluginNames[i] == targetPlugin) {
				_selectedPluginIndex = static_cast<int>(i);
				break;
			}
		}
	}

	_validIdles.clear();
	_idleNames.clear();

	_validIdles.reserve(_allIdles.size() + 1);
	_idleNames.reserve(_allIdles.size() + 1);

	_validIdles.push_back({ std::string("$RACE_SelectIdle"_T) + "##NOFILTER", nullptr });
	_idleNames.push_back(_validIdles.back().first.c_str());

	targetPlugin = _selectedPluginIndex > 0 ? _pluginNames[_selectedPluginIndex] : "";

	for (const auto& pair : _allIdles) {
		if (player->CanUseIdle(pair.second) && pair.second->CheckConditions(player, nullptr, false)) {
			if (!targetPlugin.empty()) {
				auto        file  = pair.second->GetFile(0);
				std::string pName = file ? std::string(file->GetFilename()) : "Unknown";
				if (pName != targetPlugin) {
					continue;
				}
			}

			_validIdles.push_back(pair);
			_idleNames.push_back(_validIdles.back().first.c_str());
		}
	}

	_selectedIndex = 0;
	_idlesValid    = true;
}

void RaceWidget::SetPlayerFrozen(bool a_frozen)
{
	auto player = RE::PlayerCharacter::GetSingleton();
	if (!player)
		return;

	if (a_frozen) {
		if (const auto currentProcess = player->currentProcess) {
			currentProcess->ClearMuzzleFlashes();
		}

		player->boolFlags.reset(RE::Actor::BOOL_FLAGS::kShouldAnimGraphUpdate);

		if (const auto charController = player->GetCharController()) {
			charController->flags.set  (RE::CHARACTER_FLAGS::kNotPushable);
			charController->flags.reset(RE::CHARACTER_FLAGS::kRecordHits);
			charController->flags.reset(RE::CHARACTER_FLAGS::kHitFlags);
		}

		player->EnableAI(false);
		player->StopMoving(1.0f);

		if (const auto animData = player->GetFaceGenAnimationData()) {
			animData->eyesHeadingOffset = 0.0f;
			animData->eyesPitchOffset   = 0.0f;
			animData->eyesOffsetTimer   = FLT_MAX;
			animData->eyesBlinkingTimer = FLT_MAX;
			animData->eyesBlinkingStage = RE::BSFaceGenAnimationData::EyesBlinkingStage::BlinkDelay;
		}
	} else {
		player->boolFlags.set(RE::Actor::BOOL_FLAGS::kShouldAnimGraphUpdate);

		if (const auto charController = player->GetCharController()) {
			charController->flags.reset(RE::CHARACTER_FLAGS::kNotPushable);
			charController->flags.set  (RE::CHARACTER_FLAGS::kRecordHits);
			charController->flags.set  (RE::CHARACTER_FLAGS::kHitFlags);
		}

		player->EnableAI(true);

		if (const auto animData = player->GetFaceGenAnimationData()) {
			animData->eyesOffsetTimer   = 0.0f;
			animData->eyesBlinkingTimer = 0.0f;
		}
	}

	_isFrozen = a_frozen;
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
	if (_isFrozen) {
		auto player = RE::PlayerCharacter::GetSingleton();
		if (player) {
			if (const auto animData = player->GetFaceGenAnimationData()) {
				animData->eyesHeadingOffset                                                            = 0.0f;
				animData->eyesPitchOffset                                                              = 0.0f;
				animData->eyesOffsetTimer                                                              = FLT_MAX;
				animData->eyesBlinkingTimer                                                            = FLT_MAX;
				animData->modifierKeyFrame.values[RE::BSFaceGenKeyframeMultiple::Modifier::BlinkLeft]  = 0.0f;
				animData->modifierKeyFrame.values[RE::BSFaceGenKeyframeMultiple::Modifier::BlinkRight] = 0.0f;
			}
		}
	}

	if (a_menu && a_menu->uiMovie && GetCurrentMode() == 0) {
		RE::GFxValue menuInstance;
		if (GetMenuInstance(a_menu->uiMovie.get(), menuInstance)) {
			bool isGamepad = (FUCK::GetInputDevice() == FUCK::InputDevice::kGamepad);

			RE::GFxValue undefinedVal;
			undefinedVal.SetUndefined();
			menuInstance.SetMember("_zoomControl", undefinedVal);

			if (!isGamepad) {
				menuInstance.SetMember("_lightControl", undefinedVal);
			}

			RE::GFxValue bottomBar, buttonPanel;
			if (menuInstance.GetMember("bottomBar", &bottomBar) && bottomBar.GetMember("buttonPanel", &buttonPanel)) {
				int zoomBtnIdx  = isGamepad ? 1 : 2;
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

				killButton(zoomBtnIdx);

				if (!isGamepad) {
					killButton(lightBtnIdx);
				}
			}
		}
	}
}

void RaceWidget::LoadSettings()
{
	GetSettings().Load([this](CSimpleIniA& ini) {
		float  resScale   = FUCK::GetResolutionScale();
		ImVec2 defaultPos = RaceWidgetWindow::GetSingleton()->GetDefaultPos();

		_anchorPos.x = FUCK::Scale(FUCK::INI::LoadFloat(ini, "Widget", "X", defaultPos.x / resScale));
		_anchorPos.y = FUCK::Scale(FUCK::INI::LoadFloat(ini, "Widget", "Y", defaultPos.y / resScale));

		_startFrozen = FUCK::INI::LoadBool(ini, "Widget", "StartFrozen", false);
		_hideIdles   = FUCK::INI::LoadBool(ini, "Widget", "HideIdles", false);

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

		FUCK::INI::SaveBool(ini, "Widget", "StartFrozen", _startFrozen, false);
		FUCK::INI::SaveBool(ini, "Widget", "HideIdles", _hideIdles, false);

		RaceCamera::GetSingleton()->SaveSettings(ini);
	});
}

bool RaceWidget::IsOpen() const
{
	auto ui   = RE::UI::GetSingleton();
	bool open = ui && ui->IsMenuOpen(RE::RaceSexMenu::MENU_NAME);

	if (!open && _lastMode != -1) {
		auto* self          = const_cast<RaceWidget*>(this);
		self->_idlesValid   = false;
		self->_lastMode     = -1;
		self->_uiHidden     = false;
		self->_showSettings = false;

		if (self->_isFrozen) {
			self->SetPlayerFrozen(false);
		}

		RaceCamera::GetSingleton()->ResetOffsets();

		RaceEquipManager::GetSingleton()->RestoreEquipped();
		RaceEquipManager::GetSingleton()->Clear();
	}

	bool journalOpen = ui && ui->IsMenuOpen(RE::JournalMenu::MENU_NAME);
	return open && !journalOpen;
}

bool RaceWidget::GetRequestedPos(ImVec2& outPos)
{
	outPos = _anchorPos;
	return true;
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
		if (std::abs(_currentPos.x - expectedPos.x) > 1.0f || std::abs(_currentPos.y - expectedPos.y) > 1.0f) {
			_anchorPos = _currentPos;
			SaveSettings();
			expectedPos = _currentPos;
		}
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
	int currentMode = GetCurrentMode();

	if (!_idlesValid) {
		UpdateValidIdles();
	}

	auto ui   = RE::UI::GetSingleton();
	auto menu = ui ? ui->GetMenu(RE::RaceSexMenu::MENU_NAME) : nullptr;

	RE::GFxValue menuInstance;
	bool         hasMenu = menu && menu->uiMovie && GetMenuInstance(menu->uiMovie.get(), menuInstance);

	if (currentMode != _lastMode) {
		if (hasMenu) {
			SKEE64Compat::OnModeChanged(currentMode, _lastMode, menuInstance);
		}
		if (_lastMode == -1 && _startFrozen) {
			SetPlayerFrozen(true);
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
		if (menu && menu->uiMovie) {
			RE::GFxValue root;
			if (menu->uiMovie->GetVariable(&root, "_root")) {
				root.SetMember("_visible", RE::GFxValue(!_uiHidden));
			}
		}
	}
	s_f11Pressed = f11Down;
	s_l3Pressed  = l3Down;
	s_r3Pressed  = r3Down;

	if (_uiHidden || currentMode == 3) {
		return;
	}

	if (_isFrozen) {
		auto player = RE::PlayerCharacter::GetSingleton();
		if (player && player->boolFlags.all(RE::Actor::BOOL_FLAGS::kShouldAnimGraphUpdate)) {
			_isFrozen = false;
		}
	}

	bool isGamepad = FUCK::GetInputDevice() == FUCK::InputDevice::kGamepad;
	auto eqManager = RaceEquipManager::GetSingleton();
	eqManager->Update();

	float clusterScale = 0.8f;
	float alignOffset  = 0.0f;

	if (isGamepad && eqManager->HasItems()) {
		float baseWidth  = FUCK::UIScale(360.0f * clusterScale);
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

	FUCK::SetWindowFontScale(clusterScale);

	ImVec2 spacing = FUCK::GetStyleVarVec(ImGuiStyleVar_ItemSpacing);
	FUCK::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spacing.x * clusterScale, spacing.y * clusterScale));

	ImVec2 framePadding = FUCK::GetStyleVarVec(ImGuiStyleVar_FramePadding);
	FUCK::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(framePadding.x * clusterScale, framePadding.y * clusterScale));

	FUCK::BeginGroup();

	DrawMainPanel();

	FUCK::EndGroup();

	FUCK::PopStyleVar(2);
	FUCK::SetWindowFontScale(1.0f);

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
	static bool s_wasRbDown = false;
	bool        rbDown      = FUCK::IsInputDown(RACE::Keys::kGP_RB);
	if (rbDown && !s_wasRbDown)
		FUCK::SetKeyboardFocusHere(0);
	s_wasRbDown = rbDown;

	bool  isGamepad    = FUCK::GetInputDevice() == FUCK::InputDevice::kGamepad;
	auto  eqManager    = RaceEquipManager::GetSingleton();
	float clusterScale = 0.8f;
	float alignOffset  = 0.0f;

	float comboWidth = FUCK::UIScale(360.0f * clusterScale);

	if (isGamepad && eqManager->HasItems()) {
		alignOffset = (comboWidth * 1.5f) - comboWidth;

		FUCK::SetNextItemWidth(comboWidth * 1.5f);
		int eqIndex = 0;
		if (FUCK::ComboWithFilter("##RACE_Equip", &eqIndex, eqManager->GetComboStrings().data(), static_cast<int>(eqManager->GetComboStrings().size()))) {
			if (eqIndex > 0)
				eqManager->ToggleItem(eqIndex - 1);
		}
		FUCK::Dummy(ImVec2(0.0f, FUCK::UIScale(2.0f)));
		FUCK::Indent(alignOffset);
	}

	if (!_hideIdles) {
		FUCK::SetNextItemWidth(comboWidth);
		if (FUCK::ComboWithFilter("##RACE_PluginFilter", &_selectedPluginIndex, _pluginNamesCStr.data(), static_cast<int>(_pluginNamesCStr.size())))
			_idlesValid = false;
		FUCK::Dummy(ImVec2(0.0f, FUCK::UIScale(2.0f)));

		FUCK::SetNextItemWidth(comboWidth);
		if (FUCK::ComboWithFilter("##RACE_Idles", &_selectedIndex, _idleNames.data(), static_cast<int>(_idleNames.size()))) {
			if (_selectedIndex > 0 && _selectedIndex < static_cast<int>(_validIdles.size())) {
				if (_isFrozen)
					SetPlayerFrozen(false);
				auto player = RE::PlayerCharacter::GetSingleton();
				if (player && player->currentProcess)
					player->currentProcess->PlayIdle(player, _validIdles[_selectedIndex].second, nullptr);
			}
		}
		FUCK::Dummy(ImVec2(0.0f, FUCK::UIScale(2.0f)));
	}

	if (FUCK::Button("$RACE_Freeze"_T)) {
		if (!_isFrozen)
			SetPlayerFrozen(true);
	}
	FUCK::SameLine();
	if (FUCK::Button("$RACE_Play"_T)) {
		if (_isFrozen)
			SetPlayerFrozen(false);
		else if (_selectedIndex > 0 && _selectedIndex < static_cast<int>(_validIdles.size())) {
			auto player = RE::PlayerCharacter::GetSingleton();
			if (player && player->currentProcess)
				player->currentProcess->PlayIdle(player, _validIdles[_selectedIndex].second, nullptr);
		}
	}
	FUCK::SameLine();
	if (FUCK::Button("$RACE_Default"_T)) {
		if (_isFrozen)
			SetPlayerFrozen(false);
		auto player = RE::PlayerCharacter::GetSingleton();
		if (player && player->currentProcess) {
			player->currentProcess->StopCurrentIdle(player, true);
			auto resetRoot = RE::TESForm::LookupByEditorID<RE::TESIdleForm>("ResetRoot");
			if (resetRoot)
				player->currentProcess->PlayIdle(player, resetRoot, nullptr);
			_selectedIndex = 0;
		}
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

	bool hasCam      = RaceCamera::GetSingleton()->HasAnyCamera();
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
				ImVec2 mousePos = FUCK::GetMousePos();
				mousePos.y += FUCK::Scale(35.0f);
				eqManager->SetSpawnPos(mousePos);
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
		if (FUCK::Button("$RACE_ResetCam"_T))
			RaceCamera::GetSingleton()->ResetOffsets();
	}

	FUCK::Unindent();

	if (_showSettings) {
		DrawSettingsPanel();
	}

	if (alignOffset > 0.0f)
		FUCK::Unindent(alignOffset);
}

void RaceWidget::DrawSettingsPanel()
{
	FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(5.0f)));
	FUCK::SeparatorText("$RACE_Settings"_T);

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

	if (FUCK::BeginTabBar("RaceSettingsTabs", 0)) {
		// Keyboard & Mouse Tab
		if (FUCK::BeginTabItem("$RACE_CamSettings_KBM"_T, flagsKBM)) {
			FUCK::PushID("KBM");
			FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(4.0f)));

			changed |= FUCK::SliderFloat("$RACE_SpeedPan"_T,   &camSettings.kbmPanSpeed,  1.0f,  50.0f,  "%.1f");
			changed |= FUCK::SliderFloat("$RACE_SpeedOrbit"_T, &camSettings.kbmRotSpeed,  0.1f,   5.0f,  "%.1f");
			changed |= FUCK::SliderFloat("$RACE_SpeedFOV"_T,   &camSettings.kbmFovSpeed,  5.0f, 100.0f,  "%.1f");
			changed |= FUCK::SliderFloat("$RACE_MouseOrbit"_T, &camSettings.mouseRotMult, 0.001f, 0.05f, "%.3f");

			FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(4.0f)));
			FUCK::PopID();
			FUCK::EndTabItem();
		}

		// Gamepad Tab
		if (FUCK::BeginTabItem("$RACE_CamSettings_GP"_T, flagsGP)) {
			FUCK::PushID("GP");
			FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(4.0f)));

			changed |= FUCK::SliderFloat("$RACE_SpeedPan"_T,   &camSettings.gpPanSpeed,  10.0f, 300.0f, "%.0f");
			changed |= FUCK::SliderFloat("$RACE_SpeedZoom"_T,  &camSettings.gpZoomSpeed, 10.0f, 300.0f, "%.0f");
			changed |= FUCK::SliderFloat("$RACE_SpeedOrbit"_T, &camSettings.gpRotSpeed,   0.1f,  10.0f, "%.1f");
			changed |= FUCK::SliderFloat("$RACE_SpeedFOV"_T,   &camSettings.gpFovSpeed,   5.0f, 100.0f, "%.1f");
			changed |= FUCK::SliderFloat("$RACE_Deadzone"_T,   &camSettings.gpDeadzone,   0.0f,   0.5f, "%.2f");

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
		std::abs(camSettings.kbmPanSpeed  - def.kbmPanSpeed)  > 0.001f  ||
		std::abs(camSettings.kbmRotSpeed  - def.kbmRotSpeed)  > 0.001f  ||
		std::abs(camSettings.kbmFovSpeed  - def.kbmFovSpeed)  > 0.001f  ||
		std::abs(camSettings.mouseRotMult - def.mouseRotMult) > 0.0001f ||
		std::abs(camSettings.gpPanSpeed   - def.gpPanSpeed)   > 0.001f  ||
		std::abs(camSettings.gpZoomSpeed  - def.gpZoomSpeed)  > 0.001f  ||
		std::abs(camSettings.gpRotSpeed   - def.gpRotSpeed)   > 0.001f  ||
		std::abs(camSettings.gpFovSpeed   - def.gpFovSpeed)   > 0.001f  ||
		std::abs(camSettings.gpDeadzone   - def.gpDeadzone)   > 0.001f   ;

	if (isModified) {
		if (FUCK::Button("$RACE_RestoreDefaults"_T)) {
			camSettings = def;
			changed     = true;
		}
	}

	if (changed) {
		SaveSettings();
	}
}
