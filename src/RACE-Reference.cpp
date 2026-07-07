#include "RACE-Reference.h"

void RaceReferenceManager::ScanReferences()
{
	_refImageNames.clear();
	_refImagePaths.clear();
	_refImageNamesCStr.clear();

	_refImageNames.push_back("$RACE_RefNone"_T);
	_refImagePaths.push_back("");

	std::string     refDir = FUCK::PluginSettings().GetConfigDirectory() + "\\references";
	std::error_code ec;

	if (!fs::exists(refDir, ec)) {
		fs::create_directories(refDir, ec);
	}

	if (fs::exists(refDir, ec) && fs::is_directory(refDir, ec)) {
		for (const auto& entry : fs::directory_iterator(refDir, ec)) {
			if (entry.is_regular_file(ec)) {
				auto ext = entry.path().extension().string();
				std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
				if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp") {
					_refImageNames.push_back(entry.path().filename().string());
					_refImagePaths.push_back(entry.path().string());
				}
			}
		}
	}

	if (_refImageNames.size() > 1) {
		std::vector<size_t> indices;
		for (size_t i = 1; i < _refImageNames.size(); ++i) indices.push_back(i);

		std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
			return _stricmp(_refImageNames[a].c_str(), _refImageNames[b].c_str()) < 0;
		});

		std::vector<std::string> sortedNames = { _refImageNames[0] };
		std::vector<std::string> sortedPaths = { _refImagePaths[0] };
		for (size_t idx : indices) {
			sortedNames.push_back(_refImageNames[idx]);
			sortedPaths.push_back(_refImagePaths[idx]);
		}
		_refImageNames = std::move(sortedNames);
		_refImagePaths = std::move(sortedPaths);
	}

	for (const auto& name : _refImageNames) {
		_refImageNamesCStr.push_back(name.c_str());
	}
	_selectedIndex = 0;
}

void RaceReferenceManager::SelectReference(int a_index)
{
	_selectedIndex = a_index;

	if (_selectedIndex > 0 && _selectedIndex < static_cast<int>(_refImagePaths.size())) {
		_image = FUCK::Image(_refImagePaths[_selectedIndex].c_str(), false);

		_currentWindowId = "RACE_Reference_" + _refImageNames[_selectedIndex];
		_currentWindowTitle = _refImageNames[_selectedIndex];

		_isOpen = true;
	} else {
		ClearImage();
	}
}

void RaceReferenceManager::ClearImage()
{
	_image.Reset();
	_selectedIndex = 0;

	_currentWindowId = "RACE_Reference";
	_currentWindowTitle = "Reference";

	_isOpen = false;
}

void RaceReferenceManager::DrawWindow()
{
	if (!_image.IsLoaded()) {
		FUCK::TextDisabled("$RACE_RefLoadFail"_T);
		return;
	}

	ImVec2 avail = FUCK::GetContentRegionAvail();
	if (avail.x <= 0.0f || avail.y <= 0.0f)
		return;

	float imgW = _image.GetWidth();
	float imgH = _image.GetHeight();

	if (imgW <= 0.0f || imgH <= 0.0f) {
		FUCK::TextDisabled("$RACE_RefLoadFail"_T);
		return;
	}

	float  scale = std::min(avail.x / imgW, avail.y / imgH);
	ImVec2 size(imgW * scale, imgH * scale);

	float cursorX = FUCK::GetCursorPos().x + (avail.x - size.x) * 0.5f;
	float cursorY = FUCK::GetCursorPos().y + (avail.y - size.y) * 0.5f;

	FUCK::SetCursorPos({ cursorX, cursorY });
	FUCK::DrawImage(_image.GetID(), size);
}
