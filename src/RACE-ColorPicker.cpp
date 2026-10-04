#include "RACE-ColorPicker.h"
#include "RACE-Settings.h"
#include "RACE-Widget.h"

namespace
{
	constexpr double TINT_TYPE_HAIR    = 128.0;  // RaceMenuDefines.TINT_TYPE_HAIR
	constexpr double ENTRY_TYPE_SLIDER = 2.0;    // RaceMenuDefines.ENTRY_TYPE_SLIDER

	constexpr double APPLY_INTERVAL  = 0.1;  // Same pace as RaceMenu's requestUpdate
	constexpr double RESTORE_TIMEOUT = 5.0;  // Gives up on a colour that never lands
	constexpr double SETTLE_TIME     = 0.25;
	constexpr double GUARD_WINDOW    = 2.0;
	constexpr double GUARD_INTERVAL  = 0.2;

	// Replaces an ActionScript function while keeping the original callable
	class WrappedHandler : public RE::GFxFunctionHandler
	{
	public:
		explicit WrappedHandler(std::string a_origName) :
			_origName(std::move(a_origName)) {}

	protected:
		void CallOriginal(Params& a_params) const
		{
			if (a_params.thisPtr) {
				a_params.thisPtr->Invoke(_origName.c_str(), a_params.retVal, a_params.args, a_params.argCount);
			}
		}

	private:
		std::string _origName;
	};

	template <class THandler>
	void WrapMember(RE::GFxMovieView* a_movie, RE::GFxValue& a_object, const char* a_name)
	{
		RE::GFxValue orig;
		if (!a_object.GetMember(a_name, &orig) || !orig.IsObject())
			return;

		std::string origName = std::string("FUCK_orig_") + a_name;
		a_object.SetMember(origName.c_str(), orig);

		RE::GFxValue wrapped;
		a_movie->CreateFunction(&wrapped, new THandler(std::move(origName)));
		a_object.SetMember(a_name, wrapped);
	}

	bool FindOpenField(RE::GFxValue& a_menuInstance, RE::GFxValue& a_colorField)
	{
		RE::GFxValue visible;
		return a_menuInstance.GetMember("colorField", &a_colorField) && a_colorField.IsObject() &&
		       a_colorField.GetMember("_visible", &visible) && visible.IsBool() && visible.GetBool();
	}

	std::uint32_t FieldArgb(double a_rgb, double a_alpha)
	{
		auto rgb   = static_cast<std::uint32_t>(a_rgb) & 0xFFFFFF;
		auto alpha = static_cast<std::uint32_t>(std::clamp(a_alpha, 0.0, 255.0));
		return (alpha << 24) | rgb;
	}

	// AS colours are signed, from its bitwise ops, and RaceMenu's scripts read them back as int
	std::uint32_t ToArgb(double a_value)
	{
		return static_cast<std::uint32_t>(static_cast<std::int64_t>(a_value));
	}

	RE::GFxValue ToNumber(std::uint32_t a_argb)
	{
		return RE::GFxValue(static_cast<double>(static_cast<std::int32_t>(a_argb)));
	}

	std::uint32_t EntryColor(RE::GFxValue& a_entry)
	{
		RE::GFxValue fillColor;
		return a_entry.GetMember("fillColor", &fillColor) && fillColor.IsNumber() ? ToArgb(fillColor.GetNumber()) : 0;
	}

	// Mask alpha is a float, so allow a step of rounding
	bool SameColor(std::uint32_t a_lhs, std::uint32_t a_rhs)
	{
		int alphaDiff = static_cast<int>(a_lhs >> 24) - static_cast<int>(a_rhs >> 24);
		return (a_lhs & 0xFFFFFF) == (a_rhs & 0xFFFFFF) && std::abs(alphaDiff) <= 1;
	}

