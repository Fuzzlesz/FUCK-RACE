#include "FUCK_Register.h"

#include "IconsFontAwesome6.h"

#include "RACE-Anims.h"
#include "RACE-Camera.h"
#include "RACE-Compat.h"
#include "RACE-Equip.h"
#include "RACE-Inputs.h"
#include "RACE-Light.h"
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
		auto             widget = RaceWidget::GetSingleton();
		std::string_view name(menuName);

		if (name == RE::RaceSexMenu::MENU_NAME) {
			widget->_isRaceMenuOpen = opening;
			if (opening) {
				widget->_wasActive = true;
				if (auto ui = RE::UI::GetSingleton()) {
					if (auto menu = ui->GetMenu(RE::RaceSexMenu::MENU_NAME)) {
						widget->_cachedRaceMenuMovie = menu->uiMovie.get();
					}
				}
			} else {
				widget->_cachedRaceMenuMovie = nullptr;
			}
		} else if (name == RE::JournalMenu::MENU_NAME) {
			widget->_isJournalOpen = opening;
			if (opening && widget->_isRaceMenuOpen) {
				if (auto ui = RE::UI::GetSingleton()) {
					if (auto jMenuObj = ui->GetMenu(RE::JournalMenu::MENU_NAME); jMenuObj && jMenuObj->uiMovie) {
						if (RE::GFxValue jMenu; jMenuObj->uiMovie->GetVariable(&jMenu, "_root.QuestJournalFader.Menu_mc")) {
							RE::GFxValue args[2]{ RE::GFxValue(2), RE::GFxValue(false) };
							jMenu.Invoke("RestoreSavedSettings", nullptr, args, 2);
						}
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

					// Intercept ShowRaceDescription to collapse stats and extend the list
					RE::GFxValue origShowRaceDesc;
					if (menuInstance.GetMember("ShowRaceDescription", &origShowRaceDesc) && origShowRaceDesc.IsObject()) {
						menuInstance.SetMember("FUCK_origShowRaceDescription", origShowRaceDesc);

						class ShowRaceDescHandler : public RE::GFxFunctionHandler
						{
						public:
							void Call(Params& a_params) override
							{
								if (a_params.argCount > 0 && a_params.thisPtr) {
									bool show = a_params.args[0].GetBool();
									if (RaceWidget::GetSingleton()->IsRaceStatsHidden()) {
										show = false;
									}
									RE::GFxValue arg(show);
									a_params.thisPtr->Invoke("FUCK_origShowRaceDescription", a_params.retVal, &arg, 1);
								}
							}
						};

						RE::GFxValue newShowRaceDesc;
						a_menu->uiMovie->CreateFunction(&newShowRaceDesc, new ShowRaceDescHandler());
						menuInstance.SetMember("ShowRaceDescription", newShowRaceDesc);
					}

					menuInstance.SetMember("FUCK_HooksApplied", RE::GFxValue(true));
				}

				// Apply Sculpt Panel Hooks
				if (GetCurrentMode() == 3) {
					RE::GFxValue vertexEditor;
					if (menuInstance.GetMember("vertexEditor", &vertexEditor) && vertexEditor.IsObject()) {
						// Hook Wireframe
						RE::GFxValue wireframeDisplay, foreground;
						if (vertexEditor.GetMember("wireframeDisplay", &wireframeDisplay) && wireframeDisplay.GetMember("foreground", &foreground)) {
							RE::GFxValue currentBeginRotate;
							if (foreground.GetMember("beginRotateMesh", &currentBeginRotate) && currentBeginRotate.IsObject()) {
								if (!currentBeginRotate.HasMember("FUCK_IsHooked")) {
									foreground.SetMember("FUCK_origBeginRotateMesh", currentBeginRotate);

									class BeginRotateHandler : public RE::GFxFunctionHandler
									{
									public:
										void Call(Params& a_params) override
										{
											if (RaceWidget::GetSingleton()->IsWireframeFrozen())
												return;  // Block execution
											if (a_params.thisPtr) {
												a_params.thisPtr->Invoke("FUCK_origBeginRotateMesh", a_params.retVal, a_params.args, a_params.argCount);
											}
										}
									};

									RE::GFxValue newBeginRotate;
									a_menu->uiMovie->CreateFunction(&newBeginRotate, new BeginRotateHandler());

									newBeginRotate.SetMember("FUCK_IsHooked", RE::GFxValue(true));

									foreground.SetMember("beginRotateMesh", newBeginRotate);
									foreground.SetMember("onPressAux", newBeginRotate);
								}
							}
						}
					}
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
		_hideRaceStats = FUCK::INI::LoadBool(ini, "Widget", "HideRaceStats", false);

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
		FUCK::INI::SaveBool(ini, "Widget", "HideRaceStats", _hideRaceStats, false);

		RaceCamera::GetSingleton()->SaveSettings(ini);
	});
}

bool RaceWidget::IsOpen() const
{
	bool isActive = IsActive();

	if (!isActive && _wasActive) {
		auto* self        = const_cast<RaceWidget*>(this);
		auto  animManager = RaceAnimManager::GetSingleton();

		animManager->InvalidateIdles();
		self->_lastMode          = -1;
		self->_uiHidden          = false;
		self->_showSettings      = false;
		self->_showLightSettings = false;
		self->_wasActive         = false;

		if (animManager->IsFrozen()) {
			animManager->SetPlayerFrozen(false);
		}

		RaceCamera::GetSingleton()->ResetOffsets();

		RaceEquipManager::GetSingleton()->RestoreEquipped();
		RaceEquipManager::GetSingleton()->Clear();

		RaceLightManager::GetSingleton()->SetWindowOpen(false);
		RaceReferenceManager::GetSingleton()->CloseAllWindows();
	}

	return isActive && !_isJournalOpen;
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
	if (!_cachedRaceMenuMovie) {
		return -1;
	}

	RE::GFxValue menuInstance;
	if (!GetMenuInstance(_cachedRaceMenuMovie, menuInstance)) {
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

bool RaceWidget::IsOnRaceTab() const
{
	if (GetCurrentMode() != 0 || !_cachedRaceMenuMovie)
		return false;

	RE::GFxValue menuInstance;
	if (GetMenuInstance(_cachedRaceMenuMovie, menuInstance)) {
		RE::GFxValue categoryList, selectedEntry, filterFlag;
		if (menuInstance.GetMember("categoryList", &categoryList) &&
			categoryList.GetMember("selectedEntry", &selectedEntry) &&
			selectedEntry.GetMember("flag", &filterFlag) &&
			filterFlag.IsNumber()) {
			return static_cast<int>(filterFlag.GetNumber()) == 2;  // CATEGORY_RACE
		}
	}
	return false;
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

	RE::GFxValue menuInstance;
	bool         hasMenu = _cachedRaceMenuMovie && GetMenuInstance(_cachedRaceMenuMovie, menuInstance);

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
			if (_cachedRaceMenuMovie->GetVariable(&root, "_root")) {
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

	auto lightManager = RaceLightManager::GetSingleton();
	lightManager->Update();

	float clusterScale = 0.8f;
	float alignOffset  = 0.0f;

	float baseWidth = FUCK::Scale(360.0f * clusterScale);

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

	FUCK::PushScale(clusterScale);

	FUCK::BeginGroup();

	float pad = FUCK::Scale(15.0f);

	FUCK::Dummy(ImVec2(0.0f, pad));

	FUCK::Indent(pad);
	FUCK::BeginGroup();
	if (currentMode == 3) {
		if (!isGamepad) {
			DrawSculptPanel();
		}
	} else {
		DrawMainPanel();
	}
	FUCK::EndGroup();
	FUCK::Unindent(pad);

	FUCK::Dummy(ImVec2(0.0f, pad));

	FUCK::EndGroup();

	FUCK::PopScale();

	if (isEditing) {
		ImVec2 winMin = FUCK::GetItemRectMin();
		ImVec2 winMax = FUCK::GetItemRectMax();

		float padding = FUCK::Scale(4.0f);
		winMin.x -= padding;
		winMin.y -= padding;
		winMax.x += padding;
		winMax.y += padding;

		FUCK::DrawEditorBounds(winMin, winMax, FUCK::IsWindowHovered(0) ? FUCK::EditorBoundsState::kHovered : FUCK::EditorBoundsState::kNormal, 2.0f, true);
	}
}

void RaceWidget::DrawIdleSelector(float a_comboWidth, bool& a_requestFocus)
{
	auto animManager = RaceAnimManager::GetSingleton();

	if (!_hideIdles) {
		if (a_requestFocus) {
			FUCK::SetKeyboardFocusHere(0);
			a_requestFocus = false;
		}

		FUCK::SetNextItemWidth(a_comboWidth);
		if (FUCK::ComboWithFilter("##RACE_PluginFilter", &animManager->GetSelectedPluginIndex(), animManager->GetPluginNamesCStr().data(), static_cast<int>(animManager->GetPluginNamesCStr().size()), 15))
			animManager->InvalidateIdles();
		FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(2.0f)));

		FUCK::SetNextItemWidth(a_comboWidth);
		if (FUCK::ComboWithFilter("##RACE_Idles", &animManager->GetSelectedIndex(), animManager->GetIdleNames().data(), static_cast<int>(animManager->GetIdleNames().size()), 15)) {
			animManager->PlaySelectedIdle();
		}
		FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(2.0f)));
	}
}

