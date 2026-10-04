#pragma once

namespace FUCK
{
	class ColorPalette;
}

class RaceColorPicker : public REX::Singleton<RaceColorPicker>
{
public:
	void InstallHooks(RE::GFxMovieView* a_movie, RE::GFxValue& a_menuInstance);
	void OnAdvanceMovie(RE::GFxMovieView* a_movie, RE::GFxValue& a_menuInstance);

	void CommitRecent();

	bool IsFieldOpen() const;
	void Draw(float a_width);

private:
	bool GetOpenField(RE::GFxValue& a_colorField) const;
	void SyncFromField(RE::GFxValue& a_colorSelector);
	void DisableHairAlpha(RE::GFxValue& a_colorField);
	void SendColor(RE::GFxMovieView* a_movie, RE::GFxValue& a_colorField, std::uint32_t a_argb);
	void ApplyWhileBusy(RE::GFxValue& a_menuInstance);

	void QueueColor(std::uint32_t a_argb);
	void DrawPalette(FUCK::ColorPalette& a_palette, const char* a_id, const char* a_title, bool a_noAlpha);

	float                        _rgba[4]{ 1.0f, 1.0f, 1.0f, 1.0f };
	std::optional<std::uint32_t> _lastArgb;

	std::mutex                   _pendingLock;
	std::optional<std::uint32_t> _pendingArgb;
	std::optional<std::uint32_t> _acceptedArgb;

	bool                                  _awaitingApply = false;
	std::chrono::steady_clock::time_point _lastApply;
};
