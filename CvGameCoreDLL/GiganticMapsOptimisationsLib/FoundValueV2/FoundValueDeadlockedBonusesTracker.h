#pragma once

#include "../Util.h"

namespace GiganticMapsOptimisationsLib::FoundValueSystem
{
	class FoundValueDeadlockedBonusesTracker
	{
	public:
		explicit FoundValueDeadlockedBonusesTracker(CivMapGeometry mapGeom);

		void onPlotOwnerChanged(ivec2 coord);
		// Call for all cities.
		void onAfterCityCreatedOrDestroyed(ivec2 coord);
		void onVisibleHasBonusChanged(ivec2 coord);

		// CvPlayer::canFound(coord, with bTestVisible false)
		std::span<const i16vec2> getCanFoundDirtyList() const;
		std::span<const i16vec2> getDeadlockedBonusCountDirtyList() const;

		// Make sure you call this!
		void clearDirtyLists();

		bool canFound(ivec2 coord, PlayerTypes playerI) const;
		int getNumDeadlockedBonuses(ivec2 coord, PlayerTypes playerI) const;

		void verify() const;

	private:
		CivMapGeometry mMapGeom;

		struct PlotProps
		{
			std::bitset<MAX_PLAYERS> canFound{};
			std::bitset<MAX_PLAYERS> hasRevealedBonus{};
		};

		// 18MB for 1040x640
		DynamicArray2D<PlotProps> mPlotPropsMap;
		DynamicArray2D<std::array<uint8_t, MAX_PLAYERS>> mDeadlockedCountsMap;

		std::vector<i16vec2> mCanFoundDirtyList;
		std::vector<i16vec2> mDeadlockedBonusCountDirtyList;

		void updateCanFound(ivec2 coord);
		void updateDeadlockedBonuses(ivec2 coord);

		std::array<uint8_t, MAX_PLAYERS> AI_countDeadlockedBonuses(ivec2 coord) const;
	};
}