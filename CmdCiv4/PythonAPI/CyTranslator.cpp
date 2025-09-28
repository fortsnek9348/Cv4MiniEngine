#include "CyTranslator.h"
#include "../DLLInterface/MyCvDLLUtility.h"
#include "../Common.h"
#include "../CvTranslator.h"

#include <CvGlobals.h>
#include <CvInfos.h>

#include <pybind11/stl.h>

std::wstring CyTranslator::changeTextColor(const std::wstring& szText, int iColor) const
{
	return CvTranslator().changeTextColor(szText, iColor);
}

std::wstring CyTranslator::getColorText(const std::string& szTag, const pybind11::tuple& args, int iColor) const
{
	return changeTextColor(getText(szTag, args), iColor);
}

std::wstring CyTranslator::getObjectText(const std::string& szTag, int i) const
{
	return MyCvDLLUtility::getInstance().getObjectText(CvWString(szTag), i, false);
}

std::wstring CyTranslator::getText(const std::string& szTag, const pybind11::tuple& args) const
{
	//if (szTag == "INTERFACE_GREAT_PERSON_TURNS")
	//	_CrtDbgBreak();

	std::vector<MyCvDLLUtility::TextArg> myargs;

	std::list<std::wstring> wstrings;
	std::list<std::string> strings;

	for (const pybind11::handle& arg : args)
	{
		if (pybind11::isinstance<pybind11::int_>(arg))
			myargs.push_back(pybind11::cast<int>(arg));
		else if (pybind11::isinstance<pybind11::str>(arg))
			myargs.push_back(wstrings.emplace_back(pybind11::cast<std::wstring>(arg)));
		else if (pybind11::isinstance<pybind11::bytes>(arg))
		{
			const std::string view = pybind11::cast<pybind11::bytes>(arg);
			myargs.push_back(strings.emplace_back(pybind11::cast<std::string>(pybind11::reinterpret_steal<pybind11::str>(PyUnicode_DecodeLatin1(view.data(), view.size(), nullptr)))));
		}
		else
			throw std::runtime_error("Unknown arg type in CyTranslator::getText.");
	}

	return MyCvDLLUtility::getInstance().getTextGeneric(CvWString(szTag), myargs);
}

std::wstring CyTranslator::stripHTML([[maybe_unused]] std::string szText) const
{
	std::abort();
}
