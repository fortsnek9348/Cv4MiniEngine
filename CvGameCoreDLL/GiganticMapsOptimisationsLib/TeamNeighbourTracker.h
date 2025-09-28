#pragma once

#include "../CvEnums.h"

#include <array>

class CvPlot;
class CvCity;

namespace GiganticMapsOptimisationsLib
{
	class TeamNeighbourTracker
	{
	public:
		TeamNeighbourTracker();

		void onPlotOwnerChanged(const CvPlot&, TeamTypes oldOwner);

		int getAdjacentLandPlotsCount(TeamTypes plotTeamI, TeamTypes adjTeamI) const;

	private:
		std::array<std::array<int, MAX_TEAMS>, MAX_TEAMS> mAdjacencyCounts{};
	};

	class StolenCityRadiusPlotsTracker
	{
	public:
		StolenCityRadiusPlotsTracker();

		void onPlotOwnerChanged(const CvPlot&, PlayerTypes oldOwner);
		//void onCityCreate(const CvCity&);
		//void onCityDestroy(const CvCity&);
		void onChangePlayerCityRadiusCount(const CvPlot& plot, PlayerTypes playerI, int from, int to);

		int getStolenCityRadiusPlots(PlayerTypes playerI, PlayerTypes neighbourPlayerI) const;

		void verify() const;

	private:
		std::array<std::array<int, MAX_PLAYERS>, MAX_PLAYERS> mCounts{};

		
	};
}