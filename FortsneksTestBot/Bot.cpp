#include <PlayerBotGameBinding/IPlayerBot.h>
#include <PlayerBotGameBinding/IPlayerBotPlugin.h>
#include <PlayerBotGameBinding/IGame.h>
#include <PlayerBotGameBinding/GameStructs.h>
#include <PlayerBotGameBinding/EnumDefs.h>

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <ostream>
#include <ranges>
#include <random>

using namespace cvbot;

static constexpr EBuildingType kCityBuildingOrder[]{
	EBuildingType::Granary,
	EBuildingType::Lighthouse,
	EBuildingType::Library,
	EBuildingType::Forge,
	EBuildingType::Courthouse,
};

class MyBot : public IBot
{
public:
	std::ostream& log;
	const GameSetup setup;

	std::minstd_rand rng{ 42 };
	
	explicit MyBot(const IBotInit& init)
		: log(init.getLoggingStream())
		, setup(init.getGameSetup())
	{
	}

	virtual void run(IGame& game) override
	{
		const std::vector<Unit> allVisibleUnits = game.getVisibleUnits(iaabb2::sized(ivec2(), setup.mapGeometry.dim));
		const std::vector<City> allRevealedCities = game.getRevealedCities();

		const std::vector<Unit> myUnits = allVisibleUnits | std::views::filter([this](const Unit& x) { return x.owner == setup.activePlayerI; }) | std::ranges::to<std::vector>();
		std::vector<City> myCities = allRevealedCities | std::views::filter([this](const City& x) { return x.owner == setup.activePlayerI; }) | std::ranges::to<std::vector>();

		if (myCities.empty())
			if (const auto it = std::ranges::find(myUnits, EUnitType::Settler, &Unit::type); it != myUnits.end())
			{
				game.pushMission(std::array{ it->id }, EMission::Found, -1, -1);
				if (auto optCity = game.getRevealedCityAt(it->coord))
					myCities.push_back(std::move(*optCity));
			}

		for (const Unit& unit : myUnits)
		{
			switch (unit.type)
			{
			case EUnitType::Scout:
			case EUnitType::Warrior:
				game.tryAutomate(std::array{ unit.id }, EAutomation::Explore);
				break;
			case EUnitType::Worker:
				game.tryAutomate(std::array{ unit.id }, EAutomation::Build);
				break;
			default:
				break;
			}
		}

		size_t numWorkers = std::ranges::count(myUnits, EUnitType::Worker, &Unit::type);
		const size_t desiredNumWorkers = myCities.size() * 3 / 2;

		for (const City& city : myCities)
		{
			if (std::holds_alternative<std::monostate>(city.optInspectableCityInfo->productionChoice))
			{
				bool producing = false;
				for (const auto x : kCityBuildingOrder)
					if (producing = game.tryChangeProduction(city.coord, x), producing)
						break;

				producing = producing || (numWorkers < desiredNumWorkers && game.tryChangeProduction(city.coord, EUnitType::Worker));
				
				if (!producing)
				{
					const auto choices = game.getCityProductionChoices(city.coord);

					const auto buildings = choices | std::views::filter([](const ProductionChoice& c) { return std::holds_alternative<EBuildingType>(c); }) | std::ranges::to<std::vector>();
					const auto units = choices | std::views::filter([](const ProductionChoice& c) { return std::holds_alternative<EUnitType>(c); }) | std::ranges::to<std::vector>();

					producing = producing || (!buildings.empty() && game.tryChangeProduction(city.coord, buildings[0]));
					producing = producing || game.tryChangeProduction(city.coord, EProcess::Wealth);
					producing = producing || game.tryChangeProduction(city.coord, EProcess::Research);
					producing = producing || game.tryChangeProduction(city.coord, EProcess::Culture);
					producing = producing || (!units.empty() && game.tryChangeProduction(city.coord, units[std::uniform_int_distribution<size_t>(0, units.size() - 1)(rng)]));
				}
			}
		}

		//const std::vector<std::optional<Player>> players = game.getRevealedPlayers();
		const CivState civState = game.getCivState();

		if (!civState.optCurrentResearch)
		{
			if (const auto it = std::ranges::find_if(civState.techs, &TechState::canResearch); it != civState.techs.end())
			{
				game.changeResearch(static_cast<ETech>(it - civState.techs.begin()));
			}
		}

		auto civics = civState.civics;
		auto attempt = civics;
		attempt[ECivicOptionType::Government] = ECivic::HereditaryRule;
		if (game.canChangeCivicsTo(civics))
			civics = attempt;
		attempt[ECivicOptionType::Legal] = ECivic::Bureaucracy;
		if (game.canChangeCivicsTo(civics))
			civics = attempt;
		attempt[ECivicOptionType::Labor] = ECivic::Slavery;
		if (game.canChangeCivicsTo(civics))
			civics = attempt;
		game.tryChangeCivicsTo(civics);
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
		throw std::runtime_error(std::string(__func__) + " unimplemented.");
	}

	

	virtual ETech handleFreeTechChoice(const IGame&) override
	{
		throw std::runtime_error(std::string(__func__) + " unimplemented.");
	}

	virtual ECityCaptureChoice handleCityCaptureChoice(const IGame&, i16vec2) override
	{
		throw std::runtime_error(std::string(__func__) + " unimplemented.");
	}
	virtual ECityCaptureChoice handleCityAcquireChoice(const IGame&, i16vec2) override
	{
		throw std::runtime_error(std::string(__func__) + " unimplemented.");
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
