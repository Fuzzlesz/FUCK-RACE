#pragma once

class RaceWidget : public REX::Singleton<RaceWidget>
{
public:
	void Initialize();

	bool IsOpen() const;

	void Draw();
	void DrawMainPanel();
	void DrawSettingsPanel();
	void DrawSculptPanel();

	void OnAdvanceMovie(RE::RaceSexMenu* a_menu);

	void SetSkee64Present(bool a_present) { _skee64Present = a_present; }
	bool IsSkee64Present() const { return _skee64Present; }
	bool IsUIHidden() const { return _uiHidden; }

	bool              IsRaceMenuOpen() const { return _isRaceMenuOpen; }
	bool              IsJournalOpen() const { return _isJournalOpen; }
	RE::GFxMovieView* GetCachedMenuMovie() const { return _cachedRaceMenuMovie; }

	int  GetCurrentMode() const;
	bool IsOnSculptTab() const { return GetCurrentMode() == 3; }
	bool IsOnCameraTab() const { return GetCurrentMode() == 2; }
	bool IsOnSlidersTab() const { return GetCurrentMode() == 0; }

	bool GetMenuInstance(RE::GFxMovieView* a_movie, RE::GFxValue& a_outInstance) const;

private:
	void LoadSettings();
	void SaveSettings();
	void HandlePositioning(ImVec2& expectedPos);
	void ApplyMirrorLock();

	bool _isDragging = false;

	ImVec2 _anchorPos;
	ImVec2 _currentPos;

	bool _uiHidden      = false;
	bool _skee64Present = false;

	bool              _isRaceMenuOpen      = false;
	bool              _isJournalOpen       = false;
	RE::GFxMovieView* _cachedRaceMenuMovie = nullptr;

	bool _showSettings       = false;
	bool _settingsJustOpened = false;

	bool _startFrozen   = false;
	bool _hideIdles     = false;
	bool _disableMirror = false;

	int _lastMode = -1;
};