bool RaceWidget::DrawReferenceSelector(float a_comboWidth, bool* a_requestFocus)
{
	auto refManager = RaceReferenceManager::GetSingleton();
	bool isGamepad  = FUCK::GetInputDevice() == FUCK::InputDevice::kGamepad;

	if (!IsOnCameraTab() && !isGamepad && refManager->HasReferences()) {
		if (a_requestFocus && *a_requestFocus) {
			FUCK::SetKeyboardFocusHere(0);
			*a_requestFocus = false;
		}

		FUCK::SetNextItemWidth(a_comboWidth);
		int refIndex = 0;
		if (FUCK::ComboWithFilter("##RACE_RefImage", &refIndex, refManager->GetComboStrings().data(), static_cast<int>(refManager->GetComboStrings().size()), 15)) {
			if (refIndex > 0) {
				refManager->ToggleReference(refIndex - 1);
			}
		}
		return true;
	}
	return false;
}

void RaceWidget::DrawPlaybackControls()
{
	auto        animManager = RaceAnimManager::GetSingleton();
	bool        isFrozen    = animManager->IsFrozen();
	const char* playIcon    = isFrozen ? ICON_FA_PLAY : ICON_FA_PAUSE;

	FUCK::PushID("RACE_PlayToggle");
	if (FUCK::Button(playIcon)) {
		if (isFrozen) {
			animManager->TogglePlay();
		} else {
			animManager->SetPlayerFrozen(true);
		}
	}
	if (FUCK::IsItemHovered(0)) {
		FUCK::SetTooltip("$RACE_PlayPauseTooltip"_T);
	}
	FUCK::PopID();

	if (!_hideIdles && !IsOnSculptTab()) {
		FUCK::SameLine();
		FUCK::PushID("RACE_Replay");
		if (FUCK::Button(ICON_FA_BACKWARD)) {
			animManager->PlaySelectedIdle();
		}
		if (FUCK::IsItemHovered(0)) {
			FUCK::SetTooltip("$RACE_ReplayTooltip"_T);
		}
		FUCK::PopID();
	}

	FUCK::SameLine();
	FUCK::PushID("RACE_StopToggle");
	if (FUCK::Button(ICON_FA_STOP)) {
		animManager->StopCurrentIdle();
	}
	if (FUCK::IsItemHovered(0)) {
		FUCK::SetTooltip("$RACE_StopTooltip"_T);
	}
	FUCK::PopID();
}

