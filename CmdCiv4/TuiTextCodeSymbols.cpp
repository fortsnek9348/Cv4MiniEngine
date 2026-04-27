#include "TuiTextCode.h"

#include <Cv4CommonEngineLib/CvTranslator.h>

#include <CvGameCoreDLL/CvGlobals.h>
#include <CvGameCoreDLL/CvInfos.h>
#include <CvGameCoreDLL/CvDLLUtilityIFaceBase.h>

#include <CommonStuff/range.h>

#include <iostream>
#include <unordered_map>

using heck::range;

static std::wstring changeTextColours(std::wstring_view str, const char* fgColour, const char* bgColour = nullptr)
{
	const ColorTypes fgColourI = fgColour ? (ColorTypes)gGlobals.getInfoTypeForString(fgColour) : NO_COLOR;
	const ColorTypes bgColourI = bgColour ? (ColorTypes)gGlobals.getInfoTypeForString(bgColour) : NO_COLOR;

	return CvTranslator::changeBackColor(CvTranslator::changeTextColor(std::wstring(str), fgColourI), bgColourI);
}



static std::unordered_map<int, std::wstring> buildSymbolLookup()
{
	std::unordered_map<int, std::wstring> m;
	for (const auto i : range<YieldTypes>(3))
		m.emplace(gGlobals.getYieldInfo(i).getChar(), gGlobals.getYieldInfo(i).getDescription());

	m[gGlobals.getYieldInfo(YIELD_FOOD).getChar()]       /**/ = changeTextColours(L"ƒ", "COLOR_YELLOW", "COLOR_PLAYER_DARK_GREEN"); // ꜡
	m[gGlobals.getYieldInfo(YIELD_PRODUCTION).getChar()] /**/ = changeTextColours(L"ħ", "COLOR_MENU_BLUE", "COLOR_DARK_GREY");
	m[gGlobals.getYieldInfo(YIELD_COMMERCE).getChar()]   /**/ = changeTextColours(L"●", "COLOR_YELLOW", "COLOR_PLAYER_BLACK"); // ç©

	for (const auto i : range<CommerceTypes>(4))
		m.emplace(gGlobals.getCommerceInfo(i).getChar(), gGlobals.getCommerceInfo(i).getDescription());

	m[gGlobals.getCommerceInfo(COMMERCE_CULTURE).getChar()]     /**/ = changeTextColours(L"♫♪", "COLOR_MAGENTA", "COLOR_PLAYER_BLACK");
	m[gGlobals.getCommerceInfo(COMMERCE_GOLD).getChar()]        /**/ = changeTextColours(L"§", "COLOR_YIELD_COMMERCE", "COLOR_PLAYER_BLACK");
	m[gGlobals.getCommerceInfo(COMMERCE_RESEARCH).getChar()]    /**/ = changeTextColours(L"β∑", "COLOR_RESEARCH_RATE", "COLOR_PLAYER_DARK_BLUE"); // ελ
	m[gGlobals.getCommerceInfo(COMMERCE_ESPIONAGE).getChar()]   /**/ = changeTextColours(L"ξϨ", "COLOR_RED", "COLOR_PLAYER_BLACK"); // Ѯ
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

	auto& utility = *gGlobals.getDLLIFaceNonInl();
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
	m[utility.getSymbolID(BAD_FOOD_CHAR      /**/)] = L"Bad Food";
	m[utility.getSymbolID(EATEN_FOOD_CHAR    /**/)] = L"Eaten";
	m[utility.getSymbolID(GOLDEN_AGE_CHAR    /**/)] = L"Golden Age";
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

std::wstring cvengine::lookupSymbolChar(wchar_t c)
{
	assert(c >= CvTranslator::getInstance().firstSymbolCode);
	static const auto kSymbolLookup = buildSymbolLookup();
	static const int kMaxSymbol = [] {
		const int m = std::ranges::max(kSymbolLookup | std::views::keys);
		std::clog << std::format("CvTranslator symbol lookup uses unicode range [{:04x}..{:04x}].", static_cast<int>(CvTranslator::getInstance().firstSymbolCode), m) << std::endl;
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