#pragma once

#include "RACE-Anims.h"
#include "RACE-Camera.h"
#include "RACE-Equip.h"
#include "RACE-Inputs.h"
#include "RACE-Reference.h"
#include "RACE-Widget.h"

class RaceWidgetWindow : public FUCK::IWindow, public REX::Singleton<RaceWidgetWindow>
{
public:
	const char* Id() const override { return "RACE_Widget"; }
	const char* Title() const override { return "$RACE_Title"_T; }

	bool OnAsyncInput(const void* a_event) override
	{
		if (!a_event)
			return false;
		auto        inputEvents    = static_cast<RE::InputEvent* const*>(a_event);
		static bool s_eatingEscape = false;

		for (auto event = *inputEvents; event; event = event->next) {
			auto btn = event->AsButtonEvent();

			if (btn && btn->GetDevice() == RE::INPUT_DEVICE::kKeyboard && btn->GetIDCode() == static_cast<uint32_t>(RE::BSWin32KeyboardDevice::Key::kEscape)) {
				if (btn->IsDown()) {
					auto ui        = RE::UI::GetSingleton();
					bool isRSM     = ui && ui->IsMenuOpen(RE::RaceSexMenu::MENU_NAME);
					bool isJournal = ui && ui->IsMenuOpen(RE::JournalMenu::MENU_NAME);

					if (!s_eatingEscape && isRSM && !isJournal) {
						RE::UIMessageQueue::GetSingleton()->AddMessage(RE::JournalMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kShow, nullptr);
						s_eatingEscape = true;
						return true;
					}
					if (s_eatingEscape)
						return true;
				} else {
					s_eatingEscape = false;
				}
			}
		}
		return false;
	}

	void Draw() override
	{
		float mouseWheel = FUCK::GetMouseWheel();
		if (mouseWheel != 0.0f) {
			RaceCamera::GetSingleton()->AccumulateScroll(mouseWheel);
		}

		RaceWidget::GetSingleton()->Draw();

		_lastPos  = FUCK::GetWindowPos();
		_lastSize = FUCK::GetWindowSize();
	}

	bool IsOpen() const override
	{
		return RaceWidget::GetSingleton()->IsOpen();
	}

	void SetOpen(bool /*a_open*/) override {}

	FUCK::WindowFlags GetFlags() const override
	{
		FUCK::WindowFlags flags =
			FUCK::WindowFlags::kNoDecoration    |
			FUCK::WindowFlags::kNoBackground    |
			FUCK::WindowFlags::kIgnoreUserScale |
			FUCK::WindowFlags::kAutoResize      |
			FUCK::WindowFlags::kNoResize        |
			FUCK::WindowFlags::kCustomPosition  ;

		if (!FUCK::IsMenuOpen()) {
			flags = flags | FUCK::WindowFlags::kNoMove;

			ImVec2 mouse   = FUCK::GetMousePos();
			bool   hovered = mouse.x >= _lastPos.x && mouse.x <= _lastPos.x + _lastSize.x &&
			               mouse.y >= _lastPos.y && mouse.y <= _lastPos.y + _lastSize.y;

			bool isPopupOpen = FUCK::IsPopupOpen(nullptr, FUCK::PopupFlags::kAnyPopup);
			bool isHidden    = RaceWidget::GetSingleton()->IsUIHidden();

			bool ctrlDown = FUCK::IsModifierPressed(FUCK::Modifier::kCtrl);
			bool rbDown   = FUCK::IsInputDown(RACE::Keys::kGP_RB);
			bool mmbDown  = FUCK::IsInputDown(RACE::Keys::kMouse_Middle);

			if (!hovered && !FUCK::IsAnyItemActive() && !isPopupOpen && !ctrlDown && !rbDown && !mmbDown && !isHidden) {
				flags = flags | FUCK::WindowFlags::kPassInputToGame;
			}
		}

		return flags;
	}

	ImVec2 GetDefaultPos() const override
	{
		ImVec2 displaySize = FUCK::GetDisplaySize();
		float  startX      = displaySize.x - FUCK::Scale(334.0f);
		return { std::max(0.0f, startX), FUCK::Scale(100.0f) };
	}

private:
	ImVec2 _lastPos{};
	ImVec2 _lastSize{};
};

class RaceEquipWindow : public FUCK::IWindow, public REX::Singleton<RaceEquipWindow>
{
public:
	const char* Id() const override { return "RACE_Equip"; }
	const char* Title() const override { return "$RACE_EquipTitle"_T; }

	void Draw() override
	{
		ImVec2 spawnPos;
		if (RaceEquipManager::GetSingleton()->ConsumeSpawnRequest(spawnPos)) {
			FUCK::SetWindowPos(spawnPos, ImGuiCond_Always);
		}

		RaceEquipManager::GetSingleton()->DrawWindow();
		_lastPos  = FUCK::GetWindowPos();
		_lastSize = FUCK::GetWindowSize();
	}

	bool IsOpen() const override
	{
		return RaceEquipManager::GetSingleton()->IsWindowOpen();
	}

	void SetOpen(bool a_open) override
	{
		RaceEquipManager::GetSingleton()->SetWindowOpen(a_open);
	}

	FUCK::WindowFlags GetFlags() const override
	{
		FUCK::WindowFlags flags =
			FUCK::WindowFlags::kNoDecoration  |
			FUCK::WindowFlags::kAutoResize    |
			FUCK::WindowFlags::kNoResize      |
			FUCK::WindowFlags::kCustomPosition;

		if (!FUCK::IsMenuOpen()) {
			flags = flags | FUCK::WindowFlags::kNoMove;

			ImVec2 mouse   = FUCK::GetMousePos();
			bool   hovered = mouse.x >= _lastPos.x && mouse.x <= _lastPos.x + _lastSize.x &&
			               mouse.y >= _lastPos.y && mouse.y <= _lastPos.y + _lastSize.y;

			bool isPopupOpen = FUCK::IsPopupOpen(nullptr, FUCK::PopupFlags::kAnyPopup);
			bool isHidden    = RaceWidget::GetSingleton()->IsUIHidden();

			if (!hovered && !FUCK::IsAnyItemActive() && !isPopupOpen && !isHidden) {
				flags = flags | FUCK::WindowFlags::kPassInputToGame;
			}
		}

		return flags;
	}

	ImVec2 GetDefaultSize() const override
	{
		return FUCK::Scale(250.0f, 350.0f);
	}

private:
	ImVec2 _lastPos{};
	ImVec2 _lastSize{};
};

namespace FUCK_Register
{
	inline void Install()
	{
		RaceAnimManager::GetSingleton()->Initialize();
		RaceEquipManager::GetSingleton()->Initialize();
		RaceWidget::GetSingleton()->Initialize();

		FUCK::RegisterWindow(RaceWidgetWindow::GetSingleton());
		FUCK::RegisterWindow(RaceEquipWindow::GetSingleton());
	}
}
