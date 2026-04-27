#include "CyPopupInfo.h"

#include <CvPlayerAI.h>

#include <iostream>

void CyPopupInfo::registerWithPython(const pybind11::module& m)
{
#define R(x) def(#x, &CyPopupInfo::x)

	pybind11::class_<CyPopupInfo>(m, "CyPopupInfo")
		.def(pybind11::init())
		.R(addPopup)
		.R(addPythonButton)
		.R(getButtonPopupType)
		.R(getData1)
		.R(getData2)
		.R(getData3)
		.R(getFlags)
		.R(getNumPythonButtons)
		.R(getOnClickedPythonCallback)
		.R(getOnFocusPythonCallback)
		.R(getOption1)
		.R(getOption2)
		.R(getPythonButtonArt)
		.R(getPythonButtonText)
		.R(getPythonModule)
		.R(getText)
		.R(isNone)
		.R(setButtonPopupType)
		.R(setData1)
		.R(setData2)
		.R(setData3)
		.R(setFlags)
		.R(setOnClickedPythonCallback)
		.R(setOnFocusPythonCallback)
		.R(setOption1)
		.R(setOption2)
		.R(setPythonModule)
		.R(setText)
		;
}

void CyPopupInfo::addPopup(int iPlayer)
{
	std::clog << "CyPopupInfo::addPopup for player " << iPlayer << '\n';
	CvPlayerAI::getPlayerNonInl((PlayerTypes)iPlayer).addPopup(ptr.release());
}

void CyPopupInfo::addPythonButton(const std::wstring& szText, const std::string& szArt)
{
	ptr->addPythonButton(szText.c_str(), szArt.c_str());
}

ButtonPopupTypes CyPopupInfo::getButtonPopupType()
{
	std::abort();
}

int CyPopupInfo::getData1()
{
	std::abort();
}

int CyPopupInfo::getData2()
{
	std::abort();
}

int CyPopupInfo::getData3()
{
	std::abort();
}

int CyPopupInfo::getFlags()
{
	std::abort();
}

int CyPopupInfo::getNumPythonButtons()
{
	std::abort();
}

std::string CyPopupInfo::getOnClickedPythonCallback()
{
	std::abort();
}

std::string CyPopupInfo::getOnFocusPythonCallback()
{
	std::abort();
}

bool CyPopupInfo::getOption1()
{
	std::abort();
}

bool CyPopupInfo::getOption2()
{
	std::abort();
}

std::string CyPopupInfo::getPythonButtonArt()
{
	std::abort();
}

std::string CyPopupInfo::getPythonButtonText()
{
	std::abort();
}

std::string CyPopupInfo::getPythonModule()
{
	std::abort();
}

std::string CyPopupInfo::getText()
{
	std::abort();
}

bool CyPopupInfo::isNone()
{
	std::abort();
}

void CyPopupInfo::setButtonPopupType(ButtonPopupTypes eValue)
{
	ptr->setButtonPopupType(eValue);
}

void CyPopupInfo::setData1(int iValue)
{
	ptr->setData1(iValue);
}

void CyPopupInfo::setData2(int iValue)
{
	ptr->setData2(iValue);
}

void CyPopupInfo::setData3(int iValue)
{
	ptr->setData3(iValue);
}

void CyPopupInfo::setFlags(int iValue)
{
	ptr->setFlags(iValue);
}

void CyPopupInfo::setOnClickedPythonCallback(const std::string& szOnFocus)
{
	ptr->setOnClickedPythonCallback(szOnFocus.c_str());
}

void CyPopupInfo::setOnFocusPythonCallback(const std::string& szOnFocus)
{
	ptr->setOnFocusPythonCallback(szOnFocus.c_str());
}

void CyPopupInfo::setOption1(bool bValue)
{
	ptr->setOption1(bValue);
}

void CyPopupInfo::setOption2(bool bValue)
{
	ptr->setOption2(bValue);
}

void CyPopupInfo::setPythonModule(const std::string& szOnFocus)
{
	ptr->setPythonModule(szOnFocus.c_str());
}

void CyPopupInfo::setText(const std::wstring& szText)
{
	ptr->setText(szText.c_str());
}
