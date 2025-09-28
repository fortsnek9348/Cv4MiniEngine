#include "CyPopup.h"
#include "../CvInterface.h"
#include "../PythonPopup.h"

void CyPopup::registerWithPython(const pybind11::module& m)
{
#define R(x) def(#x, &CyPopup::x)

	pybind11::class_<CyPopup>(m, "CyPopup")
		.def(pybind11::init<int, EventContextTypes, bool>())
		.R(addButton)
		.R(addButtonXY)
		.R(addDDS)
		.R(addFixedSeparator)
		.R(addLeaderhead)
		.R(addListBoxString)
		.R(addPullDownString)
		.R(addPythonButton)
		.R(addPythonButtonXY)
		.R(addPythonDDS)
		.R(addSeparator)
		.R(addTableCellDDS)
		.R(addTableCellImage)
		.R(addTableCellText)
		.R(completeTableAndAttach)
		.R(completeTableAndAttachXY)
		.R(createCheckBoxes)
		.R(createEditBox)
		.R(createEditBoxXY)
		.R(createListBox)
		.R(createListBoxXY)
		.R(createPullDown)
		.R(createPullDownXY)
		.R(createPythonCheckBoxes)
		.R(createPythonEditBox)
		.R(createPythonEditBoxXY)
		.R(createPythonListBox)
		.R(createPythonListBoxXY)
		.R(createPythonPullDown)
		.R(createPythonPullDownXY)
		.R(createPythonRadioButtons)
		.R(createRadioButtons)
		.R(createSpinBox)
		.R(createTable)
		//.R(isNone)
		.R(launch)
		.R(setBodyString)
		.R(setCheckBoxText)
		.R(setEditBoxMaxCharCount)
		.R(setHeaderString)
		.R(setPosition)
		.R(setPythonBodyString)
		.R(setPythonCheckBoxText)
		.R(setPythonRadioButtonText)
		.R(setRadioButtonText)
		.R(setSelectedListBoxString)
		.R(setSelectedPulldownID)
		.R(setSize)
		.R(setTableCellSize)
		.R(setTableYSize)
		.R(setTimer)
		.R(setUserData)
		;
}

using EControlType = CvPopup::EControlType;

CyPopup::CyPopup(int eventId, EventContextTypes eventCtxType, bool bDynamic) : mPopup(std::make_unique<PythonPopup>(eventId, eventCtxType))
{
	// TODO: What does this mean??
	if (!bDynamic)
		std::abort();
}

void CyPopup::addButton(const std::wstring& szText)
{
	mPopup->controls.push_back({ .type = EControlType::Button, .text = szText });
}

void CyPopup::addButtonXY(const std::wstring& szText, [[maybe_unused]] int iX, [[maybe_unused]] int iY)
{
	mPopup->controls.push_back({ .type = EControlType::Button, .text = szText });
}

void CyPopup::addDDS(const std::wstring& szPathName, [[maybe_unused]] int iX, [[maybe_unused]] int iY, [[maybe_unused]] int iWidth, [[maybe_unused]] int iHeight)
{
	mPopup->controls.push_back({ .type = EControlType::Image, .text = szPathName });
}

void CyPopup::addFixedSeparator(int iSpace)
{
	mPopup->controls.push_back({ .type = EControlType::Separator, .sepSize = iSpace });
}

void CyPopup::addLeaderhead([[maybe_unused]] const std::string& szPathName, [[maybe_unused]] int eWho, [[maybe_unused]] int eInitAttitude, [[maybe_unused]] int iX, [[maybe_unused]] int iY)
{
	std::abort();
}

void CyPopup::addListBoxString(const std::string& szText, int iID, int iGroup)
{
	mPopup->findControl(EControlType::ListBox, iGroup).listItems.push_back({ szText, iID });
}

void CyPopup::addPullDownString(const std::string& szText, int iID, int iGroup)
{
	mPopup->findControl(EControlType::ComboBox, iGroup).listItems.push_back({ szText, iID });
}

void CyPopup::addPythonButton([[maybe_unused]] const std::string& szFunctionName, [[maybe_unused]] const std::string& szBtnText, [[maybe_unused]] const std::string& szHelpText, [[maybe_unused]] const std::string& szArtFile, [[maybe_unused]] int iData1, [[maybe_unused]] int iData2, [[maybe_unused]] bool bOption)
{
	std::abort();
}

