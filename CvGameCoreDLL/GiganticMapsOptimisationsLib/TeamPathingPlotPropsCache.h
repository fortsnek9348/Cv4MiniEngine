#pragma once

#include "Util.h"
#include "VersionedBitmap2D.h"

#include "../CvEnums.h"

#include <bitset>
#include <span>

namespace GiganticMapsOptimisationsLib
{
	class AllBorderPlotDangerSourceCache;
	class UnitPlotDangerCache;

	class TeamPathingPlotPropsCache
	{
	public:
		enum ECostPlotProp
		{
			// pToPlot->defenseModifier(pLoopUnit->getTeam(), false)
			// defenseModifier(team) = f(terrain, feature, isFort, isFriendlyTerritory, pCity->getDefenseModifier)
			// pCity->getDefenseModifier = f(getBuildingDefense, getNaturalDefense, GET_PLAYER(getOwnerINLINE()).getCityDefenseModifier())
			kCostPlotPropBitIndex_Defence,
			// Takes into account river crossing, and can also be used to signal no route.
			// Only four bits are needed as you can store the other half in the adjacent plots.
			//  0    1    2
			//  3    X   4(3)
			// 5(2) 6(1) 7(0)
			// Adj rel to from plot (adj rel to plot that has river crossing info mask == 0b1000 >> (adjI & 3))
			// UPDATE: Okay, now that I'm splitting plot props into cost and validity bits, let's use the extra space to store all adj.
			kCostPlotPropBitIndex_AdjNoRoute = kCostPlotPropBitIndex_Defence + 8,
			// plot->getRouteType() if has route, otherwise, ignored
			kCostPlotPropBitIndex_RouteType = kCostPlotPropBitIndex_AdjNoRoute + 8,
			
			// pathCost
			// iRegularCost = ((getFeatureType() == NO_FEATURE) ? GC.getTerrainInfo(getTerrainType()).getMovementCost() : GC.getFeatureInfo(getFeatureType()).getMovementCost());
			// if (isHills())
			// 	iRegularCost += GC.getHILLS_EXTRA_MOVEMENT();
			kCostPlotPropBitIndex_RegularCost,

			// plot isForest, plot isHills
			//kCostPlotPropBitIndex_HalfCostMovement = kCostPlotPropBitIndex_RegularCost + 2,
			kCostPlotPropBitIndex_ForestHalfCostMovement = kCostPlotPropBitIndex_RegularCost + 2,
			kCostPlotPropBitIndex_HillsHalfCostMovement,

			// pathCost PATH_TERRITORY_WEIGHT: if (pToPlot->getTeam() != pLoopUnit->getTeam())
			kCostPlotPropBitIndex_NotMyTeam,

			// pathCost plot movement cost filter for routes
			// atWar(plot team, pathing team)
			kCostPlotPropBitIndex_IsEnemyTeam,

			kCostPlotPropNumBits,
		};

		enum EValidityPlotProp
		{
			// And used by pathValid MOVE_NO_ENEMY_TERRITORY.
			// atWar(plot team, pathing team)
			kValidityPlotPropBitIndex_IsEnemyTeam,

			kValidityPlotPropBitIndex_IsOwned,

			// Various false conditions for pathValid.
			kValidityPlotPropBitIndex_NotSafeTerritory,
			// Splitting plot danger into border and unit, which makes a heck of a lot of sense now.
			kValidityPlotPropBitIndex_BorderPlotDanger,
			kValidityPlotPropBitIndex_UnitPlotDanger,
			kValidityPlotPropBitIndex_CanMoveInto_IsAnimalAvoid,
			kValidityPlotPropBitIndex_CanMoveInto_IsEnemyCity,
			// canMoveInto(false, false, true): pathing team != plot owner && plot is city
			// NOTE: Don't need this as it will only be used if isNoCapture, but the only isNoCapture unit is a gunship.
			//       Also used if !canAttack, but canAttack is true for privateer.
			//kPlotPropsMask_AlwaysHostileIsEnemyCity = 1 << 21;
			kValidityPlotPropBitIndex_CanMoveInto_IsVisibleEnemyUnit,
			kValidityPlotPropBitIndex_CanMoveInto_IsVisibleEnemyUnitForAlwaysHostile,
			kValidityPlotPropBitIndex_CanMoveInto_NotIsVisibleEnemyUnit,
			kValidityPlotPropBitIndex_CanMoveInto_NotIsVisibleEnemyUnitForAlwaysHostile,
			// This is specifcally a combination of domain and !bCanEnterArea in canMoveInto.
			// Extra bits for the different unit war AI cases.
			kValidityPlotPropBitIndex_CanMoveInto_IsCantEnterArea_UnitWarAIType_False,
			kValidityPlotPropBitIndex_CanMoveInto_IsCantEnterArea_UnitWarAIType_Limited,
			kValidityPlotPropBitIndex_CanMoveInto_IsCantEnterArea_UnitWarAIType_True,
			kValidityPlotPropBitIndex_CanMoveInto_IsCantEnterArea_AlwaysHostile,

			// As the coastal plots for regular units is the same superset of always hostile, the extra cities can be shared for both coast huggers and ocean goers.
			kValidityPlotPropBitIndex_CanMoveInto_SeaUnit_NotCoastHuggerDomain_Regular,
			kValidityPlotPropBitIndex_CanMoveInto_SeaUnit_NotOceanDomain_Regular,
			kValidityPlotPropBitIndex_CanMoveInto_SeaUnit_NotCoastalCity_AlwaysHostile,

