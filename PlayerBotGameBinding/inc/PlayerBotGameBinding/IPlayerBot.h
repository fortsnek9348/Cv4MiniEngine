#pragma once

#include "DLLDefs.h"
#include "EnumFwd.h"

namespace cvbot
{
	struct UnitKilledTurnMessage
	{
		EPlayer visibleOwner{};
		EUnitType myUnitType{};
		EUnitType enemyUnitType{};
	};

	struct GreatPersonTurnMessage
	{
		EPlayer optPlayer{};
		EUnitType unitType{};
	};


	struct TradeList;
	struct i16vec2;

	class IGame;

	class IBot
	{
	public:
		// From any function below, you may throw a `BotFailure` to display an error.

		// Run the bot for one turn.
		// You should give all units orders.
		virtual void run(IGame& game) = 0;

		virtual std::string buildPlotDebugString(const IGame& game, ivec2 coord) const = 0;

		// Return your choice.
		//virtual DiploChoice handleDiplo(const IGame& game, EPlayer themI, EDiploComment aiComment, std::span<const DiploChoice> choices,
		//	const TradeList& fromThem, const TradeList& fromMe) = 0;

		virtual void handleDiploFirstContact(const IGame& game, EPlayer themI) = 0;

		// Return true from these to accept.
		virtual bool handleDiploGift(const IGame& game, EPlayer themI, const TradeList& fromThem) = 0;
		virtual bool handleDiploHelp(const IGame& game, EPlayer themI, const TradeList& fromMe) = 0;
		virtual bool handleDiploDemandTribute(const IGame& game, EPlayer themI, const TradeList& fromMe) = 0;
		virtual bool handleDiploReligionPressure(const IGame& game, EPlayer themI, EReligion religion) = 0;
		virtual bool handleDiploCivicPressure(const IGame& game, EPlayer themI, ECivicOptionType civicOptionType, ECivic civic) = 0;
		virtual bool handleDiploJoinWar(const IGame& game, EPlayer themI, ETeam targetTeam) = 0;
		virtual bool handleDiploStopTrading(const IGame& game, EPlayer themI, ETeam targetTeam) = 0;

		// Accept your fate.
		virtual void handleDiploWarDeclaration(const IGame& game, ETeam themI) = 0;

		// Return your choice.
		virtual EEvent handleRandomEvent(const IGame& game, EEventTrigger event, std::optional<i16vec2> optCity, std::optional<i16vec2> optPlot, std::span<const EEvent> choices) = 0;

		// Use tech list from CivState.
		virtual ETech handleFreeTechChoice(const IGame& game) = 0;

		enum class ECityCaptureChoice
		{
			// These correspond to popup button IDs.
			Keep,
			Raze,
			Gift,
		};

		virtual ECityCaptureChoice handleCityCaptureChoice(const IGame& game, i16vec2 coord) = 0;

		enum class ECityAcquireChoice
		{
			// These correspond to popup button IDs.
			Keep,
			Disband,
		};

		virtual ECityAcquireChoice handleCityAcquireChoice(const IGame& game, i16vec2 coord) = 0;

		virtual void onTurnMessage(UnitKilledTurnMessage) = 0;
		virtual void onTurnMessage(GreatPersonTurnMessage) = 0;

		// Save the game, write out logs, do whatever you need to do.
		// Bot may continue running if not headless.
		virtual void onVictory(const IGame& game, ETeam winningTeam, EVictory victoryType) = 0;

		// Save the game, write out logs, do whatever you need to do.
		// Bot stops here.
		virtual void onDefeated(const IGame& game) = 0;

		virtual ~IBot() = default;
	};
}