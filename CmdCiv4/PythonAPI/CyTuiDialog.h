#pragma once

#include "../CvTuiDialog.h"

#include <pybind11/pybind11.h>

// Replacement of CyGTabCtrl
// I think this should indeed "own" the UI.
// If you do `self.pTabControl = None` in CvOptionsScreen.py, python seemingly destroys the object and the UI doesn't show.
// This is unlike "screen" UI which controls windows stored globally in the engine.
class CyTuiDialog
{
public:
	static void registerWithPython(const pybind11::module& m);

	CvTuiDialog impl;
	std::shared_ptr<hecktui::Window> window;

	//void clear();

	//void addChild(std::string parent, std::string child);
	//void setParent(std::string child, std::string parent);
	//void moveToFirst(std::string child);
	//void delByPrefix(std::string prefix);
	//void delAllChildren(std::string parent);
	//void show(std::string name);
	//void hide(std::string name);
	//void setVisible(std::string name, bool visible);
	//void disable(std::string name);
	//void setEnabled(std::string name, bool x);
	void setInitialTitle(std::wstring title);

	void newPanel             /**/(std::string parent, std::string name);
	void setPanelBackgroundColour(std::string name, std::optional<hecktui::EColour> colour);

	void newHRule             /**/(std::string parent, std::string name, hecktui::EBorderStyle style);

	void newButton(std::string parent, std::string name, std::wstring label, std::string callbackInterfaceName, std::string callbackFunctionName);

	void newCheckBox     /**/(std::string parent, std::string name, std::wstring label, std::string callbackInterfaceName, std::string callbackFunctionName);
	//void setCheckBoxLabel/**/(std::string name, std::wstring label);
	void setCheckBoxValue/**/(std::string name, bool value);
	bool getCheckBoxValue/**/(std::string name);

	void newCombobox(std::string parent, std::string name, std::vector<std::wstring> items, std::string callbackInterfaceName, std::string callbackFunctionName);
	void setComboboxSelectedIndex(std::string name, int i);
	int getComboboxSelectedIndex(std::string name);
	
	void newControl(const std::string& parent, std::string name, std::shared_ptr<hecktui::Element> ctrl);

	void setFillLayout(std::string ctrlName);
	void setHFlowLayout(std::string ctrlName, hecktui::EJustilign halign, hecktui::EJustilign valign);
	//void setHWrapLayout(std::string ctrlName);
	void setVFlowLayout(std::string ctrlName);

	//void setTableLayout(std::string ctrlName, hecktui::TableLayoutConfig config);

	void showAsModalDialog();

	// Called by handleExitButtonInput
	void destroy();
};