void RaceWidget::DrawCameraHelpMarker()
{
	// Pull the help marker inwards
	FUCK::SameLine(0.0f, FUCK::Scale(2.0f));
	if (FUCK::GetInputDevice() == FUCK::InputDevice::kGamepad) {
		FUCK::HelpMarker("$RACE_CamTooltip_GP"_T);
	} else {
		FUCK::HelpMarker("$RACE_CamTooltip_KBM"_T);
	}
}

void RaceWidget::DrawToolButtons(float a_comboWidth, float a_rowStartX)
{
	bool isGamepad    = FUCK::GetInputDevice() == FUCK::InputDevice::kGamepad;
	auto eqManager    = RaceEquipManager::GetSingleton();
	auto lightManager = RaceLightManager::GetSingleton();

	bool hasLightStudio = (!isGamepad && lightManager->HasLightStudio() && !_isRaceMenuOpen);
	bool hasEquipBtn    = (!isGamepad && eqManager->HasItems());
	bool hasLight       = (SKEE64Compat::IsPresent() && !isGamepad && _isRaceMenuOpen);

	float spacingX = FUCK::GetStyleVarVec(ImGuiStyleVar_ItemSpacing).x;

	float clusterScale = 0.8f;
	float dynScale     = FUCK::GetGlobalScale() * clusterScale;

	float btnPadX    = 8.0f * dynScale;
	float gearWidth  = FUCK::CalcTextSize(ICON_FA_GEAR).x + (btnPadX * 2.0f);
	float toolsWidth = gearWidth;

	if (hasLight || hasLightStudio) {
		toolsWidth += spacingX + FUCK::CalcTextSize(ICON_FA_LIGHTBULB).x + (btnPadX * 2.0f);
	}
	if (hasEquipBtn) {
		toolsWidth += spacingX + FUCK::CalcTextSize("$RACE_EquipBtn"_T).x + (btnPadX * 2.0f);
	}

	FUCK::SameLine();
	float currentX = FUCK::GetCursorPos().x;
	float targetX  = a_rowStartX + a_comboWidth - toolsWidth;

	if (targetX > currentX) {
		FUCK::SetCursorPosX(targetX);
	}

	if (hasEquipBtn) {
		if (FUCK::Button("$RACE_EquipBtn"_T)) {
			if (FUCK::GetTime() - eqManager->GetLastCloseTime() > 0.25) {
				eqManager->ToggleWindow();
				if (eqManager->IsWindowOpen()) {
					ImVec2 maxPos = FUCK::GetItemRectMax();
					float  startX = FUCK::GetWindowPos().x + FUCK::Scale(15.0f);
					eqManager->SetSpawnPos(ImVec2(startX, maxPos.y + FUCK::Scale(4.0f)));

					_showSettings      = false;
					_showLightSettings = false;
					lightManager->SetWindowOpen(false);
				}
			}
		}
		FUCK::SameLine();
	}

	bool   openLightKBM = false;
	ImVec2 lightBtnMax;

	if (hasLight) {
		RE::GFxValue menuInstance;
		bool         isLightOn = false;
		if (_cachedRaceMenuMovie && GetMenuInstance(_cachedRaceMenuMovie, menuInstance)) {
			RE::GFxValue bShowLight;
			if (menuInstance.GetMember("bShowLight", &bShowLight) && bShowLight.IsBool())
				isLightOn = bShowLight.GetBool();
		}

		if (isLightOn) {
			FUCK::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.9f, 0.2f, 1.0f));
			FUCK::PushStyleColor(ImGuiCol_TextDisabled, ImVec4(0.8f, 0.7f, 0.15f, 1.0f));
		}

		FUCK::PushID("RACE_LightToggle");
		if (FUCK::Button(ICON_FA_LIGHTBULB)) {
			menuInstance.Invoke("onLightClicked", nullptr, nullptr, 0);
		}

		lightBtnMax = FUCK::GetItemRectMax();

		if (FUCK::IsItemClicked(1)) {
			openLightKBM = true;  // Queue the open request
		}

		if (FUCK::IsItemHovered(0)) {
			FUCK::SetTooltip("$RACE_LightTooltip"_T);
		}

		FUCK::PopID();

		if (isLightOn)
			FUCK::PopStyleColor(2);

		FUCK::SameLine();
	} else if (hasLightStudio) {
		FUCK::PushID("RACE_LightStudioToggle");
		if (FUCK::Button(ICON_FA_LIGHTBULB)) {
			SKSE::GetMessagingInterface()->Dispatch(0x1001, nullptr, 0, "FUCK-LIGHT");
		}
		if (FUCK::IsItemHovered(0)) {
			FUCK::SetTooltip("$RACE_LightStudio"_T);
		}
		FUCK::PopID();
		FUCK::SameLine();
	}

	if (FUCK::Button(ICON_FA_GEAR)) {
		_showSettings = !_showSettings;
		if (_showSettings) {
			_settingsJustOpened = true;
			_showLightSettings  = false;
			lightManager->SetWindowOpen(false);
			eqManager->SetWindowOpen(false);
		}
	}

	if (openLightKBM) {
		if (FUCK::GetTime() - lightManager->GetLastCloseTime() > 0.25) {
			lightManager->ToggleWindow();
			if (lightManager->IsWindowOpen()) {
				float startX = FUCK::GetWindowPos().x + FUCK::Scale(15.0f);
				lightManager->SetSpawnPos(ImVec2(startX, lightBtnMax.y + FUCK::Scale(4.0f)));

				_showSettings      = false;
				_showLightSettings = false;
				eqManager->SetWindowOpen(false);
			}
		}
	}
}

