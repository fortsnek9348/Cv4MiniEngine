#pragma once

#include "Common.h"
#include "DLLDefs.h"
// Only forward declarations. This makes CvGameCoreDLL independent of how many infos there are.
#include "EnumFwd.h"

#include <bitset>
#include <span>
#include <variant>
#include <vector>
#include <optional>

namespace cvbot
{
	struct MapGeometry
	{
		ivec2 dim{};
		bool isWrapX{};
		bool isWrapY{};

		int wrapX(int x) const
		{
			if (isWrapX)
			{
				while (x < 0)
					x += dim.x;
				while (x >= dim.x)
					x -= dim.x;
			}
			return x;
		}

		int wrapY(int y) const
		{
			if (isWrapY)
			{
				while (y < 0)
					y += dim.y;
				while (y >= dim.y)
					y -= dim.y;
			}
			return y;
		}

		ivec2 wrap(ivec2 coord) const
		{
			return { wrapX(coord.x), wrapY(coord.y) };
		}

		std::optional<ivec2> resolve(ivec2 coord) const
		{
			coord.x = wrapX(coord.x);
			coord.y = wrapY(coord.y);
			if (!isWrapX && unsigned(coord.x) >= unsigned(dim.x))
				return std::nullopt;
			if (!isWrapY && unsigned(coord.y) >= unsigned(dim.y))
				return std::nullopt;
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
		//EUnitType type{};
		// Storing the class may be more useful to bots.
		EUnitClass klass{};
		uint8_t exp{}; // Clamped. 0 for other players.
		uint8_t strength{}; // Rounded, clamped. 0 for non-combat.
		uint8_t maxStrength{};
		uint16_t remMoves{}; // 0 for other players. 0 when that event makes your unit immobile for one turn.
		// Divide by kMoveDenominator to get move "count".
		uint16_t maxMoves{};
		uint8_t numFortifyTurns{};
		std::bitset<kAPIMaxPromotions> promotions{};
	};

	struct Plot
	{
		EPlotType type = EPlotType::None; // None if not revealed or out of bounds.
		EPlayer owner = kNoPlayer;
		EImprovement improvement{};
		ERoute route{};
		EFeature feature{};
		ETerrain terrain{};
		EBonus bonus{};
		bool isVisible : 1 = false;
		bool hasMyUnits : 1 = false;
		bool hasOtherVisibleUnits : 1 = false; // You can't see rival spies, etc.
		bool hasEnemyUnit : 1 = false;
		bool hasRevealedCity : 1 = false;
		bool isRiverside : 1 = false;
		bool isLake : 1 = false;
		bool isCoastalWater : 1 = false; // Including lakes
		bool isCoastalLand : 1 = false; // Including lakes
		std::array<int8_t, EYield::Num> yields{}; // Visible yields
	};

	//struct ProductionChoice : std::variant<std::monostate, EUnitType, EBuildingType, EProcess, EProject>
	struct ProductionChoice : std::variant<std::monostate, EUnitClass, EBuildingClass, EProcess, EProject>
	{
		using std::variant<std::monostate, EUnitClass, EBuildingClass, EProcess, EProject>::variant;
		friend bool operator==(ProductionChoice, ProductionChoice) = default;
	};

	struct City
	{
		// TODO: Knowing the coordinate lets you know where you are in the world before Calendar. Before circumnavigation, maybe coordinates should be capital-relative?
		i16vec2 coord{};
		EPlayer owner{};
		bool isCapital = false;
		bool isGovernmentCenter = false;
		// Coastal enough to build a lighthouse.
		bool isCoastal = false;
		bool isOccupation = false;
		bool isPowered = false;
		int pop{};
		int defence{};
		int defenceIgnoringBuildings{};
		
		std::wstring name;

		std::vector<EReligion> religions{};

