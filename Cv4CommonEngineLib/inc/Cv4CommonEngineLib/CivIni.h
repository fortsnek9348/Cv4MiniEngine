#pragma once

#include "IniReader.h"

namespace cvengine
{
	extern IniData gCivilizationIVIni;
	extern const IniDocKey kCivilizationIVIniSection_CONFIG;
	extern const IniDocKey kCivilizationIVIniProp_MapRandSeed;
	extern const IniDocKey kCivilizationIVIniProp_SyncRandSeed;
	extern const IniDocKey kCivilizationIVIniProp_CheatCode;

	extern const IniDocKey kCivilizationIVIniSection_GAME;
	extern const IniDocKey kCivilizationIVIniProp_GameName;
	extern const IniDocKey kCivilizationIVIniProp_WorldSize;
	extern const IniDocKey kCivilizationIVIniProp_Climate;
	extern const IniDocKey kCivilizationIVIniProp_SeaLevel;
	extern const IniDocKey kCivilizationIVIniProp_Era;
	extern const IniDocKey kCivilizationIVIniProp_GameSpeed;
	extern const IniDocKey kCivilizationIVIniProp_VictoryConditions;
	extern const IniDocKey kCivilizationIVIniProp_GameOptions;
	extern const IniDocKey kCivilizationIVIniProp_Map;
	extern const IniDocKey kCivilizationIVIniProp_Alias;
	extern const IniDocKey kCivilizationIVIniProp_QuickHandicap;

	extern const IniDocKey kCivilizationIVIniSection_CVGAMECOREDLL_EXTENSION;
	extern const IniDocKey kCivilizationIVIniProp_WorldSizeMultiplier;

	void loadCivIni();

	void saveCivilizationIniIfChanged();

	void loadEnhancedDLLConfigFromINI();
}