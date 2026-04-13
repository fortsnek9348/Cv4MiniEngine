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

using namespace cvbot;

class MyBot : public IBot
{
public:
	std::ostream& log;
	const GameSetup setup;
	
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
		const std::vector<City> myCities = allRevealedCities | std::views::filter([this](const City& x) { return x.owner == setup.activePlayerI; }) | std::ranges::to<std::vector>();
		
		if (myCities.empty())
			if (const auto it = std::ranges::find(myUnits, EUnitType::Settler, &Unit::type); it != myUnits.end())
				game.pushMission(std::array{ it->id }, EMission::Found, -1, -1);
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
		return true;
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