void RaceWidget::DrawCameraReset(float a_comboWidth, float a_rowStartX, bool a_isRaceTab)
{
	bool hasCam       = RaceCamera::GetSingleton()->IsCameraModified();
	bool drawCheckbox = a_isRaceTab;

	if (!hasCam && !drawCheckbox)
		return;

	FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(2.0f)));

	if (drawCheckbox) {
		FUCK::SetCursorPosX(a_rowStartX);
		if (FUCK::Checkbox("$RACE_HideRaceStats"_T, &_hideRaceStats, false)) {
			SaveSettings();
			ToggleRaceStats();
		}
	}

	if (hasCam) {
		float clusterScale = 0.8f;
		float dynScale     = FUCK::GetGlobalScale() * clusterScale;
		float btnPadX      = 8.0f * dynScale;
		float resetWidth   = FUCK::CalcTextSize("$RACE_ResetCam"_T).x + (btnPadX * 2.0f);

		if (drawCheckbox) {
			FUCK::SameLine();
		}

		FUCK::SetCursorPosX(a_rowStartX + a_comboWidth - resetWidth);

		if (FUCK::Button("$RACE_ResetCam"_T)) {
			RaceCamera::GetSingleton()->ResetOffsets();

			// Sync the ActionScript UI toggle so Quick Zoom doesn't require a double-press to re-engage
			RE::GFxValue menuInst;
			if (_cachedRaceMenuMovie && GetMenuInstance(_cachedRaceMenuMovie, menuInst)) {
				RE::GFxValue bPlayerZoom;
				if (menuInst.GetMember("bPlayerZoom", &bPlayerZoom)) {
					menuInst.SetMember("bPlayerZoom", RE::GFxValue(false));
					menuInst.Invoke("updateBottomBar", nullptr, nullptr, 0);
				}
			}
		}
	}
}