	// The entry SetSliderColors writes to, so we always agree on the swatch
	bool GetColorEntry(RE::GFxValue& a_menuInstance, double a_tintType, RE::GFxValue& a_entry)
	{
		RE::GFxValue tintType(a_tintType);
		return a_menuInstance.Invoke("GetSliderByType", &a_entry, &tintType, 1) && a_entry.IsObject();
	}

	bool FindSliderByID(RE::GFxValue& a_menuInstance, double a_sliderID, RE::GFxValue& a_entry)
	{
		RE::GFxValue itemList;
		RE::GFxValue entryList;
		if (!a_menuInstance.GetMember("itemList", &itemList) || !itemList.GetMember("entryList", &entryList) || !entryList.IsArray())
			return false;

		for (std::uint32_t i = 0; i < entryList.GetArraySize(); ++i) {
			RE::GFxValue sliderID;
			if (entryList.GetElement(i, &a_entry) && a_entry.IsObject() &&
				a_entry.GetMember("sliderID", &sliderID) && sliderID.IsNumber() && sliderID.GetNumber() == a_sliderID) {
				return true;
			}
		}
		return false;
	}

	enum class SliderState
	{
		kSteady,
		kFresh,
		kMoved
	};

	// Entries are tagged on first sight, so a rebuilt list (menu open, race change, preset load)
	// reads as fresh rather than a user change
	SliderState TrackSlider(RE::GFxValue& a_entry, const char* a_tag, double& a_lastPos)
	{
		RE::GFxValue position;
		if (!a_entry.GetMember("position", &position) || !position.IsNumber())
			return SliderState::kSteady;

		double pos = position.GetNumber();
		if (!a_entry.HasMember(a_tag)) {
			a_entry.SetMember(a_tag, RE::GFxValue(true));
			a_lastPos = pos;
			return SliderState::kFresh;
		}

		return pos == std::exchange(a_lastPos, pos) ? SliderState::kSteady : SliderState::kMoved;
	}

	// One mask per texture, in the order RaceMenu's tint index counts them
	std::vector<RE::TintMask*> GetMasks(RE::TintMask::Type a_type)
	{
		std::vector<RE::TintMask*> masks;
		if (auto player = RE::PlayerCharacter::GetSingleton()) {
			for (auto mask : player->GetPlayerRuntimeData().tintMasks) {
				if (mask && mask->type == a_type) {
					masks.push_back(mask);
				}
			}
		}
		return masks;
	}

	// Only the selected texture's mask should have any alpha. The rest keep whatever colour they
	// last had, and SetSliderColors takes the last visible one
	std::optional<std::uint32_t> GetActiveMask(RE::TintMask::Type a_type, double a_index)
	{
		if (a_index < 0.0)
			return std::nullopt;

		auto masks = GetMasks(a_type);
		auto index = static_cast<std::size_t>(a_index);
		if (index >= masks.size() || masks[index]->alpha <= 0.0f)
			return std::nullopt;

		auto mask  = masks[index];
		auto alpha = static_cast<std::uint32_t>(std::lround(std::clamp(mask->alpha, 0.0f, 1.0f) * 255.0f));
		return (alpha << 24) | (mask->color.red << 16) | (mask->color.green << 8) | mask->color.blue;
	}

	std::uint32_t ToByte(float a_value)
	{
		return static_cast<std::uint32_t>(std::lround(std::clamp(a_value, 0.0f, 1.0f) * 255.0f));
	}

	std::uint32_t PackArgb(const float a_rgba[4])
	{
		return (ToByte(a_rgba[3]) << 24) | (ToByte(a_rgba[0]) << 16) | (ToByte(a_rgba[1]) << 8) | ToByte(a_rgba[2]);
	}

	void UnpackArgb(std::uint32_t a_argb, float a_rgba[4])
	{
		a_rgba[0] = ((a_argb >> 16) & 0xFF) / 255.0f;
		a_rgba[1] = ((a_argb >> 8) & 0xFF) / 255.0f;
		a_rgba[2] = (a_argb & 0xFF) / 255.0f;
		a_rgba[3] = (a_argb >> 24) / 255.0f;
	}
}