void CyPopup::addPythonButtonXY([[maybe_unused]] const std::string& szFunctionName, [[maybe_unused]] const std::string& szBtnText, [[maybe_unused]] const std::string& szHelpText, [[maybe_unused]] const std::string& szArtFile, [[maybe_unused]] int iData1, [[maybe_unused]] int iData2, [[maybe_unused]] bool bOption, [[maybe_unused]] int iX, [[maybe_unused]] int iY)
{
	std::abort();
}

void CyPopup::addPythonDDS([[maybe_unused]] const std::string& szPathName, [[maybe_unused]] const std::string& szText, [[maybe_unused]] int iX, [[maybe_unused]] int iY, [[maybe_unused]] int iWidth, [[maybe_unused]] int iHeight)
{
	std::abort();
}

void CyPopup::addSeparator()
{
	mPopup->controls.push_back({ .type = EControlType::Separator, .sepSize = 1 });
}

void CyPopup::addTableCellDDS([[maybe_unused]] int iRow, [[maybe_unused]] int iCol, [[maybe_unused]] const std::string& szFile, [[maybe_unused]] int iX, [[maybe_unused]] int iY, [[maybe_unused]] int iWidth, [[maybe_unused]] int iHeight, [[maybe_unused]] int iGroup)
{
	std::abort();
}

void CyPopup::addTableCellImage([[maybe_unused]] int iRow, [[maybe_unused]] int iCol, [[maybe_unused]] const std::string& szFile, [[maybe_unused]] int iGroup)
{
	std::abort();
}

void CyPopup::addTableCellText([[maybe_unused]] int iRow, [[maybe_unused]] int iCol, [[maybe_unused]] const std::string& szText, [[maybe_unused]] int iGroup)
{
	std::abort();
}

void CyPopup::completeTableAndAttach([[maybe_unused]] int iGroup)
{
	std::abort();
}

void CyPopup::completeTableAndAttachXY([[maybe_unused]] int iGroup, [[maybe_unused]] int iX, [[maybe_unused]] int iY)
{
	std::abort();
}

void CyPopup::createCheckBoxes(int iNumBoxes, int iGroup)
{
	mPopup->controls.push_back({ .type = EControlType::CheckBoxGroup, .iGroup = iGroup, .numControls = iNumBoxes });
}

void CyPopup::createEditBox(const std::wstring& szText, int iGroup)
{
	mPopup->controls.push_back({ .type = EControlType::EditBox, .text = szText, .iGroup = iGroup });
}

void CyPopup::createEditBoxXY(const std::wstring& szText, int iGroup, [[maybe_unused]] int iX, [[maybe_unused]] int iY)
{
	mPopup->controls.push_back({ .type = EControlType::EditBox, .text = szText, .iGroup = iGroup });
}

void CyPopup::createListBox(int iGroup)
{
	mPopup->controls.push_back({ .type = EControlType::ListBox, .iGroup = iGroup });
}

void CyPopup::createListBoxXY(int iGroup, [[maybe_unused]] int iX, [[maybe_unused]] int iY)
{
	mPopup->controls.push_back({ .type = EControlType::ListBox, .iGroup = iGroup });
}

void CyPopup::createPullDown(int iGroup)
{
	mPopup->controls.push_back({ .type = EControlType::ComboBox, .iGroup = iGroup });
}

void CyPopup::createPullDownXY(int iGroup, [[maybe_unused]] int iX, [[maybe_unused]] int iY)
{
	mPopup->controls.push_back({ .type = EControlType::ComboBox, .iGroup = iGroup });
}

void CyPopup::createPythonCheckBoxes([[maybe_unused]] int iNumBoxes, [[maybe_unused]] int iGroup)
{
	std::abort();
}

void CyPopup::createPythonEditBox([[maybe_unused]] const std::string& szText, [[maybe_unused]] const std::string& szHelpText, [[maybe_unused]] int iGroup)
{
	std::abort();
}

void CyPopup::createPythonEditBoxXY([[maybe_unused]] const std::string& szText, [[maybe_unused]] const std::string& szHelpText, [[maybe_unused]] int iGroup, [[maybe_unused]] int iX, [[maybe_unused]] int iY)
{
	std::abort();
}

void CyPopup::createPythonListBox([[maybe_unused]] const std::string& szText, [[maybe_unused]] int iGroup)
{
	std::abort();
}

void CyPopup::createPythonListBoxXY([[maybe_unused]] const std::string& szText, [[maybe_unused]] int iGroup, [[maybe_unused]] int iX, [[maybe_unused]] int iY)
{
	std::abort();
}

