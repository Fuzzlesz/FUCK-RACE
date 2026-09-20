#include "RACE-Light.h"
#include "RACE-Widget.h"

#include "IconsFontAwesome6.h"

void RaceLightManager::Initialize()
{
	_hasLightStudio = (GetModuleHandleW(L"FUCK-LIGHT.dll") != nullptr);

	FUCK::AddMenuListener(this, [](const char* menuName, bool opening, void* userdata) {
		if (std::string_view(menuName) == RE::RaceSexMenu::MENU_NAME) {
			auto mgr = static_cast<RaceLightManager*>(userdata);
			if (opening) {
				mgr->OnRaceMenuOpen();
			} else {
				mgr->OnRaceMenuClose();
			}
		}
	});
}

void RaceLightManager::OnRaceMenuOpen()
{
	_isOpen     = false;
	_lightFound = false;
	_rmLightRef.reset();

	auto dataHandler = RE::TESDataHandler::GetSingleton();
	auto rmLightBase = dataHandler ? dataHandler->LookupForm<RE::TESObjectLIGH>(0x803, "RaceMenu.esp") : nullptr;

	if (rmLightBase) {
		_radius = rmLightBase->data.radius;
		_fade   = rmLightBase->fade;
		_color  = { rmLightBase->data.color.red, rmLightBase->data.color.green, rmLightBase->data.color.blue, 0 };
	}
}

void RaceLightManager::OnRaceMenuClose()
{
	_lightFound = false;
	_rmLightRef.reset();
}

void RaceLightManager::ScanForRaceMenuLight()
{
	auto player = RE::PlayerCharacter::GetSingleton();
	if (!player || !player->GetParentCell()) {
		return;
	}

	auto dataHandler = RE::TESDataHandler::GetSingleton();
	auto rmLightBase = dataHandler ? dataHandler->LookupForm<RE::TESObjectLIGH>(0x803, "RaceMenu.esp") : nullptr;

	if (!rmLightBase) {
		return;
	}

	for (auto& item : player->GetParentCell()->references) {
		if (auto ref = item.get()) {
			if (ref->GetBaseObject() == rmLightBase && !ref->IsDeleted()) {
				_rmLightRef = ref;
				_lightFound = true;
				break;
			}
		}
	}
}

void RaceLightManager::ApplyToLight()
{
	auto ref = _rmLightRef.get();
	if (!ref) {
		_lightFound = false;
		return;
	}

	if (auto base = ref->GetBaseObject()->As<RE::TESObjectLIGH>()) {
		base->data.radius      = _radius;
		base->fade             = _fade;
		base->data.color.red   = _color.red;
		base->data.color.green = _color.green;
		base->data.color.blue  = _color.blue;
	}

	if (ref->Is3DLoaded()) {
		if (auto root = ref->Get3D()) {
			RE::NiPointLight* pointLight = nullptr;

			std::function<void(RE::NiAVObject*)> findLight = [&](RE::NiAVObject* node) {
				if (pointLight)
					return;
				if (auto l = netimmerse_cast<RE::NiPointLight*>(node)) {
					pointLight = l;
					return;
				}
				if (auto niNode = node->AsNode()) {
					for (auto& child : niNode->children) {
						if (child)
							findLight(child.get());
					}
				}
			};
			findLight(root);

			if (pointLight) {
				pointLight->radius = { (float)_radius, (float)_radius, (float)_radius };
				pointLight->SetLightAttenuation((float)_radius);
				pointLight->fade    = _fade;
				pointLight->diffuse = RE::NiColor(_color.red / 255.0f, _color.green / 255.0f, _color.blue / 255.0f);

				RE::NiUpdateData ctx;
				pointLight->Update(ctx);
			}
		}
	}
}

void RaceLightManager::Update()
{
	if (!RaceWidget::GetSingleton()->IsRaceMenuOpen()) {
		return;
	}

	if (_needsUpdate && !_lightFound) {
		ScanForRaceMenuLight();
	}

	if (_needsUpdate && _lightFound) {
		ApplyToLight();
		_needsUpdate = false;
	}
}

bool RaceLightManager::IsWindowOpen() const
{
	if (!_isOpen)
		return false;
	return RaceWidget::GetSingleton()->IsRaceMenuOpen() && !RaceWidget::GetSingleton()->IsJournalOpen();
}

void RaceLightManager::SetWindowOpen(bool a_open)
{
	if (!a_open && _isOpen) {
		_lastCloseTime = FUCK::GetTime();
	}
	_isOpen = a_open;
}

void RaceLightManager::ToggleWindow()
{
	SetWindowOpen(!_isOpen);
}

void RaceLightManager::SetSpawnPos(const ImVec2& a_pos)
{
	_spawnPos        = a_pos;
	_requestSpawnPos = true;
}

bool RaceLightManager::ConsumeSpawnRequest(ImVec2& outPos)
{
	if (_requestSpawnPos) {
		outPos           = _spawnPos;
		_requestSpawnPos = false;
		return true;
	}
	return false;
}