void RaceColorPicker::InstallHooks(RE::GFxMovieView* a_movie, RE::GFxValue& a_menuInstance)
{
	// Once per menu instance, so the last session's state is stale
	_wasFieldOpen  = false;
	_awaitingApply = false;
	for (auto& layer : _tintLayers) {
		layer.Forget();
	}

	// RaceMenu's only route from the game's tints to the swatches. On the instance, this shadows the class method
	class SliderColorsHandler : public WrappedHandler
	{
	public:
		using WrappedHandler::WrappedHandler;

		void Call(Params& a_params) override
		{
			CallOriginal(a_params);
			if (a_params.thisPtr) {
				RaceColorPicker::GetSingleton()->OnSliderColorsSet(*a_params.thisPtr);
			}
		}
	};

	WrapMember<SliderColorsHandler>(a_movie, a_menuInstance, "SetSliderColors");

	RE::GFxValue colorField;
	if (!a_menuInstance.GetMember("colorField", &colorField) || !colorField.IsObject())
		return;

	// Swallow keys while typing in the picker, or E/Tab would accept/cancel the field
	class InputHandler : public WrappedHandler
	{
	public:
		using WrappedHandler::WrappedHandler;

		void Call(Params& a_params) override
		{
			if (FUCK::IsAnyItemActive()) {
				if (a_params.retVal) {
					*a_params.retVal = RE::GFxValue(true);
				}
				return;
			}
			CallOriginal(a_params);
		}
	};

	// Record accepted colours for the recent row
	class AcceptHandler : public WrappedHandler
	{
	public:
		using WrappedHandler::WrappedHandler;

		void Call(Params& a_params) override
		{
			RE::GFxValue colorSelector;
			RE::GFxValue color;
			RE::GFxValue alpha;
			if (a_params.thisPtr && a_params.thisPtr->GetMember("colorSelector", &colorSelector) &&
				colorSelector.Invoke("getColor", &color, nullptr, 0) && colorSelector.Invoke("getAlpha", &alpha, nullptr, 0) &&
				color.IsNumber() && alpha.IsNumber()) {
				auto             picker = RaceColorPicker::GetSingleton();
				std::scoped_lock lock(picker->_pendingLock);
				picker->_acceptedArgb = FieldArgb(color.GetNumber(), alpha.GetNumber());
			}
			CallOriginal(a_params);
		}
	};

	// RaceMenu's single save/load slot, replaced entirely by our swatches
	class PresetHandler : public WrappedHandler
	{
	public:
		using WrappedHandler::WrappedHandler;

		void Call(Params&) override
		{
			// Block RaceMenu's native system entirely in favour of ours
		}
	};

	WrapMember<InputHandler>(a_movie, colorField, "handleInput");
	WrapMember<AcceptHandler>(a_movie, colorField, "onAccept");
	WrapMember<PresetHandler>(a_movie, colorField, "onLoadColor");
	WrapMember<PresetHandler>(a_movie, colorField, "onSaveColor");
}

void RaceColorPicker::OnAdvanceMovie(RE::GFxMovieView* a_movie, RE::GFxValue& a_menuInstance)
{
	std::optional<std::uint32_t> argb;
	{
		std::scoped_lock lock(_pendingLock);
		argb = std::exchange(_pendingArgb, std::nullopt);
	}

	// A change queued after the field closed is dropped, so it can't preview over an accept/cancel
	RE::GFxValue colorField;
	bool         fieldOpen = FindOpenField(a_menuInstance, colorField);
	if (fieldOpen) {
		DisableHairAlpha(colorField);

		RE::GFxValue presetPanel;
		if (colorField.GetMember("presetPanel", &presetPanel) && presetPanel.IsObject()) {
			presetPanel.SetMember("_visible", RE::GFxValue(false));
		}
		if (argb) {
			SendColor(a_movie, colorField, *argb);
		}
	}

	ApplyWhileBusy(a_menuInstance);

	// Whatever the field left on a layer's entry (accept or cancel) becomes its colour
	if (_wasFieldOpen && !fieldOpen) {
		RememberFieldColor(a_menuInstance);
	}
	_wasFieldOpen = fieldOpen;

	if (RaceWidget::GetSingleton()->IsOnSlidersTab()) {
		for (auto& layer : _tintLayers) {
			SyncTintLayer(a_menuInstance, layer, fieldOpen);
		}
	}
}

