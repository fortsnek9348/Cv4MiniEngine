#pragma once

#include "FoundValuePlayerNearestCityCache.h"

#include "../VersionedBitmap2D.h"

namespace GiganticMapsOptimisationsLib::FoundValueSystem
{
	struct FoundValueCalculationPlayerMapInterface;
	struct FoundValueCalculationPlayerGlobalDataInterface;
	class FoundValueDeadlockedBonusesTracker;
	struct AreaMap;

	class PlayerCache
	{
	public:
		explicit PlayerCache(CivMapGeometry, const AreaMap& areaMap, PlayerTypes playerI, const FoundValueDeadlockedBonusesTracker& deadlockedBonusesTracker);

		void onCanFoundChanged(const FoundValueDeadlockedBonusesTracker& deadlockedBonusesTracker, ivec2 coord);
		void onDeadlockedBonusesChanged(const FoundValueDeadlockedBonusesTracker& deadlockedBonusesTracker, ivec2 coord);

		// Invalidates canFound, hasForeignOwnerOrOutOfBounds
		void onPlotOwnerChanged(const CvPlot&);
		// Call for all cities.
		// Invalidates canFound
		void onAfterCityCreated(const CvPlot&, PlayerTypes owner);
		void onAfterCityDestroyed(const CvPlot&, PlayerTypes owner);
		void onCapitalChanged(const CvCity* to);
		// Invalidates canFound
		void onRevealedBonusChanged(const CvPlot&);
		// Invalidates isPlayerAIPlotCitySite, isCityPlotWithinPlayerAICitySites
		void onPlayerAICitySitesChanged();
		// Call for our culture, and owner culture for plots of foreign owner players.
		void onCultureChanged(const CvPlot&);

		void onRevealedChanged(const CvPlot&);

		void onAreaCityCountChanged(int from, int to);
	
		void update();

		FoundValueCalculationPlayerMapInterface createPlayerMapInterface(ivec2 foundValueCoord) const;
		FoundValueCalculationPlayerGlobalDataInterface createPlayerGlobalsInterface() const;

		// We assume deadlockedBonusesTracker has already been verified.
		// Call update first!
		void verify(const AreaMap& areaMap, const FoundValueDeadlockedBonusesTracker& deadlockedBonusesTracker) const;

	private:
		PlayerTypes mPlayerI = NO_PLAYER;
		TeamTypes mTeamI = NO_TEAM;
		int mGreed = 0;
		int mClaimThreshold = 0;
		int mMaxDistanceFromCapital = 0;
		bool mIsMaxDistanceFromCapitalDirty = false;
		std::array<uint16_t, 16> mGoodBonusConditionFlags{};
		std::array<int16_t, 128 + 2> mBonusValuationFirstSeen{};
		std::array<int16_t, 128 + 2> mBonusValuationExtra{};

		// Depends on FoundValueDeadlockedBonusesTracker
		// static constexpr uint8_t kPlayerMapFlag_canFound                          /**/ = 1 << 0;
		// Depends on AI city site list
		// static constexpr uint8_t kPlayerMapFlag_isPlayerAIPlotCitySite            /**/ = 1 << 1; // player.AI_isPlotCitySite(pPlot)
		// Depends on AI city site list
		// static constexpr uint8_t kPlayerMapFlag_isCityPlotWithinPlayerAICitySites /**/ = 1 << 2; // data.plotDistance(*pLoopPlot, pCitySitePlot.x) <= CITY_PLOTS_RADIUS
		// Depends on plot owner
		// static constexpr uint8_t kPlayerMapFlag_hasForeignOwnerOrOutOfBounds      /**/ = 1 << 3; // if (!pLoopPlot || pLoopPlot->isOwned() && pLoopPlot->getTeam() != player.getTeam())
		// Depends on city distance field
		// kPlayerMapFlag_hasMyCitiesOnArea                 /**/ = 1 << 4; // pLoopPlot->area()->getCitiesPerPlayer(player.getID()) != 0
		// Depends on area's player city count
		// static constexpr uint8_t kPlayerMapFlag_hasMyCitiesOnArea                 /**/ = 1 << 5; // pLoopPlot->area()->getCitiesPerPlayer(player.getID()) != 0
		// Depends on plot owner, player team
		// static constexpr uint8_t kPlayerMapFlag_isOwnedByMe                       /**/ = 1 << 6;
		// Depends on plot bonus, reveal techs
		// static constexpr uint8_t kPlayerMapFlag_hasRevealedBonus                  /**/ = 1 << 7; // 1 iff plot has a bonus and it's revealed.
		// static constexpr int16_t kPlayerMapFlag_isOwnedByOurTeam                  /**/ = 1 << 8; // if (pLoopPlot->isOwned() && pLoopPlot->getTeam() == player.getTeam())
		// Depends on area city counts (player, team, and barbarian)
		PaddedArray2D<uint8_t> mFlags;

		// If plot does not have a foreign player owner: This is 100.
		// Else: Depends on our culture value and the foreign culture value.
		// High bit is revealed flag.
		PaddedArray2D<uint8_t> mRevealedFlagAndOurCultureMultipliers;

		PlayerNearestCityCache mNearestCityCache;

		// 6 bits: cityRadiusDeadlockedBonusesCount (max: 81-21: 60). Depends on FoundValueDeadlockedBonusesTracker.
		// 1 bit: kPlayerMapFlag_teamAreaCityCountCaseBit0         /**/ = 1 << 9;
		// 1 bit: kPlayerMapFlag_teamAreaCityCountCaseBit1         /**/ = 1 << 10;
		PaddedArray2D<uint8_t> mStage2Data;

		bool mPlayerAICitySitesDirty = true;
		std::vector<ivec2> mCachedAICitySites;

		std::unordered_map<int, unsigned int> mTeamCityCountCaseBitsByAreaId;

		void updateCultureMultiplier(const CvPlot& plot);
		[[nodiscard]] bool updateTeamCityCountCaseBitsMap();

		int calculateMaxDistanceFromCapital() const;
	};
}