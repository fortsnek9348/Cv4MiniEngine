#pragma once

#include <pybind11/pytypes.h>

class CyTranslator
{
public:
	std::wstring changeTextColor(const std::wstring& szText, int iColor) const;
	//std::wstring changeBackColor(const std::wstring& szText, int iColor) const;
	std::wstring getColorText(const std::string& szTag, const pybind11::tuple& args, int iColor) const;
	std::wstring getObjectText(const std::string& szTag, int i) const;
	std::wstring getText(const std::string& szTag, const pybind11::tuple& args) const;
	std::wstring stripHTML(std::string szText) const;
};