void RaceLightManager::DrawInlineSettings()
{
	if (!_lightFound) {
		FUCK::TextDisabled("$RACE_LightNotSpawned"_T);
		_needsUpdate = true;
		return;
	}

	float clusterScale  = 0.8f;
	float settingsWidth = FUCK::Scale(360.0f * clusterScale);

	if (FUCK::BeginTable("LightInlineTable", 2, FUCK::TableFlags::kSizingStretchProp, ImVec2(settingsWidth, 0.0f))) {
		FUCK::TableSetupColumn("Labels", FUCK::TableColumnFlags::kWidthStretch, 0.35f);
		FUCK::TableSetupColumn("Widgets", FUCK::TableColumnFlags::kWidthStretch, 0.65f);

		FUCK::TableNextRow();
		FUCK::TableNextColumn();
		FUCK::AlignTextToFramePadding();
		FUCK::Text("$RACE_LightFade"_T);

		bool isGamepad = FUCK::GetInputDevice() == FUCK::InputDevice::kGamepad;

		FUCK::TableNextColumn();
		FUCK::SetNextItemWidth(-1.0f);
		if (isGamepad) {
			float displayFade = _fade * 100.0f;
			if (FUCK::DragFloat("##Brightness", &displayFade, 1.0f, 0.0f, 1000.0f, "%.0f")) {
				_fade        = displayFade / 100.0f;
				_needsUpdate = true;
			}
		} else {
			if (FUCK::ScaledSliderFloat("##Brightness", &_fade, 0.0f, 10.0f, 100.0f, "%.0f")) {
				_needsUpdate = true;
			}
		}

		FUCK::TableNextRow();
		FUCK::TableNextColumn();
		FUCK::AlignTextToFramePadding();
		FUCK::Text("$RACE_LightRadius"_T);

		FUCK::TableNextColumn();
		FUCK::SetNextItemWidth(-1.0f);
		int rad = static_cast<int>(_radius);
		if (isGamepad) {
			if (FUCK::DragInt("##Radius", &rad, 1.0f, 1, 2000)) {
				_radius      = static_cast<std::uint32_t>(rad);
				_needsUpdate = true;
			}
		} else {
			if (FUCK::SliderInt("##Radius", &rad, 1, 2000)) {
				_radius      = static_cast<std::uint32_t>(rad);
				_needsUpdate = true;
			}
		}

		FUCK::TableNextRow();
		FUCK::TableNextColumn();
		FUCK::AlignTextToFramePadding();
		FUCK::Text("$RACE_LightColor"_T);

		FUCK::TableNextColumn();
		FUCK::SetNextItemWidth(-1.0f);
		float col[3] = { _color.red / 255.0f, _color.green / 255.0f, _color.blue / 255.0f };
		if (FUCK::ColorEdit3("##ColorPicker", col, ImGuiColorEditFlags_NoLabel)) {
			_color.red   = static_cast<std::uint8_t>(col[0] * 255.0f);
			_color.green = static_cast<std::uint8_t>(col[1] * 255.0f);
			_color.blue  = static_cast<std::uint8_t>(col[2] * 255.0f);
			_needsUpdate = true;
		}

		FUCK::EndTable();
	}
}

void RaceLightManager::DrawWindow()
{
	float clusterScale  = 0.8f;
	float expectedWidth = FUCK::Scale(360.0f * clusterScale);

	FUCK::PushScale(clusterScale);

	if (FUCK::BeginTable("LightWidthLocker", 1, FUCK::TableFlags::kSizingFixedFit, ImVec2(expectedWidth, 0.0f))) {
		FUCK::TableSetupColumn("LightCol", FUCK::TableColumnFlags::kWidthFixed, expectedWidth);
		FUCK::TableNextRow();
		FUCK::TableNextColumn();

		FUCK::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "$RACE_LightSettingsTitle"_T);
		FUCK::Separator();
		FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(2.0f)));

		DrawInlineSettings();

		if (HasLightStudio()) {
			FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(8.0f)));
			FUCK::Separator();
			FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(2.0f)));
			std::string btnText = std::string(ICON_FA_GEAR " ") + FUCK::Translate("$RACE_LightStudio");
			if (FUCK::Button(btnText.c_str())) {
				auto         widget = RaceWidget::GetSingleton();
				RE::GFxValue menuInstance;
				if (auto movie = widget->GetCachedMenuMovie()) {
					if (widget->GetMenuInstance(movie, menuInstance)) {
						RE::GFxValue bShowLight;
						if (menuInstance.GetMember("bShowLight", &bShowLight) && bShowLight.IsBool() && bShowLight.GetBool()) {
							menuInstance.Invoke("onLightClicked", nullptr, nullptr, 0);
						}
					}
				}

				SKSE::GetMessagingInterface()->Dispatch(0x1001, nullptr, 0, "FUCK-LIGHT");
				_isOpen = false;
			}
		}

		FUCK::EndTable();
	}

	FUCK::PopScale();
}
