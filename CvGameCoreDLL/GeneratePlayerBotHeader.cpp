#include "GeneratePlayerBotHeader.h"

#include <CommonStuff/range.h>
#include <CommonStuff/StringConversion.h>

#include "CvInfos.h"
#include "CvGlobals.h"
#include "CvDLLUtilityIFaceBase.h"

#include <fstream>
#include <iomanip>

using heck::range;

namespace
{
	std::string_view decideEnumType(int maxValue)
	{
		if (maxValue <= INT8_MAX)
			return "std::int8_t";
		else
			return "std::int16_t";
	}

	struct Generator
	{
		std::ostringstream fwds;
		std::ostringstream aliases;
		std::ostringstream defs;
		std::ostringstream stringDecl;
		std::ostringstream stringDefs;

		std::string makeEnumValueName(std::string_view prefix, std::string name)
		{
			if (!name.starts_with(prefix))
				throw std::runtime_error("Enum member name does not start with expected prefix.");

			name = std::move(name).substr(prefix.size());

			for (const size_t j : range(size_t(1), name.size()))
				if (name[j - 1] != '_')
					name[j] = heck::toAsciiLower(name[j]);
			std::erase(name, '_');

			return name;
		}

		template<class T, class E>
		void emitEnum(const char* name, std::string_view prefix, T& (CvGlobals::*getInfo)(E), int n)
		{
			defs << "\tnamespace " << name << "\n";
			defs << "\t{\n";
			defs << "\t\tenum Enum : " << decideEnumType(n) << '\n';
			defs << "\t\t{\n";

			if (prefix != "CULTURELEVEL")
				defs << "\t\t\tNone = -1,\n";

			for (const int i : range(n))
			{
				const CvInfoBase& info = (gGlobals.*getInfo)(static_cast<E>(i));
				defs << "\t\t\t" << makeEnumValueName(prefix, info.getType()) << ",\n";
			}

			defs << "\t\t\tNum,\n";

			defs << "\t\t};\n";
			defs << "\t};\n";
			defs << "\t\n";

			fwds << "\tnamespace " << name << " { enum Enum : " << decideEnumType(n) << "; }\n";
			aliases << "\tusing E" << name << " = enums::" << name << "::Enum;\n";

			stringDecl << "\textern const std::array<const char*, E" << name << "::Num> k" << name << "Names;\n";
			stringDefs << "constexpr std::array<const char*, E" << name << "::Num> cvbot::k" << name << "Names{{\n";
			for (const int i : range(n))
			{
				const CvInfoBase& info = (gGlobals.*getInfo)(static_cast<E>(i));
				stringDefs << '\t' << std::quoted(makeEnumValueName(prefix, info.getType())) << ",\n";
			}
			stringDefs << "}};\n\n";
		}
	};
}

