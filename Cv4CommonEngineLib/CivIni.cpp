#include "inc/Cv4CommonEngineLib/CivIni.h"
#include "CommonEngineGlobal.h"

#include <CvGlobals.h>

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

constexpr IniDocKey cvengine::kCivilizationIVIniSection_CV4ENGINE{ "CV4MINIENGINE" };
constexpr IniDocKey cvengine::kCivilizationIVIniProp_VanillaCiv4RootDir{ "VanillaCiv4RootDir", "Path to root directory of your Civilization 4 installation." };
constexpr IniDocKey cvengine::kCivilizationIVIniProp_LogOutput{ "LogOutput", "stderr, or OutputDebugString, or file [filename]" };
constexpr IniDocKey cvengine::kCivilizationIVIniProp_EnableRightClickToShiftClickRemap{ "EnableRightClickToShiftClickRemap", "Terminals typically hijack shift left-click, so to get around that, action buttons may remap right-click to shift left-click. This may be disabled for Windows Console." };
constexpr IniDocKey cvengine::kCivilizationIVIniProp_MaxConcurrentSounds{ "MaxConcurrentSounds", "Limit the maximum number of the same sound that can play at the same time." };
constexpr IniDocKey cvengine::kCivilizationIVIniProp_MaxConcurrentSoundTimeMilliseconds{ "MaxConcurrentSoundTimeMilliseconds", "Consider playing offsets less than this to be concurrent." };
constexpr IniDocKey cvengine::kCivilizationIVIniProp_WorldSizeMultiplier{ "WorldSizeMultiplier", "Simply scales up the final size of the map. Also used by startingPlotRange." };

static const std::filesystem::path& getPath()
{
	static const std::filesystem::path kPath = cvengine::gCommonEngineConfig.userConfigDirPath / cvengine::gCommonEngineConfig.iniFilename;
	return kPath;
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