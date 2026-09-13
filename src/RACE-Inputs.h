#pragma once

namespace RACE::Keys
{
	// Base Offsets
	inline constexpr uint32_t kGPBase = static_cast<uint32_t>(SKSE::InputMap::kMacro_GamepadOffset);      // 266
	inline constexpr uint32_t kMBBase = static_cast<uint32_t>(SKSE::InputMap::kMacro_MouseButtonOffset);  // 256

	// Gamepad
	inline constexpr uint32_t kGP_LB = kGPBase + SKSE::InputMap::kGamepadButtonOffset_LEFT_SHOULDER;
	inline constexpr uint32_t kGP_RB = kGPBase + SKSE::InputMap::kGamepadButtonOffset_RIGHT_SHOULDER;
	inline constexpr uint32_t kGP_LT = kGPBase + SKSE::InputMap::kGamepadButtonOffset_LT;
	inline constexpr uint32_t kGP_RT = kGPBase + SKSE::InputMap::kGamepadButtonOffset_RT;

	inline constexpr uint32_t kGP_Up    = kGPBase + SKSE::InputMap::kGamepadButtonOffset_DPAD_UP;
	inline constexpr uint32_t kGP_Down  = kGPBase + SKSE::InputMap::kGamepadButtonOffset_DPAD_DOWN;
	inline constexpr uint32_t kGP_Left  = kGPBase + SKSE::InputMap::kGamepadButtonOffset_DPAD_LEFT;
	inline constexpr uint32_t kGP_Right = kGPBase + SKSE::InputMap::kGamepadButtonOffset_DPAD_RIGHT;

	inline constexpr uint32_t kGP_A = kGPBase + SKSE::InputMap::kGamepadButtonOffset_A;
	inline constexpr uint32_t kGP_B = kGPBase + SKSE::InputMap::kGamepadButtonOffset_B;
	inline constexpr uint32_t kGP_X = kGPBase + SKSE::InputMap::kGamepadButtonOffset_X;
	inline constexpr uint32_t kGP_Y = kGPBase + SKSE::InputMap::kGamepadButtonOffset_Y;

	inline constexpr uint32_t kGP_L3   = kGPBase + SKSE::InputMap::kGamepadButtonOffset_LEFT_THUMB;
	inline constexpr uint32_t kGP_R3   = kGPBase + SKSE::InputMap::kGamepadButtonOffset_RIGHT_THUMB;
	inline constexpr uint32_t kGP_Back = kGPBase + SKSE::InputMap::kGamepadButtonOffset_BACK;

	// Thumbsticks
	// Mapped to the custom offsets defined in FUCK Input.cpp
	inline constexpr uint32_t kGP_LSX = kGPBase + 32;
	inline constexpr uint32_t kGP_LSY = kGPBase + 33;
	inline constexpr uint32_t kGP_RSX = kGPBase + 34;
	inline constexpr uint32_t kGP_RSY = kGPBase + 35;

	// Keyboard
	inline constexpr uint32_t kKB_Q   = static_cast<uint32_t>(RE::BSWin32KeyboardDevice::Key::kQ);
	inline constexpr uint32_t kKB_W   = static_cast<uint32_t>(RE::BSWin32KeyboardDevice::Key::kW);
	inline constexpr uint32_t kKB_E   = static_cast<uint32_t>(RE::BSWin32KeyboardDevice::Key::kE);
	inline constexpr uint32_t kKB_R   = static_cast<uint32_t>(RE::BSWin32KeyboardDevice::Key::kR);
	inline constexpr uint32_t kKB_A   = static_cast<uint32_t>(RE::BSWin32KeyboardDevice::Key::kA);
	inline constexpr uint32_t kKB_S   = static_cast<uint32_t>(RE::BSWin32KeyboardDevice::Key::kS);
	inline constexpr uint32_t kKB_D   = static_cast<uint32_t>(RE::BSWin32KeyboardDevice::Key::kD);
	inline constexpr uint32_t kKB_F   = static_cast<uint32_t>(RE::BSWin32KeyboardDevice::Key::kF);
	inline constexpr uint32_t kKB_F11 = static_cast<uint32_t>(RE::BSWin32KeyboardDevice::Key::kF11);

	// Mouse
	inline constexpr uint32_t kMouse_Right     = kMBBase + 1;
	inline constexpr uint32_t kMouse_Middle    = kMBBase + 2;
	inline constexpr uint32_t kMouse_WheelUp   = kMBBase + 8;
	inline constexpr uint32_t kMouse_WheelDown = kMBBase + 9;
}
