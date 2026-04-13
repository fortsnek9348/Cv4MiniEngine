#pragma once

#include "DLLDefs.h"
// Only forward declarations. This makes CvGameCoreDLL independent of how many infos there are.
#include "EnumFwd.h"

#include <bitset>
#include <span>
#include <variant>
#include <vector>

namespace cvbot
{
	struct ivec2
	{
		int x{};
		int y{};

		friend ivec2 operator+(ivec2 a, ivec2 b)
		{
			return { a.x + b.x, a.y + b.y };
		}
	};

	struct i16vec2
	{
		int16_t x{};
		int16_t y{};

		operator ivec2() const
		{
			return { x, y };
		}
	};

	struct iaabb2
	{
		ivec2 min{};
		ivec2 max{}; // exclusive

		static iaabb2 sized(ivec2 min, ivec2 dim)
		{
			return { min, min + dim };
		}

		int area() const
		{
			return (max.x - min.x) * (max.y - min.y);
		}
	};

	struct MapGeometry
	{
		ivec2 dim{};
		bool wrapX{};
		bool wrapY{};

		ivec2 wrap(ivec2 coord) const
		{
			if (wrapX)
			{
				while (coord.x < 0)
					coord.x += dim.x;
				while (coord.x >= dim.x)
					coord.x -= dim.x;
			}
			if (wrapY)
			{
				while (coord.y < 0)
					coord.y += dim.y;
				while (coord.y >= dim.y)
					coord.y -= dim.y;
			}
			return coord;
		}
	};

	struct VisibleDemographics
	{
		int gdp{};
		int power{};
		int food{};
		int prod{};
	};

	struct Unit
	{
		EUnitId id{}; // -1 for other players
		EPlayer owner{}; // Visual owner, so, barbs for rival privateers
		i16vec2 coord{};
		EUnitType type{};
		uint8_t exp{}; // Clamped. 0 for other players.
		uint8_t strength{}; // Rounded, clamped. 0 for non-combat.
		uint8_t maxStrength{};
		uint16_t remMoves{}; // 0 for other players. 0 when that event makes your unit immobile for one turn.
		// Divide by kMoveDenominator to get move "count".
		uint16_t maxMoves{};
		std::bitset<kAPIMaxPromotions> promotions{};
	};

	struct Plot
	{
		EPlotType type = EPlotType::None; // None if not revealed or out of bounds.
		EPlayer owner = kNoPlayer;
		EImprovement improvement{};
		EFeature feature{};
		ETerrain terrain{};
		EBonus bonus{};
		bool isVisible : 1 = false;
		bool hasMyUnits : 1 = false;
		bool hasOtherVisibleUnits : 1 = false; // You can't see enemy spies, etc.
		bool hasRevealedCity : 1 = false;
		bool isRiverside : 1 = false;
		bool isLake : 1 = false;
		std::array<int8_t, EYield::Num> yields{}; // Visible yields
	};

	struct ProductionChoice : std::variant<std::monostate, EUnitType, EBuildingType, EProcess, EProject>
	{
		using std::variant<std::monostate, EUnitType, EBuildingType, EProcess, EProject>::variant;
	};

	struct City
	{
		i16vec2 coord{};
		EPlayer owner{};
		bool isCapital = false;
		bool isGovernmentCenter = false;
		int pop{};
		int defence{};
		int defenceIgnoringBuildings{};
		std::wstring name;

		std::vector<EReligion> religions{};

		// Info in CityBillboard conditioned on CvCity::canBeSelected.
		struct InspectableCityInfo
		{
			std::bitset<kNumCityWorkPlots> workedPlotsMask{};

			// Left
			int tradeRouteCommerce{};
			std::vector<EBuildingType> buildings{};
			int culture{};

			// Middle
			int foodBinLevel = 0;
			int foodBinRate = 0;
			int foodBinMax = 0;
			int prodBinLevel = 0;
			int prodBinRate = 0;
			int prodBinMax = 0;
			ProductionChoice productionChoice;

			// CvCity::happyLevel, CvCity::unhappyLevel
			// Note that we store only the minimal state. No derived variables.
			int extraHappiness{};
			int extraHappyTimer{};
			std::array<int, kMaxPlayers> plotCultureValues{};
			int hurryAngerTimer{};
			int conscriptAngerTimer{};
			int defyResolutionAngerTimer{};
			int espionageUnhappinessCounter{};
			// CvCity::goodHealth, CvCity::badHealth
			int freshWaterHealth{};
			int extraHealth{};
			int espionageUnhealthinessCounter{};

			// Derived. Store these anyway for debug checking.
			int happyiness = 0;
			int healthiness = 0;

			// Right
			std::vector<EBonus> bonuses{}; // Derived. For debug checking.
			//std::array<uint8_t, ESpecialist::Num> specialists{}; // Unneeded.
		};

		std::optional<InspectableCityInfo> optInspectableCityInfo{};
	};

	struct CityBuildChoice : ProductionChoice
	{
		int cost{};
	};

	struct TechState
	{
		bool isInQueue : 1 = false;
		bool isResearched : 1 = false;
		bool canBeFirstToFoundReligion : 1 = false;
		bool canBeFirstToSpawnsGreatPerson : 1 = false;
		bool canBeFirstToGetFreeTech : 1 = false;
		bool canResearch : 1 = false;
		int progress = 0;
		int cost = 0;
	};

