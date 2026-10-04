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

	bool   _awaitingApply = false;
	double _lastApply     = 0.0;

	// A tint chosen by one slider and coloured by another. Changing texture applies the game's
	// default colour, so we resend ours and keep the swatch on it
	struct TintLayer
	{
		double             tintType;
		double             sliderID;
		RE::TintMask::Type maskType;
		const char*        indexTag;
		const char*        colorTag;

		double                       lastIndex    = 0.0;
		double                       lastColorPos = 0.0;
		std::optional<std::uint32_t> color;
		double                       restoreUntil = 0.0;  // Set while our colour is on its way, as the game's mask is stale until it lands
		double                       matchedSince = 0.0;
		double                       guardUntil   = 0.0;
		double                       lastGuard    = 0.0;

		void Restore(double a_until)
		{
			restoreUntil = a_until;
			matchedSince = 0.0;
		}
		void Forget()
		{
			color.reset();
			restoreUntil = 0.0;
			guardUntil   = 0.0;
		}
	};

	void OnSliderColorsSet(RE::GFxValue& a_menuInstance);
	void SyncTintLayer(RE::GFxValue& a_menuInstance, TintLayer& a_layer, bool a_fieldOpen);
	void FollowGame(TintLayer& a_layer, double a_now);
	void SendLayerColor(RE::GFxValue& a_menuInstance, const TintLayer& a_layer, double a_index, std::uint32_t a_argb);
	void ClearStrayMasks(RE::GFxValue& a_menuInstance, const TintLayer& a_layer);
	void SetLayerColor(RE::GFxValue& a_menuInstance, RE::GFxValue& a_colorEntry, std::uint32_t a_argb);
	void RememberFieldColor(RE::GFxValue& a_menuInstance);

	// RaceMenuDefines TINT_TYPE_WARPAINT / STATIC_SLIDER_WARPAINT, TINT_TYPE_DIRT / STATIC_SLIDER_DIRT
	std::array<TintLayer, 2> _tintLayers{ {
		{ 7.0, 7.0, RE::TintMask::Type::kWarPaint, "FUCK_seenWarpaint", "FUCK_seenWarpaintColor" },
		{ 14.0, 4.0, RE::TintMask::Type::kDirt, "FUCK_seenDirt", "FUCK_seenDirtColor" },
	} };
	bool                     _wasFieldOpen = false;
};
