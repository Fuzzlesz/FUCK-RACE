#include "RACE-Anims.h"

void RaceAnimManager::Initialize()
{
	std::sort(_allIdles.begin(), _allIdles.end(), [](const auto& a, const auto& b) {
		return _stricmp(a.first.c_str(), b.first.c_str()) < 0;
	});

	_pluginNames.clear();
	_pluginNames.push_back(std::string("$RACE_AllPlugins"_T) + "##NOFILTER");
	_pluginNamesCStr.clear();
	_pluginNamesCStr.push_back(_pluginNames[0].c_str());
}

void RaceAnimManager::AddIdle(const std::string& a_name, RE::TESIdleForm* a_idle)
{
	auto ptrIt = std::find_if(_allIdles.begin(), _allIdles.end(), [&](const auto& pair) {
		return pair.second == a_idle;
	});

	if (ptrIt != _allIdles.end()) {
		ptrIt->first = a_name;
		return;
	}

	auto nameIt = std::find_if(_allIdles.begin(), _allIdles.end(), [&](const auto& pair) {
		return _stricmp(pair.first.c_str(), a_name.c_str()) == 0;
	});

	if (nameIt != _allIdles.end()) {
		nameIt->second = a_idle;
	} else {
		_allIdles.emplace_back(a_name, a_idle);
	}
}

void RaceAnimManager::UpdateValidIdles()
{
	auto player = RE::PlayerCharacter::GetSingleton();
	if (!player || !player->Is3DLoaded())
		return;

	std::sort(_allIdles.begin(), _allIdles.end(), [](const auto& a, const auto& b) {
		return _stricmp(a.first.c_str(), b.first.c_str()) < 0;
	});

	std::string targetPlugin = _selectedPluginIndex > 0 && _selectedPluginIndex < static_cast<int>(_pluginNames.size()) ? _pluginNames[_selectedPluginIndex] : "";

	std::set<std::string> activePlugins;
	for (const auto& pair : _allIdles) {
		if (player->CanUseIdle(pair.second) && pair.second->CheckConditions(player, nullptr, false)) {
			if (auto file = pair.second->GetFile(0)) {
				activePlugins.insert(std::string(file->GetFilename()));
			} else {
				activePlugins.insert("Unknown");
			}
		}
	}

	_pluginNames.clear();
	_pluginNames.push_back("$RACE_AllPlugins"_T);

	if (auto dataHandler = RE::TESDataHandler::GetSingleton()) {
		for (auto* file : dataHandler->files) {
			if (file) {
				std::string fileName(file->GetFilename());
				if (activePlugins.contains(fileName)) {
					_pluginNames.push_back(fileName);
					activePlugins.erase(fileName);
				}
			}
		}
	}

	for (const auto& p : activePlugins) {
		_pluginNames.push_back(p);
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

	_validIdles.clear();
	_idleNames.clear();

	_validIdles.reserve(_allIdles.size() + 1);
	_idleNames.reserve(_allIdles.size() + 1);

	_validIdles.push_back({ std::string("$RACE_SelectIdle"_T) + "##NOFILTER", nullptr });
	_idleNames.push_back(_validIdles.back().first.c_str());

	targetPlugin = _selectedPluginIndex > 0 ? _pluginNames[_selectedPluginIndex] : "";

	for (const auto& pair : _allIdles) {
		if (player->CanUseIdle(pair.second) && pair.second->CheckConditions(player, nullptr, false)) {
			if (!targetPlugin.empty()) {
				auto        file  = pair.second->GetFile(0);
				std::string pName = file ? std::string(file->GetFilename()) : "Unknown";
				if (pName != targetPlugin) {
					continue;
				}
			}

			_validIdles.push_back(pair);
			_idleNames.push_back(_validIdles.back().first.c_str());
		}
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
