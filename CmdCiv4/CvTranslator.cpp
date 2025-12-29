#include "CvTranslator.h"
#include "Common.h"
#include "DLLInterface/MyCvDLLUtility.h"
#include "TuiTextCode.h"

#include <CvGlobals.h>
#include <CvInfos.h>

#include <CommonStuff/range.h>

#include <unordered_map>
#include <cwctype>
#include <expected>
#include <iostream>

using heck::range;

// This is the value that Civ4 uses. Derived through some method. Who knows what.
// Currently, the range is [2123..2201].
// That is:
//     https://www.compart.com/en/unicode/block/U+2100
//     https://www.compart.com/en/unicode/block/U+2150
//     https://www.compart.com/en/unicode/block/U+2190
//     https://www.compart.com/en/unicode/block/U+2200
// Using the original range is important to display event logs saved by the original engine.
constexpr wchar_t CvTranslator::kFirstSymbolCode = L'\u2123';

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

std::wstring CvTranslator::changeTextColor(const std::wstring& szText, int iColor) const
{
	return changeColour(std::move(szText), iColor, L"color");
}
std::wstring CvTranslator::changeBackColor(const std::wstring& szText, int iColor) const
{
	return changeColour(std::move(szText), iColor, L"bgcolor");
}

static std::wstring changeTextColours(std::wstring_view str, const char* fgColour, const char* bgColour = nullptr)
{
	const ColorTypes fgColourI = fgColour ? (ColorTypes)gGlobals.getInfoTypeForString(fgColour) : NO_COLOR;
	const ColorTypes bgColourI = bgColour ? (ColorTypes)gGlobals.getInfoTypeForString(bgColour) : NO_COLOR;

	return CvTranslator().changeBackColor(CvTranslator().changeTextColor(std::wstring(str), fgColourI), bgColourI);
}

