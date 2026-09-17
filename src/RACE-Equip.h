#pragma once

#include "PCH.h"
#include "RACE-Widget.h"

class RaceEquipManager : public REX::Singleton<RaceEquipManager>
{
public:
	void Initialize();
	void Populate();
	void Update();
	void RestoreEquipped();
	void Clear();

	void ToggleItem(int a_index);
	void DrawWindow();

	bool                            HasItems() const { return !_trackedItems.empty(); }
	const std::vector<const char*>& GetComboStrings() const { return _comboStringsCStr; }

	// Window Controls (for KBM)
	bool IsWindowOpen() const
	{
		if (!_isOpen)
			return false;
		return !RaceWidget::GetSingleton()->IsJournalOpen();
	}
	void SetWindowOpen(bool a_open) { _isOpen = a_open; }
	void ToggleWindow() { _isOpen = !_isOpen; }

	void SetSpawnPos(const ImVec2& a_pos)
	{
		_spawnPos        = a_pos;
		_requestSpawnPos = true;
	}

	bool ConsumeSpawnRequest(ImVec2& outPos)
	{
		if (_requestSpawnPos) {
			outPos           = _spawnPos;
			_requestSpawnPos = false;
			return true;
		}
		return false;
	}

private:
	struct TrackedItem
	{
		RE::TESBoundObject* item;
		RE::ExtraDataList*  extraData;
		std::string         name;
		bool                isEquipped;
	};

	void UpdateComboStrings();
	void SyncStatuses();

	std::vector<TrackedItem> _trackedItems;
	std::vector<std::string> _comboStrings;
	std::vector<const char*> _comboStringsCStr;

	bool   _isOpen = false;
	ImVec2 _spawnPos{};
	bool   _requestSpawnPos = false;

	float _scanTimer = 0.0f;
};
