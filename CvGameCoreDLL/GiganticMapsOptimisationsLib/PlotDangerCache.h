#pragma once

#include "VersionedBitmap2D.h"
#include "GameEvents.h"

#include "../CvDefines.h"
#include "../CvEnums.h"

class CvPlot;

namespace GiganticMapsOptimisationsLib
{
	class AllBorderPlotDangerSourceCache
	{
	public:
		AllBorderPlotDangerSourceCache();

		// Depends only on: plot owner, plot route type, and city. Very simple.

		void onPlotOwnerChanged(const CvPlot&);
		void onPlotRouteTypeChanged(const CvPlot&);
		void onPlotIsCityChanged(const CvPlot&);

		void update();
		void verify(ivec2 coord) const;

		const VersionedBlocks2D& getBlockVersioning() const;

		std::bitset<MAX_TEAMS> operator[](ivec2 v) const
		{
			return mBitmap[v];
		}

	private:
		DynamicArray2D<std::bitset<MAX_TEAMS>> mBitmap;
		VersionedBlocks2D mBlockVersioning;
		InvalidationState mInvalidationState;

		std::bitset<MAX_TEAMS> calcDangerBits(CvMap& map, ivec2 coord) const;
	};

	enum class EPlotDangerDiploChange
	{
		// Total invalidation
		AnyWar,
		// This changes canEnterArea for the master.
		// (teamA, teamB) = (master, vassal)
		AnyVassal,
		// teamA, teamB
		OpenBorders,
		// canMoveOrAttackInto
		// (teamA, teamB) = (enemy, defender)
		CanDeclareWar,
		AI_isSneakAttackReady,
		AI_isDeclareWar,
		// Greatwall.
		// teamA
		AreaIsBorderObstacleChanged,

		// Team change.
		PermAlliance,

		PlayerCityDefenceModifier,
	};

	class UnitPlotDangerCache
	{
	public:
		UnitPlotDangerCache();

		void onPlotChange(EGamePlotChangeEvent e, const CvPlot&, int oldValue, int newValue);
		void onUnitChange(EGameUnitChangeEvent e, const CvUnit&);
		void onUnitMoved(const CvUnit&, ivec2 from, ivec2 to); // Thread-safe
		void onDiploChange(EPlotDangerDiploChange change, TeamTypes teamA, TeamTypes teamB);

		void update();

		uint64_t getVersionSerial(TeamTypes defenderTeamI) const;
		std::generator<iaabb2> getDefenderDirtyRects(TeamTypes defenderTeamI, uint64_t myOldVersion) const;

		bool getDefenderPlotDanger(TeamTypes myTeamI, ivec2 coord) const;

		void verify(TeamTypes defenderTeamI, ivec2 coord) const;
		void verifyAll() const;

	private:
		std::array<std::bitset<MAX_TEAMS>, MAX_TEAMS> mWarState{};
		// These defender plot danger bits need to be updated.
		std::bitset<MAX_TEAMS> mDirtyDefenderTeams{};
		InvalidationState mDefenderInvalidationState;
		// These units need to be visited to update defender plot danger.
		// Trivially all teams, initially.
		std::bitset<MAX_TEAMS> mDirtyEnemyTeams{};
		// Defender plot danger.
		DynamicArray2D<std::bitset<MAX_TEAMS>> mPlotDanger;
		// Does not distinguish between defender teams for now.
		VersionedBlocks2D mPlotDangerVersioning;

		IDInfo mDebugAttackerUnitID{};

		void invalidateMoveRadiusAroundDangerousUnits(TeamTypes, bool alwaysHostileOnly = false);
		void invalidateAroundPlot(ivec2 position, int radius);
		void invalidateSinglePlot(ivec2 position);
		void setPlotDangerFromEnemyTeam(TeamTypes, std::atomic_bool& changed);
		void setPlotDangerFromEnemyGroup(const CvSelectionGroup&, std::atomic_bool& changed);
		void updatePlotDangerForDefenderPlot(const CvPlot&, std::atomic_bool& changed);

		std::bitset<MAX_TEAMS> computePlotDanger(const CvPlot& plot) const;
		std::bitset<MAX_TEAMS> computePlotDangerFromAttacker(const CvUnit& attacker, const CvPlot& plot) const;

		void debugFindAttacker();
	};
}