#include "RACE-Anims.h"
#include "RACE-Camera.h"
#include "RACE-Hooks.h"
#include "RACE-Widget.h"

namespace Hooks
{
	struct SetFormEditorID
	{
		static bool thunk(RE::TESIdleForm* a_this, const char* a_str)
		{
			if (!clib_util::string::is_empty(a_str)) {
				if (const std::string_view str(a_str); !str.starts_with("pa_")) {
					RaceAnimManager::GetSingleton()->AddIdle(a_str, a_this);
				}
			}
			return func(a_this, a_str);
		}

		static inline REL::Relocation<decltype(thunk)> func;
		static inline constexpr std::size_t            idx{ 0x33 };
	};

	struct RaceSexMenu_AdvanceMovie
	{
		static void thunk(RE::RaceSexMenu* a_this, float a_interval, std::uint32_t a_currentTime)
		{
			auto camera = RE::PlayerCamera::GetSingleton();
			auto root   = camera ? camera->cameraRoot.get() : nullptr;

			RaceCamera::GetSingleton()->RevertCameraTransform(root);

			func(a_this, a_interval, a_currentTime);

			RaceWidget::GetSingleton()->OnAdvanceMovie(a_this);

			RaceCamera::GetSingleton()->HandleInput(a_interval);
			RaceCamera::GetSingleton()->ApplyTransform(root);
		}

		static inline REL::Relocation<decltype(thunk)> func;
		static inline constexpr std::size_t            idx{ 0x05 };
	};

	void Install()
	{
		stl::write_vfunc<SetFormEditorID>(RE::TESIdleForm::VTABLE[0]);
		stl::write_vfunc<RaceSexMenu_AdvanceMovie>(RE::VTABLE_RaceSexMenu[0]);
	}
}
