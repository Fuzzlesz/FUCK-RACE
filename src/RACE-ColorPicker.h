#pragma once

class RaceColorPicker : public REX::Singleton<RaceColorPicker>
{
public:
	void InstallHooks(RE::GFxMovieView* a_movie, RE::GFxValue& a_menuInstance);
	void OnAdvanceMovie(RE::GFxMovieView* a_movie, RE::GFxValue& a_menuInstance);

	bool IsFieldOpen() const;
	void Draw(float a_width);

private:
	bool GetOpenField(RE::GFxValue& a_colorField) const;
	void SyncFromField(RE::GFxValue& a_colorSelector);
	void DisableHairAlpha(RE::GFxValue& a_colorField);
	void SendColor(RE::GFxMovieView* a_movie, RE::GFxValue& a_colorField, std::uint32_t a_argb);
	void ApplyWhileBusy(RE::GFxValue& a_menuInstance);

	float                        _rgba[4]{ 1.0f, 1.0f, 1.0f, 1.0f };
	std::optional<std::uint32_t> _lastArgb;

	std::mutex                   _pendingLock;
	std::optional<std::uint32_t> _pendingArgb;

	bool                                  _awaitingApply = false;
	std::chrono::steady_clock::time_point _lastApply;
};
