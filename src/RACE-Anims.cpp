#include "RACE-Anims.h"

void RaceAnimManager::Initialize()
{
	std::sort(_allIdles.begin(), _allIdles.end(), [](const auto& a, const auto& b) {
		return _stricmp(a.first.c_str(), b.first.c_str()) < 0;
	});

	// Rebuild indices after sort
	_idleIndexByForm.clear();
	_idleIndexByName.clear();
	for (std::size_t i = 0; i < _allIdles.size(); ++i) {
		_idleIndexByForm[_allIdles[i].second]                            = i;
		_idleIndexByName[clib_util::string::tolower(_allIdles[i].first)] = i;
	}

	_pluginNames.clear();
	_pluginNames.push_back(std::string("$RACE_AllPlugins"_T) + "##NOFILTER");
	_pluginNamesCStr.clear();
	_pluginNamesCStr.push_back(_pluginNames[0].c_str());
}

void RaceAnimManager::AddIdle(const std::string& a_name, RE::TESIdleForm* a_idle)
{
	auto nameKey = clib_util::string::tolower(a_name);

	// Same form, new EditorID
	if (const auto it = _idleIndexByForm.find(a_idle); it != _idleIndexByForm.end()) {
		auto& entry = _allIdles[it->second];
		_idleIndexByName.erase(clib_util::string::tolower(entry.first));
		entry.first               = a_name;
		_idleIndexByName[nameKey] = it->second;
		return;
	}

	// Same EditorID, new form
	if (const auto it = _idleIndexByName.find(nameKey); it != _idleIndexByName.end()) {
		auto& entry = _allIdles[it->second];
		_idleIndexByForm.erase(entry.second);
		entry.second             = a_idle;
		_idleIndexByForm[a_idle] = it->second;
		return;
	}

	_idleIndexByForm[a_idle]  = _allIdles.size();
	_idleIndexByName[nameKey] = _allIdles.size();
	_allIdles.emplace_back(a_name, a_idle);
}

void RaceAnimManager::UpdateValidIdles()
{
	auto player = RE::PlayerCharacter::GetSingleton();
	if (!player || !player->Is3DLoaded())
		return;

	std::string targetPlugin = _selectedPluginIndex > 0 && _selectedPluginIndex < static_cast<int>(_pluginNames.size()) ? _pluginNames[_selectedPluginIndex] : "";

	struct UsableIdle
	{
		const std::pair<std::string, RE::TESIdleForm*>* idle;
		std::string_view                                plugin;
	};

	std::vector<UsableIdle>    usable;
	std::set<std::string_view> activePlugins;
	usable.reserve(_allIdles.size());

	for (const auto& pair : _allIdles) {
		if (!player->CanUseIdle(pair.second) || !pair.second->CheckConditions(player, nullptr, false))
			continue;

		const auto       file   = pair.second->GetFile(0);
		std::string_view plugin = file ? file->GetFilename() : "Unknown"sv;

		usable.push_back({ &pair, plugin });
		activePlugins.insert(plugin);
	}

	_pluginNames.clear();
	_pluginNames.push_back(std::string("$RACE_AllPlugins"_T) + "##NOFILTER");

	if (auto dataHandler = RE::TESDataHandler::GetSingleton()) {
		for (auto* file : dataHandler->files) {
			if (file && activePlugins.erase(file->GetFilename())) {
				_pluginNames.emplace_back(file->GetFilename());
			}
		}
	}

	for (const auto& p : activePlugins) {
		_pluginNames.emplace_back(p);
	}

	_pluginNamesCStr.clear();
	for (const auto& p : _pluginNames) {
		_pluginNamesCStr.push_back(p.c_str());
	}

	_selectedPluginIndex = 0;
	if (!targetPlugin.empty()) {
		for (size_t i = 1; i < _pluginNames.size(); ++i) {
			if (_pluginNames[i] == targetPlugin) {
				_selectedPluginIndex = static_cast<int>(i);
				break;
			}
		}
	}

	targetPlugin = _selectedPluginIndex > 0 ? _pluginNames[_selectedPluginIndex] : "";

	_validIdles.clear();
	_validIdles.reserve(usable.size() + 1);
	_validIdles.push_back({ std::string("$RACE_SelectIdle"_T) + "##NOFILTER", nullptr });

	for (const auto& u : usable) {
		if (targetPlugin.empty() || u.plugin == targetPlugin) {
			_validIdles.push_back(*u.idle);
		}
	}

	// Build after _validIdles is final so c_str() pointers stay valid
	_idleNames.clear();
	_idleNames.reserve(_validIdles.size());
	for (const auto& v : _validIdles) {
		_idleNames.push_back(v.first.c_str());
	}

	_selectedIndex = 0;
	_idlesValid    = true;
}