bool RaceWidget::UpdateGamepadFocus()
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

	return requestFocus;
}

void RaceWidget::UpdateBackButtonHold(bool a_useRaceMenuLight)
{
	// --- Gamepad Back Button Logic (Enforce Light ON or open Light Studio) ---
	static float s_backHoldTime             = 0.0f;
	static bool  s_backLongPressedTriggered = false;

	if (FUCK::GetInputDevice() != FUCK::InputDevice::kGamepad) {
		return;
	}

	if (!FUCK::IsInputDown(RACE::Keys::kGP_Back)) {
		s_backHoldTime             = 0.0f;
		s_backLongPressedTriggered = false;
		return;
	}

	s_backHoldTime += FUCK::GetDeltaTime();

	if (s_backHoldTime <= 0.5f || s_backLongPressedTriggered) {
		return;
	}

	if (a_useRaceMenuLight) {
		_showLightSettings = !_showLightSettings;

		if (_showLightSettings) {
			_showSettings = false;
			RaceEquipManager::GetSingleton()->SetWindowOpen(false);

			RE::GFxValue menuInstance;
			if (_cachedRaceMenuMovie && GetMenuInstance(_cachedRaceMenuMovie, menuInstance)) {
				RE::GFxValue bShowLight;
				if (menuInstance.GetMember("bShowLight", &bShowLight) && bShowLight.IsBool()) {
					if (!bShowLight.GetBool()) {
						menuInstance.Invoke("onLightClicked", nullptr, nullptr, 0);
					}
				}
			}
		}
	} else if (RaceLightManager::GetSingleton()->HasLightStudio()) {
		SKSE::GetMessagingInterface()->Dispatch(0x1001, nullptr, 0, "FUCK-LIGHT");
	}

	s_backLongPressedTriggered = true;
}

float RaceWidget::DrawGamepadEquipCombo(float a_comboWidth, bool& a_requestFocus)
{
	auto eqManager = RaceEquipManager::GetSingleton();

	if (FUCK::GetInputDevice() != FUCK::InputDevice::kGamepad || !eqManager->HasItems()) {
		return 0.0f;
	}

	float equipWidth  = a_comboWidth * 1.5f;
	float alignOffset = equipWidth - a_comboWidth;

	if (a_requestFocus) {
		FUCK::SetKeyboardFocusHere(0);
		a_requestFocus = false;
	}

	FUCK::SetNextItemWidth(equipWidth);
	int eqIndex = 0;
	if (FUCK::ComboWithFilter("##RACE_Equip", &eqIndex, eqManager->GetComboStrings().data(), static_cast<int>(eqManager->GetComboStrings().size()), 15)) {
		if (eqIndex > 0)
			eqManager->ToggleItem(eqIndex - 1);
	}
	FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(2.0f)));
	FUCK::Indent(alignOffset);

	return alignOffset;
}

