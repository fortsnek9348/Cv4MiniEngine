#pragma once

#include "DLLDefs.h"
#include "EnumFwd.h"

#include <iosfwd>
#include <vector>
#include <string_view>
#include <stdexcept>

namespace cvbot
{
	struct Plot;
	struct Unit;
	struct iaabb2;
	struct Player;
	struct GameSetup;
	struct GlobalInfo;
	struct CivState;
	struct City;
	struct TradeList;
	struct i16vec2;
	struct ProductionChoice;
	struct CityBuildChoice;
	struct GlobalInfoData;

	// To be used to build up a knowledge database only. Not for playing the current game.
	class IAllKnowingGameInterface
	{
	public:
		virtual void getPlots(ivec2 origin, Span2D<Plot> out) const = 0;
		virtual std::vector<Unit> getUnits(iaabb2 rect) const = 0;
		virtual std::vector<std::optional<Player>> getPlayers() const = 0;
		virtual ~IAllKnowingGameInterface() = default;
	};

	class IBotInit
	{
	public:
		// Use this to log your messages. Do not print to stdout in case it is used for the TUI.
		virtual std::ostream& getLoggingStream() const = 0;

		virtual GameSetup getGameSetup() const = 0;

		virtual GlobalInfoData buildGlobalInfoData() const = 0;

		// Use this to build up map knowledge.
		virtual const IAllKnowingGameInterface& getAllKnowingGameInterface() const = 0;

		virtual ~IBotInit() = default;
	};

	class IGame
	{
	public:
		virtual const IAllKnowingGameInterface& getAllKnowingGameInterface() const = 0;

		virtual int getTurnNumber() const = 0;
		virtual GlobalInfo getGlobalInfo() const = 0;

		virtual CivState getCivState() const = 0;
		virtual std::vector<std::optional<Player>> getRevealedPlayers() const = 0;

		// All coords are wrapped.

		virtual void getPlots(ivec2 origin, Span2D<Plot> out) const = 0;

		virtual std::vector<Unit> getVisibleUnits(iaabb2 rect) const = 0;

		virtual int getPlotBuildProgress(ivec2 coord, EBuild build) const = 0;
		virtual std::vector<EBuild> getWorkerBuildChoicesAt(EUnitId unit, ivec2 coord) const = 0;
		virtual std::vector<EBuild> getPlotBuildChoicesAt(ivec2 coord) const = 0;

		virtual bool hasFoundActionAt(ivec2 coord) const = 0;

		

		/// Unit commands
		virtual bool canStartMission(CommandUnitGroup group, EMission mission, int data1, int data2) const = 0;
		virtual i16vec2 getUnitCoord(EUnitId id) const = 0;
		// Clears mission queue, checks if mission can start, then starts it.
		virtual bool startMission(CommandUnitGroup group, EMission mission, int data1, int data2) = 0;
		virtual std::vector<EPromotion> getAvailablePromotions(EUnitId unit) const = 0;
		virtual bool tryPromote(EUnitId unit, EPromotion promotion) = 0;

		virtual bool tryUpgrade(EUnitId unit, EUnitType type) = 0;
		virtual bool tryWake(CommandUnitGroup group) = 0;
		virtual bool trySkipTurn(CommandUnitGroup group) = 0;
		virtual bool tryCancelOrders(CommandUnitGroup group) = 0; // All orders
		virtual EAutomation getAutomation(CommandUnitGroup group) const = 0;
		virtual bool tryAutomate(CommandUnitGroup group, EAutomation automation) = 0; // Does not interrupt automation if it's the same automation.
		virtual bool tryStopAutomation(CommandUnitGroup group) = 0;
		virtual bool tryDelete(EUnitId unit) = 0;
		virtual bool tryGift(EUnitId unitId) = 0;
		virtual bool tryLoad(EUnitId unitId, EUnitId transport) = 0;
		virtual bool tryUnload(EUnitId unitId) = 0;
		virtual bool tryUnloadAll(EUnitId transport) = 0;

		// Civ commands
		virtual bool canChangeCivics() const = 0;
		virtual bool canChangeReligion() const = 0;
		virtual bool canChangeCivicTo(ECivic) const = 0;
		virtual bool canChangeStateReligionTo(std::optional<EReligion>) const = 0;
		virtual bool tryChangeCivicsTo(std::span<const ECivic>) const = 0;
		virtual bool tryChangeStateReligionTo(std::optional<EReligion>) const = 0;
		// Input is ratio. Only flexible commerce is considered, and result is rounded to 10% increments, as in UI.
		virtual void adjustSliders(std::array<int, ECommerce::Num> ratio) = 0;
		virtual std::array<int, ECommerce::Num> getCivCommerceRates() const = 0;
		// If tech can't be researched, research is set to None.
		virtual void changeResearch(ETech tech) = 0;
		

		// City commands
		virtual std::vector<City> getRevealedCities() const = 0;
		virtual std::optional<City> getRevealedCityAt(ivec2 coord) const = 0;
		virtual bool tryChangeProduction(i16vec2, ProductionChoice choice) = 0;
		virtual bool tryWhip(i16vec2) = 0;
		// Will clamp to allowed values. Extra free specialists are assigned to citizen.
		virtual void setSpecialistCounts(i16vec2, std::span<const int> counts) = 0;
		virtual std::vector<CityBuildChoice> getCityProductionChoices(i16vec2) const = 0;

		// Diplo
		virtual bool tryNegotiate(EPlayer them, TradeList& fromThem, TradeList& fromMe) = 0;
		virtual bool tryTrade(EPlayer them, const TradeList& fromThem, const TradeList& fromMe) = 0;
		virtual bool tryDeclareWar(ETeam) = 0;
		virtual bool tryCancelOpenBorders(EPlayer) = 0;

		virtual void saveGame(std::string_view name) const = 0;

		virtual ~IGame() = default;
	};
}