void CyPopup::createPythonPullDown([[maybe_unused]] const std::string& szText, [[maybe_unused]] int iGroup)
{
	std::abort();
}

void CyPopup::createPythonPullDownXY([[maybe_unused]] const std::string& szText, [[maybe_unused]] int iGroup, [[maybe_unused]] int iX, [[maybe_unused]] int iY)
{
	std::abort();
}

void CyPopup::createPythonRadioButtons([[maybe_unused]] int iNumButtons, [[maybe_unused]] int iGroup)
{
	std::abort();
}

void CyPopup::createRadioButtons(int iNumButtons, int iGroup)
{
	mPopup->controls.push_back({ .type = EControlType::RadioButtonGroup, .iGroup = iGroup, .numControls = iNumButtons });
}

void CyPopup::createSpinBox([[maybe_unused]] int iIndex, [[maybe_unused]] const std::string& szHelpText, [[maybe_unused]] int iDefault, [[maybe_unused]] int iIncrement, [[maybe_unused]] int iMax, [[maybe_unused]] int iMin)
{
	std::abort();
}

void CyPopup::createTable([[maybe_unused]] int iRows, [[maybe_unused]] int iCols, [[maybe_unused]] int iGroup)
{
	std::abort();
}

//bool CyPopup::isNone()
//{
//	return false;
//}

bool CyPopup::launch(bool bCreateOK, PopupStates eState)
{
	return CvInterface::getInstance().launchPopup(std::move(mPopup), bCreateOK, eState);
}

void CyPopup::setBodyString(const std::wstring& szText, [[maybe_unused]] int uiFlags)
{
	mPopup->bodyString = szText;
}

void CyPopup::setCheckBoxText(int iCheckBoxID, const std::string& szText, int iGroup)
{
	mPopup->findControl(EControlType::CheckBoxGroup, iGroup).listItems.emplace_back(szText, iCheckBoxID);
}

void CyPopup::setEditBoxMaxCharCount(int maxCharCount, [[maybe_unused]] int preferredCharCount, int iGroup)
{
	mPopup->findControl(EControlType::EditBox, iGroup).editBoxMaxLength = maxCharCount;
}

void CyPopup::setHeaderString(const std::wstring& szText, [[maybe_unused]] int uiFlags)
{
	mPopup->headerString = szText;
}

void CyPopup::setPosition([[maybe_unused]] int iX, [[maybe_unused]] int iY)
{
	std::abort();
}

void CyPopup::setPythonBodyString([[maybe_unused]] const std::string& szDefText, [[maybe_unused]] const std::string& szName, [[maybe_unused]] const std::string& szText, [[maybe_unused]] int uiFlags)
{
	std::abort();
}

void CyPopup::setPythonCheckBoxText([[maybe_unused]] int iCheckBoxID, [[maybe_unused]] const std::string& szText, [[maybe_unused]] const std::string& szHelpText, [[maybe_unused]] int iGroup)
{
	std::abort();
}

void CyPopup::setPythonRadioButtonText([[maybe_unused]] int iRadioButtonID, [[maybe_unused]] const std::string& szText, [[maybe_unused]] const std::string& szHelpText, [[maybe_unused]] int iGroup)
{
	std::abort();
}

void CyPopup::setRadioButtonText(int iRadioButtonID, const std::string& szText, int iGroup)
{
	mPopup->findControl(EControlType::RadioButtonGroup, iGroup).listItems.emplace_back(szText, iRadioButtonID);
}

void CyPopup::setSelectedListBoxString([[maybe_unused]] int iID, [[maybe_unused]] int iGroup)
{
	std::abort();
}

void CyPopup::setSelectedPulldownID([[maybe_unused]] int iID, [[maybe_unused]] int iGroup)
{
	std::abort();
}

void CyPopup::setSize([[maybe_unused]] int iXS, [[maybe_unused]] int iYS)
{
	std::abort();
}

void CyPopup::setTableCellSize([[maybe_unused]] int iCol, [[maybe_unused]] int iPixels, [[maybe_unused]] int iGroup)
{
	std::abort();
}

void CyPopup::setTableYSize([[maybe_unused]] int iRow, [[maybe_unused]] int iSize, [[maybe_unused]] int iGroup)
{
	std::abort();
}

void CyPopup::setTimer([[maybe_unused]] int uiTime)
{
	std::abort();
}

void CyPopup::setUserData(pybind11::tuple userData)
{
	mPopup->userData = std::move(userData);
}
