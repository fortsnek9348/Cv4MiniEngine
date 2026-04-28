#include "Pathing.h"
#include "SettlingAdvisor.h"
#include "MilitaryAdvisor.h"
#include "ResearchAdvisor.h"
#include "DomesticAnalysis.h"
#include "MapUnitLookup.h"
#include "CityBuildingProductionAdvisor.h"
#include "CityProduction.h"

#include <PlayerBotGameBinding/IPlayerBot.h>
#include <PlayerBotGameBinding/IPlayerBotPlugin.h>
#include <PlayerBotGameBinding/IGame.h>
#include <PlayerBotGameBinding/GameStructs.h>
#include <PlayerBotGameBinding/EnumDefs.h>
#include <PlayerBotGameBinding/Infos.h>

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <ostream>
#include <ranges>
#include <random>
#include <cassert>
#include <sstream>
#include <iostream>

using namespace mybot;

// TODO: Detect changes in plot rival culture and prioritise cultural bonuses accordingly.
// TODO: Use great people. And evaluate whether it's better to join or construct academy. And dump artists in border cities.
// TODO: Produce research if we've got plenty of GPT.
// TODO: City production valuation.
// TODO: War.
// TODO: When an AI goes red fist immediately after denying them, prep for war.
// TODO: Pick different civics. Go for emancipation if beneficial.
// TODO: Abstract city simulation.
// TODO: Abstract empire simulation.
// TODO: Abstract game simulation.

static auto filterProj(auto value, auto proj)
{
	return std::views::filter([proj = std::forward<decltype(proj)>(proj), value = std::forward<decltype(value)>(value)](auto&& x) { return std::invoke(proj, x) == value; });
}

class MyBot : public IBot
{
public:
	std::ostream& log;
	const GameSetup setup;
	const GlobalInfoData infos;

	std::minstd_rand rng{ 42 };

	mybot::DynamicArray2D<mybot::MultipleSourceDistanceFieldCell> cityDistanceField;
	mybot::DynamicArray2D<mybot::MultipleSourceDistanceFieldCell> pathLengthField;

	bool hasAnalyses = false;

	mybot::SettlingAdvisor settlingAdvisor;
	mybot::MilitaryKnowledge militaryKnowledge;
	mybot::MilitaryAnalysis militaryAnalysis;
	mybot::ResearchAdvisor researchAdvisor;
	mybot::DomesticAnalysis domesticAnalysis;
	
	explicit MyBot(const cvbot::IBotInit& init)
		: log(init.getLoggingStream())
		, setup(init.getGameSetup())
		, infos(init.buildGlobalInfoData())
	{
	}

