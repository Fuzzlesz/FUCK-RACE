#include "RACE-ColorPicker.h"
#include "RACE-Settings.h"
#include "RACE-Widget.h"

namespace
{
	constexpr double TINT_TYPE_HAIR = 128.0;  // RaceMenuDefines.TINT_TYPE_HAIR

	constexpr auto APPLY_INTERVAL = std::chrono::milliseconds(100);  // Same pace as RaceMenu's requestUpdate

	bool IsGamepad()
	{
		return FUCK::GetInputDevice() == FUCK::InputDevice::kGamepad;
	}

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

	// RaceMenu's single save/load slot, replaced by our swatches outside gamepad
	class PresetHandler : public WrappedHandler
	{
	public:
		using WrappedHandler::WrappedHandler;

		void Call(Params& a_params) override
		{
			if (IsGamepad()) {
				CallOriginal(a_params);
			}
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
	if (FindOpenField(a_menuInstance, colorField)) {
		DisableHairAlpha(colorField);

		RE::GFxValue presetPanel;
		if (colorField.GetMember("presetPanel", &presetPanel) && presetPanel.IsObject()) {
			presetPanel.SetMember("_visible", RE::GFxValue(IsGamepad()));
		}
		if (argb) {
			SendColor(a_movie, colorField, *argb);
		}
	}

	ApplyWhileBusy(a_menuInstance);
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

	auto now = std::chrono::steady_clock::now();
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
		settings->Save();
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

	auto swatchFlags = a_noAlpha ? FUCK::ColorEditFlags::kNoAlpha : FUCK::ColorEditFlags::kAlphaPreviewHalf;
	auto result      = a_palette.Draw(a_id, _rgba, 0.0f, swatchFlags);

	if (result.picked) {
		if (a_noAlpha) {
			_rgba[3] = 1.0f;
		}
		QueueColor(PackArgb(_rgba));
	}
	if (result.changed) {
		RaceSettings::GetSingleton()->Save();
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
		if (!colors.recent.Empty()) {
			DrawPalette(colors.recent, "##Recent", "$RACE_ColorRecent"_T, noAlpha);
		}
		DrawPalette(colors.swatches, "##Swatches", "$RACE_ColorSwatches"_T, noAlpha);

		FUCK::EndTable();
	}
}
