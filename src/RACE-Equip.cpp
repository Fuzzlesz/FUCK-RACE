#include "RACE-Equip.h"

void RaceEquipManager::Initialize()
{
	FUCK::AddMenuListener(this, [](const char* menuName, bool opening, void* userdata) {
		if (opening && std::string_view(menuName) == RE::RaceSexMenu::MENU_NAME) {
			static_cast<RaceEquipManager*>(userdata)->Populate();
		}
	});
}

void RaceEquipManager::Populate()
{
	_trackedItems.clear();
	_scanTimer = 0.0f;

	auto player = RE::PlayerCharacter::GetSingleton();
	if (!player)
		return;

	auto inventory = player->GetInventory();
	for (const auto& [item, data] : inventory) {
		if (data.second && data.second->IsWorn()) {
			std::string name = item->GetName();
			if (!name.empty()) {
				RE::ExtraDataList* equipExtra = nullptr;
				if (data.second->extraLists) {
					for (auto* xList : *data.second->extraLists) {
						if (xList && (xList->HasType(RE::ExtraDataType::kWorn) || xList->HasType(RE::ExtraDataType::kWornLeft))) {
							equipExtra = xList;
							break;
						}
					}
				}

				TrackedItem t;
				t.item       = item;
				t.extraData  = equipExtra;
				t.name       = name;
				t.isEquipped = true;
				_trackedItems.push_back(t);
			}
		}
	}

	std::sort(_trackedItems.begin(), _trackedItems.end(), [](const TrackedItem& a, const TrackedItem& b) {
		return _stricmp(a.name.c_str(), b.name.c_str()) < 0;
	});

	UpdateComboStrings();
}

void RaceEquipManager::Update()
{
	if (FUCK::GetInputDevice() == FUCK::InputDevice::kGamepad) {
		_isOpen = false;
	}

	if (_trackedItems.empty())
		return;

	_scanTimer += FUCK::GetDeltaTime();

	if (_scanTimer >= 1.0f) {
		_scanTimer = 0.0f;
		SyncStatuses();
	}
}

void RaceEquipManager::SyncStatuses()
{
	auto player = RE::PlayerCharacter::GetSingleton();
	if (!player)
		return;

	auto inventory = player->GetInventory();
	bool changed   = false;

	for (auto& tracked : _trackedItems) {
		auto it             = inventory.find(tracked.item);
		bool isActuallyWorn = false;

		if (it != inventory.end() && it->second.second) {
			isActuallyWorn = it->second.second->IsWorn();
		}

		if (tracked.isEquipped != isActuallyWorn) {
			tracked.isEquipped  = isActuallyWorn;
			changed             = true;
		}
	}

	if (changed) {
		UpdateComboStrings();
	}
}

void RaceEquipManager::UpdateComboStrings()
{
	_comboStrings.clear();

	_comboStrings.push_back("$RACE_EquipBtn"_T);

	for (const auto& item : _trackedItems) {
		std::string prefix = item.isEquipped ? "[ X ] " : "[    ] ";
		_comboStrings.push_back(prefix + item.name);
	}

	_comboStringsCStr.clear();
	for (const auto& str : _comboStrings) {
		_comboStringsCStr.push_back(str.c_str());
	}
}

void RaceEquipManager::ToggleItem(int a_index)
{
	if (a_index < 0 || a_index >= static_cast<int>(_trackedItems.size()))
		return;

	auto& tracked      = _trackedItems[a_index];
	auto  equipManager = RE::ActorEquipManager::GetSingleton();
	auto  player       = RE::PlayerCharacter::GetSingleton();

	if (equipManager && player && tracked.item) {
		if (tracked.isEquipped) {
			equipManager->UnequipObject(player, tracked.item, nullptr, 1, nullptr, true, false, false, true);
			tracked.isEquipped = false;
		} else {
			equipManager->EquipObject(player, tracked.item, tracked.extraData, 1, nullptr, true, false, false, true);
			tracked.isEquipped = true;
		}
		player->Update3DModel();
		UpdateComboStrings();
		_scanTimer = 0.0f;
	}
}

void RaceEquipManager::DrawWindow()
{
	FUCK::Dummy(ImVec2(FUCK::Scale(200.0f), 0.0f));

	FUCK::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "$RACE_EquipTitle"_T);
	FUCK::Separator();

	if (_trackedItems.empty()) {
		FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(5.0f)));
		FUCK::TextDisabled("$RACE_EquipEmpty"_T);
		FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(5.0f)));
	} else {
		for (size_t i = 0; i < _trackedItems.size(); ++i) {
			bool selected = _trackedItems[i].isEquipped;

			// Red color for unequipped items
			if (!selected) {
				FUCK::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
			}

			if (FUCK::Selectable(_trackedItems[i].name.c_str(), selected, 0, ImVec2(0, 0))) {
				ToggleItem(static_cast<int>(i));
			}

			if (!selected) {
				FUCK::PopStyleColor(1);
			}
		}
	}
}

void RaceEquipManager::RestoreEquipped()
{
	auto equipManager = RE::ActorEquipManager::GetSingleton();
	auto player       = RE::PlayerCharacter::GetSingleton();
	if (!equipManager || !player)
		return;

	bool needsUpdate = false;
	for (auto& tracked : _trackedItems) {
		if (!tracked.isEquipped) {
			equipManager->EquipObject(player, tracked.item, tracked.extraData, 1, nullptr, true, false, false, true);
			tracked.isEquipped = true;
			needsUpdate        = true;
		}
	}

	if (needsUpdate) {
		player->Update3DModel();
	}
}

void RaceEquipManager::Clear()
{
	_trackedItems.clear();
	_comboStrings.clear();
	_comboStringsCStr.clear();
	_isOpen = false;
}
