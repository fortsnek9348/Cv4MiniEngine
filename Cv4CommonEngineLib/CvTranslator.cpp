#include "inc/Cv4CommonEngineLib/CvTranslator.h"

#include <CvGameCoreDLL/CvGlobals.h>
#include <CvGameCoreDLL/CvInfos.h>
#include <CvGameCoreDLL/CvDllTranslator.h>

#include <CommonStuff/range.h>

#include <algorithm>
#include <unordered_map>
#include <cwctype>
#include <expected>
#include <iostream>
#include <map>

using heck::range;

// This is the value that Civ4 uses. Derived through some method. Who knows what.
// Currently, the range is [2123..2201].
// That is:
//     https://www.compart.com/en/unicode/block/U+2100
//     https://www.compart.com/en/unicode/block/U+2150
//     https://www.compart.com/en/unicode/block/U+2190
//     https://www.compart.com/en/unicode/block/U+2200
// Using the original range is important to display event logs saved by the original engine.
CvTranslator::CvTranslator() : firstSymbolCode(L'\u2123')
{
}

CvTranslator& CvTranslator::getInstance()
{
	static CvTranslator g;
	return g;
}

static int quantise(float x)
{
	return (uint8_t)std::round(std::clamp(x * 255.0f, 0.0f, 255.0f));
}

static std::wstring toXmlRGBA(int iColor)
{
	const NiColorA colour = gGlobals.getColorInfo((ColorTypes)iColor).getColor();
	return std::format(L"{},{},{},{}", quantise(colour.r), quantise(colour.g), quantise(colour.b), quantise(colour.a));
}

//static std::wstring toXmlRGBA(const char* colour)
//{
//	return toXmlRGBA(gGlobals.getInfoTypeForString(colour));
//}

static std::wstring changeColour(const std::wstring& szText, int iColor, std::wstring_view tag)
{
	if (iColor != NO_COLOR)
		return std::format(L"<{}={}>{}</{}>", tag, toXmlRGBA(iColor), szText, tag);
	else
		return szText;
}

std::wstring CvTranslator::changeTextColor(const std::wstring& szText, int iColor)
{
	return changeColour(std::move(szText), iColor, L"color");
}
std::wstring CvTranslator::changeBackColor(const std::wstring& szText, int iColor)
{
	return changeColour(std::move(szText), iColor, L"bgcolor");
}

// Unicode private-use area.
//inline constexpr wchar_t kFirstSymbolCode = L'\uE000';

static const auto kWCSPropAlnum = std::wctype("alnum");

static bool isIdentifierChar(wchar_t c)
{
	return (bool)std::iswctype(c, kWCSPropAlnum) || c == '_';
}

static int tryParseAsciiDigit(wchar_t c)
{
	if (L'0' <= c && c <= L'9')
		return c - L'0';
	else
		return -1;
}

namespace
{
	enum class EError
	{
		None,
		Overflow,
		BadSyntax,
	};
}

static std::expected<uint32_t, EError> tryConsumeUInt(std::wstring_view& text, uint32_t max)
{
	if (text.empty())
		return std::unexpected(EError::BadSyntax);

	int digit = tryParseAsciiDigit(text.front());

	if (digit < 0)
		return std::unexpected(EError::BadSyntax);

	text.remove_prefix(1);

	uint32_t value = digit;

	while (!text.empty())
	{
		digit = tryParseAsciiDigit(text.front());
		if (digit < 0)
			break;
		text.remove_prefix(1);
		if (uint64_t(value) * 10 + digit > UINT32_MAX)
			return std::unexpected(EError::Overflow);
		value = value * 10 + digit;
	}

	if (value > max)
		return std::unexpected(EError::Overflow);

	return value;
}







namespace
{
	//void escapeToXml(std::wstring& result, std::wstring_view text)
	//{
	//	for (const wchar_t c : text)
	//		if (c == L'<')
	//			result += L"&lt;";
	//		else if (c == L'>')
	//			result += L"&gt;";
	//		else if (c == L'&')
	//			result += L"&amp;";
	//		else
	//			result += c;
	//}

