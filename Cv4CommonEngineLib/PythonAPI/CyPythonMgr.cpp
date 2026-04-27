#include "CyPythonMgr.h"
#include "../inc/Cv4CommonEngineLib/MyCvDLLPython.h"

#include <iostream>

void CyPythonMgr::registerWithPython(const pybind11::module& m)
{
#define R(x) def(#x, &CyPythonMgr::x)

	pybind11::class_<CyPythonMgr>(m, "CyPythonMgr")
		.def(pybind11::init())
		.R(allowDefaultImpl)
		.R(debugMsg)
		.R(debugMsgWide)
		.R(errorMsg)
		.R(errorMsgWide)
		;
}

void CyPythonMgr::allowDefaultImpl()
{
	MyCvDLLPython::setUsingDefaultImpl();
}

void CyPythonMgr::debugMsg(const std::string& msg)
{
	std::clog << msg << std::flush;
}

void CyPythonMgr::debugMsgWide(const std::wstring& msg)
{
	std::wclog << msg << std::flush;
}

void CyPythonMgr::errorMsg(const std::string& msg)
{
	std::cerr << msg << std::flush;
}

void CyPythonMgr::errorMsgWide(const std::wstring& msg)
{
	std::wcerr << msg << std::flush;
}
