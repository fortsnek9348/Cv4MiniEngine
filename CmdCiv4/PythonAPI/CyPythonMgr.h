#pragma once

#include <pybind11/pybind11.h>

class CyPythonMgr
{
public:
	static void registerWithPython(const pybind11::module& m);

	void allowDefaultImpl();
	void debugMsg(const std::string& msg);
	void debugMsgWide(const std::wstring& msg);
	void errorMsg(const std::string& msg);
	void errorMsgWide(const std::wstring& msg);
};