bool RaceColorPicker::GetOpenField(RE::GFxValue& a_colorField) const
{
	auto widget = RaceWidget::GetSingleton();
	auto movie  = widget->GetCachedMenuMovie();

	RE::GFxValue menuInstance;
	return movie && widget->GetMenuInstance(movie, menuInstance) && FindOpenField(menuInstance, a_colorField);
}

bool RaceColorPicker::IsFieldOpen() const
{
	RE::GFxValue colorField;
	return GetOpenField(colorField);
}

void RaceColorPicker::SyncFromField(RE::GFxValue& a_colorSelector)
{
	RE::GFxValue color;
	RE::GFxValue alpha;
	if (!a_colorSelector.Invoke("getColor", &color, nullptr, 0) || !a_colorSelector.Invoke("getAlpha", &alpha, nullptr, 0) ||
		!color.IsNumber() || !alpha.IsNumber()) {
		return;
	}

	std::uint32_t argb = FieldArgb(color.GetNumber(), alpha.GetNumber());

	// Only resync on outside changes, so our own edits aren't rounded to 8 bits every frame
	if (_lastArgb == argb)
		return;

	UnpackArgb(argb, _rgba);
	_lastArgb = argb;
}

void RaceColorPicker::DisableHairAlpha(RE::GFxValue& a_colorField)
{
	// Hair colour has no alpha in game, but RaceMenu still shows the slider for it
	RE::GFxValue noAlpha;
	if (a_colorField.GetMember("bNoAlpha", &noAlpha) && noAlpha.IsBool() && noAlpha.GetBool())
		return;

	RE::GFxValue entry;
	RE::GFxValue tintType;
	if (!a_colorField.GetMember("entry", &entry) || !entry.IsObject() ||
		!entry.GetMember("tintType", &tintType) || !tintType.IsNumber() || tintType.GetNumber() != TINT_TYPE_HAIR) {
		return;
	}

	RE::GFxValue disable(true);
	a_colorField.Invoke("disableAlpha", nullptr, &disable, 1);
	a_colorField.Invoke("updateHexCode", nullptr, nullptr, 0);
}

void RaceColorPicker::SendColor(RE::GFxMovieView* a_movie, RE::GFxValue& a_colorField, std::uint32_t a_argb)
{
	RE::GFxValue colorSelector;
	if (!a_colorField.GetMember("colorSelector", &colorSelector) || !colorSelector.IsObject())
		return;

	RE::GFxValue args[2] = { RE::GFxValue(static_cast<double>(a_argb & 0xFFFFFF)), RE::GFxValue(static_cast<double>(a_argb >> 24)) };

	// Same as a native slider change: move the sliders, then send the preview event
	colorSelector.Invoke("setColor", nullptr, args, 2);

	RE::GFxValue event;
	a_movie->CreateObject(&event);
	event.SetMember("color", args[0]);
	event.SetMember("alpha", args[1]);
	a_colorField.Invoke("onSliderChange", nullptr, &event, 1);

	_awaitingApply = true;
}

