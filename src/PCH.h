#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define IMGUI_DEFINE_MATH_OPERATORS

#include "RE/Skyrim.h"
#include "REX/REX/Singleton.h"
#include "SKSE/SKSE.h"

#include <ClibUtil/simpleINI.hpp>
#include <ClibUtil/string.hpp>

#include <spdlog/sinks/basic_file_sink.h>

using namespace std::literals;

namespace fs     = std::filesystem;
namespace logger = SKSE::log;

namespace stl
{
	using namespace SKSE::stl;

	template <class T>
	void write_vfunc(REL::VariantID a_vtable)
	{
		REL::Relocation<std::uintptr_t> vtbl{ a_vtable };
		T::func = vtbl.write_vfunc(T::idx, T::thunk);
	}
}

#define FUCK_API_ENABLE_SIMPLEINI
#include "API/FUCK_API.h"
#include "Version.h"
