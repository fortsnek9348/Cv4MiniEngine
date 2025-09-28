#pragma once

#include <CvDLLUtilityIFaceBase.h>

class CvTranslator
{
public:
	static const wchar_t kFirstSymbolCode;

	std::wstring changeTextColor(const std::wstring& szText, int iColor) const;
	std::wstring changeBackColor(const std::wstring& szText, int iColor) const;
	std::wstring lookupSymbolChar(wchar_t c) const;
	std::wstring formatText(std::wstring_view text, std::span<const CvDLLUtilityIFaceBase::TextArg> args) const;
};