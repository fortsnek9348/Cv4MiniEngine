#pragma once

#include <CvGameCoreDLL/CvEnums.h>

#include <pybind11/pybind11.h>

class PopupReturn;

class CyPopupReturn
{
public:
	static void registerWithPython(const pybind11::module& m);

	const PopupReturn* ptr = nullptr;

	bool isNone() const;
	int getSelectedRadioButton(int iGroup) const;
	std::wstring getEditBoxString(int iGroup) const;
	int getSpinnerWidgetValue(int iGroup) const;
	int getSelectedPullDownValue(int iGroup) const;
	int getSelectedListBoxValue(int iGroup) const;
	int getButtonClicked() const;
};