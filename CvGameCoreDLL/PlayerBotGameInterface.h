#pragma once

#include "LinkedList.h"

#include <PlayerBotGameBinding/IGame.h>

struct TradeData;
class CvPlayerAI;

namespace cvbot
{
	// To be used to build up a knowledge database only. Not for playing the current game.
	class AllKnowingGameInterface : public IAllKnowingGameInterface
	{
	public:
		virtual void getPlots(ivec2 origin, Span2D<Plot> out) const override;
		virtual std::vector<Unit> getUnits(iaabb2 rect) const override;
		virtual std::vector<std::optional<Player>> getPlayers() const override;
	};

	class BotInit : public IBotInit
	{
	public:
		static BotInit& getInstance();

		// Use this to log your messages. Do not print to stdout in case it is used for the TUI.
		virtual std::ostream& getLoggingStream() const override;

		virtual GameSetup getGameSetup() const override;

		// Use this to build up map knowledge.
		virtual const IAllKnowingGameInterface& getAllKnowingGameInterface() const override;
	};

	class Game : public IGame
	{
	public:
		static Game& getInstance();

		virtual const IAllKnowingGameInterface& getAllKnowingGameInterface() const override;

		virtual int getTurnNumber() const override;
		virtual GlobalInfo getGlobalInfo() const override;

		virtual CivState getCivState() const override;
		virtual std::vector<std::optional<Player>> getRevealedPlayers() const override;

		// All coords are wrapped.

		virtual void getPlots(ivec2 origin, Span2D<Plot> out) const override;

		virtual std::vector<Unit> getVisibleUnits(iaabb2 rect) const override;

		virtual int getPlotBuildProgress(ivec2 coord, EBuild build) const override;
		virtual std::vector<EBuild> getWorkerBuildChoicesAt(EUnitId unit, ivec2 coord) const override;
		virtual std::vector<EBuild> getPlotBuildChoicesAt(ivec2 coord) const override;

		virtual bool hasFoundActionAt(ivec2 coord) const override;

		// Unit commands
		virtual bool canStartMission(CommandUnitGroup group, EMission mission, int data1, int data2) const override;
		virtual bool pushMission(CommandUnitGroup group, EMission mission, int data1, int data2) override;
		virtual std::vector<EPromotion> getAvailablePromotions(EUnitId unit) const override;
		virtual bool tryPromote(EUnitId unit, EPromotion promotion) override;

		virtual bool tryUpgrade(EUnitId unit, EUnitType type) override;
		virtual bool tryWake(CommandUnitGroup group) override;
		virtual bool trySkipTurn(CommandUnitGroup group) override;
		virtual bool tryCancelOrders(CommandUnitGroup group) override; // All orders
		virtual bool tryStopAutomation(CommandUnitGroup group) override;
		virtual bool tryDelete(EUnitId unit) override;
		virtual bool tryGift(EUnitId unitId) override;
		virtual bool tryLoad(EUnitId unitId, EUnitId transport) override;
		virtual bool tryUnload(EUnitId unitId) override;
		virtual bool tryUnloadAll(EUnitId transport) override;

		// Civ commands
		virtual bool canChangeCivics() const override;
		virtual bool canChangeReligion() const override;
		virtual bool canChangeCivicsTo(std::span<const ECivic>) const override;
		virtual bool canChangeStateReligionTo(std::optional<EReligion>) const override;
		// Only flexible sliders are set, and gold is ignored.
		virtual void adjustSliders(std::array<int, ECommerce::Num> percents) override;
		// If tech can't be researched, research is set to None.
		virtual void changeResearch(ETech tech) override;


		// City commands
		virtual std::vector<City> getRevealedCities() const override;
		virtual std::optional<City> getRevealedCityAt(ivec2 coord) const override;
		virtual bool tryChangeProduction(i16vec2, ProductionChoice choice) override;
		virtual bool tryWhip(i16vec2) override;
		// Will clamp to allowed values. Extra free specialists are assigned to citizen.
		virtual void setSpecialistCounts(i16vec2, std::span<const int> counts) override;
		virtual std::vector<CityBuildChoice> getCityProductionChoices(i16vec2) const override;

		// Diplo
		virtual bool tryNegotiate(EPlayer them, TradeList& fromThem, TradeList& fromMe) override;
		virtual bool tryTrade(EPlayer them, const TradeList& fromThem, const TradeList& fromMe) override;
		virtual bool tryDeclareWar(ETeam) override;
		virtual bool tryCancelOpenBorders(EPlayer) override;

		virtual void saveGame(std::string_view name) const override;
	};

	TradeList convertTradeList(const CLinkList<TradeData>& table, const CvPlayerAI& us, const CvPlayerAI& them);
}