	struct ArgFormatter
	{
		const CvTranslator& self;
		std::wstring& result;

		void operator()(wchar_t type, std::wstring_view& remainingSpec, std::string_view s) const
		{
			(*this)(type, remainingSpec, CvWString(CvString(s)));
		}

		void operator()(wchar_t type, std::wstring_view& remainingSpec, std::wstring_view s) const
		{
			if (type == L's')
			{
				uint32_t selection = 0;
				// It is possible to get the colon without the form selection arg.
				if (remainingSpec.size() >= 3 && remainingSpec[0] == L':' && tryParseAsciiDigit(remainingSpec[1]) >= 0)
				{
					remainingSpec.remove_prefix(1);
					selection = tryConsumeUInt(remainingSpec, UINT32_MAX).value() - 1;
				}

				result += gGlobals.getDLLIFaceNonInl()->getObjectText(s, selection, true);
			}
			else
				result += L"{wrong arg type}";
		}

		void operator()(wchar_t type, std::wstring_view&, int x) const
		{
			if (type == L'd')
				result += std::to_wstring(x);
			else if (type == L'D')
				result += (x >= 0 ? CvWString(L"+") : CvWString()) + CvWString(std::to_wstring(x));
			else if (type == L'F')
			{
				if (result.size() && !std::iswspace(result.back()))
					result += L' ';
				// Substitute in the replacement or the symbol character value? Looking at the Sailing tooltip, looks like it should be the value.
				//result += self.lookupSymbolChar(wchar_t(x));
				result += static_cast<wchar_t>(x);
			}
			else
				result += L"{wrong arg type}";
		}
	};

	struct NumSelect
	{
		std::wstring_view singular;
		std::wstring_view plural;

		std::wstring_view operator()(std::string_view) const
		{
			return L"{wrong arg type for NUM}";
		}

		std::wstring_view operator()(std::wstring_view) const
		{
			return L"{wrong arg type for NUM}";
		}

		std::wstring_view operator()(int x) const
		{
			return x == 1 ? singular : plural;
		}
	};
}

// https://www.google.com/search?q=civ+4+text+format+%22%25d1%22
// https://forums.civfanatics.com/threads/singular-and-plural-for-xml-of-texts.507995/
// https://forums.civfanatics.com/threads/meaning-of-s1-s2-s3-s4-s5-and-s6.268355/



