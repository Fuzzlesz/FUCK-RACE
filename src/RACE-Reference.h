#pragma once
#include "PCH.h"

class RaceRefWindow : public FUCK::IWindow
{
public:
	RaceRefWindow(int a_index, const std::string& a_name);

	const char* Id() const override { return _id.c_str(); }
	const char* Title() const override { return _title.c_str(); }

	void              Draw() override;
	bool              IsOpen() const override;
	void              SetOpen(bool a_open) override;
	FUCK::WindowFlags GetFlags() const override;
	ImVec2            GetDefaultSize() const override { return FUCK::Scale(350.0f, 500.0f); }

private:
	int         _index;
	std::string _id;
	std::string _title;
	ImVec2      _lastPos{};
	ImVec2      _lastSize{};
};

class RaceReferenceManager : public REX::Singleton<RaceReferenceManager>
{
public:
	void ScanReferences();
	void ToggleReference(int a_index);
	void SetReferenceOpen(int a_index, bool a_open);
	bool IsReferenceOpen(int a_index) const;
	void DrawReference(int a_index);
	void CloseAllWindows();
	void ClearAll();

	bool                            HasReferences() const { return !_refItems.empty(); }
	const std::vector<const char*>& GetComboStrings() const { return _comboStringsCStr; }

private:
	struct RefItem
	{
		std::string name;
		std::string path;
		bool        isOpen = false;
		FUCK::Image image;
	};

	void UpdateComboStrings();

	std::vector<RefItem>        _refItems;
	std::vector<std::string>    _comboStrings;
	std::vector<const char*>    _comboStringsCStr;
	std::vector<RaceRefWindow*> _windows;
};
