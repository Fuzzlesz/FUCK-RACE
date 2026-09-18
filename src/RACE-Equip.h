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

	bool IsWindowOpen() const;
	void SetWindowOpen(bool a_open);
	void ToggleWindow();

	void SetSpawnPos(const ImVec2& a_pos);
	bool ConsumeSpawnRequest(ImVec2& outPos);

	double GetLastCloseTime() const { return _lastCloseTime; }

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

	float  _scanTimer     = 0.0f;
	double _lastCloseTime = 0.0;
};
