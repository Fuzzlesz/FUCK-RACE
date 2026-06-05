#pragma once

namespace SKEE64Compat
{
	inline bool g_present = false;

	inline bool Detect()
	{
		g_present = (GetModuleHandleW(L"skee64.dll") != nullptr);
		return g_present;
	}

	inline bool IsPresent() { return g_present; }

	inline void OnModeChanged(int a_currentMode, int a_previousMode, RE::GFxValue& menuInstance)
	{
		bool isGamepad = (FUCK::GetInputDevice() == FUCK::InputDevice::kGamepad);

		if (a_currentMode == 0) {
			RE::GFxValue undefinedVal;
			undefinedVal.SetUndefined();

			menuInstance.SetMember("_zoomControl", undefinedVal);

			if (!isGamepad) {
				menuInstance.SetMember("_lightControl", undefinedVal);
			}

		} else if (a_previousMode == 0) {
			RE::GFxValue platformVal, ps3Val;
			menuInstance.GetMember("_platform", &platformVal);
			menuInstance.GetMember("_bPS3Switch", &ps3Val);

			RE::GFxValue args[2] = { platformVal, ps3Val };
			menuInstance.Invoke("SetPlatform", nullptr, args, 2);
		}
	}
}
