#pragma once

#include <CvEnums.h>

#include <pybind11/pybind11.h>

class PythonPopup;

class CyPopup
{
public:
	static void registerWithPython(const pybind11::module& m);

	explicit CyPopup(int eventId, EventContextTypes eventCtxType, bool bDynamic);

	void addButton(const std::wstring& szText);
	void addButtonXY(const std::wstring& szText, int iX, int iY);
	void addDDS(const std::wstring& szPathName, int iX, int iY, int iWidth, int iHeight);
	void addFixedSeparator(int iSpace);
	void addLeaderhead(const std::string& szPathName, int eWho, int eInitAttitude, int iX, int iY);
	void addListBoxString(const std::string& szText, int iID, int iGroup);
	void addPullDownString(const std::string& szText, int iID, int iGroup);
	void addPythonButton(const std::string& szFunctionName, const std::string& szBtnText, const std::string& szHelpText, const std::string& szArtFile, int iData1, int iData2, bool bOption);
	void addPythonButtonXY(const std::string& szFunctionName, const std::string& szBtnText, const std::string& szHelpText, const std::string& szArtFile, int iData1, int iData2, bool bOption, int iX, int iY);
	void addPythonDDS(const std::string& szPathName, const std::string& szText, int iX, int iY, int iWidth, int iHeight);
	void addSeparator();
	void addTableCellDDS(int iRow, int iCol, const std::string& szFile, int iX, int iY, int iWidth, int iHeight, int iGroup);
	void addTableCellImage(int iRow, int iCol, const std::string& szFile, int iGroup);
	void addTableCellText(int iRow, int iCol, const std::string& szText, int iGroup);
	void completeTableAndAttach(int iGroup);
	void completeTableAndAttachXY(int iGroup, int iX, int iY);
	void createCheckBoxes(int iNumBoxes, int iGroup);
	void createEditBox(const std::wstring& szText, int iGroup);
	void createEditBoxXY(const std::wstring& szText, int iGroup, int iX, int iY);
	void createListBox(int iGroup);
	void createListBoxXY(int iGroup, int iX, int iY);
	void createPullDown(int iGroup);
	void createPullDownXY(int iGroup, int iX, int iY);
	void createPythonCheckBoxes(int iNumBoxes, int iGroup);
	void createPythonEditBox(const std::string& szText, const std::string& szHelpText, int iGroup);
	void createPythonEditBoxXY(const std::string& szText, const std::string& szHelpText, int iGroup, int iX, int iY);
	void createPythonListBox(const std::string& szText, int iGroup);
	void createPythonListBoxXY(const std::string& szText, int iGroup, int iX, int iY);
	void createPythonPullDown(const std::string& szText, int iGroup);
	void createPythonPullDownXY(const std::string& szText, int iGroup, int iX, int iY);
	void createPythonRadioButtons(int iNumButtons, int iGroup);
	void createRadioButtons(int iNumButtons, int iGroup);
	void createSpinBox(int iIndex, const std::string& szHelpText, int iDefault, int iIncrement, int iMax, int iMin);
	void createTable(int iRows, int iCols, int iGroup);
	//bool isNone();
	bool launch(bool bCreateOK, PopupStates eState);
	void setBodyString(const std::wstring& szText, int uiFlags);
	void setCheckBoxText(int iCheckBoxID, const std::string& szText, int iGroup);
	void setEditBoxMaxCharCount(int maxCharCount, int preferredCharCount, int iGroup);
	void setHeaderString(const std::wstring& szText, int uiFlags);
	void setPosition(int iX, int iY);
	void setPythonBodyString(const std::string& szDefText, const std::string& szName, const std::string& szText, int uiFlags);
	void setPythonCheckBoxText(int iCheckBoxID, const std::string& szText, const std::string& szHelpText, int iGroup);
	void setPythonRadioButtonText(int iRadioButtonID, const std::string& szText, const std::string& szHelpText, int iGroup);
	void setRadioButtonText(int iRadioButtonID, const std::string& szText, int iGroup);
	void setSelectedListBoxString(int iID, int iGroup);
	void setSelectedPulldownID(int iID, int iGroup);
	void setSize(int iXS, int iYS);
	void setTableCellSize(int iCol, int iPixels, int iGroup);
	void setTableYSize(int iRow, int iSize, int iGroup);
	void setTimer(int uiTime);
	void setUserData(pybind11::tuple userData);

private:
	std::unique_ptr<PythonPopup> mPopup;
	
};