void RaceWidget::DrawInlineLightSettings(float a_comboWidth)
{
	FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(5.0f)));

	if (FUCK::BeginTable("LightTopSepLocker", 1, FUCK::TableFlags::kSizingFixedFit, ImVec2(a_comboWidth, 0.0f))) {
		FUCK::TableSetupColumn("SepCol", FUCK::TableColumnFlags::kWidthFixed, a_comboWidth);
		FUCK::TableNextRow();
		FUCK::TableNextColumn();
		FUCK::SeparatorText("$RACE_LightSettingsTitle"_T);
		FUCK::EndTable();
	}

	RaceLightManager::GetSingleton()->DrawInlineSettings();

	FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(5.0f)));

	if (FUCK::BeginTable("LightBottomSepLocker", 1, FUCK::TableFlags::kSizingFixedFit, ImVec2(a_comboWidth, 0.0f))) {
		FUCK::TableSetupColumn("SepCol", FUCK::TableColumnFlags::kWidthFixed, a_comboWidth);
		FUCK::TableNextRow();
		FUCK::TableNextColumn();
		FUCK::Separator();
		FUCK::EndTable();
	}
}

void RaceWidget::DrawMainPanel()
{
	bool  requestFocus = UpdateGamepadFocus();
	bool  isGamepad    = FUCK::GetInputDevice() == FUCK::InputDevice::kGamepad;
	float clusterScale = 0.8f;
	float comboWidth   = FUCK::Scale(360.0f * clusterScale);
	bool  isRaceTab    = IsOnRaceTab();

	UpdateBackButtonHold(_isRaceMenuOpen);

	float equipIndent = DrawGamepadEquipCombo(comboWidth, requestFocus);

	DrawIdleSelector(comboWidth, requestFocus);

	if (DrawReferenceSelector(comboWidth, &requestFocus)) {
		FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(2.0f)));
	}

	if (requestFocus) {
		FUCK::SetKeyboardFocusHere(0);
		requestFocus = false;
	}

	float rowStartX = FUCK::GetCursorPos().x;

	DrawPlaybackControls();
	DrawToolButtons(comboWidth, rowStartX);
	DrawCameraHelpMarker();
	DrawCameraReset(comboWidth, rowStartX, isRaceTab);

	if (_showSettings) {
		DrawSettingsPanel();
	} else if (isGamepad && _showLightSettings) {
		DrawInlineLightSettings(comboWidth);
	}

	if (equipIndent > 0.0f) {
		FUCK::Unindent(equipIndent);
	}
}

void RaceWidget::CloseSettings()
{
	_showSettings      = false;
	_showLightSettings = false;
}

void RaceWidget::ToggleRaceStats()
{
	if (!_cachedRaceMenuMovie)
		return;

	RE::GFxValue menuInstance;
	if (GetMenuInstance(_cachedRaceMenuMovie, menuInstance)) {
		RE::GFxValue itemList;
		if (menuInstance.GetMember("itemList", &itemList) && itemList.IsObject()) {
			RE::GFxValue eventObj, selectedIndex;
			_cachedRaceMenuMovie->CreateObject(&eventObj);
			if (itemList.GetMember("selectedIndex", &selectedIndex)) {
				eventObj.SetMember("index", selectedIndex);
				menuInstance.Invoke("onSelectionChange", nullptr, &eventObj, 1);

				itemList.Invoke("validateNow", nullptr, nullptr, 0);
				itemList.Invoke("UpdateList", nullptr, nullptr, 0);
			}
		}
	}
}

void RaceWidget::DrawSettingsPanel()
{
	FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(5.0f)));

	FUCK::SeparatorText("$RACE_Settings"_T);

	float panelScale    = 0.9f;
	float clusterScale  = 0.8f * panelScale;
	float settingsWidth = FUCK::Scale(440.0f * clusterScale);

	FUCK::PushScale(clusterScale);
	FUCK::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0.0f, FUCK::Scale(4.0f) * panelScale));

	if (FUCK::BeginTable("SettingsWidthLocker", 1, FUCK::TableFlags::kSizingFixedFit, ImVec2(settingsWidth, 0.0f))) {
		FUCK::TableSetupColumn("SettingsCol", FUCK::TableColumnFlags::kWidthFixed, settingsWidth);
		FUCK::TableNextRow();
		FUCK::TableNextColumn();

		FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(2.0f)));

		bool changed = false;

		changed |= DrawCameraSettings();

		// One-shot: only applies on the frame the panel opens
		_settingsJustOpened = false;

		changed |= DrawGeneralSettings();

		if (changed) {
			SaveSettings();
		}

		FUCK::EndTable();
	}
	FUCK::PopStyleVar(1);
	FUCK::PopScale();
}

