#pragma once

#include <CvDLLUtilityIFaceBase.h>

#include <unordered_map>

class CvTranslator
{
public:
	static CvTranslator& getInstance();

	wchar_t firstSymbolCode{};
	// [SymbolTypes]?
	std::vector<wchar_t> symbols;

	struct TextEntryValue
	{
		std::wstring text;
		std::wstring gender;
		std::wstring plural;
	};

	std::unordered_map<std::wstring, TextEntryValue> textEntries;

	static void initialiseTextCodeTags();

	static std::wstring changeTextColor(const std::wstring& szText, int iColor);
	static std::wstring changeBackColor(const std::wstring& szText, int iColor);
	
	std::wstring formatTextcode(std::wstring_view text, std::span<const CvDLLUtilityIFaceBase::TextArg> args) const;
	std::wstring getText(const std::wstring& textId) const;
	std::wstring getText(const std::wstring& textId, std::span<const CvDLLUtilityIFaceBase::TextArg> args) const;

	static std::wstring lowerCiv4TextCodeToXml(std::wstring_view textcode);

private:
	CvTranslator();
};