std::wstring CvTranslator::formatTextcode(std::wstring_view text, std::span<const CvDLLUtilityIFaceBase::TextArg> args) const
{
	[[maybe_unused]] const auto originalText = text;

	std::wstring result;

	const ArgFormatter argFormatter{ *this, result };

	while (text.size())
	{
		const auto parseArgIndex = [&]() -> std::optional<uint32_t> {
			auto expValue = tryConsumeUInt(text, (uint32_t)args.size());
			if (expValue.has_value() && (expValue.value() < 1 || expValue.value() > args.size()))
				expValue = std::unexpected(EError::Overflow);

			if (expValue.has_value())
			{
				return expValue.value() - 1;
			}
			else if (expValue.error() == EError::Overflow)
			{
				result += L"{arg index out of range}";
				return std::nullopt;
			}
			else
			{
				text.remove_prefix(1);
				result += L"{expected arg index in format spec}";
				return std::nullopt;
			}
		};

		if (text.front() == L'%')
		{
			text.remove_prefix(1);
			wchar_t type = text.front();
			text.remove_prefix(1);
			if (type == L'%')
				result += L'%';
			else
			{
				const bool isBracketed = type == L'[';
				if (isBracketed)
				{
					if (text.empty())
					{
						result += L"{inalid format spec at end of text}";
						break;
					}
					// Why did I do this after remove_prefix...
					// This fixes combat messages.
					type = text.front();
					text.remove_prefix(1);
				}

				if (const std::optional<uint32_t> optArgI = parseArgIndex())
				{
					std::visit([argFormatter, type, &text](const auto& value) {
						argFormatter(type, text, value);
					}, args[*optArgI]);
				}

				// Skip trailing identifier.
				while (text.size() && isIdentifierChar(text.front()) && text.front() < firstSymbolCode)
					text.remove_prefix(1);

				if (isBracketed)
				{
					if (text.front() != L']')
						result += L"{expected rsqbracket}";
					else
						text.remove_prefix(1);
				}
			}
		}
		else if (text.front() == L'[')
		{
			// [NUM%d:Turn:Turns]
			if (text.starts_with(L"[NUM"))
			{
				//const size_t endI = std::min(text.find(L']'), text.size() - 1) + 1;

				//const std::wstring_view spec = text.substr(0, endI);

				text.remove_prefix(4);

				if (const std::optional<uint32_t> optArgI = parseArgIndex())
				{
					if (text.size() && text.front() == L':')
					{
						text.remove_prefix(1);
						const size_t form1Len = std::min(text.find(L':'), text.size());
						if (form1Len < text.size())
						{
							const std::wstring_view form1 = text.substr(0, form1Len);
							text.remove_prefix(form1Len + 1);
							const size_t form2Len = std::min(text.find(L']'), text.size());
							if (form2Len < text.size())
							{
								const std::wstring_view form2 = text.substr(0, form2Len);
								text.remove_prefix(form2Len + 1);
								result += std::visit(NumSelect{ form1, form2 }, args[*optArgI]);
							}
							else
							{
								result += L"{number spec form 2 end bracket missing}";
							}
						}
						else
						{
							result += L"{number spec form 1 end colon missing}";
						}
					}
					else
					{
						result += L"{number spec form 1 missing}";
					}
				}
			}
			else
			{
				result += text.front();
				text.remove_prefix(1);
			}
		}
		else
		{
			const size_t pos = std::min<size_t>(text.find_first_of(L"%["), text.size());
			result += text.substr(0, pos);
			text.remove_prefix(pos);
		}
	}

	//// Escape any special characters for XML.
	//if (result.find_first_of(L"<>&") != result.npos)
	//{
	//	std::wstring escaped;
	//	for (const wchar_t c : result)
	//		if (c == L'<')
	//			escaped += L"&lt;";
	//		else if (c == L'>')
	//			escaped += L"&gt;";
	//		else if (c == L'&')
	//			escaped += L"&amp;";
	//		else
	//			escaped += c;
	//	return escaped;
	//}
	//else
	return lowerCiv4TextCodeToXml(result);
}

std::wstring CvTranslator::getText(const std::wstring& textId) const
{
	return getText(textId, {});
}

std::wstring CvTranslator::getText(const std::wstring& textId, std::span<const CvDLLUtilityIFaceBase::TextArg> args) const
{
	if (textId.empty())
		return textId;

	const auto it = textEntries.find(textId);
	if (it == textEntries.end())
		return textId;

	return formatTextcode(it->second.text, args);
}

namespace
{
	struct TranslatorInfo
	{
		CvWString szTagStartIcon;
		CvWString szTagStartOur;
		CvWString szTagStartCT;
		CvWString szTagStartColour;
		CvWString szTagStartLink;
		CvWString szTagEndLink;
		CvWString szEndLinkReplacement;
		std::map<std::wstring, CvWString> aIconMap;
		std::map<std::wstring, CvWString> aColourMap;
	};

	static TranslatorInfo kInfo{};

	const TranslatorInfo& getTranslatorInfo()
	{
		return kInfo;
	}
}


void CvTranslator::initialiseTextCodeTags()
{
	kInfo = {};
	CvDllTranslator::initializeTags(
		kInfo.szTagStartIcon, kInfo.szTagStartOur, kInfo.szTagStartCT, kInfo.szTagStartColour, kInfo.szTagStartLink,
		kInfo.szTagEndLink, kInfo.szEndLinkReplacement, kInfo.aIconMap, kInfo.aColourMap
	);
}

