#pragma once

#include <HeckTextUI/Combobox.h>

std::wstring getRandomText();

class IWindowChildEventHandler
{
public:
	virtual void onComboBoxSelectionChanged(hecktui::Combobox&) {}
	virtual void onCheckBoxValueChanged(hecktui::Checkbox&) {}
	virtual ~IWindowChildEventHandler() = default;
};

class MyComboBox : public hecktui::Combobox
{
public:
	explicit MyComboBox(IWindowChildEventHandler& eventHandler, hecktui::EComboboxStyle style = hecktui::EComboboxStyle::Bulky);

	virtual void onSelectionChanged() override;

private:
	IWindowChildEventHandler& mHandler;
};

class MyCheckBox : public hecktui::Checkbox
{
public:
	explicit MyCheckBox(IWindowChildEventHandler& eventHandler, hecktui::ECheckStyle style, std::wstring label);

	virtual void onCheckChanged() override;

private:
	IWindowChildEventHandler& mHandler;
};

class UIInputError : public std::exception
{
public:
	std::wstring msg;

	explicit UIInputError(std::wstring msg);

	virtual char const* what() const noexcept override;
};

unsigned int strictStringToUInt(std::wstring_view str, std::wstring_view inputName);