void cvbot::generatePlayerBotGameBindingHeaders(const std::filesystem::path& dir)
{
	Generator generator;

	generator.emitEnum("Improvement", "IMPROVEMENT_", &CvGlobals::getImprovementInfo, gGlobals.getNumImprovementInfos());
	generator.emitEnum("Feature", "FEATURE", &CvGlobals::getFeatureInfo, gGlobals.getNumFeatureInfos());
	generator.emitEnum("Terrain", "TERRAIN", &CvGlobals::getTerrainInfo, gGlobals.getNumTerrainInfos());
	generator.emitEnum("UnitClass", "UNITCLASS", &CvGlobals::getUnitClassInfo, gGlobals.getNumUnitClassInfos());
	generator.emitEnum("UnitType", "UNIT", &CvGlobals::getUnitInfo, gGlobals.getNumUnitInfos());
	generator.emitEnum("Route", "ROUTE", &CvGlobals::getRouteInfo, gGlobals.getNumRouteInfos());
	generator.emitEnum("Promotion", "PROMOTION", &CvGlobals::getPromotionInfo, gGlobals.getNumPromotionInfos());
	//generator.emitEnum("DiploComment", "", &CvGlobals::getDiplomacyInfo, gGlobals.getNumDiplomacyInfos());
	generator.emitEnum("EventTrigger", "EVENTTRIGGER", &CvGlobals::getEventTriggerInfo, gGlobals.getNumEventTriggerInfos());
	generator.emitEnum("Event", "EVENT", &CvGlobals::getEventInfo, gGlobals.getNumEventInfos());
	generator.emitEnum("BuildingClass", "BUILDINGCLASS", &CvGlobals::getBuildingClassInfo, gGlobals.getNumBuildingClassInfos());
	generator.emitEnum("BuildingType", "BUILDING", &CvGlobals::getBuildingInfo, gGlobals.getNumBuildingInfos());
	generator.emitEnum("Process", "PROCESS", &CvGlobals::getProcessInfo, gGlobals.getNumProcessInfos());
	generator.emitEnum("Bonus", "BONUS", &CvGlobals::getBonusInfo, gGlobals.getNumBonusInfos());
	generator.emitEnum("Specialist", "SPECIALIST", &CvGlobals::getSpecialistInfo, gGlobals.getNumSpecialistInfos());
	generator.emitEnum("Build", "BUILD", &CvGlobals::getBuildInfo, gGlobals.getNumBuildInfos());
	generator.emitEnum("Tech", "TECH", &CvGlobals::getTechInfo, gGlobals.getNumTechInfos());
	generator.emitEnum("CivicOptionType", "CIVICOPTION", &CvGlobals::getCivicOptionInfo, gGlobals.getNumCivicOptionInfos());
	generator.emitEnum("Civic", "CIVIC", &CvGlobals::getCivicInfo, gGlobals.getNumCivicInfos());
	generator.emitEnum("Religion", "RELIGION", &CvGlobals::getReligionInfo, gGlobals.getNumReligionInfos());
	generator.emitEnum("GameSpeed", "GAMESPEED", &CvGlobals::getGameSpeedInfo, gGlobals.getNumGameSpeedInfos());
	generator.emitEnum("WorldSize", "WORLDSIZE", &CvGlobals::getWorldInfo, gGlobals.getNumWorldInfos());
	generator.emitEnum("Handicap", "HANDICAP", &CvGlobals::getHandicapInfo, gGlobals.getNumHandicapInfos());
	generator.emitEnum("CultureLevel", "CULTURELEVEL", &CvGlobals::getCultureLevelInfo, gGlobals.getNumCultureLevelInfos());
	generator.emitEnum("Victory", "VICTORY", &CvGlobals::getVictoryInfo, gGlobals.getNumVictoryInfos());
	generator.emitEnum("Project", "PROJECT", &CvGlobals::getProjectInfo, gGlobals.getNumProjectInfos());
	generator.emitEnum("LeaderTrait", "TRAIT", &CvGlobals::getTraitInfo, gGlobals.getNumTraitInfos());
	

	//generator.emitHardCodedEmum("Yield", std::to_array<std::string_view>({
	//	"Food",
	//	"Production",
	//	"Commerce",
	//	}), NUM_YIELD_TYPES);
	//
	//generator.emitHardCodedEmum("Commerce", std::to_array<std::string_view>({
	//	"Gold",
	//	"Research",
	//	"Culture",
	//	"Espionage",
	//	}), NUM_COMMERCE_TYPES);

	std::ofstream fwdsFile(dir / "inc" / "PlayerBotGameBinding" / "EnumFwd.h");
	fwdsFile << "// This file is generated by Cv4MiniEngine/CvGameCoreDLL.\n";
	fwdsFile << "\n";
	fwdsFile << "#pragma once\n";
	fwdsFile << "\n";
	fwdsFile << "#include <cstdint>\n";
	fwdsFile << "\n";
	fwdsFile << "namespace cvbot::enums\n";
	fwdsFile << "{\n";
	fwdsFile << generator.fwds.view();
	fwdsFile << "}\n";
	fwdsFile << "\n";
	fwdsFile << "namespace cvbot\n";
	fwdsFile << "{\n";
	fwdsFile << generator.aliases.view();
	fwdsFile << "}\n";

	std::ofstream defsFile(dir / "inc" / "PlayerBotGameBinding" / "EnumDefs.h");
	defsFile << "// This file is generated by Cv4MiniEngine/CvGameCoreDLL.\n";
	defsFile << "\n";
	defsFile << "#pragma once\n";
	defsFile << "\n";
	defsFile << "#include \"EnumFwd.h\"\n";
	defsFile << "\n";
	defsFile << "namespace cvbot::enums\n";
	defsFile << "{\n";
	defsFile << generator.defs.view();
	defsFile << "}\n";
	defsFile << "\n";
	defsFile << "namespace cvbot\n";
	defsFile << "{\n";
	defsFile << "\tinline constexpr const wchar_t* kModName = L" << std::quoted(heck::convertWideToUtf8(gGlobals.getDLLIFaceNonInl()->getModName(false))) << ";\n";
	defsFile << "}\n";

	std::ofstream stringDeclFile(dir / "inc" / "PlayerBotGameBinding" / "EnumStrings.h");
	stringDeclFile << "// This file is generated by Cv4MiniEngine/CvGameCoreDLL.\n";
	stringDeclFile << "\n";
	stringDeclFile << "#pragma once\n";
	stringDeclFile << "\n";
	stringDeclFile << "#include \"EnumDefs.h\"\n";
	stringDeclFile << "\n";
	stringDeclFile << "#include <array>\n";
	stringDeclFile << "\n";
	stringDeclFile << "namespace cvbot\n";
	stringDeclFile << "{\n";
	stringDeclFile << generator.stringDecl.view();
	stringDeclFile << "}\n";

	std::ofstream stringDefsFile(dir / "EnumStrings.cpp");
	stringDefsFile << "// This file is generated by Cv4MiniEngine/CvGameCoreDLL.\n";
	stringDefsFile << "#include \"inc/PlayerBotGameBinding/EnumStrings.h\"\n";
	stringDefsFile << "\n";
	stringDefsFile << "using namespace cvbot;\n";
	stringDefsFile << "\n";
	stringDefsFile << generator.stringDefs.view();
}
