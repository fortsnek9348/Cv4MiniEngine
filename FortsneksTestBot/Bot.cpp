#include "Pathing.h"
#include "SettlingAdvisor.h"
#include "MilitaryAdvisor.h"

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

using namespace cvbot;

static constexpr EBuildingClass kCityBuildingOrder[]{
	EBuildingClass::Granary,
	EBuildingClass::Lighthouse,
	EBuildingClass::Barracks,
	EBuildingClass::Library,
	EBuildingClass::Forge,
	EBuildingClass::Courthouse,
};

static auto filterProj(auto value, auto proj)
{
	return std::views::filter([proj = std::forward<decltype(proj)>(proj), value = std::forward<decltype(value)>(value)](auto&& x) { return std::invoke(proj, x) == value; });
}

static auto projGetCityOrder()
{
	return [](const City& city) -> ProductionChoice {
		if (city.optInspectableCityInfo)
			return city.optInspectableCityInfo->productionChoice;
		else
			return std::monostate();
		};
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

	mybot::SettlingAdvisor settlingAdvisor;
	mybot::MilitaryAdvisor militaryAdvisor;
	
	explicit MyBot(const IBotInit& init)
		: log(init.getLoggingStream())
		, setup(init.getGameSetup())
		, infos(init.buildGlobalInfoData())
	{
	}

	virtual void run(IGame& game) override
	{
		const cvbot::GlobalInfo globalInfo = game.getGlobalInfo();

		const std::vector<Unit> allVisibleUnits = game.getVisibleUnits(iaabb2::sized(ivec2(), setup.mapGeometry.dim));

		const std::vector<std::optional<cvbot::Player>> players = game.getRevealedPlayers();

		std::array<std::bitset<kMaxPlayers>, kMaxPlayers> warMatrix{};
		for (size_t i = 0; i < kMaxCivPlayers; ++i)
		{
			if (players[i])
			{
				for (const cvbot::Relation& relation : players[i]->relations)
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

		ptrdiff_t workerProductionDemand = desiredNumWorkers
			- std::ranges::count(myUnits, EUnitClass::Worker, &Unit::klass);
		//- std::ranges::count(myCities, EUnitClass::Worker, projGetCityOrder());

		settlingAdvisor.update(setup.mapGeometry, map, pathLengthField, cityDistanceField, game);

		const bool readyToExpand = workerProductionDemand <= 0 && settlingAdvisor.optTarget;

		militaryAdvisor.update(
			myUnits,
			enemyUnits,
			readyToExpand ? settlingAdvisor.optTarget : std::nullopt,
			myCities,
			setup.mapGeometry,
			map.view(),
			pathLengthField,
			civState.techs,
			infos,
			globalInfo,
			game
		);
		

		

		const int settlerDemand = !readyToExpand ? 0 : settlingDemand;
		//const int extraCityDefenderDemand = settlingDemand - settlerDemand;

		ptrdiff_t settlerProductionDemand = settlerDemand
				- std::ranges::count(myUnits, EUnitClass::Settler, &Unit::klass);
			//- std::ranges::count(myCities, EUnitClass::Settler, projGetCityOrder());

		//const size_t numDefendersInProduction = std::ranges::count_if(myCities, [this](ProductionChoice order) {
		//	if (const EUnitClass* const klass = std::get_if<EUnitClass>(&order))
		//		return infos.units[infos.unitClasses[*klass].activeType].cityDefenceModifier > 0 || *klass == EUnitClass::Warrior;
		//	else
		//		return false;
		//	}, projGetCityOrder());

		//const size_t numMilitaryUnitsInProduction = std::ranges::count_if(myCities, [this](ProductionChoice order) {
		//	if (const EUnitClass* const klass = std::get_if<EUnitClass>(&order))
		//		return infos.units[infos.unitClasses[*klass].activeType].canAttack;
		//	else
		//		return false;
		//	}, projGetCityOrder());
		const size_t numMilitaryUnitsInProduction = 0;

		const size_t militaryAdvisorProductionDemand = militaryAdvisor.cityDefenceDemand + militaryAdvisor.antiBarbDemand;

		ptrdiff_t cityDefenceDemand{}; // = militaryAdvisor.cityDefenceDemand;
		ptrdiff_t antiBarbDemand{}; // = militaryAdvisor.antiBarbDemand;

		if (numMilitaryUnitsInProduction >= militaryAdvisorProductionDemand)
			;
		else if (numMilitaryUnitsInProduction >= militaryAdvisor.cityDefenceDemand)
		{
			cityDefenceDemand = 0;
			antiBarbDemand = militaryAdvisorProductionDemand - numMilitaryUnitsInProduction;
		}
		else
		{
			cityDefenceDemand = militaryAdvisor.cityDefenceDemand - numMilitaryUnitsInProduction;
			antiBarbDemand = militaryAdvisor.antiBarbDemand;
		}

		enum class EProductionPriority
		{
			None,
			NothingBetterToDo,
			Buildings,
			Worker,
			Settler,
			AntiBarb,
			Defender,
		};

		for (const City& city : myCities)
		{
			//if (std::holds_alternative<std::monostate>(city.optInspectableCityInfo->productionChoice))
			{
				EProductionPriority bestPriority = EProductionPriority::None;

				const auto tryProduce = [&](ProductionChoice choice, EProductionPriority priority) {
					if (priority > bestPriority && game.tryChangeProduction(city.coord, choice))
					{
						bestPriority = priority;
						return true;
					}
					else if (priority == bestPriority && choice == city.optInspectableCityInfo->productionChoice)
					{
						game.tryChangeProduction(city.coord, choice);
						return true;
					}
					else
						return false;
					};

				//if (!map[city.coord].hasMyUnits)
				if (cityDefenceDemand > 0)
				{
					cityDefenceDemand -= tryProduce(EUnitClass::Warrior, EProductionPriority::Defender);
					cityDefenceDemand -= tryProduce(EUnitClass::Archer, EProductionPriority::Defender);
					cityDefenceDemand -= tryProduce(EUnitClass::Longbowman, EProductionPriority::Defender);
				}

				if (antiBarbDemand > 0)
				{
					antiBarbDemand -= tryProduce(EUnitClass::Chariot, EProductionPriority::AntiBarb);
					antiBarbDemand -= tryProduce(EUnitClass::Axeman, EProductionPriority::AntiBarb);
					antiBarbDemand -= tryProduce(EUnitClass::Archer, EProductionPriority::AntiBarb);
					antiBarbDemand -= tryProduce(EUnitClass::Warrior, EProductionPriority::AntiBarb);
				}

				if (settlerProductionDemand > 0 && city.optInspectableCityInfo->happiness <= 0)
					settlerProductionDemand -= tryProduce(EUnitClass::Settler, EProductionPriority::Settler);

				if (workerProductionDemand > 0)
					workerProductionDemand -= tryProduce(EUnitClass::Worker, EProductionPriority::Worker);

				for (const auto x : kCityBuildingOrder)
					if (tryProduce(x, EProductionPriority::Buildings))
						break;

				if (bestPriority == EProductionPriority::None)
				{
					const auto choices = game.getCityProductionChoices(city.coord);

					const auto buildings = choices | std::views::filter([](const ProductionChoice& c) {
						return std::holds_alternative<EBuildingClass>(c)
							// Yeah, don't build palaces randomly.
							&& c != EBuildingClass::Palace;
						}) | std::ranges::to<std::vector>();
					const auto units = choices | std::views::filter([](const ProductionChoice& c) { return std::holds_alternative<EUnitClass>(c); }) | std::ranges::to<std::vector>();

					if (!buildings.empty())
						tryProduce(buildings[0], EProductionPriority::Buildings);
					tryProduce(EProcess::Wealth, EProductionPriority::NothingBetterToDo);
					tryProduce(EProcess::Research, EProductionPriority::NothingBetterToDo);
					tryProduce(EProcess::Culture, EProductionPriority::NothingBetterToDo);
					if (!units.empty())
						tryProduce(units[std::uniform_int_distribution<size_t>(0, units.size() - 1)(rng)], EProductionPriority::NothingBetterToDo);
				}
			}
		}

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
				if (infos.units[infos.unitClasses[unit.klass].activeType].optMissionaryReligion != EReligion::None)
					game.tryAutomate(std::array{ unit.id }, EAutomation::Religion);
				break;
			}
		}

		//const std::vector<std::optional<Player>> players = game.getRevealedPlayers();
		

		if (!civState.optCurrentResearch)
		{
			std::vector<ptrdiff_t> choices = civState.techs | std::views::enumerate
				| std::views::filter([](const std::pair<ptrdiff_t, TechState>& x) { return x.second.canResearch && x.first != ETech::DivineRight; })
				| std::views::keys | std::ranges::to<std::vector>();

			std::ranges::stable_sort(choices, std::less(), [&](ptrdiff_t x) {
				return civState.techs[x].cost - civState.techs[x].progress;
				});

			if (!choices.empty())
			{
				game.changeResearch(static_cast<ETech>(choices[0]));
			}
		}

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
	}

	virtual std::string buildPlotDebugString(const IGame&, ivec2 coord) const override
	{
		std::ostringstream ss;
		ss << "Player bot info at [" << coord.x << ", " << coord.y << "]\n";
		if (settlingAdvisor.optTarget)
			ss << "Will settle at [" << settlingAdvisor.optTarget->x << ", " << settlingAdvisor.optTarget->y << "]\n";
		ss << "canBarbsEnterTerritory = " << militaryAdvisor.canBarbsEnterTerritory << '\n';
		if (cityDistanceField.cells.size())
			ss << "cityDistanceField = " << cityDistanceField[coord].distance << '\n';
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
		return kModName;
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
