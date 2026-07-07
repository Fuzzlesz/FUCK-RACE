#pragma once

class RaceReferenceManager : public REX::Singleton<RaceReferenceManager>
{
public:
	void ScanReferences();
	void SelectReference(int a_index);
	void ClearImage();
	void DrawWindow();

	bool IsWindowOpen() const { return _isOpen; }
	void SetWindowOpen(bool a_open) { _isOpen = a_open; }

	bool                            HasReferences() const { return _refImageNames.size() > 1; }
	const std::vector<const char*>& GetComboStrings() const { return _refImageNamesCStr; }
	int                             GetSelectedIndex() const { return _selectedIndex; }

	const char* GetWindowId() const { return _currentWindowId.c_str(); }
	const char* GetWindowTitle() const { return _currentWindowTitle.c_str(); }

private:
	FUCK::Image _image;
	bool        _isOpen = false;

	std::vector<std::string> _refImageNames;
	std::vector<std::string> _refImagePaths;
	std::vector<const char*> _refImageNamesCStr;
	int                      _selectedIndex = 0;

	std::string _currentWindowId    = "RACE_Reference";
	std::string _currentWindowTitle = "Reference";
};
