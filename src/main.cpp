#include "FUCK_Register.h"

#include "RACE-Compat.h"
#include "RACE-Hooks.h"

void InitializeLog()
{
	auto path = SKSE::log::log_directory();
	if (!path)
		SKSE::stl::report_and_fail("Failed to find standard logging directory"sv);

	*path /= fmt::format(FMT_STRING("{}.log"), Version::PROJECT);
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
	auto log  = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

	log->set_level(spdlog::level::info);
	log->flush_on(spdlog::level::info);

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("%v"s);

	SKSE::log::info(FMT_STRING("{} v{}"), Version::PROJECT, Version::NAME);
}

#ifdef SKYRIM_AE
extern "C" DLLEXPORT constinit auto SKSEPlugin_Version = []() {
	SKSE::PluginVersionData v;
	v.PluginVersion(Version::MAJOR);
	v.PluginName("FUCK-RACE");
	v.AuthorName("Fuzzles");
	v.UsesAddressLibrary();
	v.UsesUpdatedStructs();
	v.CompatibleVersions({ SKSE::RUNTIME_SSE_LATEST });
	return v;
}();
#else
extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
	a_info->infoVersion = SKSE::PluginInfo::kVersion;
	a_info->name        = "FUCK-RACE";
	a_info->version     = Version::MAJOR;

	if (a_skse->IsEditor()) {
		logger::critical("Loaded in editor, marking as incompatible"sv);
		return false;
	}
	return true;
}
#endif

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	InitializeLog();
	SKSE::Init(a_skse, false);

	Hooks::Install();

	const auto messaging = SKSE::GetMessagingInterface();
	messaging->RegisterListener([](SKSE::MessagingInterface::Message* msg) {
		if (msg->type == SKSE::MessagingInterface::kDataLoaded) {
			if (FUCK::Connect("FUCK-RACE")) {
				bool skee64Present = SKEE64Compat::Detect();
				RaceWidget::GetSingleton()->SetSkee64Present(skee64Present);

				FUCK_Register::Install();

			} else {
				SKSE::log::error("FUCK.dll not found or version mismatch. Tool disabled.");
			}
		}
	});
	return true;
}