void RaceColorPicker::ApplyWhileBusy(RE::GFxValue& a_menuInstance)
{
	if (!_awaitingApply)
		return;

	RE::GFxValue pendingData;
	if (!a_menuInstance.GetMember("_pendingData", &pendingData) || !pendingData.IsObject()) {
		_awaitingApply = false;
		return;
	}

	double now = FUCK::GetTime();
	if (now - _lastApply < APPLY_INTERVAL)
		return;

	// RaceMenu's update timer stalls while our picker is in use, so we apply its pending colour here.
	// Mirrors processDataUpdate, which we can't call as it also clears the timer InitializeSliders shares
	auto member = [&](const char* a_name) {
		RE::GFxValue value;
		pendingData.GetMember(a_name, &value);
		return value;
	};

	RE::GFxValue     type     = member("type");
	std::string_view typeName = type.IsString() ? type.GetString() : "";

	if (typeName == "sliderColor") {
		RE::GFxValue args[2] = { member("slider"), member("argbColor") };
		a_menuInstance.Invoke("SendPlayerTintBySlider", nullptr, args, 2);
	} else if (typeName == "makeupColor") {
		RE::GFxValue args[3] = { member("tintType"), member("tintIndex"), member("argbColor") };
		a_menuInstance.Invoke("SendPlayerTint", nullptr, args, 3);
	} else if (typeName == "glowColor") {
		RE::GFxValue args[3] = { member("tintType"), member("tintIndex"), member("argbColor") };
		a_menuInstance.Invoke("SendPlayerGlow", nullptr, args, 3);
	} else {
		_awaitingApply = false;
		return;
	}

	RE::GFxValue cleared;
	cleared.SetNull();
	a_menuInstance.SetMember("_pendingData", cleared);

	_lastApply     = now;
	_awaitingApply = false;
}

void RaceColorPicker::OnSliderColorsSet(RE::GFxValue& a_menuInstance)
{
	// RaceMenu's colours here can be stale or from a stray mask. SetMakeup redraws the list after
	RE::GFxValue colorField;
	bool         fieldOpen = FindOpenField(a_menuInstance, colorField);
	double       now       = FUCK::GetTime();

	for (auto& layer : _tintLayers) {
		RE::GFxValue colorEntry;
		if (!GetColorEntry(a_menuInstance, layer.tintType, colorEntry))
			continue;

		// Untagged means a rebuilt list our state doesn't describe, so leave RaceMenu's colour
		if (!colorEntry.HasMember(layer.colorTag)) {
			layer.Forget();
			continue;
		}

		if (!fieldOpen) {
			FollowGame(layer, now);
		}
		if (layer.color) {
			colorEntry.SetMember("fillColor", ToNumber(*layer.color));
		}
	}
}

void RaceColorPicker::SyncTintLayer(RE::GFxValue& a_menuInstance, TintLayer& a_layer, bool a_fieldOpen)
{
	RE::GFxValue colorEntry;
	RE::GFxValue indexEntry;
	if (!GetColorEntry(a_menuInstance, a_layer.tintType, colorEntry) ||
		!FindSliderByID(a_menuInstance, a_layer.sliderID, indexEntry)) {
		return;
	}

	double now = FUCK::GetTime();

	auto colorSlider = TrackSlider(colorEntry, a_layer.colorTag, a_layer.lastColorPos);
	if (colorSlider == SliderState::kFresh) {
		a_layer.Forget();  // A rebuilt list belongs to a different face
	} else if (colorSlider == SliderState::kMoved) {
		a_layer.restoreUntil = 0.0;  // Preset colours are the game's to pick
	}

	if (TrackSlider(indexEntry, a_layer.indexTag, a_layer.lastIndex) == SliderState::kMoved && a_layer.color) {
		if (a_layer.lastIndex >= 0.0) {  // -1 is none
			SendLayerColor(a_menuInstance, a_layer, a_layer.lastIndex, *a_layer.color);
			a_layer.Restore(now + RESTORE_TIMEOUT);
		} else {
			a_layer.restoreUntil = 0.0;
		}
		a_layer.guardUntil = now + GUARD_WINDOW;
	}

	// A colour can land after the slider has moved past its texture, switching that one back on
	if (now < a_layer.guardUntil && now - a_layer.lastGuard >= GUARD_INTERVAL) {
		a_layer.lastGuard = now;
		ClearStrayMasks(a_menuInstance, a_layer);
	}

	// The field owns the colour while open, and RememberFieldColor takes it on close
	if (a_fieldOpen)
		return;

	FollowGame(a_layer, now);
	if (a_layer.color) {
		SetLayerColor(a_menuInstance, colorEntry, *a_layer.color);
	}
}