	virtual void run(IGame& game) override
	{
		

		const GlobalInfo globalInfo = game.getGlobalInfo();

		const std::vector<Unit> allVisibleUnits = game.getVisibleUnits(iaabb2::sized(ivec2(), setup.mapGeometry.dim));

		const std::vector<std::optional<Player>> players = game.getRevealedPlayers();
		const Player& activePlayer = *players[setup.activePlayerI];

		std::array<std::bitset<kMaxPlayers>, kMaxPlayers> warMatrix{};
		for (size_t i = 0; i < kMaxCivPlayers; ++i)
		{
			if (players[i])
			{
				for (const Relation& relation : players[i]->relations)
				{
					if (relation.war)
						warMatrix[i][relation.target] = true;
				}
			}
			warMatrix[i][kMaxCivPlayers] = true;
		}
		warMatrix[kMaxCivPlayers] = ~warMatrix[kMaxCivPlayers];

		const std::vector<Unit> enemyUnits(std::from_range, allVisibleUnits | std::views::filter([&, this](const Unit& unit) {
			return warMatrix[setup.activePlayerI][unit.owner];
			}));

		const std::vector<City> allRevealedCities = game.getRevealedCities();

		const std::vector<Unit> myUnits = allVisibleUnits | filterProj(setup.activePlayerI, &Unit::owner) | std::ranges::to<std::vector>();
		std::vector<City> myCities = allRevealedCities | filterProj(setup.activePlayerI, &City::owner) | std::ranges::to<std::vector>();

		if (myCities.empty())
			if (const auto it = std::ranges::find(myUnits, EUnitClass::Settler, &Unit::klass); it != myUnits.end())
			{
				// Settle in-place.
				game.startMission(std::array{ it->id }, EMission::Found, -1, -1);
				if (auto optCity = game.getRevealedCityAt(it->coord))
					myCities.push_back(std::move(*optCity));
			}

		std::ranges::stable_partition(myCities, &City::isCapital);

		mybot::DynamicArray2D<Plot> map(setup.mapGeometry.dim);
		game.getPlots({}, map.view());

		mybot::DynamicArray2D<mybot::PathingPlot> pathingMap(map.dim);
		for (int y = 0; y < setup.mapGeometry.dim.y; ++y)
			for (int x = 0; x < setup.mapGeometry.dim.x; ++x)
			{
				const Plot& plot = map[{ x, y }];
				mybot::PathingPlot& pathing = pathingMap[{ x, y }];
				if (plot.type == EPlotType::None)
					pathing.flags |= mybot::PathingPlot::Unrevealed;
				if (plot.hasEnemyUnit)
					pathing.flags |= mybot::PathingPlot::EnemyUnit;
				if (plot.type == EPlotType::Ocean)
					pathing.flags |= mybot::PathingPlot::Water;
				else
					pathing.flags |= mybot::PathingPlot::Land;
				if (plot.type == EPlotType::Peak)
					pathing.flags |= mybot::PathingPlot::Impassible;

				if (!(plot.type == EPlotType::Ocean && (plot.isCoastalWater || plot.owner == setup.activePlayerI)))
					pathing.flags |= mybot::PathingPlot::NoGoForOurCoastalUnits;
				if (!(plot.type == EPlotType::Ocean && (plot.isCoastalWater || plot.owner == kBarbarianPlayer)))
					pathing.flags |= mybot::PathingPlot::NoGoForBarbCoastalUnits;

				if (plot.owner != setup.activePlayerI && plot.owner != kNoPlayer)
					pathing.flags |= mybot::PathingPlot::OwnedByOtherPlayer;

				//NoGoTerritory       /**/ = 1 << 2,
				//NoGoForCoastalUnits /**/ = 1 << 5,
			}


		

		std::vector<ivec2> myCityCoords(std::from_range, myCities | std::views::transform(&City::coord));
		cityDistanceField = mybot::computeMultipleSourceDistanceField({ setup.mapGeometry, pathingMap.view() }, mybot::EDistanceMetric::Step, myCityCoords);
		pathLengthField = mybot::computeMultipleSourcePathLengthField({ setup.mapGeometry, pathingMap.view() },
			static_cast<mybot::PathingPlot::EFlag>(mybot::PathingPlot::Unrevealed | mybot::PathingPlot::Impassible | mybot::PathingPlot::Water),
			INT_MAX,
			myCityCoords
		);

		const CivState civState = game.getCivState();

		
		const int settlingDemand = settlingAdvisor.getSettlerDemand();
		
		const size_t desiredNumWorkers = myCities.size() * 3 / 2;

		ptrdiff_t workerProductionDemand = std::max<ptrdiff_t>(0, desiredNumWorkers
			- std::ranges::count(myUnits, EUnitClass::Worker, &Unit::klass));
		//- std::ranges::count(myCities, EUnitClass::Worker, projGetCityOrder());

		settlingAdvisor.update(setup.mapGeometry, map, pathingMap, myCityCoords, cityDistanceField, game);

		const bool readyToExpand = workerProductionDemand <= 0 && settlingAdvisor.optTarget;

		militaryKnowledge.update(infos, civState.activePlayerI, map.view(), allVisibleUnits, enemyUnits);

		militaryAnalysis = doMilitaryAnalysis(
			civState,
			myUnits,
			enemyUnits,
			readyToExpand ? settlingAdvisor.optTarget : std::nullopt,
			myCities,
			players,
			setup.mapGeometry,
			map.view(),
			pathLengthField,
			civState.techs,
			infos,
			globalInfo,
			game,
			militaryKnowledge
		);

		domesticAnalysis = doDomesticAnalysis(
			setup.activePlayerI,
			myCities,
			setup.mapGeometry,
			map.view(),
			pathingMap.view(),
			pathLengthField.view(),
			allVisibleUnits,
			mybot::MapUnitLookup(myUnits),
			infos
		);

		const std::vector<CityBuildingProductionRecomendation> cityBuildingProductionRecomendations = computeCityBuildingProductionRecomendations(
			game,
			infos,
			civState,
			activePlayer,
			myCities,
			domesticAnalysis.cityAnalyses
		);


		const int settlerDemand = readyToExpand ? settlingDemand : 0;
		//const int extraCityDefenderDemand = settlingDemand - settlerDemand;
		
		ptrdiff_t settlerProductionDemand = std::max<ptrdiff_t>(0, settlerDemand
				- std::ranges::count(myUnits, EUnitClass::Settler, &Unit::klass));
			//- std::ranges::count(myCities, EUnitClass::Settler, projGetCityOrder());

		std::vector<UnitProductionDemand> unitProductionDemands = militaryAnalysis.unitProductionDemands;

		if (settlingAdvisor.optTarget && settlerProductionDemand)
		{
			unitProductionDemands.push_back(UnitProductionDemand{
				.klass = EUnitClass::Settler,
				.target = static_cast<i16vec2>(settlingAdvisor.optTarget.value()),
				.turns = 10,
				.count = static_cast<int>(settlerProductionDemand),
				.urgency = EUnitProductionUrgency::NonCombat,
				});
		}
		
		std::clog << "settlerProductionDemand = " << settlerProductionDemand << '\n';
		std::clog << "workerProductionDemand = " << workerProductionDemand << '\n';

		unitProductionDemands.push_back(UnitProductionDemand{
			.klass = EUnitClass::Worker,
			.target = myCities.front().coord,
			.turns = 10,
			.count = static_cast<int>(workerProductionDemand),
			.urgency = EUnitProductionUrgency::NonCombat,
			});

		assignCityProduction(
			game,
			infos,
			setup.mapGeometry,
			myCities,
			cityBuildingProductionRecomendations,
			unitProductionDemands
		);

		for (const Unit& unit : myUnits)
		{
			switch (unit.klass)
			{
			//case EUnitClass::Scout:
			//case EUnitClass::Warrior:
			//	game.tryAutomate(std::array{ unit.id }, EAutomation::Explore);
			//	break;
			case EUnitClass::Worker:
				game.tryAutomate(std::array{ unit.id }, EAutomation::Build);
				break;
			case EUnitClass::Settler:
				if (settlingAdvisor.optTarget)
				{
					game.startMission(std::array{ unit.id }, EMission::MoveTo, settlingAdvisor.optTarget->x, settlingAdvisor.optTarget->y);
					if (game.getUnitCoord(unit.id) == *settlingAdvisor.optTarget)
						game.startMission(std::array{ unit.id }, EMission::Found, -1, -1);
				}
				break;
			default:
				game.tryAutomate(std::array{ unit.id }, EAutomation::Religion);
				break;
			}
		}

		//const std::vector<std::optional<Player>> players = game.getRevealedPlayers();
		
		const ETech techChoice = researchAdvisor.update(
			civState,
			players,
			map.view(),
			myCities,
			militaryAnalysis.barbsThreatTurn,
			militaryAnalysis.rivalPowerRatioPercent,
			globalInfo,
			infos,
			game
		);

		game.changeResearch(techChoice);

		//if (!civState.optCurrentResearch)
		//{
		//	std::vector<ptrdiff_t> choices = civState.techs | std::views::enumerate
		//		| std::views::filter([](const std::pair<ptrdiff_t, TechState>& x) { return x.second.canResearch && x.first != ETech::DivineRight; })
		//		| std::views::keys | std::ranges::to<std::vector>();
		//
		//	std::ranges::stable_sort(choices, std::less(), [&](ptrdiff_t x) {
		//		return civState.techs[x].cost - civState.techs[x].progress;
		//		});
		//
		//	if (!choices.empty())
		//	{
		//		game.changeResearch(static_cast<ETech>(choices[0]));
		//	}
		//}

		auto civics = civState.civics;
		if (game.canChangeCivicTo(ECivic::HereditaryRule))
			civics[ECivicOptionType::Government] = ECivic::HereditaryRule;
		if (game.canChangeCivicTo(ECivic::Bureaucracy))
			civics[ECivicOptionType::Legal] = ECivic::Bureaucracy;
		if (game.canChangeCivicTo(ECivic::Slavery))
			civics[ECivicOptionType::Labor] = ECivic::Slavery;
		game.tryChangeCivicsTo(civics);


		std::array<int, ECommerce::Num> commerceRatio{};
		commerceRatio[ECommerce::Research] = 1;
		game.adjustSliders(commerceRatio);

		if (game.getSpaceshipChancePercent() >= 100)
			game.launchSpaceship();

		hasAnalyses = true;
	}

