#pragma once

class RaceLightManager :
	public REX::Singleton<RaceLightManager>,
	public RE::BSTEventSink<SKSE::ModCallbackEvent>
{
public:
	RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event, RE::BSTEventSource<SKSE::ModCallbackEvent>*) override;

	void Initialize();
	void Update();

	void DrawWindow();
	void DrawInlineSettings();

	bool IsWindowOpen() const;
	void SetWindowOpen(bool a_open);
	void ToggleWindow();

	void SetSpawnPos(const ImVec2& a_pos);
	bool ConsumeSpawnRequest(ImVec2& outPos);

	double GetLastCloseTime() const { return _lastCloseTime; }

	void OnRaceMenuOpen();
	void OnRaceMenuClose();
	void OnAdvanceMovie();

	bool HasLightStudio() const { return _hasLightStudio; }

private:
	RE::TESObjectLIGH*       GetRaceMenuLightBase();
	static RE::NiPointLight* FindPointLight(RE::NiAVObject* a_node);

	void ScanForRaceMenuLight();
	void ApplyToLight();
	void PlaceOwnLight();
	void RemoveOwnLight();

	bool   _isOpen         = false;
	bool   _hasLightStudio = false;

	ImVec2 _spawnPos{};
	bool   _requestSpawnPos = false;
	double _lastCloseTime   = 0.0;

	RE::TESObjectLIGH*  _rmLightBase = nullptr;
	RE::ObjectRefHandle _rmLightRef;
	bool                _lightFound = false;

	// Placed here when RaceMenu's script is too slow to place its own (see OnAdvanceMovie)
	RE::ObjectRefHandle _ownLightRef;
	double              _openTime     = 0.0;
	double              _lastScanTime = 0.0;

	float         _fade        = 1.0f;
	std::uint32_t _radius      = 500;
	RE::Color     _color       = { 255, 255, 255, 0 };
	bool          _needsUpdate = false;
};
