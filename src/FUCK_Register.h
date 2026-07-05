#pragma once

#include "RACE-Equip.h"
#include "RACE-Inputs.h"
#include "RACE-Reference.h"
#include "RACE-Widget.h"

class RaceWidgetWindow : public FUCK::IWindow, public REX::Singleton<RaceWidgetWindow>
{
public:
	const char* Id() const override { return "RACE_Widget"; }
	const char* Title() const override { return "$RACE_Title"_T; }

	void Draw() override
	{
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

			if (!hovered && !FUCK::IsAnyItemActive() && !isPopupOpen && !ctrlDown && !rbDown && !isHidden) {
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

class RaceReferenceWindow : public FUCK::IWindow, public REX::Singleton<RaceReferenceWindow>
{
public:
	const char* Id() const override { return "RACE_Reference"; }
	const char* Title() const override { return "$RACE_ReferenceTitle"_T; }

	void Draw() override
	{
		RaceReferenceManager::GetSingleton()->DrawWindow();
	}

	bool IsOpen() const override
	{
		return RaceReferenceManager::GetSingleton()->IsWindowOpen();
	}

	void SetOpen(bool a_open) override
	{
		RaceReferenceManager::GetSingleton()->SetWindowOpen(a_open);
	}

	ImVec2 GetDefaultSize() const override
	{
		return FUCK::Scale(350.0f, 500.0f);
	}
};

namespace FUCK_Register
{
	inline void Install()
	{
		RaceEquipManager::GetSingleton()->Initialize();
		RaceWidget::GetSingleton()->Initialize();

		FUCK::RegisterWindow(RaceWidgetWindow::GetSingleton());
		FUCK::RegisterWindow(RaceEquipWindow::GetSingleton());
		FUCK::RegisterWindow(RaceReferenceWindow::GetSingleton());
	}
}
