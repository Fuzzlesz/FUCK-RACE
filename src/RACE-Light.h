#pragma once

class RaceLightManager : public REX::Singleton<RaceLightManager>
{
public:
	void Initialize();
	void Update();

	void DrawWindow();
	void DrawInlineSettings();

	bool IsWindowOpen() const;
	void SetWindowOpen(bool a_open);
	void ToggleWindow();

	void SetSpawnPos(const ImVec2& a_pos);
	bool ConsumeSpawnRequest(ImVec2& outPos);

	void OnRaceMenuOpen();
	void OnRaceMenuClose();

private:
	void ScanForRaceMenuLight();
	void ApplyToLight();

	bool   _isOpen = false;
	ImVec2 _spawnPos{};
	bool   _requestSpawnPos = false;

	RE::ObjectRefHandle _rmLightRef;
	bool                _lightFound = false;

	float         _fade        = 1.0f;
	std::uint32_t _radius      = 500;
	RE::Color     _color       = { 255, 255, 255, 0 };
	bool          _needsUpdate = false;
};