void RaceAnimManager::SetPlayerFrozen(bool a_frozen)
{
	auto player = RE::PlayerCharacter::GetSingleton();
	if (!player)
		return;

	if (a_frozen) {
		if (const auto currentProcess = player->GetActorRuntimeData().currentProcess) {
			currentProcess->ClearMuzzleFlashes();
		}

		player->GetActorRuntimeData().boolFlags.reset(RE::Actor::BOOL_FLAGS::kShouldAnimGraphUpdate);

		if (const auto charController = player->GetCharController()) {
			charController->flags.set(RE::CHARACTER_FLAGS::kNotPushable);
			charController->flags.reset(RE::CHARACTER_FLAGS::kRecordHits);
			charController->flags.reset(RE::CHARACTER_FLAGS::kHitFlags);
		}

		player->EnableAI(false);
		player->StopMoving(1.0f);

		if (const auto animData = player->GetFaceGenAnimationData()) {
			animData->eyesHeadingOffset = 0.0f;
			animData->eyesPitchOffset   = 0.0f;
			animData->eyesOffsetTimer   = FLT_MAX;
			animData->eyesBlinkingTimer = FLT_MAX;
			animData->eyesBlinkingStage = RE::BSFaceGenAnimationData::EyesBlinkingStage::BlinkDelay;
		}
	} else {
		player->GetActorRuntimeData().boolFlags.set(RE::Actor::BOOL_FLAGS::kShouldAnimGraphUpdate);

		if (const auto charController = player->GetCharController()) {
			charController->flags.reset(RE::CHARACTER_FLAGS::kNotPushable);
			charController->flags.set(RE::CHARACTER_FLAGS::kRecordHits);
			charController->flags.set(RE::CHARACTER_FLAGS::kHitFlags);
		}

		player->EnableAI(true);

		if (const auto animData = player->GetFaceGenAnimationData()) {
			animData->eyesOffsetTimer   = 0.0f;
			animData->eyesBlinkingTimer = 0.0f;
		}
	}

	_isFrozen = a_frozen;
}

void RaceAnimManager::OnAdvanceMovie()
{
	if (_isFrozen) {
		auto player = RE::PlayerCharacter::GetSingleton();
		if (player) {
			if (const auto animData = player->GetFaceGenAnimationData()) {
				animData->eyesHeadingOffset                                                            = 0.0f;
				animData->eyesPitchOffset                                                              = 0.0f;
				animData->eyesOffsetTimer                                                              = FLT_MAX;
				animData->eyesBlinkingTimer                                                            = FLT_MAX;
				animData->modifierKeyFrame.values[RE::BSFaceGenKeyframeMultiple::Modifier::BlinkLeft]  = 0.0f;
				animData->modifierKeyFrame.values[RE::BSFaceGenKeyframeMultiple::Modifier::BlinkRight] = 0.0f;
			}
		}
	}
}

void RaceAnimManager::PlaySelectedIdle()
{
	if (_selectedIndex > 0 && _selectedIndex < static_cast<int>(_validIdles.size())) {
		if (_isFrozen)
			SetPlayerFrozen(false);
		auto player = RE::PlayerCharacter::GetSingleton();
		if (auto process = player ? player->GetActorRuntimeData().currentProcess : nullptr)
			process->PlayIdle(player, _validIdles[_selectedIndex].second, nullptr);
	}
}

void RaceAnimManager::TogglePlay()
{
	if (_isFrozen) {
		SetPlayerFrozen(false);
	} else if (_selectedIndex > 0 && _selectedIndex < static_cast<int>(_validIdles.size())) {
		auto player = RE::PlayerCharacter::GetSingleton();
		if (auto process = player ? player->GetActorRuntimeData().currentProcess : nullptr)
			process->PlayIdle(player, _validIdles[_selectedIndex].second, nullptr);
	}
}

void RaceAnimManager::StopCurrentIdle()
{
	if (_isFrozen)
		SetPlayerFrozen(false);
	auto player = RE::PlayerCharacter::GetSingleton();
	if (auto process = player ? player->GetActorRuntimeData().currentProcess : nullptr) {
		process->StopCurrentIdle(player, true);
		auto resetRoot = RE::TESForm::LookupByEditorID<RE::TESIdleForm>("ResetRoot");
		if (resetRoot)
			process->PlayIdle(player, resetRoot, nullptr);
		_selectedIndex = 0;
	}
}