	struct CivState
	{
		// nullopt if not flexible
		std::array<std::optional<int>, ECommerce::Num> optSliders{};
		std::optional<ETech> optCurrentResearch{};
		std::optional<EReligion> optStateReligion{};
		std::vector<ECivic> civics{};
		std::vector<TechState> techs{};
	};

	struct TradeList
	{
		int gold{};
		int goldPerTurn{};
		bool openBorders{};
		bool peaceTreaty{};
		bool tempPeace{};
		bool vassalState{};
		bool surrender{};
		bool defensivePact{};
		bool map{};
		std::vector<EBonus> bonuses;
		std::vector<ETech> techs;
		std::vector<ETeam> declareWar;
		std::vector<ETeam> makePeace;
	};

	struct Relation
	{
		EPlayer subject{};
		EPlayer target{};
		bool war : 1 = false;
		bool hasPeaceTreaty : 1 = false;
		bool hasOpenBorders : 1 = false;
		bool hasDefensivePact : 1 = false;
		bool wontTalk : 1 = false; // True for barbarians.
		bool isVassalOf : 1 = false; // Player vassal of relation player.

		EAttitude attitude{}; // Relation attitude with player.
		std::array<int, EAttitudeModifier::Num> attitudeModifiers{};
	};

	struct Player
	{
		struct VisibleResearch
		{
			ETech tech{};
			int turns{};
		};

		struct VisibleDemographics
		{
			int gdp{};
			int power{};
			int food{};
			int prod{};
		};

		ETeam team{};
		std::wstring_view name;
		int score{};

		std::optional<int> optNumCities{}; // If known.
		int numRevealedCities{}; // Like in BUG

		// Red Fist
		// Enhanced from BUG mod in that we report unknown.
		std::optional<bool> optIsWarPreping{};

		std::optional<EReligion> optStateReligion{};

		std::vector<ECivic> civicChoices{}; // [ECivicOptionType]

		std::optional<VisibleResearch> optVisibleResearch{};
		std::optional<VisibleDemographics> optVisibleDemographics{};

		struct TradeAdvisorData
		{
			// Some of these may seem duplicated from available trade, but this lists techs even if tech trading is disabled.
			std::vector<ETech> techsWants{};
			std::vector<ETech> techsWantsButWeCantTrade{};
			std::vector<ETech> techsCanResearch{};
			std::vector<ETech> techsWillTrade{};
			std::vector<ETech> techsWontTrade{};
			std::vector<ETech> techsCantTrade{};

			// If we can't trade, these bonuses aren't listed.
			//std::vector<EBonus> bonusesMayImportFrom{}; // availableImports
			//std::vector<EBonus> bonusesMayExportTo{}; // availableExports

			// Includes bonuses they're unwilling to trade.
			std::vector<EBonus> bonusesHas{};

			// This can be seen even if we can't trade.
			int maxTradeGold{};
		};

		// If trading is allowed, otherwise, empty.
		TradeList availableImports;
		TradeList availableExports;

		TradeAdvisorData tradeAdvisorData;

		int tradeRoutesTotalCommerce{};
		int tradeRoutesCount{};

		// TODO: Separate out deals.
		//int currentDealGold{};
		//std::vector<int> currentDealBonuses{}; // [EBonus]

		int spyPointsOnThem = 0;
		int spyPointsOnMe = 0;

		std::optional<ETeam> optWorstEnemy;
		std::optional<ETeam> optMasterTeam;

		// Only known for human player.
		std::optional<int> optWarWeariness{};

		// Only for revealed players.
		std::vector<Relation> relations{};
	};

	struct GameSetup
	{
		MapGeometry mapGeometry{};
		std::bitset<EGameOption::Num> options{};
		EPlayer activePlayerI{};
		std::wstring modName;
		std::wstring mapScriptName;
		EGameSpeed speed{};
		EWorldSize worldSize{};
		EHandicap handicap{};
	};

	struct GlobalDemographicsStat
	{
		int value{};
		int rivalBest{};
		int rivalAverage{};
		int rivalWorst{};
		int rank{};
	};

	struct GlobalInfo
	{
		enum EGlobalDemographic
		{
			GDP, // abstract
			Prod, // abstract
			Food, // abstract
			Power, // abstract
			Land, // plots
			Pop, // abstract
			ApprovalRate,
			LifeExpectancy,
			//Trade, // Meh... whatever
			Num,
		};

		struct BuiltWonder
		{
			EBuildingClass buildingClass{};
			EPlayer optPlayer{};
			bool inProgress = false;
		};

		std::vector<BuiltWonder> builtWonders;

		std::array<GlobalDemographicsStat, EGlobalDemographic::Num> globalDemographics{};

		struct ReligionInfo
		{
			EReligion religion{};
			std::optional<ivec2> optRevealedHolyCityCoord{};
		};

		std::vector<ReligionInfo> foundedReligions{};

		bool hasCircumnavigated = false;

		// TODO: Handle turn message?
		//EPlayer optCircumnavigationPlayer{}; // Can be None.
	};
}