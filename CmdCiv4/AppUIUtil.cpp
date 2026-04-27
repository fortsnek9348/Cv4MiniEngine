#include "AppUIUtil.h"

#include <Cv4CommonEngineLib/CvTranslator.h>
#include <Cv4CommonEngineLib/Common.h>

using namespace cvengine;

std::wstring cvengine::getRandomText()
{
	return CvTranslator::getInstance().getText(L"TXT_KEY_MAIN_MENU_RANDOM");
}

MyComboBox::MyComboBox(IWindowChildEventHandler& eventHandler, hecktui::EComboboxStyle style) : Combobox(style), mHandler(eventHandler)
{
}

void MyComboBox::onSelectionChanged()
{
	Combobox::onSelectionChanged();
	mHandler.onComboBoxSelectionChanged(*this);
}

MyCheckBox::MyCheckBox(IWindowChildEventHandler& eventHandler, hecktui::ECheckStyle style, std::wstring label) : Checkbox(style, std::move(label)), mHandler(eventHandler)
{
}

void MyCheckBox::onCheckChanged()
{
	Checkbox::onCheckChanged();
	mHandler.onCheckBoxValueChanged(*this);
}


UIInputError::UIInputError(std::wstring msg) : msg(std::move(msg))
{
}

char const* UIInputError::what() const noexcept
{
	return "UI Input Error";
}


unsigned int cvengine::strictStringToUInt(std::wstring_view str, std::wstring_view inputName)
{
	std::wstring trimmedStr = trim(std::wstring(str));

	size_t endIndex{};
	try
	{
		const unsigned long value = std::stoul(trimmedStr, &endIndex);
		if (endIndex == trimmedStr.size() && value <= std::numeric_limits<unsigned int>::max())
			return value;
	}
	catch (const std::exception&)
	{
	}
	throw UIInputError(std::wstring(inputName) + std::wstring(L": ") + static_cast<std::wstring>(CvTranslator::getInstance().getText(L"TXT_KEY_PITBOSS_CITYELIMINATION_ERROR_DESC")));
}