		// Info in CityBillboard conditioned on CvCity::canBeSelected.
		struct InspectableCityInfo
		{
			std::bitset<kNumCityWorkPlots> currentPlotsMask{}; // Plots the city is currently working
			std::bitset<kNumCityWorkPlots> workablePlotsMask{}; // Plots that are both assigned and workable by the city
			std::bitset<kNumCityWorkPlots> assignedPlotsMask{}; // Plots that have been assigned to this city

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

			// Data necessary to calculate empire commerce output given sliders.
			std::array<int, EYield::Num> yieldRates{};
			std::array<int, ECommerce::Num> commerceRates{};
			int baseCommerceRateTimes100WithZeroSlider{};
			std::array<int, ECommerce::Num> productionToCommerceModifiers{};
			std::array<int, ECommerce::Num> commerceRateModifiers{};

			// CvCity::happyLevel, CvCity::unhappyLevel
			int extraHappiness{};
			int extraHappyTimer{};
			std::array<int, kMaxPlayers> plotCultureValues{};

			uint8_t maintainence{}; // clamped, rounded
			
			// CvGameTextMgr::setAngerHelp

			// Anger here was scaled by population: iUnhappiness = ((iAngerPercent * (getPopulation() + iExtra)) / GC.getPERCENT_ANGER_DIVISOR());
			enum PercentAngerContribution
			{
				Overcrowding,
				MilitaryProtection,
				Occupied,
				Religion,
				Oppression,
				Draft,
				DefyResolution,
				WarWeariness,
				Emancipation,
				Num,
			};

			std::array<int8_t, Num> percentAngerContributions{};
			int8_t defyResolutionAngerTimer{};
			int8_t vassalUnhappiness = 0;
			int8_t espionageUnhappinessCounter{};
			int hurryAngerTimer{};
			int conscriptAngerTimer{};

			// CvCity::goodHealth, CvCity::badHealth
			int freshWaterHealth{};
			int extraHealth{};
			int espionageUnhealthinessCounter{};

			// Derived. Store these anyway for debug checking.
			int happiness = 0;
			int healthiness = 0;

			// Right
			std::vector<EBonus> bonuses{}; // Derived. For debug checking.
			//std::array<uint8_t, ESpecialist::Num> specialists{}; // Unneeded.
		};

		std::optional<InspectableCityInfo> optInspectableCityInfo{};
	};

	struct CityBuildChoice : ProductionChoice
	{
		int progress{};
		int cost{};
	};

	struct GlobalInfoData;

	// Never empty. Use `std::optional` for possibly empty intervals.
	struct [[nodiscard]] Interval
	{
		int min{};
		// Inclusive.
		int imax{};

		Interval() = default;
		constexpr Interval(int x) : min(x), imax(x) {}
		constexpr Interval(int min, int imax) : min(min), imax(imax) {}

		int center() const
		{
			return (min + imax + 1) / 2;
		}

		bool contains(int x) const
		{
			return min <= x && x <= imax;
		}

		std::optional<Interval> optional() const
		{
			if (imax < min)
				return std::nullopt;
			else
				return *this;
		}

		Interval& operator+=(Interval b)
		{
			return *this = *this + b;
		}

		Interval& operator-=(Interval b)
		{
			return *this = *this - b;
		}

		friend Interval operator+(Interval a, Interval b)
		{
			return { a.min + b.min, a.imax + b.imax };
		}

		friend Interval operator-(Interval a, Interval b)
		{
			return { a.min - b.imax, a.imax - b.min };
		}

		friend Interval operator*(Interval a, int b)
		{
			const int x0 = a.min * b;
			const int x3 = a.imax * b;
			return { std::min(x0, x3), std::max(x0, x3) };
		}

		friend Interval operator*(Interval a, Interval b)
		{
			if (a.min >= 0 && b.min >= 0)
				return { a.min * b.min, a.imax * b.imax };
			else
			{
				const int x0 = a.min * b.min;
				const int x1 = a.min * b.imax;
				const int x2 = a.imax * b.min;
				const int x3 = a.imax * b.imax;
				return { std::min({ x0, x1, x2, x3 }), std::max({ x0, x1, x2, x3 }) };
			}
		}