bool RaceWidget::DrawCameraSettings()
{
	auto& camSettings = RaceCamera::GetSingleton()->GetSettings();
	bool  changed     = false;
	bool  isGamepad   = FUCK::GetInputDevice() == FUCK::InputDevice::kGamepad;

	int flagsKBM = 0;
	int flagsGP  = 0;

	if (_settingsJustOpened) {
		if (isGamepad) {
			flagsGP = 2;  // ImGuiTabItemFlags_SetSelected
		} else {
			flagsKBM = 2;  // ImGuiTabItemFlags_SetSelected
		}
	}

	if (FUCK::BeginTabBar("RaceSettingsTabs", 0)) {
		// Keyboard & Mouse Tab
		if (!isGamepad) {
			if (FUCK::BeginTabItem("$RACE_CamSettings_KBM"_T, flagsKBM)) {
				FUCK::PushID("KBM");
				FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(4.0f)));

				changed |= FUCK::ScaledSliderFloat("$RACE_SpeedPan"_T,      &camSettings.kbmPanSpeed,         1.0f,  50.0f,    1.0f);
				changed |= FUCK::ScaledSliderFloat("$RACE_SpeedOrbit"_T,    &camSettings.kbmRotSpeed,         0.1f,   3.0f,   10.0f);
				changed |= FUCK::ScaledSliderFloat("$RACE_SpeedRoll"_T,     &camSettings.kbmRollSpeed,        0.1f,   3.0f,   10.0f);
				changed |= FUCK::ScaledSliderFloat("$RACE_SpeedFOV"_T,      &camSettings.kbmZoomFovSpeed,     5.0f,  60.0f,    1.0f);

				FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(4.0f)));
				FUCK::PopID();
				FUCK::EndTabItem();
				}
			}

			// Gamepad Tab
			if (FUCK::BeginTabItem("$RACE_CamSettings_GP"_T, flagsGP)) {
				FUCK::PushID("GP");
				FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(4.0f)));

				changed |= FUCK::ScaledSliderFloat("$RACE_SpeedPan"_T,      &camSettings.gpPanSpeed,          10.0f, 200.0f,   1.0f);
				changed |= FUCK::ScaledSliderFloat("$RACE_SpeedZoom"_T,     &camSettings.gpZoomSpeed,         10.0f, 200.0f,   1.0f);
				changed |= FUCK::ScaledSliderFloat("$RACE_SpeedOrbit"_T,    &camSettings.gpRotSpeed,           0.1f,   5.0f,  10.0f);
				changed |= FUCK::ScaledSliderFloat("$RACE_SpeedRoll"_T,     &camSettings.gpRollSpeed,          0.1f,   5.0f,  10.0f);
				changed |= FUCK::ScaledSliderFloat("$RACE_SpeedCharRot"_T,  &camSettings.gpCharRotSpeed,       0.1f,   5.0f,  10.0f);
				changed |= FUCK::ScaledSliderFloat("$RACE_Deadzone"_T,      &camSettings.gpDeadzone,           0.0f,   0.5f, 100.0f);

				FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(4.0f)));
				FUCK::PopID();
				FUCK::EndTabItem();
			}
			FUCK::EndTabBar();
		}

		FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(4.0f)));

		changed |= FUCK::ScaledSliderFloat("$RACE_QuickZoomDist"_T, &camSettings.quickZoomOffset,      10.0f, 200.0f,    1.0f);
		changed |= FUCK::ScaledSliderFloat("$RACE_QuickZoomDown"_T, &camSettings.quickZoomDownOffset,   0.0f, 100.0f,    1.0f);
		changed |= FUCK::ScaledSliderFloat("$RACE_QuickZoomPan"_T,  &camSettings.quickZoomSideOffset,-100.0f, 100.0f,    1.0f);

	FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(4.0f)));

	if (FUCK::Checkbox("$RACE_SmoothCamera"_T, &camSettings.smoothCamera, true, true)) {
		changed = true;
	}
	FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(4.0f)));

	constexpr float EPSILON = 0.001f;

		CameraSettings def;
	bool           isModified =
		std::abs(camSettings.kbmPanSpeed         - def.kbmPanSpeed)         > EPSILON ||
		std::abs(camSettings.kbmRotSpeed         - def.kbmRotSpeed)         > EPSILON ||
		std::abs(camSettings.kbmZoomFovSpeed     - def.kbmZoomFovSpeed)     > EPSILON ||
		std::abs(camSettings.gpPanSpeed          - def.gpPanSpeed)          > EPSILON ||
		std::abs(camSettings.gpZoomSpeed         - def.gpZoomSpeed)         > EPSILON ||
		std::abs(camSettings.gpRotSpeed          - def.gpRotSpeed)          > EPSILON ||
		std::abs(camSettings.gpFovSpeed          - def.gpFovSpeed)          > EPSILON ||
		std::abs(camSettings.gpCharRotSpeed      - def.gpCharRotSpeed)      > EPSILON ||
		std::abs(camSettings.quickZoomOffset     - def.quickZoomOffset)     > EPSILON ||
		std::abs(camSettings.quickZoomDownOffset - def.quickZoomDownOffset) > EPSILON ||
		std::abs(camSettings.quickZoomSideOffset - def.quickZoomSideOffset) > EPSILON ||
		std::abs(camSettings.gpDeadzone          - def.gpDeadzone)          > EPSILON ;

	if (isModified) {
		if (FUCK::Button("$RACE_RestoreDefaults"_T)) {
			camSettings = def;
			changed     = true;
		}
		FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(4.0f)));
	}

	return changed;
}

