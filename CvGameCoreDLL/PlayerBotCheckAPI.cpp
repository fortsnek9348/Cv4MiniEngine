#include "PlayerBotEnablement.h"

#if ENABLE_PLAYER_BOT

#include "CvGlobals.h"
#include "CvGameAI.h"
#include "PlayerBotUtil.h"

#include <PlayerBotGameBinding/DLLDefs.h>
#include <PlayerBotGameBinding/EnumFwd.h>

using namespace cvbot;

template<class E, class T>
static bool inRangeEnum(T value)
{
	return std::in_range<std::underlying_type_t<E>>(value);
}

template<class E>
static constexpr auto kMaxEnum = std::numeric_limits<std::underlying_type_t<E>>::max();

bool cvbot::checkAPI()
{
	return true
		&& MAX_PLAYERS <= kMaxPlayers
		&& MAX_TEAMS <= kMaxTeams
		&& MAX_CIV_PLAYERS == MAX_PLAYERS - 1
		&& MAX_CIV_TEAMS == MAX_TEAMS - 1
		&& inRangeEnum<EPlayer>(kMaxPlayers - 1)
		&& inRangeEnum<ETeam>(kMaxTeams - 1)
		&& gGlobals.getNumPromotionInfos() <= kAPIMaxPromotions
		&& NUM_PLOT_TYPES == std::to_underlying(EPlotType::Num)
		&& NUM_ACTIVITY_TYPES == std::to_underlying(EActivity::Num)
		&& NUM_AUTOMATE_TYPES == std::to_underlying(EAutomation::Num)
		&& NUM_GAMEOPTION_TYPES == std::to_underlying(EGameOption::Num)
		&& NUM_YIELD_TYPES == std::to_underlying(EYield::Num)
		&& NUM_COMMERCE_TYPES == std::to_underlying(ECommerce::Num)
		&& NUM_ATTITUDE_TYPES == std::to_underlying(EAttitude::Num)
		&& NUM_CITY_PLOTS == kNumCityWorkPlots
		&& gGlobals.getNumImprovementInfos() <= kMaxEnum<EImprovement>
		&& gGlobals.getNumFeatureInfos() <= kMaxEnum<EFeature>
		&& gGlobals.getNumTerrainInfos() <= kMaxEnum<ETerrain>
		&& gGlobals.getNumUnitClassInfos() <= kMaxEnum<EUnitClass>
		&& gGlobals.getNumUnitInfos() <= kMaxEnum<EUnitType>
		&& gGlobals.getNumRouteInfos() <= kMaxEnum<ERoute>
		&& gGlobals.getNumPromotionInfos() <= kMaxEnum<EPromotion>
		//&& gGlobals.getNumDiplomacyInfos() <= kMaxEnum<EDiploComment>
		&& gGlobals.getNumEventTriggerInfos() <= kMaxEnum<EEventTrigger>
		&& gGlobals.getNumEventInfos() <= kMaxEnum<EEvent>
		&& gGlobals.getNumBuildingClassInfos() <= kMaxEnum<EBuildingClass>
		&& gGlobals.getNumBuildingInfos() <= kMaxEnum<EBuildingType>
		&& gGlobals.getNumProcessInfos() <= kMaxEnum<EProcess>
		&& gGlobals.getNumBonusInfos() <= kMaxEnum<EBonus>
		&& gGlobals.getNumSpecialistInfos() <= kMaxEnum<ESpecialist>
		&& gGlobals.getNumBuildInfos() <= kMaxEnum<EBuild>
		&& gGlobals.getNumTechInfos() <= kMaxEnum<ETech>
		&& gGlobals.getNumCivicOptionInfos() <= kMaxEnum<ECivicOptionType>
		&& gGlobals.getNumCivicInfos() <= kMaxEnum<ECivic>
		&& gGlobals.getNumReligionInfos() <= kMaxEnum<EReligion>
		&& gGlobals.getNumGameSpeedInfos() <= kMaxEnum<EGameSpeed>
		&& gGlobals.getNumWorldInfos() <= kMaxEnum<EWorldSize>
		&& gGlobals.getNumHandicapInfos() <= kMaxEnum<EHandicap>
		&& gGlobals.getNumCultureLevelInfos() <= kMaxEnum<ECultureLevel>
		&& gGlobals.getNumVictoryInfos() <= kMaxEnum<EVictory>
		&& gGlobals.getNumProjectInfos() <= kMaxEnum<EProject>
		;
}

#endif