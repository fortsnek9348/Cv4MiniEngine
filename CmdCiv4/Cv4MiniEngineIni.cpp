#include "Cv4MiniEngineIni.h"

using namespace cvengine;

constexpr IniDocKey cvengine::kCivilizationIVIniSection_CV4ENGINE{ "CV4MINIENGINE" };
constexpr IniDocKey cvengine::kCivilizationIVIniProp_VanillaCiv4RootDir{ "VanillaCiv4RootDir", "Path to root directory of your Civilization 4 installation." };
constexpr IniDocKey cvengine::kCivilizationIVIniProp_LogOutput{ "LogOutput", "stderr, or OutputDebugString, or file [filename]" };
constexpr IniDocKey cvengine::kCivilizationIVIniProp_EnableRightClickToShiftClickRemap{ "EnableRightClickToShiftClickRemap", "Terminals typically hijack shift left-click, so to get around that, action buttons may remap right-click to shift left-click. This may be disabled for Windows Console." };
constexpr IniDocKey cvengine::kCivilizationIVIniProp_MaxConcurrentSounds{ "MaxConcurrentSounds", "Limit the maximum number of the same sound that can play at the same time." };
constexpr IniDocKey cvengine::kCivilizationIVIniProp_MaxConcurrentSoundTimeMilliseconds{ "MaxConcurrentSoundTimeMilliseconds", "Consider playing offsets less than this to be concurrent." };