bool RaceWidget::DrawGeneralSettings()
{
	bool changed = false;

	changed |= FUCK::Checkbox("$RACE_HideIdles"_T, &_hideIdles, true, true);
	changed |= FUCK::Checkbox("$RACE_StartFrozen"_T, &_startFrozen, true, true);

	FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(4.0f)));

	return changed;
}

void RaceWidget::ApplyMirrorLock()
{
	if (!SKEE64Compat::IsPresent() || !_cachedRaceMenuMovie)
		return;

	double targetValue = _disableMirror ? 0.0 : 1.0;

	RE::GFxValue charGen;
	bool         hasCharGen = _cachedRaceMenuMovie->GetVariable(&charGen, "_global.skse.plugins.CharGen");

	RE::GFxValue menuInstance;
	if (GetMenuInstance(_cachedRaceMenuMovie, menuInstance)) {
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

	float clusterScale = 0.8f;
	float comboWidth   = FUCK::Scale(360.0f * clusterScale);
	float rowStartX    = FUCK::GetCursorPos().x;

	float spacingX = FUCK::GetStyleVarVec(ImGuiStyleVar_ItemSpacing).x;

	bool drewRef = DrawReferenceSelector(comboWidth);
	if (drewRef) {
		FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(4.0f)));
	}

	FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(3.0f)));

	// Calculate checkbox width: Checkbox Square + Spacing + Text
	float cbSquare = FUCK::GetFrameHeight();

	float cbTextMirror  = FUCK::CalcTextSize("$RACE_DisableMirror"_T).x;
	float cbWidthMirror = cbSquare + spacingX + cbTextMirror;

	FUCK::SetCursorPosX(rowStartX + comboWidth - cbWidthMirror);
	if (FUCK::Checkbox("$RACE_DisableMirror"_T, &_disableMirror, false)) {
		SaveSettings();
		ApplyMirrorLock();
	}

	float cbTextFreeze  = FUCK::CalcTextSize("$RACE_FreezeWireframe"_T).x;
	float cbWidthFreeze = cbSquare + spacingX + cbTextFreeze;

	FUCK::SetCursorPosX(rowStartX + comboWidth - cbWidthFreeze);

	if (FUCK::Checkbox("$RACE_FreezeWireframe"_T, &_freezeWireframe, false)) {}

	if (drewRef) {
		FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(3.0f)));
	}
	FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(5.0f)));

	// Calculate width: Play Btn + Spacing + Stop Btn
	auto        animManager = RaceAnimManager::GetSingleton();
	const char* playIcon    = animManager->IsFrozen() ? ICON_FA_PLAY : ICON_FA_PAUSE;

	float dynScale = FUCK::GetGlobalScale() * clusterScale;
	float btnPadX  = 8.0f * dynScale;

	float playWidth = FUCK::CalcTextSize(playIcon).x + (btnPadX * 2.0f);
	float stopWidth = FUCK::CalcTextSize(ICON_FA_STOP).x + (btnPadX * 2.0f);

	float controlsWidth = playWidth + spacingX + stopWidth;

	FUCK::SetCursorPosX(rowStartX + comboWidth - controlsWidth);
	DrawPlaybackControls();

	DrawCameraHelpMarker();

	DrawCameraReset(comboWidth, rowStartX, false);
}