void RaceColorPicker::FollowGame(TintLayer& a_layer, double a_now)
{
	auto applied = GetActiveMask(a_layer.maskType, a_layer.lastIndex);

	if (a_layer.restoreUntil > 0.0) {
		bool landed = applied && a_layer.color && SameColor(*applied, *a_layer.color);
		if (!landed) {
			a_layer.matchedSince = 0.0;
		} else if (a_layer.matchedSince == 0.0) {
			a_layer.matchedSince = a_now;
		}

		// The game can apply the texture's default a few frames late, so ours has to hold a moment
		if ((!landed || a_now - a_layer.matchedSince < SETTLE_TIME) && a_now < a_layer.restoreUntil)
			return;
		a_layer.restoreUntil = 0.0;
	}

	// No texture keeps the colour for the next one picked
	if (applied) {
		a_layer.color = applied;
	}
}

void RaceColorPicker::SendLayerColor(RE::GFxValue& a_menuInstance, const TintLayer& a_layer, double a_index, std::uint32_t a_argb)
{
	RE::GFxValue args[3] = { RE::GFxValue(a_layer.tintType), RE::GFxValue(a_index), ToNumber(a_argb) };
	a_menuInstance.Invoke("SendPlayerTint", nullptr, args, 3);
}

void RaceColorPicker::ClearStrayMasks(RE::GFxValue& a_menuInstance, const TintLayer& a_layer)
{
	auto masks    = GetMasks(a_layer.maskType);
	auto selected = a_layer.lastIndex >= 0.0 ? std::optional(static_cast<std::size_t>(a_layer.lastIndex)) : std::nullopt;

	std::optional<std::size_t> firstStray;
	for (std::size_t i = 0; i < masks.size(); ++i) {
		if (i != selected && masks[i]->alpha > 0.0f) {
			masks[i]->alpha = 0.0f;
			firstStray      = firstStray ? firstStray : std::optional(i);
		}
	}
	if (!firstStray)
		return;

	// Cleared masks only show once the face is rebuilt, which sending a colour does. With nothing
	// to resend, a transparent one on a stray does it harmlessly
	if (selected && a_layer.color) {
		SendLayerColor(a_menuInstance, a_layer, a_layer.lastIndex, *a_layer.color);
	} else {
		SendLayerColor(a_menuInstance, a_layer, static_cast<double>(*firstStray), a_layer.color.value_or(0) & 0x00FFFFFF);
	}
}

void RaceColorPicker::SetLayerColor(RE::GFxValue& a_menuInstance, RE::GFxValue& a_colorEntry, std::uint32_t a_argb)
{
	if (EntryColor(a_colorEntry) == a_argb)
		return;

	// No tint count bookkeeping, as RSM_AddTints recounts from the game's masks
	a_colorEntry.SetMember("fillColor", ToNumber(a_argb));

	// Redrawn now, as requestUpdate would wait a frame
	RE::GFxValue itemList;
	if (a_menuInstance.GetMember("itemList", &itemList) && itemList.IsObject()) {
		itemList.Invoke("UpdateList", nullptr, nullptr, 0);
	}
}

void RaceColorPicker::RememberFieldColor(RE::GFxValue& a_menuInstance)
{
	// Colour sliders only, as warpaint makeup entries share the tint type
	RE::GFxValue colorField;
	RE::GFxValue entry;
	RE::GFxValue entryType;
	RE::GFxValue tintType;
	if (!a_menuInstance.GetMember("colorField", &colorField) || !colorField.IsObject() ||
		!colorField.GetMember("entry", &entry) || !entry.IsObject() ||
		!entry.GetMember("type", &entryType) || !entryType.IsNumber() || entryType.GetNumber() != ENTRY_TYPE_SLIDER ||
		!entry.GetMember("tintType", &tintType) || !tintType.IsNumber()) {
		return;
	}

	for (auto& layer : _tintLayers) {
		if (layer.tintType != tintType.GetNumber())
			continue;

		// The game shows the last preview until RaceMenu's update lands
		std::uint32_t argb = EntryColor(entry);
		layer.color        = (argb >> 24) ? std::optional(argb) : std::nullopt;
		layer.Restore(layer.color && layer.lastIndex >= 0.0 ? FUCK::GetTime() + RESTORE_TIMEOUT : 0.0);
	}
}