	virtual std::string buildPlotDebugString(const IGame&, ivec2 coord) const override
	{
		std::ostringstream ss;
		ss << "Player bot info at [" << coord.x << ", " << coord.y << "]\n";
		if (settlingAdvisor.optTarget)
			ss << "Will settle at [" << settlingAdvisor.optTarget->x << ", " << settlingAdvisor.optTarget->y << "]\n";
		ss << "canBarbsEnterTerritory = " << militaryAnalysis.canBarbsEnterTerritory << '\n';
		if (hasAnalyses)
		{
			if (cityDistanceField.cells.size())
				ss << "cityDistanceField = " << cityDistanceField[coord].distance << '\n';
			ss << "barbsSuppressed = " << domesticAnalysis.barbSuppressionMap[coord] << '\n';
			ss << "barbsPotential = " << domesticAnalysis.barbPotentialSpawnMap[coord] << '\n';
		}
		return std::move(ss).str();
	}

	virtual void handleDiploFirstContact([[maybe_unused]] const IGame& game, [[maybe_unused]] EPlayer themI) override
	{
	}
	virtual bool handleDiploGift([[maybe_unused]] const IGame& game, [[maybe_unused]] EPlayer themI, [[maybe_unused]] const TradeList& fromThem) override
	{
		return true;
	}
	virtual bool handleDiploHelp([[maybe_unused]] const IGame& game, [[maybe_unused]] EPlayer themI, [[maybe_unused]] const TradeList& fromMe) override
	{
		return true;
	}
	virtual bool handleDiploDemandTribute([[maybe_unused]] const IGame& game, [[maybe_unused]] EPlayer themI, [[maybe_unused]] const TradeList& fromMe) override
	{
		return true;
	}
	virtual bool handleDiploReligionPressure([[maybe_unused]] const IGame& game, [[maybe_unused]] EPlayer themI, [[maybe_unused]] EReligion religion) override
	{
		return true;
	}
	virtual bool handleDiploCivicPressure([[maybe_unused]] const IGame& game, [[maybe_unused]] EPlayer themI, [[maybe_unused]] ECivicOptionType civicOptionType, [[maybe_unused]] ECivic civic) override
	{
		return false;
	}
	virtual bool handleDiploJoinWar([[maybe_unused]] const IGame& game, [[maybe_unused]] EPlayer themI, [[maybe_unused]] ETeam targetTeam) override
	{
		return false;
	}
	virtual bool handleDiploStopTrading([[maybe_unused]] const IGame& game, [[maybe_unused]] EPlayer themI, [[maybe_unused]] ETeam targetTeam) override
	{
		return true;
	}
	virtual void handleDiploWarDeclaration(const IGame&, ETeam) override
	{
	}
	
	
	
