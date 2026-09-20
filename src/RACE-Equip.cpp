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
				TrackedItem t;
				t.item       = item;
				t.extraData  = nullptr;
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

	auto& tracked = _trackedItems[a_index];

	tracked.isEquipped = !tracked.isEquipped;
	UpdateComboStrings();
	_scanTimer = 0.0f;

	auto item      = tracked.item;
	bool equipping = tracked.isEquipped;

	SKSE::GetTaskInterface()->AddTask([item, equipping]() {
		auto equipManager = RE::ActorEquipManager::GetSingleton();
		auto player       = RE::PlayerCharacter::GetSingleton();

		if (equipManager && player && item) {
			if (equipping) {
				equipManager->EquipObject(player, item, nullptr, 1, nullptr, true, false, false, true);
			} else {
				equipManager->UnequipObject(player, item, nullptr, 1, nullptr, true, false, false, true);
			}
			player->Update3DModel();
		}
	});
}

void RaceEquipManager::DrawWindow()
{
	float clusterScale  = 0.8f;
	float expectedWidth = FUCK::Scale(360.0f * clusterScale);

	FUCK::PushScale(clusterScale);

	if (FUCK::BeginTable("EquipWidthLocker", 1, FUCK::TableFlags::kSizingFixedFit, ImVec2(expectedWidth, 0.0f))) {
		FUCK::TableSetupColumn("EquipCol", FUCK::TableColumnFlags::kWidthFixed, expectedWidth);
		FUCK::TableNextRow();
		FUCK::TableNextColumn();

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

		FUCK::EndTable();
	}

	FUCK::PopScale();
}

void RaceEquipManager::RestoreEquipped()
{
	std::vector<RE::TESBoundObject*> itemsToEquip;

	for (auto& tracked : _trackedItems) {
		if (!tracked.isEquipped) {
			itemsToEquip.push_back(tracked.item);
			tracked.isEquipped = true;
		}
	}

	if (itemsToEquip.empty())
		return;

	SKSE::GetTaskInterface()->AddTask([itemsToEquip]() {
		auto equipManager = RE::ActorEquipManager::GetSingleton();
		auto player       = RE::PlayerCharacter::GetSingleton();
		if (!equipManager || !player)
			return;

		for (auto* item : itemsToEquip) {
			equipManager->EquipObject(player, item, nullptr, 1, nullptr, true, false, false, true);
		}
		player->Update3DModel();
	});
}

// Window Controls (for KBM)
bool RaceEquipManager::IsWindowOpen() const
{
	if (!_isOpen)
		return false;
	return !RaceWidget::GetSingleton()->IsJournalOpen();
}

void RaceEquipManager::SetWindowOpen(bool a_open)
{
	if (!a_open && _isOpen) {
		_lastCloseTime = FUCK::GetTime();
	}
	_isOpen = a_open;
}

void RaceEquipManager::ToggleWindow()
{
	SetWindowOpen(!_isOpen);
}

void RaceEquipManager::SetSpawnPos(const ImVec2& a_pos)
{
	_spawnPos        = a_pos;
	_requestSpawnPos = true;
}

bool RaceEquipManager::ConsumeSpawnRequest(ImVec2& outPos)
{
	if (_requestSpawnPos) {
		outPos           = _spawnPos;
		_requestSpawnPos = false;
		return true;
	}
	return false;
}

void RaceEquipManager::Clear()
{
	_trackedItems.clear();
	_comboStrings.clear();
	_comboStringsCStr.clear();
	_isOpen = false;
}