void RaceColorPicker::CommitRecent()
{
	std::optional<std::uint32_t> argb;
	{
		std::scoped_lock lock(_pendingLock);
		argb = std::exchange(_acceptedArgb, std::nullopt);
	}
	if (!argb)
		return;

	float rgba[4];
	UnpackArgb(*argb, rgba);

	auto settings = RaceSettings::GetSingleton();
	if (settings->color.recent.Push(rgba)) {
		settings->SaveColors();
	}
}

void RaceColorPicker::QueueColor(std::uint32_t a_argb)
{
	// The gradient reports a change every frame it's held, even when the colour hasn't moved
	if (_lastArgb == a_argb)
		return;

	_lastArgb = a_argb;

	std::scoped_lock lock(_pendingLock);
	_pendingArgb = a_argb;
}

void RaceColorPicker::DrawPalette(FUCK::ColorPalette& a_palette, const char* a_id, const char* a_title, bool a_noAlpha)
{
	FUCK::SeparatorText(a_title);

	auto swatchFlags = a_noAlpha ? FUCK::ColorEditFlags::kNoAlpha : FUCK::ColorEditFlags::kNone;
	auto result      = a_palette.Draw(a_id, _rgba, 0.0f, swatchFlags);

	if (result.picked) {
		if (a_noAlpha) {
			_rgba[3] = 1.0f;
		}
		QueueColor(PackArgb(_rgba));
	}
	if (result.changed) {
		RaceSettings::GetSingleton()->SaveColors();
	}
}

void RaceColorPicker::Draw(float a_width)
{
	RE::GFxValue colorField;
	RE::GFxValue colorSelector;
	if (!GetOpenField(colorField) || !colorField.GetMember("colorSelector", &colorSelector) || !colorSelector.IsObject())
		return;

	bool hasPending;
	{
		std::scoped_lock lock(_pendingLock);
		hasPending = _pendingArgb.has_value();
	}

	// Reading back before our last edit is sent would undo it
	if (!hasPending) {
		SyncFromField(colorSelector);
	}

	RE::GFxValue noAlphaValue;
	bool         noAlpha = colorField.GetMember("bNoAlpha", &noAlphaValue) && noAlphaValue.IsBool() && noAlphaValue.GetBool();

	int flags = ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf;
	if (noAlpha) {
		flags |= ImGuiColorEditFlags_NoAlpha;
	}

	if (FUCK::BeginTable("ColourWidthLocker", 1, FUCK::TableFlags::kSizingFixedFit, ImVec2(a_width, 0.0f))) {
		FUCK::TableSetupColumn("ColourCol", FUCK::TableColumnFlags::kWidthFixed, a_width);
		FUCK::TableNextRow();
		FUCK::TableNextColumn();

		FUCK::SeparatorText("$RACE_Color"_T);

		FUCK::SetNextItemWidth(-1.0f);
		if (FUCK::ColorEdit4("##RaceMenuColor", _rgba, flags)) {
			QueueColor(PackArgb(_rgba));
		}

		auto& colors = RaceSettings::GetSingleton()->color;

		DrawPalette(colors.swatches, "##Swatches", "$RACE_ColorSwatches"_T, noAlpha);

		if (!colors.recent.Empty()) {
			DrawPalette(colors.recent, "##Recent", "$RACE_ColorRecent"_T, noAlpha);
		}

		FUCK::EndTable();
	}
}