		friend Interval operator/(Interval a, int b)
		{
			const int x0 = a.min / b;
			const int x3 = a.imax / b;
			return { std::min(x0, x3), std::max(x0, x3) };
		}
	};

	struct TechState
	{
		bool isInQueue = false;
		bool isResearched = false;
		bool hasMissedOutOnFirstToResearch = false;
		bool canResearch = false;
		int progress = 0;
		int cost = 0;

		// If canResearch, then we can see the overflow plus the research rate in the research bar.
		// But they may be truncated in the UI.
		int overflow{};
		int researchRate{};
		bool isOverflowTruncated{};
		bool isResearchRateTruncated{}; // Even if truncated, research rate will be at least the value you'd get if the modifier was +0%.
		// overflow + researchRate = final beaker delta
		// These values may be more accurate then what you can get from the UI (when not truncated), but good luck making a bot where that matters.
		// Also,
		// final research rate = f(sliderOutput, baseOverflow, numPrereqsKnown, numKnownByRivals) // CvPlayer::getResearchTurnsLeftTimes100
		// So if you know baseOverflow == 0, you can guess numKnownByRivals.
		// And if you can bound numKnownByRivals, you can guess baseOverflow.

		// Helpful utility functions.
		// Rivals includes civs only, and yourself.
		static std::optional<Interval> guessBaseOverflow(int overflow, const GlobalInfoData& globalInfoData, int sliderOutput, int numPrereqsKnown, Interval numKnownByMetRivals, int numRivalsAlive);
		// With given research rate after modifier (with no overflow).
		static std::optional<Interval> guessNumKnownByMetRivals(int rate, const GlobalInfoData& globalInfoData, int sliderOutput, int numPrereqsKnown, int numRivalsAlive);
	};

	struct CivState
	{
		EPlayer activePlayerI{};
		ETeam activeTeamI{};
		// nullopt if not flexible
		std::array<std::optional<int>, ECommerce::Num> optSliderPercents{};
		std::optional<ETech> optCurrentResearch{};
		std::optional<EReligion> optStateReligion{};
		std::vector<ECivic> civics{};
		std::vector<TechState> techs{};

		// This is the value used to calculate researchRates.
		int techStatesReferenceSliderOutput{};

		// Total player GPT = city commerce GPT + totalTradeGoldPerTurn - inflatedCosts (CvPlayer::calculateBaseNetGold)
		int totalTradeGoldPerTurn{};
		int inflatedCosts{};
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
		ECivlisation civ{};
		ELeaderhead leader{};

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

			// False if that line of text is shown, which refers to Alphabet, not the game option.
			bool isTechnologyTradingAllowed = false;
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
		int firstTurnOfBarbMilitarySpawns = 0;
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

		struct Project
		{
			struct TeamEntry
			{
				int numBuilt{};
				int numInProgress{};
			};

			std::array<TeamEntry, kMaxTeams> teams{};

			int numBuiltTotal{};
			int numBuiltByUnknownTeams{};
		};

		EPlayer activePlayerI{};
		ETeam activeTeamI{};

		std::vector<BuiltWonder> builtWonders;
		std::vector<Project> projects; // [projectI]

		std::array<GlobalDemographicsStat, EGlobalDemographic::Num> globalDemographics{};

		struct ReligionInfo
		{
			EReligion religion{};
			std::optional<ivec2> optRevealedHolyCityCoord{};
		};

		std::vector<ReligionInfo> foundedReligions{};

		int numAliveCivPlayers = 0;
		int numAliveCivTeams = 0;
		bool hasCircumnavigated = false;

		// TODO: Handle turn message?
		//EPlayer optCircumnavigationPlayer{}; // Can be None.
	};
}