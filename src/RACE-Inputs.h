#pragma once

namespace RACE::Keys
{
	// Gamepad and mouse codes come from FUCK's API, so they always match what FUCK reports
	using namespace FUCK::Keys;

	// Keyboard keys are their scan codes, the same in FUCK and SKSE
	inline constexpr uint32_t kKB_W   = static_cast<uint32_t>(RE::BSWin32KeyboardDevice::Key::kW);
	inline constexpr uint32_t kKB_A   = static_cast<uint32_t>(RE::BSWin32KeyboardDevice::Key::kA);
	inline constexpr uint32_t kKB_S   = static_cast<uint32_t>(RE::BSWin32KeyboardDevice::Key::kS);
	inline constexpr uint32_t kKB_D   = static_cast<uint32_t>(RE::BSWin32KeyboardDevice::Key::kD);
	inline constexpr uint32_t kKB_E   = static_cast<uint32_t>(RE::BSWin32KeyboardDevice::Key::kE);
	inline constexpr uint32_t kKB_Tab = static_cast<uint32_t>(RE::BSWin32KeyboardDevice::Key::kTab);
	inline constexpr uint32_t kKB_F11 = static_cast<uint32_t>(RE::BSWin32KeyboardDevice::Key::kF11);
}