	virtual EEvent handleRandomEvent([[maybe_unused]] const IGame& game, [[maybe_unused]] EEventTrigger event, [[maybe_unused]] std::optional<i16vec2> optCity, [[maybe_unused]] std::optional<i16vec2> optPlot, [[maybe_unused]] std::span<const EEvent> choices) override
	{
		assert(!choices.empty());
		if (choices.size() <= 1)
			return choices[0];

		static constexpr EEvent kPriority[]{
			// SlaveRevolt
			EEvent::SlaveRevolt2,
			EEvent::SlaveRevolt1,
			EEvent::SlaveRevolt3,
		};
		
		static const std::vector<int> kPriorityMap = [] {
			std::vector<int> map(EEvent::Num, INT_MAX);
			for (int i = 0; i < (int)std::size(kPriority); ++i)
				map[kPriority[i]] = i;
			return map;
			}();

		const EEvent e = std::ranges::min(choices, std::less(), [](EEvent e) { return std::pair(kPriorityMap[e], e); });
		//if (kPriorityMap[e] < 0)
		//	throw std::runtime_error(std::string(__func__) + " unimplemented for event trigger.");
		return e;
	}

	

	virtual ETech handleFreeTechChoice(const IGame&) override
	{
		throw std::runtime_error(std::string(__func__) + " unimplemented.");
	}

	virtual ECityCaptureChoice handleCityCaptureChoice(const IGame&, i16vec2) override
	{
		return ECityCaptureChoice::Keep;
	}
	virtual ECityAcquireChoice handleCityAcquireChoice(const IGame&, i16vec2) override
	{
		return ECityAcquireChoice::Keep;
	}

	virtual void onTurnMessage(UnitKilledTurnMessage) override
	{
	}
	virtual void onTurnMessage(GreatPersonTurnMessage) override
	{
	}

	virtual void onVictory(const IGame&, [[maybe_unused]] ETeam winningTeam, [[maybe_unused]] EVictory victoryType) override
	{
		log << "Victory!" << std::endl;
	}

	virtual void onDefeated(const IGame&) override
	{
	}
};

class MyPlayerBotPlugin : public IPlayerBotPlugin
{
public:
	virtual std::wstring_view getName() const override
	{
		return L"Fortsnek's Test Bot";
	}
	virtual std::wstring_view getModName() const override
	{
		return cvbot::kModName;
	}
	virtual std::wstring_view getAutoStartPlayerName() const override
	{
		return L"FortsneksTestBot";
	}
	virtual std::wstring_view getAutoStartGameName() const override
	{
		return L"FortsneksTestBotGame";
	}
	virtual std::unique_ptr<IBot> createBot(const IBotInit& botInit) const override
	{
		return std::make_unique<MyBot>(botInit);
	}
};

static constexpr MyPlayerBotPlugin kPlayerBotPlugin;

#include <PlayerBotGameBinding/PlayerBotPluginImpl.inl>
