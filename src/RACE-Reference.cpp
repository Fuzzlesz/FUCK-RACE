#include "RACE-Reference.h"

// --- RaceRefWindow ---

RaceRefWindow::RaceRefWindow(int a_index, const std::string& a_name) :
	_index(a_index), _title(a_name), _id("RACE_Reference_" + a_name)
{
}

void RaceRefWindow::Draw()
{
	RaceReferenceManager::GetSingleton()->DrawReference(_index);
	_lastPos  = FUCK::GetWindowPos();
	_lastSize = FUCK::GetWindowSize();
}

bool RaceRefWindow::IsOpen() const
{
	return RaceReferenceManager::GetSingleton()->IsReferenceOpen(_index);
}

void RaceRefWindow::SetOpen(bool a_open)
{
	RaceReferenceManager::GetSingleton()->SetReferenceOpen(_index, a_open);
}

FUCK::WindowFlags RaceRefWindow::GetFlags() const
{
	FUCK::WindowFlags flags   = FUCK::WindowFlags::kExtendBorder;
	ImVec2            mouse   = FUCK::GetMousePos();
	bool              hovered = mouse.x >= _lastPos.x && mouse.x <= _lastPos.x + _lastSize.x &&
	               mouse.y >= _lastPos.y && mouse.y <= _lastPos.y + _lastSize.y;

	if (!hovered) {
		flags = flags | FUCK::WindowFlags::kPassInputToGame;
	}

	return flags;
}

// --- RaceReferenceManager ---

void RaceReferenceManager::ScanReferences()
{
	ClearAll();

	std::vector<std::string> refNames;
	std::vector<std::string> refPaths;

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
					refNames.push_back(entry.path().filename().string());
					refPaths.push_back(entry.path().string());
				}
			}
		}
	}

	if (!refNames.empty()) {
		std::vector<size_t> indices(refNames.size());
		for (size_t i = 0; i < refNames.size(); ++i) indices[i] = i;

		std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
			return _stricmp(refNames[a].c_str(), refNames[b].c_str()) < 0;
		});

		for (size_t idx : indices) {
			RefItem item;
			item.name   = refNames[idx];
			item.path   = refPaths[idx];
			item.isOpen = false;
			_refItems.push_back(std::move(item));
		}
	}

	for (size_t i = 0; i < _refItems.size(); ++i) {
		auto* win = new RaceRefWindow(static_cast<int>(i), _refItems[i].name);
		_windows.push_back(win);
		FUCK::RegisterWindow(win);
	}

	UpdateComboStrings();
}

void RaceReferenceManager::UpdateComboStrings()
{
	_comboStrings.clear();
	_comboStrings.push_back("$RACE_References"_T);

	for (const auto& item : _refItems) {
		std::string prefix = item.isOpen ? "[ X ] " : "[    ] ";
		_comboStrings.push_back(prefix + item.name);
	}

	_comboStringsCStr.clear();
	for (const auto& str : _comboStrings) {
		_comboStringsCStr.push_back(str.c_str());
	}
}

void RaceReferenceManager::ToggleReference(int a_index)
{
	if (a_index < 0 || a_index >= static_cast<int>(_refItems.size()))
		return;

	bool newState = !_refItems[a_index].isOpen;
	SetReferenceOpen(a_index, newState);
}

void RaceReferenceManager::SetReferenceOpen(int a_index, bool a_open)
{
	if (a_index < 0 || a_index >= static_cast<int>(_refItems.size()))
		return;

	auto& item = _refItems[a_index];
	if (item.isOpen != a_open) {
		item.isOpen = a_open;
		if (a_open) {
			if (!item.image.IsLoaded()) {
				item.image = FUCK::Image(item.path.c_str(), false);
			}
		} else {
			item.image.Reset();
		}
		UpdateComboStrings();
	}
}

bool RaceReferenceManager::IsReferenceOpen(int a_index) const
{
	if (a_index < 0 || a_index >= static_cast<int>(_refItems.size()))
		return false;
	return _refItems[a_index].isOpen;
}

void RaceReferenceManager::DrawReference(int a_index)
{
	if (a_index < 0 || a_index >= static_cast<int>(_refItems.size()))
		return;

	auto& item = _refItems[a_index];
	if (!item.image.IsLoaded()) {
		FUCK::TextDisabled("$RACE_RefLoadFail"_T);
		return;
	}

	ImVec2 avail = FUCK::GetContentRegionAvail();
	if (avail.x <= 0.0f || avail.y <= 0.0f)
		return;

	float imgW = item.image.GetWidth();
	float imgH = item.image.GetHeight();
	if (imgW <= 0.0f || imgH <= 0.0f) {
		FUCK::TextDisabled("$RACE_RefLoadFail"_T);
		return;
	}

	float  scale = std::min(avail.x / imgW, avail.y / imgH);
	ImVec2 size(imgW * scale, imgH * scale);

	float cursorX = FUCK::GetCursorPos().x + (avail.x - size.x) * 0.5f;
	float cursorY = FUCK::GetCursorPos().y + (avail.y - size.y) * 0.5f;

	FUCK::SetCursorPos({ cursorX, cursorY });
	FUCK::DrawImage(item.image.GetID(), size);
}

void RaceReferenceManager::CloseAllWindows()
{
	for (size_t i = 0; i < _refItems.size(); ++i) {
		SetReferenceOpen(static_cast<int>(i), false);
	}
}

void RaceReferenceManager::ClearAll()
{
	for (auto* win : _windows) {
		FUCK::UnregisterWindow(win);
		delete win;
	}
	_windows.clear();
	_refItems.clear();
	UpdateComboStrings();
}