			//kValidityPlotPropBitIndex_PotentialWaterDiagonalSpecialCase,
			kValidityPlotPropBitIndex_WaterDiagonalForbid0,
			kValidityPlotPropBitIndex_WaterDiagonalForbid1,
			kValidityPlotPropBitIndex_WaterDiagonalForbid2,
			kValidityPlotPropBitIndex_WaterDiagonalForbid3,

			kValidityPlotPropBitIndex_CanMoveInto_Impassable,

			kValidityPlotPropNumBits,
		};

		static_assert(kCostPlotPropNumBits <= 32);
		static_assert(kValidityPlotPropNumBits <= 32);
		
		// Keep this updated.
		static constexpr std::string_view kCostPlotPropsDescString /*    */ = "----------ENhfrRjjjjjjjjdddddddd";
		static_assert(kCostPlotPropsDescString.size() == 32);
		static constexpr std::string_view kValidityPlotPropsDescString /**/ = "---------IxxxxaocATLFavAVCAUBSOE";
		static_assert(kValidityPlotPropsDescString.size() == 32);

		using PlotProps = uint32_t;

		static consteval PlotProps getPlotPropMask(ECostPlotProp i)
		{
			PlotProps base = 1;
			if (i == ECostPlotProp::kCostPlotPropBitIndex_AdjNoRoute)
				base = (1 << 8) - 1;
			else if (i == ECostPlotProp::kCostPlotPropBitIndex_Defence)
				base = (1 << 8) - 1;
			else if (i == ECostPlotProp::kCostPlotPropBitIndex_RegularCost)
				base = (1 << 2) - 1;
			return base << i;
		}

		static consteval PlotProps getPlotPropBit0(ECostPlotProp i)
		{
			return PlotProps(1) << i;
		}

		static consteval PlotProps getPlotPropMask(EValidityPlotProp i)
		{
			return PlotProps(1) << i;
		}
		
		enum class EPlotChange
		{
			Owner, // defence, route valid
			Revealed, // This team only. kPlotPropsMask_PathValidFalseMaskNotSafeTerritory
			Terrain, // defence, regular cost, half-cost mask
			Feature, // defence, regular cost, half-cost mask
			RouteType, // kPlotPropsMask_RouteTypeCostSel, regular cost
			Improvement, // defence (fort), kPlotPropsMask_IsAnimalAvoid
			HasBonus, // Barbarian only. kPlotPropsMask_IsAnimalAvoid
			HasUnits, // Barbarian only. kPlotPropsMask_IsAnimalAvoid
			HasCity, // defence, kPlotPropsMask_IsCity
			CityIsOccupation, // defence
			CityBuildingDefense, // defence
			CityCultureLevel, // defence
			InvisibilitySight, // This team only. kPlotPropsMask_IsVisibleEnemyUnit, kPlotPropsMask_IsVisibleEnemyUnitForAlwaysHostile
			UnitList, // All except this team.
		};

		explicit TeamPathingPlotPropsCache(TeamTypes teamI,
			const AllBorderPlotDangerSourceCache* borderDangerCache,
			const UnitPlotDangerCache* unitPlotDangerCache
		);

		// Routes, canDeclareWar
		void onWarChange();
		// Defence, canDeclareWar
		void onAnyVassalChange();
		// Defence
		void onOpenBordersChange();
		// Defence
		void onPlayerCityDefenceModifierChanged();
		// kPlotPropsMask_IsCantEnterAreaOrNotDomain
		// For barbarians only.
		void onAreaBorderObstacle();

		void onBridgeBuilding();

		// canDeclareWar
		void onTeamMet();
		// canDeclareWar
		void onForcePeaceChanged();
		// canDeclareWar
		void onPermanentWarPeaceChanged();
		// AI_isSneakAttackReady
		void onAI_getWarPlan_Changed();

		void onPlotChange(const CvPlot& plot, EPlotChange change);

		void update();

		/// Remember to update first!
		void verify() const;

		PlotProps calcCostPlotProps(ivec2 coord) const;
		PlotProps calcValidityPlotPropsForVerification(ivec2 coord, PlayerTypes playerI) const;

		const DynamicArray2D<PlotProps>& getCostPlotPropsArray() const
		{
			return mCostPlotPropsArray;
		}

		const DynamicArray2D<PlotProps>& getValidityPlotPropsArray() const
		{
			return mValidityPlotPropsArray;
		}

	private:
		TeamTypes mTeamI{};
		const AllBorderPlotDangerSourceCache* mBorderDangerCache = nullptr;
		const UnitPlotDangerCache* mUnitPlotDangerCache = nullptr;

		bool mWarConditionsInvalidated = true;
		std::bitset<MAX_TEAMS> mCanDeclareWar{};
		std::bitset<MAX_TEAMS> mAIIsSneakAttackReady{};
		std::bitset<MAX_TEAMS> mIsWarPlanLimited{};
		std::bitset<MAX_TEAMS> mWarBits{};
		std::bitset<MAX_TEAMS> mLastBorderDangerWarBits{};

		std::bitset<MAX_TEAMS> calcWarBits() const;

		//bool mTotallyInvalidated = true;
		InvalidationState mInvalidationState;
		//std::array<uint64_t, MAX_TEAMS> mBorderPlotDangerVersions{};
		uint64_t mBorderPlotDangerVersion = 0;
		uint64_t mUnitPlotDangerVersion = 0;
		DynamicArray2D<PlotProps> mCostPlotPropsArray;
		DynamicArray2D<PlotProps> mValidityPlotPropsArray;

		void updatePlot(const CvPlot& plot);

		PlotProps calcCostPlotProps(const CvPlot& plot) const;
		PlotProps calcValidityPlotPropsPartOne(const CvPlot& plot) const;


	};
}