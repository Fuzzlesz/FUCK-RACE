#pragma once

class RaceAnimManager : public REX::Singleton<RaceAnimManager>
{
public:
	void Initialize();

	void AddIdle(const std::string& a_name, RE::TESIdleForm* a_idle);
	void UpdateValidIdles();

	void SetPlayerFrozen(bool a_frozen);
	bool IsFrozen() const { return _isFrozen; }

	void OnAdvanceMovie();

	bool AreIdlesValid() const { return _idlesValid; }
	void InvalidateIdles() { _idlesValid = false; }

	const std::vector<const char*>& GetPluginNamesCStr() const { return _pluginNamesCStr; }
	int&                            GetSelectedPluginIndex() { return _selectedPluginIndex; }

	const std::vector<const char*>& GetIdleNames() const { return _idleNames; }
	int&                            GetSelectedIndex() { return _selectedIndex; }

	void PlaySelectedIdle();
	void TogglePlay();
	void StopCurrentIdle();

private:
	std::vector<std::pair<std::string, RE::TESIdleForm*>> _allIdles;
	std::vector<std::pair<std::string, RE::TESIdleForm*>> _validIdles;
	std::vector<const char*>                              _idleNames;

	std::vector<std::string> _pluginNames;
	std::vector<const char*> _pluginNamesCStr;
	int                      _selectedPluginIndex = 0;

	int  _selectedIndex = -1;
	bool _isFrozen      = false;
	bool _idlesValid    = false;
};