static std::unordered_map<int, std::wstring> buildSymbolLookup()
{
	std::unordered_map<int, std::wstring> m;
	for (const auto i : range<YieldTypes>(3))
		m.emplace(gGlobals.getYieldInfo(i).getChar(), gGlobals.getYieldInfo(i).getDescription());

	m[gGlobals.getYieldInfo(YIELD_FOOD).getChar()]       /**/= changeTextColours(L"ƒ", "COLOR_YELLOW", "COLOR_PLAYER_DARK_GREEN"); // ꜡
	m[gGlobals.getYieldInfo(YIELD_PRODUCTION).getChar()] /**/= changeTextColours(L"ħ", "COLOR_MENU_BLUE", "COLOR_DARK_GREY");
	m[gGlobals.getYieldInfo(YIELD_COMMERCE).getChar()]   /**/= changeTextColours(L"●", "COLOR_YELLOW", "COLOR_PLAYER_BLACK"); // ç©

	for (const auto i : range<CommerceTypes>(4))
		m.emplace(gGlobals.getCommerceInfo(i).getChar(), gGlobals.getCommerceInfo(i).getDescription());
	
	m[gGlobals.getCommerceInfo(COMMERCE_CULTURE).getChar()]     /**/= changeTextColours(L"♫♪", "COLOR_MAGENTA", "COLOR_PLAYER_BLACK");
	m[gGlobals.getCommerceInfo(COMMERCE_GOLD).getChar()]        /**/= changeTextColours(L"§", "COLOR_YIELD_COMMERCE", "COLOR_PLAYER_BLACK");
	m[gGlobals.getCommerceInfo(COMMERCE_RESEARCH).getChar()]    /**/= changeTextColours(L"β∑", "COLOR_RESEARCH_RATE", "COLOR_PLAYER_DARK_BLUE"); // ελ
	m[gGlobals.getCommerceInfo(COMMERCE_ESPIONAGE).getChar()]   /**/= changeTextColours(L"ξϨ", "COLOR_RED", "COLOR_PLAYER_BLACK"); // Ѯ
	//m[gGlobals.getCommerceInfo(COMMERCE_ESPIONAGE).getChar()] = L"<bgcolor=COLOR_PLAYER_DARK_RED><color=COLOR_PLAYER_BLACK>ξϨ</color></bgcolor>"; // Ѯ
	
	for (const auto i : range<ReligionTypes>(gGlobals.getNumReligionInfos()))
	{
		m.emplace(gGlobals.getReligionInfo(i).getChar(), gGlobals.getReligionInfo(i).getDescription());
		//m.emplace(gGlobals.getReligionInfo(i).getHolyCityChar(), std::wstring(gGlobals.getReligionInfo(i).getDescription()) + L"*");
	}

	const auto setReligionChars = [&](const char* name, std::wstring_view str, const char* fgColour, const char* bgColour) {
		auto& info = gGlobals.getReligionInfo((ReligionTypes)gGlobals.getInfoTypeForString(name));
		m[info.getHolyCityChar()] = changeTextColours(std::wstring(str) + L"*", fgColour, bgColour); 
		m[info.getChar()] = changeTextColours(str, fgColour, bgColour);
	};

	setReligionChars("RELIGION_CONFUCIANISM"/**/, L"ἦ", "COLOR_BLACK", "COLOR_WHITE"); // ἆ
	setReligionChars("RELIGION_JUDAISM"     /**/, L"☼", "COLOR_PLAYER_BLUE_TEXT", "COLOR_BLACK"); // COLOR_BLUE
	setReligionChars("RELIGION_CHRISTIANITY"/**/, L"†", "COLOR_PLAYER_BROWN_TEXT", "COLOR_BLACK");
	setReligionChars("RELIGION_ISLAM"       /**/, L"(✶", "COLOR_GREEN", "COLOR_BLACK");
	setReligionChars("RELIGION_HINDUISM"    /**/, L"ӞҨ", "COLOR_PLAYER_BLUE_TEXT", "COLOR_BLACK"); // ὃҩ
	setReligionChars("RELIGION_BUDDHISM"    /**/, L"ẞ", "COLOR_PLAYER_ORANGE_TEXT", "COLOR_BLACK");
	setReligionChars("RELIGION_TAOISM"      /**/, L"●", "COLOR_BLACK", "COLOR_WHITE");

	for (const auto i : range<CorporationTypes>(gGlobals.getNumCorporationInfos()))
	{
		m.emplace(gGlobals.getCorporationInfo(i).getChar(), gGlobals.getCorporationInfo(i).getDescription());
		//m.emplace(gGlobals.getCorporationInfo(i).getHeadquarterChar(), std::wstring(gGlobals.getCorporationInfo(i).getDescription()) + L"*");
	}

	const auto setCorporationChars = [&](const char* name, std::wstring_view str, const char* fgColour, const char* bgColour) {
		auto& info = gGlobals.getCorporationInfo((CorporationTypes)gGlobals.getInfoTypeForString(name));
		m[info.getHeadquarterChar()] = changeTextColours(std::wstring(str) + L"*", fgColour, bgColour);
		m[info.getChar()] = changeTextColours(str, fgColour, bgColour);
	};

	setCorporationChars("CORPORATION_1", L"C", "COLOR_YELLOW"            /**/, "COLOR_PLAYER_DARK_ORANGE"/**/); // Cereal Mills
	setCorporationChars("CORPORATION_2", L"●", "COLOR_WHITE"             /**/, "COLOR_FONT_RED"          /**/); // Sid Sushi //= L"<bgcolor=COLOR_RED><color=COLOR_WHITE>SƧ"; // Sid Sushi
	setCorporationChars("CORPORATION_3", L"e", "COLOR_GREEN"             /**/, "COLOR_PLAYER_ORANGE_TEXT"/**/); // Standard Ethanol
	setCorporationChars("CORPORATION_4", L"▲", "COLOR_PLAYER_ORANGE_TEXT"/**/, "COLOR_YELLOW"            /**/); // Creative Constructions
	setCorporationChars("CORPORATION_5", L"Ѩ", "COLOR_WHITE"             /**/, "COLOR_PLAYER_DARK_CYAN"  /**/); // Mining Inc
	setCorporationChars("CORPORATION_6", L"A", "COLOR_WHITE"             /**/, "COLOR_DARK_GREY"         /**/); // Aluminium Co
	setCorporationChars("CORPORATION_7", L"◊", "COLOR_PLAYER_CYAN"       /**/, "COLOR_PLAYER_WHITE_TEXT" /**/); // Civ Jewels

	for (const auto i : range<BonusTypes>(gGlobals.getNumBonusInfos()))
		m[gGlobals.getBonusInfo(i).getChar()] = gGlobals.getBonusInfo(i).getDescription();

	auto& utility = MyCvDLLUtility::getInstance();
	m[utility.getSymbolID(HAPPY_CHAR         /**/)] = changeTextColours(L":)", "COLOR_YELLOW");
	m[utility.getSymbolID(UNHAPPY_CHAR       /**/)] = changeTextColours(L":(", "COLOR_NEGATIVE_TEXT");
	m[utility.getSymbolID(HEALTHY_CHAR       /**/)] = changeTextColours(L'[' + changeTextColours(L"+", "COLOR_RED") + L']', "COLOR_WHITE"); // <bgcolor=COLOR_WHITE>
	m[utility.getSymbolID(UNHEALTHY_CHAR     /**/)] = changeTextColours(L":P", "COLOR_GREEN");
	m[utility.getSymbolID(BULLET_CHAR        /**/)] = L"• ";
	m[utility.getSymbolID(STRENGTH_CHAR      /**/)] = changeTextColours(L"ϡ", "COLOR_YELLOW");
	m[utility.getSymbolID(MOVES_CHAR         /**/)] = changeTextColours(L"&gt;&gt;", "COLOR_POSITIVE_TEXT");
	m[utility.getSymbolID(RELIGION_CHAR      /**/)] = changeTextColours(L"ȣ", "COLOR_WHITE", "COLOR_PLAYER_DARK_PURPLE"); // †Ψ (__)_(._.)__(_^_)_<(_ _)><m(__)m>m(__)mm(_ _)m
	m[utility.getSymbolID(STAR_CHAR          /**/)] = changeTextColours(L"✶", "COLOR_YIELD_COMMERCE");
	m[utility.getSymbolID(SILVER_STAR_CHAR   /**/)] = L"✶";
	m[utility.getSymbolID(TRADE_CHAR         /**/)] = changeTextColours(L"↔", "COLOR_YELLOW");
	m[utility.getSymbolID(DEFENSE_CHAR       /**/)] = L"Ξ";
	m[utility.getSymbolID(GREAT_PEOPLE_CHAR  /**/)] = L"✶☺✶"; // *☺*
	m[utility.getSymbolID(BAD_GOLD_CHAR      /**/)] = changeTextColours(L"§", "COLOR_NEGATIVE_TEXT");
	m[utility.getSymbolID(BAD_FOOD_CHAR      /**/)] =L"Bad Food";
	m[utility.getSymbolID(EATEN_FOOD_CHAR    /**/)] =L"Eaten";
	m[utility.getSymbolID(GOLDEN_AGE_CHAR    /**/)] =L"Golden Age";
	m[utility.getSymbolID(ANGRY_POP_CHAR     /**/)] = changeTextColours(L"&gt;:[", "COLOR_NEGATIVE_TEXT");
	m[utility.getSymbolID(OPEN_BORDERS_CHAR  /**/)] = changeTextColours(L'↔' + changeTextColours(L"¦", "COLOR_MENU_BLUE") + L'↔', "COLOR_WHITE"); // →
	m[utility.getSymbolID(DEFENSIVE_PACT_CHAR/**/)] = changeTextColours(L"|Ξ♥Ξ|", "COLOR_MAGENTA");
	m[utility.getSymbolID(MAP_CHAR           /**/)] = L"Map";
	m[utility.getSymbolID(OCCUPATION_CHAR    /**/)] = changeTextColours(L"!Ϡ", "COLOR_NEGATIVE_TEXT");
	m[utility.getSymbolID(POWER_CHAR         /**/)] = changeTextColours(L"Ϟ", "COLOR_YELLOW");

	// BUG symbols (see BUG's init.xml)
	int base = gGlobals.getCommerceInfo(COMMERCE_GOLD).getChar() + 25;
	m[base++] = changeTextColours(L"War", "COLOR_NEGATIVE_TEXT");
	m[base++] = changeTextColours(L"Peace", "COLOR_MAGENTA");
	
	base = utility.getSymbolID(POWER_CHAR) + 1;
	m[base++] = L"Citizen";
	m[base++] = L"✶☺Ϡ✶"; // Great General

	// https://en.wikipedia.org/wiki/List_of_emoticons
	base = utility.getSymbolID(POWER_CHAR) + 4;
	m[base++] = changeTextColours(L"&gt;:(", "COLOR_NEGATIVE_TEXT");
	m[base++] = changeTextColours(L":(", "COLOR_NEGATIVE_TEXT");
	m[base++] = L":|";
	m[base++] = changeTextColours(L":)", "COLOR_POSITIVE_TEXT");
	m[base++] = changeTextColours(L":D", "COLOR_YELLOW");

	return m;
}

std::wstring CvTranslator::lookupSymbolChar(wchar_t c) const
{
	assert(c >= kFirstSymbolCode);
	static const auto kSymbolLookup = buildSymbolLookup();
	static const int kMaxSymbol = [] {
		const int m = std::ranges::max(kSymbolLookup | std::views::keys);
		std::clog << std::format("CvTranslator symbol lookup uses unicode range [{:04x}..{:04x}].", static_cast<int>(kFirstSymbolCode), m) << std::endl;
		return m;
		}();
	const auto it = kSymbolLookup.find(c);
	if (it != kSymbolLookup.end())
		return it->second;
	else if (c <= kMaxSymbol)
		return L"{unk sym}";
	else
		return std::wstring(1, c);
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

				result += MyCvDLLUtility::getInstance().getObjectText(s, selection, true);
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



std::wstring CvTranslator::formatText(std::wstring_view text, std::span<const CvDLLUtilityIFaceBase::TextArg> args) const
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
				while (text.size() && isIdentifierChar(text.front()) && text.front() < kFirstSymbolCode)
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
	return cvengine::lowerCiv4TextCodeToXml(result);
}