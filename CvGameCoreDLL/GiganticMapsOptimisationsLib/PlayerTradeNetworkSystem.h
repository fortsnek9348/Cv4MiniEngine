#pragma once

#include "Util.h"

#include "../CvEnums.h"

#include <CommonStuff/TradeNetworkSystem.h>

class CvPlot;
class CvMap;

namespace GiganticMapsOptimisationsLib
{
	// Manages trade network plot grouping and bonus distribution to cities.
	// Entirely replaces the Firaxis CvPlotGroup-based implementation, except during deserialisation/init.
	// Built on top of the agnostic implementation in CommonStuff.
	class PlayerTradeNetworkSystem
	{
	public:
		explicit PlayerTradeNetworkSystem(PlayerTypes playerI);

		// CvPlot::isTradeNetwork dependencies
		//void onTeamWarChanged(TeamTypes otherTeamI);
		//void onTeamRiverTradeChanged();
		//void onTeamIsTerrainTradeChanged(TerrainTypes terrainI);
		//void onPlotBlockadedChanged(const CvPlot& plot);
		//void onPlotOwnerChanged(const CvPlot& plot);
		//void onPlotRevealedChanged(const CvPlot& plot);
		//void onPlotIsRouteChanged(const CvPlot& plot);
		//void onPlotTerrainChanged(const CvPlot& plot);
		//
		//// CvPlot::isTradeNetworkConnected dependencies
		//void onPlotIsFortChanged(const CvPlot& plot);
		//void onPlotIsCityChanged(const CvPlot& plot);
		//void onTeamIsVassalChanged(TeamTypes otherTeamI); // Vassal of or master of
		//void onTeamOpenBordersChanged(TeamTypes otherTeamI);

		// CvPlayer::updatePlotGroups
		void onUpdateAllPlotGroups();
		// CvPlot::updatePlotGroup
		// Called when plot groups may have been connected or disconnected.
		void onUpdatePlotGroupsForPlot(const CvPlot& plot);
		// Call when any of these change: revealed bonus, CvCity::getFreeBonus, getBonusImport/Export at the capital.
		// Or rather, call inside CvPlot::updatePlotGroupBonus.
		// Also call when owner changes, when capital changes, and when a owner city is created or destroyed at this plot.
		void onUpdateBonusesAtPlot(const CvPlot& plot);

		// Attaches city ssuming it starts with zero network bonuses, then adds the network bonuses through CvCity::changeNumBonuses.
		void newCity(CvCity* city);
		// Remove network bonuses through CvCity::changeNumBonuses and then removes the city from data structure.
		void destroyCity(CvCity* city);

		// !!a->getPlotGroup()
		[[nodiscard]] bool isInPlotGroup(const CvPlot& plot) const;
		// a->getPlotGroup() == b->getPlotGroup()
		[[nodiscard]] bool isSamePlotGroup(const CvPlot& a, const CvPlot& b) const;
		// a->getPlotGroup() ? a->getPlotGroup()->getNumBonuses(bonusI) : 0
		[[nodiscard]] int getPlotGroupNumBonuses(const CvPlot& plot, BonusTypes bonusI) const;
		// a->getPlotGroup() && a->getPlotGroup()->hasBonus(bonusI)
		[[nodiscard]] int getPlotGroupHasBonus(const CvPlot& plot, BonusTypes bonusI) const;

		[[nodiscard]] std::optional<int> tryGetPlotGroupId(const CvPlot& plot) const;

		//CvBlockPlotGroup* getBlockPlotGroupAt(const CvPlot& plot);
		//CvCivPlotGroup* getCivPlotGroupAt(const CvPlot& plot);

		//bool isTradeNetwork(heck::ivec2 coord) const
		//{
		//	return mIsTradeNetworkCache[coord];
		//}

		void writeAsStreamableFFreeListTrashArray(FDataStreamBase& stream) const;

		void onTurn();

		void verify() const;
		void dump(const char* filename) const;

	private:
		struct CivDataSource : heck::TradeNetworkSystem::IDataSource
		{
			PlayerTypes playerI{};
			TeamTypes teamI{};
			const CvMap& map;

			explicit CivDataSource(PlayerTypes playerI);

			//virtual bool isCity(ivec2 coord) const override;
			virtual bool isTradeNetwork(ivec2 coord) const override;
			virtual bool isTradeNetworkConnected(ivec2 a, ivec2 b, int adjI) const override;
			virtual EBonus getNetworkPlotBonus(ivec2 coord) const override;
			virtual std::vector<BonusCount> getNetworkCityExtraBonuses(ivec2 coord) const override;
		};

		PlayerTypes mPlayerI{};
		heck::TradeNetworkSystem mGenericSys;
		mutable bool mPendingVerify = true;

		//struct [[nodiscard]] CityInfo
		//{
		//	const CvCity* city{};
		//	std::vector<int> bonusCounts;
		//};
		
		void destroyAllCvPlotGroups();
		void flushCityUpdates();
	};

}