std::wstring CvTranslator::lowerCiv4TextCodeToXml(std::wstring_view textcode)
{
	// This can be called during initialisation before `CvDllTranslator::initializeTags` can work.
	// At least for keybindings.
	// In such cases, check that there's no tags.
	//if (CvApp::getInstance().symbols.empty())
	//{
	//	assert(!textcode.contains(L'['));
	//	return std::wstring(textcode);
	//}
	// CvApp not in this common lib for now.

	const TranslatorInfo& kTranslatorInfo = getTranslatorInfo();

	// TODO: Where the heck does this value come from. It might be stored in string form somewhere.
	constexpr int uiForm = 0;

	std::wstring xml;

	CvWString replacement;

	while (textcode.size())
	{
		const size_t textSpan = std::min(textcode.find_first_of(L"["), textcode.size());
		xml += textcode.substr(0, textSpan);
		textcode.remove_prefix(textSpan);

		if (textcode.empty())
			break;

		// Escape to XML.
		//switch (textcode.front())
		//{
		//case L'<': xml += L"&lt;"; textcode.remove_prefix(1); continue;
		//case L'>': xml += L"&gt;"; textcode.remove_prefix(1); continue;
		//case L'&': xml += L"&amp;"; textcode.remove_prefix(1); continue;
		//default:
		//	break;
		//}

		const size_t tagSpan = textcode.find(L']');

		if (tagSpan == textcode.npos)
		{
			// Could not find the end bracket. Just dump the rest of the string.
			xml += textcode;
			break;
		}

		const CvWString tagWithRBracket(textcode.substr(0, tagSpan + 1));
		const CvWString tag(textcode.substr(0, tagSpan));

		if (tag == L"[SPACE")
			xml += L' ';
		else if (tag == L"[NEWLINE")
			xml += L'\n';
		else if (tag == L"[TAB")
			xml += L"    ";
		else if (tag.starts_with(kTranslatorInfo.szTagStartIcon))
		{
			// Auto-insert a space before icons. We're doing the same in CvTranslator.
			if (xml.size() && !std::iswspace(xml.back()))
				xml += L' ';
			xml += kTranslatorInfo.aIconMap.at(tagWithRBracket);
		}
		else if (tag.starts_with(kTranslatorInfo.szTagStartOur))
		{
			if (CvDllTranslator::replaceOur(tag, uiForm, replacement))
				xml += replacement;
			else
				xml += tag;
		}
		else if (tag.starts_with(kTranslatorInfo.szTagStartCT))
		{
			if (CvDllTranslator::replaceCt(tag, uiForm, replacement))
				xml += replacement;
			else
				xml += tag;
		}
		else if (tag.starts_with(kTranslatorInfo.szTagStartColour))
			xml += kTranslatorInfo.aColourMap.at(tagWithRBracket);
		// Cv4MiniEngine extension: Background colour
		else if (tag.starts_with(L"[BG:COLOR_"))
		{
			// Modify the returned XML.
			std::wstring xmlPiece = kTranslatorInfo.aColourMap.at(L'[' + tagWithRBracket.substr(4));
			if (xmlPiece.starts_with(L"<color"))
				xmlPiece.insert(1, L"bg");
			else if (xmlPiece.starts_with(L"</color"))
				xmlPiece.insert(2, L"bg");
			else
				std::abort();
			xml += xmlPiece;
		}
		else if (tag.starts_with(kTranslatorInfo.szTagStartLink))
		{
			const std::wstring_view linkContents = std::wstring_view(tag).substr(kTranslatorInfo.szTagStartLink.size());

			if (linkContents.size() && linkContents.front() == '=')
			{
				xml += L"<link='";
				xml += linkContents.substr(1);
				xml += L"'>";
			}
			else
				xml += tagWithRBracket;
		}
		else if (tag == kTranslatorInfo.szTagEndLink)
			xml += kTranslatorInfo.szEndLinkReplacement;
		else if (tag == L"[BOLD")
			xml += L"<b>";
		else if (tag == L"[\\BOLD")
			xml += L"</b>";
		else if (tag.starts_with(L"[PARAGRAPH:"))
			xml += L"\n\n";
		else
			xml += tagWithRBracket;

		textcode.remove_prefix(tagSpan + 1);
	}

	return xml;
}