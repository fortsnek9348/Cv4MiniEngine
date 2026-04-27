#pragma once

#include <CvGameCoreDLL/CvPopupInfo.h>
#include <CvGameCoreDLL/CvEnums.h>

#include <pybind11/pybind11.h>

class CyPopupInfo
{
public:
	static void registerWithPython(const pybind11::module& m);

	std::unique_ptr<CvPopupInfo> ptr = std::make_unique<CvPopupInfo>();

	void addPopup(int iPlayer);
	void addPythonButton(const std::wstring& szText, const std::string& szArt);
	ButtonPopupTypes getButtonPopupType();
	int getData1();
	int getData2();
	int getData3();
	int getFlags();
	int getNumPythonButtons();
	std::string getOnClickedPythonCallback();
	std::string getOnFocusPythonCallback();
	bool getOption1();
	bool getOption2();
	std::string getPythonButtonArt();
	std::string getPythonButtonText();
	std::string getPythonModule();
	std::string getText();
	bool isNone();
	void setButtonPopupType(ButtonPopupTypes eValue);
	void setData1(int iValue);
	void setData2(int iValue);
	void setData3(int iValue);
	void setFlags(int iValue);
	void setOnClickedPythonCallback(const std::string& szOnFocus);
	void setOnFocusPythonCallback(const std::string& szOnFocus);
	void setOption1(bool bValue);
	void setOption2(bool bValue);
	void setPythonModule(const std::string& szOnFocus);
	void setText(const std::wstring& szText);
};