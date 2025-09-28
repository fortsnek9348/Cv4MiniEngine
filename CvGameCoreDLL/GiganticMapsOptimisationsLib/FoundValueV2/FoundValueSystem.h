#pragma once

#include "FoundValueCommonCache.h"
#include "FoundValuePlayerCache.h"
#include "FoundValueDeadlockedBonusesTracker.h"

namespace GiganticMapsOptimisationsLib::FoundValueSystem
{
	class System
	{
	public:
		System();

		void onPlotTerrainOrFeatureChanged(const CvPlot&);
		void onPlotGetYieldChanged(const CvPlot&);
		void onCityRadiusCountChanged(const CvPlot&);
		void onUnitMoved(const CvUnit& unit, ivec2 from, ivec2 to); // Thread-safe.
		void onGlobalAreaCityCountChanged(const CvArea& area, int from, int to);
		void onPlotBonusChanged(const CvPlot&, BonusTypes from, BonusTypes to);
		void onPlotOwnerChanged(const CvPlot&, PlayerTypes from, PlayerTypes to);
		void onAfterCityCreated(const CvPlot&, PlayerTypes owner);
		void onAfterCityDestroyed(const CvPlot&, PlayerTypes owner);
		void onPlayerCapitalChanged(PlayerTypes playerI, const CvCity* to);
		void onPlayerAICitySitesChanged(PlayerTypes playerI);
		//void onPlayerPlotRevealedBonusChanged(const CvPlot&, PlayerTypes playerI);
		void onPlayerPlotCultureChanged(const CvPlot&, PlayerTypes playerI);
		void onPlayerPlotRevealedChanged(const CvPlot&, PlayerTypes playerI);
		void onPlayerAreaCityCountChanged(const CvArea& area, PlayerTypes playerI, int from, int to);
		void onTeamTech(TeamTypes teamI, TechTypes techI);
		void onTeamForceRevealBonusChanged(TeamTypes teamI, BonusTypes bonusI);
		void onPlayerAliveChanged(PlayerTypes playerI, bool alive);
		
		void update(PlayerTypes playerI);

		void verify(PlayerTypes playerI = NO_PLAYER) const;

		[[noreturn]] void dumpEvaluationAtAndAbort(PlayerTypes playerI, ivec2 coord, int iMinRivalRange) const;

		enum EFilter
		{
			None,
			Revealed,
		};

		std::vector<int> computeFoundValuesRow(PlayerTypes playerI, EFilter filter, int y, int iMinRivalRange) const; // Thread-safe.


	private:
		CivMapGeometry mMapGeom;
		AreaMap mAreaMap;

		std::vector<std::vector<i16vec2>> mBonusLocations;

		FoundValueDeadlockedBonusesTracker mDeadlockedBonusesTracker;
		CommonCache mCommonPlotProps;
		std::vector<std::optional<PlayerCache>> mPlayerPlotProps;

		struct Evaluator
		{
			FoundValueCalculationCommonGlobalDataInterface commonGlobal;
			FoundValueCalculationCommonMapInterface commonLocal;

			FoundValueCalculationPlayerGlobalDataInterface playerGlobal;
			FoundValueCalculationPlayerMapInterface playerLocal;

			ivec2 commonLocalsInterfaceInitialAddressingCoord{};
			ivec2 playerLocalsInterfaceInitialAddressingCoord{};

			explicit Evaluator(const System& sys, PlayerTypes playerI, ivec2 rowStartCoord);

			void eval(int x, int iMinRivalRange, EFilter filter, std::span<int, kVectorWidth> out, std::string* logOutput = nullptr);
		};
	};
}