#pragma once

class RaceWidget
{
public:
	static RaceWidget* GetSingleton()
	{
		static RaceWidget s;
		return &s;
	}

	void Initialize();

	bool IsOpen() const;
	bool GetRequestedPos(ImVec2& outPos);

	void Draw();
	void DrawMainPanel();
	void DrawSettingsPanel();

	void AddIdle(const std::string& a_name, RE::TESIdleForm* a_idle);

	void OnAdvanceMovie(RE::RaceSexMenu* a_menu);
	void SetPlayerFrozen(bool a_frozen);
	bool IsFrozen() const { return _isFrozen; }

	void SetSkee64Present(bool a_present) { _skee64Present = a_present; }
	bool IsSkee64Present() const { return _skee64Present; }
	bool IsUIHidden() const { return _uiHidden; }

	int  GetCurrentMode() const;
	bool IsOnSculptTab() const { return GetCurrentMode() == 3; }
	bool IsOnSlidersTab() const { return GetCurrentMode() == 0; }

	bool GetMenuInstance(RE::GFxMovieView* a_movie, RE::GFxValue& a_outInstance) const;

private:
	RaceWidget() = default;

	void LoadSettings();
	void SaveSettings();
	void HandlePositioning(ImVec2& expectedPos);
	void UpdateValidIdles();

	std::vector<std::pair<std::string, RE::TESIdleForm*>> _allIdles;
	std::vector<std::pair<std::string, RE::TESIdleForm*>> _validIdles;
	std::vector<const char*>                              _idleNames;

	std::vector<std::string> _pluginNames;
	std::vector<const char*> _pluginNamesCStr;
	int                      _selectedPluginIndex = 0;

	int  _selectedIndex = -1;
	bool _isDragging    = false;
	bool _isFrozen      = false;

	mutable bool _idlesValid = false;

	ImVec2 _anchorPos;
	ImVec2 _currentPos;

	bool _uiHidden      = false;
	bool _skee64Present = false;

	bool _showSettings       = false;
	bool _settingsJustOpened = false;

	bool _startFrozen = false;

	int _lastMode = -1;
};
