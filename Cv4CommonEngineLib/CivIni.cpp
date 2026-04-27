#include "inc/Cv4CommonEngineLib/CivIni.h"
#include "CommonEngineGlobal.h"

#include <CvGameCoreDLL/CvGlobals.h>

#include <CommonStuff/range.h>

using cvengine::IniData;
using cvengine::IniDocKey;

IniData cvengine::gCivilizationIVIni;

constexpr IniDocKey cvengine::kCivilizationIVIniSection_CONFIG{ "CONFIG" };
constexpr IniDocKey cvengine::kCivilizationIVIniProp_MapRandSeed{ "MapRandSeed", "Random seed for map generation, or '0' for default" };
constexpr IniDocKey cvengine::kCivilizationIVIniProp_SyncRandSeed{ "SyncRandSeed", "Random seed for game sync, or '0' for default" };
constexpr IniDocKey cvengine::kCivilizationIVIniProp_CheatCode{ "CheatCode", "Move along" };

constexpr IniDocKey cvengine::kCivilizationIVIniSection_GAME{ "GAME" };
constexpr IniDocKey cvengine::kCivilizationIVIniProp_GameName{ "GameName", "Game Name" };
constexpr IniDocKey cvengine::kCivilizationIVIniProp_WorldSize{ "WorldSize", "Worldsize options are WORLDSIZE_DUEL/WORLDSIZE_TINY/WORLDSIZE_SMALL/WORLDSIZE_STANDARD/WORLDSIZE_LARGE/WORLDSIZE_HUGE" };
constexpr IniDocKey cvengine::kCivilizationIVIniProp_Climate{ "Climate", "Climate options are CLIMATE_ARID/CLIMATE_TEMPERATE/CLIMATE_TROPICAL" };
constexpr IniDocKey cvengine::kCivilizationIVIniProp_SeaLevel{ "SeaLevel", "Sealevel options are SEALEVEL_LOW/SEALEVEL_MEDIUM/SEALEVEL_HIGH" };
constexpr IniDocKey cvengine::kCivilizationIVIniProp_Era{ "Era", "Era options are ERA_ANCIENT/ERA_CLASSICAL/ERA_MEDIEVAL/ERA_RENAISSANCE/ERA_INDUSTRIAL/ERA_MODERN" };
constexpr IniDocKey cvengine::kCivilizationIVIniProp_GameSpeed{ "GameSpeed", "GameSpeed options are GAMESPEED_QUICK/GAMESPEED_NORMAL/GAMESPEED_EPIC/GAMESPEED_MARATHON" };
constexpr IniDocKey cvengine::kCivilizationIVIniProp_VictoryConditions{ "VictoryConditions", "Victory Conditions" };
constexpr IniDocKey cvengine::kCivilizationIVIniProp_GameOptions{ "GameOptions", "Game Options" };
constexpr IniDocKey cvengine::kCivilizationIVIniProp_Map{ "Map", "Map Script file name" };
constexpr IniDocKey cvengine::kCivilizationIVIniProp_Alias{ "Alias", "In-game Alias" };
constexpr IniDocKey cvengine::kCivilizationIVIniProp_QuickHandicap{ "QuickHandicap", "Handicap for quick play" };

constexpr IniDocKey cvengine::kCivilizationIVIniSection_CVGAMECOREDLL_EXTENSION{ "CVGAMECOREDLL_EXTENSION" };
constexpr IniDocKey cvengine::kCivilizationIVIniProp_WorldSizeMultiplier{ "WorldSizeMultiplier", "Simply scales up the final size of the map. Also used by startingPlotRange." };



static std::filesystem::path getPath()
{
	return cvengine::gCommonEngineConfig.userConfigDirPath / cvengine::gCommonEngineConfig.iniFilename;
}

void cvengine::loadCivIni()
{
	const std::filesystem::path& path = getPath();
	IniData::createIfNew(path);
	gCivilizationIVIni = loadINI(path);
}

void cvengine::saveCivilizationIniIfChanged()
{
	if (gCivilizationIVIni.isChanged())
	{
		saveINI(getPath(), gCivilizationIVIni);
		gCivilizationIVIni.clearChanged();
	}
}

void cvengine::loadEnhancedDLLConfigFromINI()
{
	static constexpr IniDocKey kSection{ "CV4MINIENGINE_PERF_ENHANCED_DLL" };

	EnhancedDLLConfig config{};

	for (const EEnhancedDLLConfigOption i : heck::range(EEnhancedDLLConfigOption::Num))
	{
		const auto info = EnhancedDLLConfig::getOptionInfo(i);
		const IniDocKey key{ info.iniName, info.iniDesc };
		config.options[static_cast<size_t>(i)] = gCivilizationIVIni.grab(kSection, key, 1);
	}

	gGlobals.setEnhancedDLLConfig(config);
}