#pragma once

class RaceWidget : public REX::Singleton<RaceWidget>
{
public:
	void Initialize();

	bool IsOpen() const;

	void Draw();
	void DrawMainPanel();
	void DrawSculptPanel();
	void DrawFittingRoomPanel();
	void DrawSettingsPanel(bool a_includeCamera);

	void DrawIdleSelector(float a_comboWidth, bool& a_requestFocus);
	bool DrawReferenceSelector(float a_comboWidth, bool* a_requestFocus = nullptr);
	void DrawPlaybackControls();
	void DrawToolButtons(float a_comboWidth, float a_rowStartX);
	void DrawCameraHelpMarker();
	void DrawCameraReset(float a_comboWidth, float a_rowStartX, bool a_isRaceTab);

	void OnAdvanceMovie(RE::RaceSexMenu* a_menu);

	void SetSkee64Present(bool a_present) { _skee64Present = a_present; }
	bool IsSkee64Present() const { return _skee64Present; }
	bool IsUIHidden() const { return _uiHidden; }

	bool              IsActive() const { return _isRaceMenuOpen || (_isFittingRoomOpen && _fittingRoomVisible); }
	bool              IsRaceMenuOpen() const { return _isRaceMenuOpen; }
	bool              IsFittingRoomOpen() const { return _isFittingRoomOpen; }
	bool              IsJournalOpen() const { return _isJournalOpen; }
	RE::GFxMovieView* GetCachedMenuMovie() const { return _cachedRaceMenuMovie; }

	int  GetCurrentMode() const;
	bool IsOnSculptTab() const { return GetCurrentMode() == 3; }
	bool IsOnCameraTab() const { return GetCurrentMode() == 2; }
	bool IsOnSlidersTab() const { return GetCurrentMode() == 0; }
	bool IsOnRaceTab() const;

	bool GetMenuInstance(RE::GFxMovieView* a_movie, RE::GFxValue& a_outInstance) const;

	bool IsWireframeFrozen() const { return _freezeWireframe; }
	bool IsRaceStatsHidden() const { return _hideRaceStats; }

	bool HasOpenPanels() const { return _showSettings || _showLightSettings; }
	void CloseSettings();
	void ToggleRaceStats();

private:
	void LoadSettings();
	void SaveSettings();
	void HandlePositioning(ImVec2& expectedPos);
	void ApplyMirrorLock();

	// Shared panel pieces
	bool  UpdateGamepadFocus();
	void  UpdateBackButtonHold();
	float DrawGamepadEquipCombo(float a_comboWidth, bool& a_requestFocus);
	void  DrawInlineLightSettings(float a_comboWidth);
	bool  DrawCameraSettings();
	bool  DrawGeneralSettings();

	bool _isDragging = false;

	ImVec2 _anchorPos;
	ImVec2 _currentPos;

	bool _uiHidden      = false;
	bool _skee64Present = false;

	bool              _isRaceMenuOpen      = false;
	bool              _isFittingRoomOpen   = false;
	bool              _fittingRoomVisible  = false;
	bool              _wasActive           = false;
	bool              _isJournalOpen       = false;
	RE::GFxMovieView* _cachedRaceMenuMovie = nullptr;

	bool _showSettings       = false;
	bool _settingsJustOpened = false;
	bool _showLightSettings  = false;

	bool _startFrozen       = false;
	bool _hideIdles         = false;
	bool _disableMirror     = false;
	bool _freezeWireframe   = false;
	bool _hideRaceStats     = false;
	bool _showInFittingRoom = true;
	bool _showAdvanced      = false;

	int _lastMode = -1;
};
