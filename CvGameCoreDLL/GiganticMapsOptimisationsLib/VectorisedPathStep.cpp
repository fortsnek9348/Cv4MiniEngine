#include "../CommonConfig.h"

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS

#include "VectorisedPathStep.h"
#include "VectorisedPathfinderMap.h"
#include <CommonStuff/IntegerTypeUtil.h>

#include "../CvPlot.h"
#include "../CvUnit.h"
#include "../CvInfos.h"
#include "../CvTeamAI.h"
#include "../CvSelectionGroup.h"

#include <iostream>

using namespace GiganticMapsOptimisationsLib;

// NOTE: Careful when using this with dry run unit state. Verification may use "incorrect" live data.
static constexpr bool kEnableStepVerification = false;

static uint64_t gCounter = 0;

// Okay, we need bAttack too. There are some units that do MOVE_THROUGH_ENEMY, which need canMoveOrAttackInto, which is (canMoveInto(pPlot, false, false, false) || canMoveInto(pPlot, true, false, false))
// Needing to handle canMoveOrAttackInto is not ideal, as it's not a typically combined false mask. It is falseMask = !canMoveInto(false) && !canMoveInto(true).
// And also, canMoveInto(true) contains a defender check. Luckily, that returns false only if the pathing unit is a siege unit. So, we forbid siege units from using MOVE_THROUGH_ENEMY in this pathfinder.
// We would need to handle `canLoad` too, or not handle it. We can assume that water units never load into other units, and land units can only load into the destination plot and that the destination plot is never a tansporting unit.

bool VectorisedPathStepFunction::isSiegeUnit(const CvUnit& unit) noexcept
{
	return unit.combatLimit() < 100;
}

using enum TeamPathingPlotPropsCache::ECostPlotProp;
using enum TeamPathingPlotPropsCache::EValidityPlotProp;

static TeamPathingPlotPropsCache::PlotProps buildCanMoveIntoFalseMask(const CvUnit& unit, bool bAttack)
{
	// Default in pathValid.
	//constexpr bool bAttack = false;
	[[maybe_unused]] constexpr bool bDeclareWar = false;
	[[maybe_unused]] constexpr bool bIgnoreLoad = true;
	TeamPathingPlotPropsCache::PlotProps falseMask{};

	const CvUnitInfo& unitInfo = unit.getUnitInfo();

	//if (atPlot(pPlot))
	//{
	//	return false;
	//}

	//if (pPlot->isImpassable())
	{
		// Submarines
		if (!unit.canMoveImpassable())
			falseMask |= TeamPathingPlotPropsCache::getPlotPropMask(kValidityPlotPropBitIndex_CanMoveInto_Impassable);



	}

	// Cannot move around in unrevealed land freely
	//if (m_pUnitInfo->isNoRevealMap() && willRevealByMove(pPlot))
	//{
	//	return false;
	//}

	// We don't implement the checks for humans.
	if (unit.isHuman())
		std::abort();

	if (unitInfo.isNoRevealMap())
		std::abort();

	if (GC.getUSE_SPIES_NO_ENTER_BORDERS())
		std::abort();

	//CvArea *pPlotArea = pPlot->area();
	//TeamTypes ePlotTeam = pPlot->getTeam();
	//bool bCanEnterArea = canEnterArea(ePlotTeam, pPlotArea);
	//if (bCanEnterArea)
	//{
	//	// Sea units only.
	//	if (pPlot->getFeatureType() != NO_FEATURE)
	//	{
	//		if (m_pUnitInfo->getFeatureImpassable(pPlot->getFeatureType()))
	//		{
	//			TechTypes eTech = (TechTypes)m_pUnitInfo->getFeaturePassableTech(pPlot->getFeatureType());
	//			if (NO_TECH == eTech || !GET_TEAM(getTeam()).isHasTech(eTech))
	//			{
	//				if (DOMAIN_SEA != getDomainType() || pPlot->getTeam() != getTeam())  // sea units can enter impassable in own cultural borders
	//				{
	//					return false;
	//				}
	//			}
	//		}
	//	}
	//	else
	//	{
	//		if (m_pUnitInfo->getTerrainImpassable(pPlot->getTerrainType()))
	//		{
	//			TechTypes eTech = (TechTypes)m_pUnitInfo->getTerrainPassableTech(pPlot->getTerrainType());
	//			if (NO_TECH == eTech || !GET_TEAM(getTeam()).isHasTech(eTech))
	//			{
	//				if (DOMAIN_SEA != getDomainType() || pPlot->getTeam() != getTeam())  // sea units can enter impassable in own cultural borders
	//				{
	//					if (bIgnoreLoad || !canLoad(pPlot))
	//					{
	//						return false;
	//					}
	//				}
	//			}
	//		}
	//	}
	//}



	// Domain check, with transport load check, but this is only used on the /from/ plot, so no loading is possible.
	switch (unit.getDomainType())
	{
	case DOMAIN_SEA:
	{
		//if (!pPlot->isWater() && !canMoveAllTerrain())
		//{
		//	if (!pPlot->isFriendlyCity(*this, true) || !pPlot->isCoastalLand())
		//	{
		//		return false;
		//	}
		//}

		const TechTypes eTech = (TechTypes)unitInfo.getTerrainPassableTech(kOceanTerrainType);
		const bool isCoastHugger = unitInfo.getTerrainImpassable(kOceanTerrainType) && (NO_TECH == eTech || !GET_TEAM(unit.getTeam()).isHasTech(eTech));

		if (isCoastHugger)
			falseMask |= TeamPathingPlotPropsCache::getPlotPropMask(kValidityPlotPropBitIndex_CanMoveInto_SeaUnit_NotCoastHuggerDomain_Regular);
		else
			falseMask |= TeamPathingPlotPropsCache::getPlotPropMask(kValidityPlotPropBitIndex_CanMoveInto_SeaUnit_NotOceanDomain_Regular);

		if (unitInfo.isAlwaysHostile())
			falseMask |= TeamPathingPlotPropsCache::getPlotPropMask(kValidityPlotPropBitIndex_CanMoveInto_SeaUnit_NotCoastalCity_AlwaysHostile);

		break;
	}

	case DOMAIN_LAND:
		falseMask |= TeamPathingPlotPropsCache::getPlotPropMask(kValidityPlotPropBitIndex_CanMoveInto_IsCantEnterArea_AlwaysHostile);

		// We require that the start and destination plot is always a land plot.
		//if (pPlot->isWater() && !canMoveAllTerrain())
		//{
		//	if (!pPlot->isCity() || 0 == GC.getDefineINT("LAND_UNITS_CAN_ATTACK_WATER_CITIES"))
		//	{
		//		if (bIgnoreLoad || !isHuman() || plot()->isWater() || !canLoad(pPlot))
		//		{
		//			return false;
		//		}
		//	}
		//}
		break;

	case DOMAIN_AIR:
	case DOMAIN_IMMOBILE:
	default:
		std::abort();
	}

	if (unit.isAnimal())
	{
		falseMask |= TeamPathingPlotPropsCache::getPlotPropMask(kValidityPlotPropBitIndex_IsOwned);
		if (!bAttack)
			falseMask |= TeamPathingPlotPropsCache::getPlotPropMask(kValidityPlotPropBitIndex_CanMoveInto_IsAnimalAvoid);
	}

	if (unit.isNoCapture())
	{
		//if (!bAttack)
		{
			//if (pPlot->isEnemyCity(*this))
			{
				falseMask |= TeamPathingPlotPropsCache::getPlotPropMask(kValidityPlotPropBitIndex_CanMoveInto_IsEnemyCity);
			}
		}
	}

	if (bAttack)
	{
		if (unit.isMadeAttack() && !unit.isBlitz())
		{
			// Unconditionally returns false for all plots.
			falseMask = 0;
			falseMask = ~falseMask;
		}
	}

	// bAttack support incomplete below.

	//if (getDomainType() == DOMAIN_AIR)
	//{
	//	if (bAttack)
	//	{
	//		if (!canAirStrike(pPlot))
	//		{
	//			return false;
	//		}
	//	}
	//}
	//else
	{
		if (unit.canAttack())
		{
			if (!bAttack)
			{
				if (!unit.canCoexistWithEnemyUnit(NO_TEAM))
				{
					//if (!unit.isHuman())// || (pPlot->isVisible(getTeam(), false))) // This is implied by isVisibleEnemyUnit, right?
					{
						//if (pPlot->isVisibleEnemyUnit(&unit) != bAttack)
						{
							//FAssertMsg(isHuman() || (!bDeclareWar || (pPlot->isVisibleOtherUnit(getOwnerINLINE()) != bAttack)), "hopefully not an issue, but tracking how often this is the case when we dont want to really declare war");
							//if (!bDeclareWar || (pPlot->isVisibleOtherUnit(getOwnerINLINE()) != bAttack && !(bAttack && pPlot->getPlotCity() && !isNoCapture())))
							{
								//return false;
								falseMask |= unit.isAlwaysHostile(nullptr)
									? TeamPathingPlotPropsCache::getPlotPropMask(kValidityPlotPropBitIndex_CanMoveInto_IsVisibleEnemyUnitForAlwaysHostile)
									: TeamPathingPlotPropsCache::getPlotPropMask(kValidityPlotPropBitIndex_CanMoveInto_IsVisibleEnemyUnit);
							}
						}
					}
				}
			}
			else
			{
				//if (bAttack || !unit.canCoexistWithEnemyUnit(NO_TEAM))
				{
					//if (!unit.isHuman())// || (pPlot->isVisible(getTeam(), false))) // This is implied by isVisibleEnemyUnit, right?
					{
						//if (pPlot->isVisibleEnemyUnit(&unit) != bAttack)
						{
							//FAssertMsg(isHuman() || (!bDeclareWar || (pPlot->isVisibleOtherUnit(getOwnerINLINE()) != bAttack)), "hopefully not an issue, but tracking how often this is the case when we dont want to really declare war");
							//if (!bDeclareWar || (pPlot->isVisibleOtherUnit(getOwnerINLINE()) != bAttack && !(bAttack && pPlot->getPlotCity() && !isNoCapture())))
							{
								//return false;

								falseMask |= unit.isAlwaysHostile(nullptr)
									? TeamPathingPlotPropsCache::getPlotPropMask(kValidityPlotPropBitIndex_CanMoveInto_NotIsVisibleEnemyUnitForAlwaysHostile)
									: TeamPathingPlotPropsCache::getPlotPropMask(kValidityPlotPropBitIndex_CanMoveInto_NotIsVisibleEnemyUnit);
							}
						}
					}
				}
			}

			// We assume we can always attack any defender because we're not a siege unit.
			if (bAttack)
				if (VectorisedPathStepFunction::isSiegeUnit(unit))
					std::abort();
			//if (bAttack)
			//{
			//	CvUnit* pDefender = pPlot->getBestDefender(NO_PLAYER, getOwnerINLINE(), this, true);
			//	if (nullptr != pDefender)
			//	{
			//		if (!canAttack(*pDefender))
			//		{
			//			return false;
			//		}
			//	}
			//}
		}
		else
		{
			if (bAttack)
			{
				// Unconditionally returns false for all plots.
				falseMask = 0;
				falseMask = ~falseMask;
			}

			if (!unit.canCoexistWithEnemyUnit(NO_TEAM))
			{
				//if (!unit.isHuman() || pPlot->isVisible(getTeam(), false))
				{
					falseMask |= TeamPathingPlotPropsCache::getPlotPropMask(kValidityPlotPropBitIndex_CanMoveInto_IsEnemyCity);

					falseMask |= unit.isAlwaysHostile(nullptr)
						? TeamPathingPlotPropsCache::getPlotPropMask(kValidityPlotPropBitIndex_CanMoveInto_IsVisibleEnemyUnitForAlwaysHostile)
						: TeamPathingPlotPropsCache::getPlotPropMask(kValidityPlotPropBitIndex_CanMoveInto_IsVisibleEnemyUnit);

					//if (pPlot->isEnemyCity(*this))
					//{
					//	return false;
					//}
					//
					//if (pPlot->isVisibleEnemyUnit(this))
					//{
					//	return false;
					//}
				}
			}
		}

		//if (isHuman())
		//{
		//	ePlotTeam = pPlot->getRevealedTeam(getTeam(), false);
		//	bCanEnterArea = canEnterArea(ePlotTeam, pPlotArea);
		//}

		//if (!bCanEnterArea)
		//{
		//	FAssert(ePlotTeam != NO_TEAM);
		//
		//	if (!(GET_TEAM(getTeam()).canDeclareWar(ePlotTeam)))
		//	{
		//		return false;
		//	}
		//
		//	//if (isHuman())
		//	//{
		//	//	if (!bDeclareWar)
		//	//	{
		//	//		return false;
		//	//	}
		//	//}
		//	//else
		//	{
		//		if (GET_TEAM(getTeam()).AI_isSneakAttackReady(ePlotTeam))
		//		{
		//			if (!(getGroup()->AI_isDeclareWar(pPlot)))
		//			{
		//				return false;
		//			}
		//		}
		//		else
		//		{
		//			return false;
		//		}
		//	}
		//}

		const bool canAlwaysEnterArea = unit.isRivalTerritory() || unit.alwaysInvisible() || unitInfo.isHiddenNationality();

		if (!canAlwaysEnterArea)
		{
			const EUnitWarAIType unitWarPlan = getSingleUnitWarAIType(*unit.getGroup()->getHeadUnit());

			switch (unitWarPlan)
			{
			case EUnitWarAIType::False:
			default:
				falseMask |= TeamPathingPlotPropsCache::getPlotPropMask(kValidityPlotPropBitIndex_CanMoveInto_IsCantEnterArea_UnitWarAIType_False);
				break;
			case EUnitWarAIType::Limited:
				falseMask |= TeamPathingPlotPropsCache::getPlotPropMask(kValidityPlotPropBitIndex_CanMoveInto_IsCantEnterArea_UnitWarAIType_Limited);
				break;
			case EUnitWarAIType::True:
				falseMask |= TeamPathingPlotPropsCache::getPlotPropMask(kValidityPlotPropBitIndex_CanMoveInto_IsCantEnterArea_UnitWarAIType_True);
				break;
			}
		}
	}

	if (GC.getUSE_UNIT_CANNOT_MOVE_INTO_CALLBACK())
		std::abort();

	return falseMask;
}

namespace
{
	// This is basically all the stuff that VectorisedPathStepFunction depends on.
	struct UnitPathingKey
	{
		static constexpr uint16_t kFlag_noDefenceBonus = 1 << 0;
		static constexpr uint16_t kFlag_usePlotDanger = 1 << 1;
		static constexpr uint16_t kFlag_canFight = 1 << 2;
		//static constexpr uint16_t kFlag_isWater = 1 << 3;
		static constexpr uint16_t kFlag_halfCostMovementMask = 1 << 4;
		static constexpr uint16_t kFlag_isEnemyRoute = 1 << 6;
		static constexpr uint16_t kFlag_isAlwaysHostile = 1 << 7;
		static constexpr uint16_t kFlag_isCoastHugger = 1 << 8;
		static constexpr uint16_t kFlag_isAnimal = 1 << 9;
		static constexpr uint16_t kFlag_isNoCapture = 1 << 10;
		static constexpr uint16_t kFlag_hasMadeAllAttacks = 1 << 11;
		static constexpr uint16_t kFlag_canAttack = 1 << 12;
		static constexpr uint16_t kFlag_canCoexistWithEnemyUnit = 1 << 13;
		static constexpr uint16_t kFlag_isSiege = 1 << 14;
		static constexpr uint16_t kFlag_canAlwaysEnterArea = 1 << 15;

		uint16_t currentMP = 0;
		uint16_t maxMP = 0;
		uint16_t flags = 0;
		uint8_t extraMoveDiscount{};
		uint8_t warAIType{};

		friend auto operator<=>(UnitPathingKey, UnitPathingKey) = default;
	};

	UnitPathingKey getUnitPathingKey(const VectorisedPathfinderMap& map, const CvUnit& unit, uint32_t pathingFlags)
	{
		const CvUnitInfo& unitInfo = unit.getUnitInfo();
		const TechTypes eTech = (TechTypes)unitInfo.getTerrainPassableTech(kOceanTerrainType);
		const bool isCoastHugger = unitInfo.getTerrainImpassable(kOceanTerrainType) && (NO_TECH == eTech || !GET_TEAM(unit.getTeam()).isHasTech(eTech));
		const bool isCanMoveOrAttackInto = (pathingFlags & MOVE_THROUGH_ENEMY) != 0;

		return {
			.currentMP = static_cast<uint16_t>(std::clamp<int>(unit.movesLeft(), 0, UINT16_MAX)),
			.maxMP = static_cast<uint16_t>(std::clamp<int>(unit.baseMoves(), 0, UINT16_MAX)),
			.flags = static_cast<uint16_t>(
				(unit.noDefensiveBonus() ? UnitPathingKey::kFlag_noDefenceBonus : 0) |
				((pathingFlags & MOVE_IGNORE_DANGER) == 0 && !unit.canFight() && !unit.alwaysInvisible() ? UnitPathingKey::kFlag_usePlotDanger : 0) |
				(unit.canFight() ? UnitPathingKey::kFlag_canFight : 0) |
				(unit.isHasPromotion(map.getForestMovementPromotion()) ? UnitPathingKey::kFlag_halfCostMovementMask : 0) |
				(unit.isHasPromotion(map.getHillsMovementPromotion()) ? UnitPathingKey::kFlag_halfCostMovementMask << 1 : 0) |
				(unit.isEnemyRoute() ? UnitPathingKey::kFlag_isEnemyRoute : 0) |
				(unit.isAlwaysHostile(nullptr) ? UnitPathingKey::kFlag_isAlwaysHostile : 0) |
				(isCoastHugger ? UnitPathingKey::kFlag_isCoastHugger : 0) |
				(unit.isAnimal() ? UnitPathingKey::kFlag_isAnimal : 0) |
				(unit.isNoCapture() ? UnitPathingKey::kFlag_isNoCapture : 0) |
				(isCanMoveOrAttackInto && unit.isMadeAttack() && !unit.isBlitz() ? UnitPathingKey::kFlag_hasMadeAllAttacks : 0) |
				(unit.canAttack() ? UnitPathingKey::kFlag_canAttack : 0) |
				(unit.canCoexistWithEnemyUnit(NO_TEAM) ? UnitPathingKey::kFlag_canCoexistWithEnemyUnit : 0) |
				(isCanMoveOrAttackInto && unit.combatLimit() < 100 ? UnitPathingKey::kFlag_isSiege : 0) |
				(unit.isRivalTerritory() || unit.alwaysInvisible() || unitInfo.isHiddenNationality() ? UnitPathingKey::kFlag_canAlwaysEnterArea : 0) |
				0),
			.extraMoveDiscount = static_cast<uint8_t>(std::clamp<int>(unit.ignoreTerrainCost() || unit.getDomainType() == DOMAIN_SEA ? 999 : unit.getExtraMoveDiscount(), 0, UINT8_MAX)),
			.warAIType = static_cast<uint8_t>(getSingleUnitWarAIType(unit)),
		};
	}
}

static constexpr VectorisedPathStepFunction::StateVector kUnusableAdjRouteMaskFrom{ std::array<uint32_t, VectorisedPathStepFunction::StateVector::kNumElements>{
	TeamPathingPlotPropsCache::getPlotPropBit0(kCostPlotPropBitIndex_AdjNoRoute) << 0,
	TeamPathingPlotPropsCache::getPlotPropBit0(kCostPlotPropBitIndex_AdjNoRoute) << 1,
	TeamPathingPlotPropsCache::getPlotPropBit0(kCostPlotPropBitIndex_AdjNoRoute) << 2,
	TeamPathingPlotPropsCache::getPlotPropBit0(kCostPlotPropBitIndex_AdjNoRoute) << 3,
	TeamPathingPlotPropsCache::getPlotPropBit0(kCostPlotPropBitIndex_AdjNoRoute) << 4,
	TeamPathingPlotPropsCache::getPlotPropBit0(kCostPlotPropBitIndex_AdjNoRoute) << 5,
	TeamPathingPlotPropsCache::getPlotPropBit0(kCostPlotPropBitIndex_AdjNoRoute) << 6,
	TeamPathingPlotPropsCache::getPlotPropBit0(kCostPlotPropBitIndex_AdjNoRoute) << 7,
} };

VectorisedPathStepFunction::VectorisedPathStepFunction(const VectorisedPathfinderMap* map, const CvSelectionGroup* group, uint32_t pathingFlags, ivec2 startCoord)
	: mPlotProps(&map->getTeamPlotProps(group->getTeam()))
	, mMap(map)
	, mGroup(group)
	, mStartCoord(startCoord)
	, mPathingFlags(pathingFlags)
	, mStartPlotI(toPlotIndex(startCoord, GC.getMapINLINE().getGridWidthINLINE()))
	, mIsWater(group->getDomainType() == DOMAIN_SEA)
	, mUsePlotDanger((pathingFlags & MOVE_IGNORE_DANGER) == 0 && !group->canFight() && !group->alwaysInvisible())
	///, mUnit(unit)
	///
	///
	///, defenceMask(unit->noDefensiveBonus() ? 0 : TeamPathingPlotPropsCache::getPlotPropMask(kCostPlotPropBitIndex_Defence))
	/////, isAttackingDest = unit->canFight() && unit->canAttack() && GC.getMapINLINE().plot(destCoord.x, destCoord.y)->isVisibleEnemyUnit(unit),
	///, usePlotDanger((pathingFlags & MOVE_IGNORE_DANGER) == 0 && !unit->canFight() && !unit->alwaysInvisible())
	///, canFight(unit->canFight())
	///
	///
	/////, destPlotI = toPlotIndex(destCoord, GC.getMapINLINE().getGridWidthINLINE()),
	///// For water units, act like ignoreTerrainCost for 60MP per plot.
	///, extraMoveDiscount((unit->ignoreTerrainCost() || unit->getDomainType() == DOMAIN_SEA ? 999 : unit->getExtraMoveDiscount()) * MOVE_DENOMINATOR)
	///, maxMP(unit->maxMoves())
	///, halfCostMovementMask(
	///	(unit->isHasPromotion(map->getForestMovementPromotion()) ? TeamPathingPlotPropsCache::getPlotPropMask(kCostPlotPropBitIndex_ForestHalfCostMovement) : 0) |
	///	(unit->isHasPromotion(map->getHillsMovementPromotion()) ? TeamPathingPlotPropsCache::getPlotPropMask(kCostPlotPropBitIndex_HillsHalfCostMovement) : 0)
	///)
	///, pathValidCanMoveIntoWithoutAttackFalseMask(buildCanMoveIntoFalseMask(*unit, false))
	///, pathValidCanMoveIntoWithAttackFalseMask(pathValidCanMoveIntoWithoutAttackFalseMask)
	///, unusableRoutesMaskFrom(kUnusableAdjRouteMaskFrom | (!unit->isEnemyRoute() ? TeamPathingPlotPropsCache::getPlotPropMask(kCostPlotPropBitIndex_IsEnemyTeam) : 0))
	///, unusableRoutesMaskTo(/*kUnusableAdjRouteMaskTo |*/ (!unit->isEnemyRoute() ? TeamPathingPlotPropsCache::getPlotPropMask(kCostPlotPropBitIndex_IsEnemyTeam) : 0))
{
	// Stack reduction.

	struct Entry
	{
		UnitPathingKey key;
		const CvUnit* unit{};
		size_t index{};
	};

	std::vector<Entry> units;
	size_t index = 0;
	for (auto* node = group->headUnitNode(); node; node = group->nextUnitNode(node), ++index)
	{
		const CvUnit& unit = *::getUnit(node->m_data);
		units.emplace_back(getUnitPathingKey(*map, unit, pathingFlags), &unit, index);
	}

	std::ranges::sort(units, std::less<>(), [](const Entry& entry) {
		return std::pair(entry.key, entry.index);
		});

	// Eliminate all except the *first* element, of each UnitPathingKey group (keep lowest index).
	units.erase(std::ranges::unique(units, std::equal_to(), &Entry::key).begin(), units.end());

	// Final sort by index. This *does* affect path costs!
	std::ranges::sort(units, std::less<>(), &Entry::index);

	mUnitEvalConfigs.reserve(units.size());
	for (const Entry& entry : units)
	{
		const CvUnit* const unit = entry.unit;
		UnitEvalConfig config{
			.unit = unit,
			.canFight = unit->canFight(),
			.defenceMask = unit->noDefensiveBonus() ? 0 : TeamPathingPlotPropsCache::getPlotPropMask(kCostPlotPropBitIndex_Defence),
			.extraMoveDiscount = (unit->ignoreTerrainCost() || unit->getDomainType() == DOMAIN_SEA ? 999 : unit->getExtraMoveDiscount()) * MOVE_DENOMINATOR,
			.maxMP = unit->maxMoves(),
			.halfCostMovementMask =
				(unit->isHasPromotion(map->getForestMovementPromotion()) ? TeamPathingPlotPropsCache::getPlotPropMask(kCostPlotPropBitIndex_ForestHalfCostMovement) : 0)
				| (unit->isHasPromotion(map->getHillsMovementPromotion()) ? TeamPathingPlotPropsCache::getPlotPropMask(kCostPlotPropBitIndex_HillsHalfCostMovement) : 0),
			.pathValidCanMoveIntoWithoutAttackFalseMask = buildCanMoveIntoFalseMask(*unit, false),
			.pathValidCanMoveIntoWithAttackFalseMask{},
			.unusableRoutesMaskFrom = kUnusableAdjRouteMaskFrom | (!unit->isEnemyRoute() ? TeamPathingPlotPropsCache::getPlotPropMask(kCostPlotPropBitIndex_IsEnemyTeam) : 0),
			.unusableRoutesMaskTo = (!unit->isEnemyRoute() ? TeamPathingPlotPropsCache::getPlotPropMask(kCostPlotPropBitIndex_IsEnemyTeam) : 0),
			.evaluatedRouteCosts = calcRouteCostCombinations(getTeamRouteChanges(group->getTeam()), unit->maxMoves()),
		};

		config.pathValidCanMoveIntoWithAttackFalseMask = config.pathValidCanMoveIntoWithoutAttackFalseMask;

		const TeamPathingPlotPropsCache::PlotProps pathValidCommonFalseMask = (pathingFlags & MOVE_SAFE_TERRITORY ? TeamPathingPlotPropsCache::getPlotPropMask(kValidityPlotPropBitIndex_NotSafeTerritory) : 0) |
			(pathingFlags & MOVE_NO_ENEMY_TERRITORY ? TeamPathingPlotPropsCache::getPlotPropMask(kValidityPlotPropBitIndex_IsEnemyTeam) : 0);
		config.pathValidCanMoveIntoWithoutAttackFalseMask |= pathValidCommonFalseMask;
		config.pathValidCanMoveIntoWithAttackFalseMask |= pathValidCommonFalseMask;

		if (pathingFlags & MOVE_THROUGH_ENEMY)
		{
			// pathValid uses canMoveOrAttackInto here which sets bIgnoreLoad = false. This is different from canMoveThrough, but we ignore unit loading inside the pathfinder.
			// So pathValidCanMoveIntoWithoutAttackFalseMask should be the same.
			config.pathValidCanMoveIntoWithAttackFalseMask = buildCanMoveIntoFalseMask(*unit, true);

			// NOTE: pathValidCanMoveIntoWithAttackFalseMask may be UINT32_MAX here.
			//       This signifies that canMoveInto will always return false.
			//       This should work without further changes, as in the validity evaluation function,
			//       the false mask is and'ed with pathValidCanMoveIntoWithoutAttackFalseMask's.
		}

		if (unit->getDomainType() == DOMAIN_SEA)
			config.evaluatedRouteCosts = MOVE_DENOMINATOR; // Ignore roads.

		//if (isAttackingDest)
		//{
		//	CvMap& map = GC.getMapINLINE();
		//	const CvPlot& toPlot = *map.plot(destCoord.x, destCoord.y);
		//	std::bitset<Vector::kNumElements> evaluatedAttackDestUseMPLeft{};
		//	for (const int adjI : range(8))
		//	{
		//		const ivec2 adjD = kAdj[adjI];
		//		if (const CvPlot* const fromPlot = GC.getMapINLINE().plot(destCoord.x - adjD.x, destCoord.y - adjD.y))
		//		{
		//			int iCost = (PATH_DEFENSE_WEIGHT * std::max(0, (200 - ((unit->noDefensiveBonus()) ? 0 : fromPlot->defenseModifier(unit->getTeam(), false)))));
		//
		//			if (!(fromPlot->isCity()))
		//			{
		//				iCost += PATH_CITY_WEIGHT;
		//			}
		//
		//			if (fromPlot->isRiverCrossing(directionXY(fromPlot, &toPlot)))
		//			{
		//				if (!(unit->isRiver()))
		//				{
		//					iCost += (PATH_RIVER_WEIGHT * -(GC.getRIVER_ATTACK_MODIFIER()));
		//					//iCost += (PATH_MOVEMENT_WEIGHT * remainingMP);
		//					evaluatedAttackDestUseMPLeft[adjI] = true;
		//				}
		//			}
		//
		//			evaluatedAttackDestCosts.setElement(adjI, iCost);
		//		}
		//	}
		//	evaluatedAttackDestUseMPLeft = Vector::Mask(evaluatedAttackDestUseMPLeft);
		//}

		mUnitEvalConfigs.push_back(config);
	}
}

void VectorisedPathStepFunction::setStartPlot(ivec2 coord)
{
	mStartCoord = coord;
	mStartPlotI = toPlotIndex(coord, getDim());
}

uint32_t VectorisedPathStepFunction::getTeamCostPlotPropsReadMask() const
{
	uint32_t result{};

	for (const UnitEvalConfig& config : mUnitEvalConfigs)
	{
		// Mostly validity.
		const uint32_t fromMask = 0;
		;
		const uint32_t toMask = TeamPathingPlotPropsCache::getPlotPropMask(kCostPlotPropBitIndex_RegularCost)
			| config.halfCostMovementMask
			| TeamPathingPlotPropsCache::getPlotPropMask(kCostPlotPropBitIndex_NotMyTeam)
			| (config.canFight ? config.defenceMask : 0)
			;
		constexpr uint32_t routeMask = TeamPathingPlotPropsCache::getPlotPropMask(kCostPlotPropBitIndex_RouteType)
			| TeamPathingPlotPropsCache::getPlotPropMask(kCostPlotPropBitIndex_AdjNoRoute);
		;

		result |= fromMask | toMask | routeMask;
	}

	return result;
}

uint32_t VectorisedPathStepFunction::getTeamValidityPlotPropsReadMask() const
{
	uint32_t result{};

	for (const UnitEvalConfig& config : mUnitEvalConfigs)
	{

		// Mostly validity.
		const uint32_t fromMask = config.pathValidCanMoveIntoWithoutAttackFalseMask
			| config.pathValidCanMoveIntoWithAttackFalseMask
			| (mUsePlotDanger ? TeamPathingPlotPropsCache::getPlotPropMask(kValidityPlotPropBitIndex_BorderPlotDanger) : 0)
			| (mUsePlotDanger ? TeamPathingPlotPropsCache::getPlotPropMask(kValidityPlotPropBitIndex_UnitPlotDanger) : 0)
			;
		const uint32_t toMask = 0;
		constexpr uint32_t routeMask = 0;

		result |= fromMask | toMask | routeMask;
	}

	return result;
}

int VectorisedPathStepFunction::getMaxMP() const
{
	return std::ranges::max(mUnitEvalConfigs | std::views::transform(&UnitEvalConfig::maxMP));
}

ivec2 VectorisedPathStepFunction::getDim() const
{
	return mPlotProps->getCostPlotPropsArray().dim;
}

heck::ISimdAStarGraph::Step HECK_VECTORCALL VectorisedPathStepFunction::getStepCost(StateVector fromState, Vector fromPlotIndices, Vector toPlotIndices, Vector adjI, bool disableVerify) const
{
	// TODO: Sligth inefficiency. Allocation.
	return getStepCost(makeStepFromInfo(fromState, fromPlotIndices, StepFromInfo()), toPlotIndices, adjI, disableVerify);
}

VectorisedPathStepFunction::StepFromInfo HECK_VECTORCALL VectorisedPathStepFunction::makeStepFromInfo(StateVector fromState, Vector fromPlotIndices, StepFromInfo&& stepFromInfo) const
{
	using PropsVector = StateVector;
	const PropsVector fromCostPlotProps = PropsVector(mPlotProps->getCostPlotPropsArray().cells, fromPlotIndices);
	const PropsVector fromValidityPlotProps = PropsVector(mPlotProps->getValidityPlotPropsArray().cells, fromPlotIndices);
	//const StateVector::Mask potentialWaterDiagonalSpecialCase = vcmpeq(fromValidityPlotProps & TeamPathingPlotPropsCache::getPlotPropMask(kValidityPlotPropBitIndex_PotentialWaterDiagonalSpecialCase));
	const Vector fromMP = Vector(fromState & kUnitStateMPMask);
	const Vector fromTurns = Vector(fromState >> std::integral_constant<size_t, kUnitStateTurnsShift>());

	stepFromInfo.anyUnit = {
		.fromPlotIndices = fromPlotIndices,
		.fromCostPlotProps = fromCostPlotProps,
		.fromValidityPlotProps = fromValidityPlotProps,
		.fromMP = fromMP,
		.fromTurns = fromTurns,

		//.fromState = fromState,
		//.fromPlotIndices = fromPlotIndices,
	};

	const StateVector::Mask validityTrueMask = vcmpeq(fromPlotIndices, mStartPlotI);

	

	stepFromInfo.perUnit.assign_range(mUnitEvalConfigs | std::views::transform([&](const UnitEvalConfig& config) -> PerUnitStepFromInfo {
	
		StateVector::Mask validityFalseMask =
			vtest(fromValidityPlotProps, config.pathValidCanMoveIntoWithoutAttackFalseMask) &
			vtest(fromValidityPlotProps, config.pathValidCanMoveIntoWithAttackFalseMask)
			;

		// Plot danger is conditional on MP.
		if (mUsePlotDanger)
		{
			// if ((parent->m_iData2 > 1) || (parent->m_iData1 == 0))
			const Vector::Mask dangerCheckMask = vcmplt(1, fromTurns) | vcmpeq(fromMP, 0);
			validityFalseMask |= dangerCheckMask & vtest(fromValidityPlotProps,
				TeamPathingPlotPropsCache::getPlotPropMask(kValidityPlotPropBitIndex_BorderPlotDanger) |
				TeamPathingPlotPropsCache::getPlotPropMask(kValidityPlotPropBitIndex_UnitPlotDanger));
		}

		// kPlotPropsCmp_WaterDiagonalSpecialCase should be cleared for plots where potentialWaterDiagonalSpecialCase.
		//fromPlotProps = vselect(potentialWaterDiagonalSpecialCase, fromPlotProps & ~TeamPathingPlotPropsCache::kPlotPropsCmp_WaterDiagonalSpecialCase, fromPlotProps);

		return {
			.validityMask = validityTrueMask | ~validityFalseMask
		};
		}));

	return stepFromInfo;
}


heck::ISimdAStarGraph::Step HECK_VECTORCALL VectorisedPathStepFunction::getStepCost(const StepFromInfo& stepFromInfo, Vector toPlotIndices, Vector adjI, bool disableVerify) const
{
	using PropsVector = StateVector;
	const PropsVector toPlotProps(mPlotProps->getCostPlotPropsArray().cells, toPlotIndices);
	return getStepCostWithProps(stepFromInfo, toPlotIndices, adjI, toPlotProps, disableVerify);
}

heck::ISimdAStarGraph::Step HECK_VECTORCALL VectorisedPathStepFunction::getStepCostWithProps(
	const VectorisedPathStepFunction::StepFromInfo& stepFromInfo, Vector toPlotIndices, Vector adjI, StateVector toCostPlotProps, bool disableVerify) const
{
	using PropsVector = StateVector; // unsigned

	if constexpr (kEnableStepVerification)
	{
		++gCounter;
		//if (gCounter == 634757)
		//	_CrtDbgBreak();
	}

	

	const PropsVector fromCostPlotProps = stepFromInfo.anyUnit.fromCostPlotProps;
	//const PropsVector fromValidityPlotProps = stepFromInfo.anyUnit.fromValidityPlotProps;

	//const Vector fromMP = Vector(fromState & kUnitStateMPMask);
	//const Vector fromTurns = Vector(fromState >> std::integral_constant<size_t, kUnitStateTurnsShift>());
	const Vector fromMP = stepFromInfo.anyUnit.fromMP;
	const Vector fromTurns = stepFromInfo.anyUnit.fromTurns;
	const Vector::Mask exhaustedMPMask = vcmpeq(fromMP, 0);

	//if constexpr (kEnableStepVerification)
	//	if (gCounter == 511992)
	//	{
	//		//calc_pathCost(*mGroup, mPathingFlags, fromTurns.getElement(0), fromMP.getElement(0), { 59, 15 }, { 58, 14 }, { -1, -1 });
	//		calc_pathValid(*mGroup, mStartCoord, mPathingFlags, fromTurns.getElement(10), fromMP.getElement(10), { 39, 25 }, { 38, 24 });
	//		_CrtDbgBreak();
	//	}

	// pathCost
	// A bit messy. Probably because it isn't truly correct.
	// The "logic" is like this:
	// (iWorstCost, iWorstMovesLeft, iWorstMax) = INT_MAX
	// for each unit:
	//     iMovesLeft = max(0, maxMP - movement MP cost)
	//     if iMovesLeft <= iWorstMovesLeft && (iMovesLeft < iWorstMovesLeft || maxMP <= iWorstMax): // (iMovesLeft, maxMP) <= (iWorstMovesLeft, iWorstMax)
	//         iCost = calculate path cost for this unit
	//         if iCost < iWorstCost: // best cost, perhaps?
	//             (iWorstCost, iWorstMovesLeft, iWorstMax) = (iCost, iMovesLeft, maxMP)
	// return iWorstCost + PATH_STEP_WEIGHT + diagonal penalty

	Vector worstCost = INT_MAX;
	Vector worstMovesLeft = INT_MAX;
	Vector worstMax = INT_MAX;
	Vector toMP = INT_MAX;

	Vector::Mask validMask = Vector::Mask::kAll; //getPlotDangerValidMask(stepFromInfo);

	for (const auto& [perUnitStepfromInfo, config] : std::views::zip(stepFromInfo.perUnit, mUnitEvalConfigs))
	{
		const Vector plotMPCost = getPlotMPCost(fromCostPlotProps, toCostPlotProps, adjI, config);
		const Vector initialMP = vselect(exhaustedMPMask, config.maxMP, fromMP);
		const Vector usedMP = vmin(plotMPCost, initialMP);
		const Vector remainingMP = initialMP - usedMP; // movesLeft
		const Vector::Mask waypointMask = vcmpeq(remainingMP, 0);

		const Vector::Mask worseMask = validMask & vcmple(remainingMP, worstMovesLeft) & (vcmplt(remainingMP, worstMovesLeft) | vcmple(initialMP, worstMax));
		if (worseMask.any())
		{
			Vector unitStepCost = usedMP * PATH_MOVEMENT_WEIGHT;
			unitStepCost = vmaskedadd(waypointMask & vtest(toCostPlotProps, TeamPathingPlotPropsCache::getPlotPropMask(kCostPlotPropBitIndex_NotMyTeam)), unitStepCost, PATH_TERRITORY_WEIGHT);

			// TODO: Can probably get rid of this check.
			if (config.canFight)
				unitStepCost = vmaskedadd(waypointMask, unitStepCost, (200 - Vector(toCostPlotProps & config.defenceMask)) * PATH_DEFENSE_WEIGHT);

			const Vector::Mask assignMask = worseMask & vcmplt(unitStepCost, worstCost);
			worstCost = vselect(assignMask, unitStepCost, worstCost);
			worstMovesLeft = vselect(assignMask, remainingMP, worstMovesLeft);
			worstMax = vselect(assignMask, initialMP, worstMax);
		}

		validMask &= getPlotValidMask(stepFromInfo.anyUnit, perUnitStepfromInfo, adjI);

		toMP = vmin(toMP, remainingMP);
	}

	const Vector pathCost = worstCost + Vector(Vector::Array{
		PATH_STEP_WEIGHT + PATH_STRAIGHT_WEIGHT, PATH_STEP_WEIGHT, PATH_STEP_WEIGHT + PATH_STRAIGHT_WEIGHT,
			PATH_STEP_WEIGHT, PATH_STEP_WEIGHT,
			PATH_STEP_WEIGHT + PATH_STRAIGHT_WEIGHT, PATH_STEP_WEIGHT, PATH_STEP_WEIGHT + PATH_STRAIGHT_WEIGHT }).permute(adjI);

	const Vector cost = vselect(validMask, pathCost, 0);

	// pathAdd
	const Vector toTurns = vmaskedadd(exhaustedMPMask, fromTurns, 1);

	if constexpr (kEnableStepVerification)
		if (!disableVerify)
			verifyPathingStep(fromTurns, fromMP, stepFromInfo.anyUnit.fromPlotIndices, toPlotIndices, adjI, cost, toTurns, toMP);

	return {
		.cost = cost,
		.state = StateVector(toMP) + (StateVector(toTurns) << std::integral_constant<size_t, kUnitStateTurnsShift>()),
	};
}

VectorisedPathStepFunction::Vector HECK_VECTORCALL VectorisedPathStepFunction::getPlotMPCost(
	StateVector fromPlotProps, StateVector toPlotProps, Vector adjI, const UnitEvalConfig& config) const
{
	Vector regularCost;

	regularCost = Vector((toPlotProps & TeamPathingPlotPropsCache::getPlotPropMask(kCostPlotPropBitIndex_RegularCost))
		>> std::integral_constant<size_t, kCostPlotPropBitIndex_RegularCost>()) * MOVE_DENOMINATOR;
	regularCost = regularCost - config.extraMoveDiscount; // handles ignoreTerrainCost too
	const auto bHasTerrainCost = vcmplt(MOVE_DENOMINATOR, regularCost);
	regularCost = vmax(MOVE_DENOMINATOR, regularCost);
	regularCost = vmin(regularCost, config.maxMP);
	// regularCost compare might not be needed if half cost only happens on hills/forest. Careful with extraMoveDiscount. If extraMoveDiscount active, disable half cost.
	regularCost = vmaskedshr(vtest(toPlotProps, config.halfCostMovementMask) & bHasTerrainCost, regularCost, heck::simd::imm<1>); // kPlotPropsHalfCostTerrainMovement

	const StateVector::Mask unusableRoutesMask =
		vtest(fromPlotProps, config.unusableRoutesMaskFrom.permute(StateVector(adjI))) |
		vtest(toPlotProps, config.unusableRoutesMaskTo.permute(StateVector(adjI)));
	constexpr auto kRouteTypeMask = TeamPathingPlotPropsCache::getPlotPropMask(TeamPathingPlotPropsCache::kCostPlotPropBitIndex_RouteType);
	const StateVector usableRoutesSel = vselect(unusableRoutesMask, UINT32_MAX,
		((fromPlotProps & kRouteTypeMask) |
		(toPlotProps & kRouteTypeMask) << heck::simd::imm<1>) >> heck::simd::imm<TeamPathingPlotPropsCache::kCostPlotPropBitIndex_RouteType>);
	const Vector routeCosts = config.evaluatedRouteCosts.permute(Vector(usableRoutesSel));
	// When no route is usable, the sel is -1, which will yield a high cost.
	regularCost = vmin(regularCost, routeCosts);

	return regularCost;
}

static constexpr VectorisedPathStepFunction::StateVector kWaterDiagonalSpecialCaseAdjMask{ std::array<uint32_t, VectorisedPathStepFunction::kCoordVectorLength>{
	TeamPathingPlotPropsCache::getPlotPropMask(kValidityPlotPropBitIndex_WaterDiagonalForbid0),
	0,
	TeamPathingPlotPropsCache::getPlotPropMask(kValidityPlotPropBitIndex_WaterDiagonalForbid1),
	0,
	0,
	TeamPathingPlotPropsCache::getPlotPropMask(kValidityPlotPropBitIndex_WaterDiagonalForbid2),
	0,
	TeamPathingPlotPropsCache::getPlotPropMask(kValidityPlotPropBitIndex_WaterDiagonalForbid3),
} };

VectorisedPathStepFunction::Vector::Mask HECK_VECTORCALL VectorisedPathStepFunction::getPlotDangerValidMask(const StepFromInfo& stepFromInfo) const
{
	// Plot danger is conditional on MP.
	if (mUsePlotDanger)
	{
		// if ((parent->m_iData2 > 1) || (parent->m_iData1 == 0))
		const Vector::Mask dangerCheckMask = vcmplt(1, stepFromInfo.anyUnit.fromTurns) | vcmpeq(stepFromInfo.anyUnit.fromMP, 0);
		return ~(dangerCheckMask & vtest(stepFromInfo.anyUnit.fromValidityPlotProps,
			TeamPathingPlotPropsCache::getPlotPropMask(kValidityPlotPropBitIndex_BorderPlotDanger) |
			TeamPathingPlotPropsCache::getPlotPropMask(kValidityPlotPropBitIndex_UnitPlotDanger)));
	}
	else
		return Vector::Mask::kAll;
}

VectorisedPathStepFunction::Vector::Mask HECK_VECTORCALL VectorisedPathStepFunction::getPlotValidMask(
	const AnyUnitStepFromInfo& stepFromInfo, const PerUnitStepFromInfo& perUnitStepFromInfo, Vector adjI) const
{
	VectorisedPathStepFunction::Vector::Mask waterSpecialCaseFalseMask{};


	if (mIsWater)
		waterSpecialCaseFalseMask = vtest(stepFromInfo.fromValidityPlotProps, kWaterDiagonalSpecialCaseAdjMask.permute(adjI));

	// This check could be removed if A* was started with adjacent plots instead of the start plot.
	//const Vector::Mask trueMask = vcmpeq(stepFromInfo.anyUnit.fromPlotIndices, mStartPlotI);
	//
	//StateVector::Mask falseMask =
	//	vtest(stepFromInfo.anyUnit.fromValidityPlotProps, config.pathValidCanMoveIntoWithoutAttackFalseMask) &
	//	vtest(stepFromInfo.anyUnit.fromValidityPlotProps, config.pathValidCanMoveIntoWithAttackFalseMask)
	//	;

	// Plot danger is conditional on MP.
	//if (mUsePlotDanger)
	//{
	//	// if ((parent->m_iData2 > 1) || (parent->m_iData1 == 0))
	//	const Vector::Mask dangerCheckMask = vcmplt(1, fromTurns) | vcmpeq(fromMP, 0);
	//	falseMask |= dangerCheckMask & vtest(fromPlotProps,
	//		TeamPathingPlotPropsCache::getPlotPropMask(kValidityPlotPropBitIndex_BorderPlotDanger) |
	//		TeamPathingPlotPropsCache::getPlotPropMask(kValidityPlotPropBitIndex_UnitPlotDanger));
	//}

	//return ~waterSpecialCaseFalseMask & (trueMask | ~falseMask);
	return ~waterSpecialCaseFalseMask & perUnitStepFromInfo.validityMask;
}

void VectorisedPathStepFunction::verifyPathingStep(
	Vector fromTurnsVec, Vector fromMPVec,
	Vector fromPlotIndices, Vector toPlotIndices, Vector adjI,
	Vector actualCostVec, Vector toTurns, Vector toMP) const
{
	const Vector::Mask actualValidMask = vcmplt(0, actualCostVec);

	const ivec2 dim = CivMapGeometry(GC.getMapINLINE()).dim;

	//if (gCounter == 13247251)
	//	__debugbreak();

	for (const size_t i : range(Vector::kNumElements))
	{
		const ivec2 fromCoord = toPlotCoord(fromPlotIndices.getElement(i), dim);
		const ivec2 toCoord = toPlotCoord(toPlotIndices.getElement(i), dim);

		// Default value when step goes outside map. Ignore.
		if (fromCoord == toCoord)
			continue;

		//if (mPathSerial == 13 && fromCoord == ivec2(287, 345) && toCoord == ivec2(288, 346))
		//	_CrtDbgBreak();


		const int fromTurns = fromTurnsVec.getElement(i);
		const int fromMP = fromMPVec.getElement(i);

		const bool expectedValid = calc_pathValid(*mGroup, mStartCoord, mPathingFlags, fromTurns, fromMP, fromCoord, toCoord);
		//const int expectedCost = expectedValid ? calc_pathCost(*mUnit->getGroup(), pathingFlags, fromTurns, fromMP, fromCoord, toCoord, mDestCoord) : 0;
		// Using INVALID_PLOT_COORD for the goal. We do goal-oblivious path costs here.
		const int expectedCost = expectedValid ? calc_pathCost(*mGroup, mPathingFlags, fromTurns, fromMP, fromCoord, toCoord, INVALID_PLOT_COORD) : 0;
		const auto [expectedMP, expectedTurns] = calc_pathAdd(*mGroup, mPathingFlags, fromTurns, fromMP, fromCoord, toCoord);

		const bool actualValid = bool((actualValidMask.asBitsUInt() >> i) & 1);
		const int actualCost = actualCostVec.getElement(i);
		const int actualMP = toMP.getElement(i);
		const int actualTurns = toTurns.getElement(i);

		if (actualValid != expectedValid || actualCost != expectedCost)
		{
			CvMap& map = GC.getMapINLINE();

			// For multithreaded unit update...
			static std::mutex mutex;
			const std::lock_guard lock(mutex);

			std::clog << "VectorisedPathfinderGraph step mismatch!" << std::endl;
			std::clog << "gCounter = " << gCounter << std::endl;
			std::wclog << L"Group at: " << getFullPlotDescription(*mGroup->plot()) << std::endl;
			std::wclog << L"Start   : " << getFullPlotDescription(*map.plot(mStartCoord.x, mStartCoord.y)) << std::endl; // Can be different due to parallel unit update
			std::wclog << L"From: " << getFullPlotDescription(*map.plot(fromCoord.x, fromCoord.y)) << std::endl;
			std::wclog << L"To  : " << getFullPlotDescription(*map.plot(toCoord.x, toCoord.y)) << std::endl;
			std::clog << "Live valid = " << expectedValid << ", cost = " << expectedCost << std::endl;
			std::clog << "Our  valid = " << actualValid   << ", cost = " << actualCost << " (difference=" << actualCost - expectedCost << ")" << std::endl;
			std::clog << "From    Turns:MP=" << fromTurns << ':' << fromMP << std::endl;
			std::clog << "Live To Turns:MP=" << expectedTurns << ':' << expectedMP << std::endl;
			std::clog << "Our  To Turns:MP=" << actualTurns << ':' << actualMP << std::endl;

			const uint32_t actualFromCostPlotProps = mPlotProps->getCostPlotPropsArray()[fromCoord];
			const uint32_t actualFromValidityPlotProps = mPlotProps->getValidityPlotPropsArray()[fromCoord];
			const uint32_t actualToCostPlotProps = mPlotProps->getCostPlotPropsArray()[toCoord];
			const uint32_t expectedFromCostPlotProps = mPlotProps->calcCostPlotProps(fromCoord);
			const uint32_t expectedFromValidityPlotProps = mPlotProps->calcValidityPlotPropsForVerification(fromCoord, mGroup->getOwnerINLINE());
			const uint32_t expectedToCostPlotProps = mPlotProps->calcCostPlotProps(toCoord);
			std::clog << "flags=" << mPathingFlags << std::endl;
			std::clog << "adjI=" << adjI.getElement(i) << ", element=" << i << std::endl;
			std::clog << "Cost                     " << TeamPathingPlotPropsCache::kCostPlotPropsDescString << std::endl;
			std::clog << "Cached From plot props : " << std::bitset<32>(actualFromCostPlotProps) << std::endl;
			std::clog << "Live   From plot props : " << std::bitset<32>(expectedFromCostPlotProps) << std::endl;
			std::clog << "Cached To plot props   : " << std::bitset<32>(actualToCostPlotProps) << std::endl;
			std::clog << "Live   To plot props   : " << std::bitset<32>(expectedToCostPlotProps) << std::endl;
			std::clog << "Validity                 " << TeamPathingPlotPropsCache::kValidityPlotPropsDescString << std::endl;
			std::clog << "Cached From plot props : " << std::bitset<32>(actualFromValidityPlotProps) << std::endl;
			std::clog << "Live   From plot props : " << std::bitset<32>(expectedFromValidityPlotProps) << std::endl;

			for (const UnitEvalConfig& config : mUnitEvalConfigs)
			{
				std::wclog << L"Unit: " << config.unit->getName() << std::endl;
				std::clog << "\tValidity                  " << TeamPathingPlotPropsCache::kValidityPlotPropsDescString << std::endl;
				std::clog << "\tPath valid false mask -A: " << std::bitset<32>(config.pathValidCanMoveIntoWithoutAttackFalseMask) << std::endl;
				std::clog << "\tPath valid false mask +A: " << std::bitset<32>(config.pathValidCanMoveIntoWithAttackFalseMask) << std::endl;
				//for (const int i : range(kCoordVectorLength))
				//	std::clog << "\tEvaluated route cost " << i << ": " << config.evaluatedRouteCosts.getElement(i) << std::endl;
				std::clog << "\tUnitAI = " << config.unit->AI_getUnitAIType() << std::endl;
				std::clog << "\tAI_isSneakAttackReady = " << (map.plot(toCoord.x, toCoord.y)->getTeam() != NO_TEAM && GET_TEAM(mGroup->getTeam()).AI_isSneakAttackReady(map.plot(toCoord.x, toCoord.y)->getTeam())) << std::endl;
				std::clog << "\tAI_isDeclareWar = " << mGroup->AI_isDeclareWar(map.plot(toCoord.x, toCoord.y)) << std::endl;
				std::clog << "\tgetSingleUnitWarAIType = " << (int)getSingleUnitWarAIType(*config.unit) << std::endl;
			}

			//fastarIFace.SetData(&fastar, mUnit->getGroup());
			//FAStarNode* const fromNode = fastarIFace.getNode(&fastar, fromCoord.x, fromCoord.y);
			//FAStarNode* const toNode = fastarIFace.getNode(&fastar, toCoord.x, toCoord.y);
			//fromNode->m_iData1 = fromState.getElement(i) & kUnitStateMPMask;
			//fromNode->m_iData2 = fromState.getElement(i) >> kUnitStateTurnsShift;

			//(void)pathValid(fromNode, toNode, 0, mUnit->getGroup(), &fastar);
			//using PropsVector = StateVector; // unsigned
			//const PropsVector fromPlotProps(mPlotProps.getArray().cells, fromPlotIndices);
			//const PropsVector toPlotProps(mPlotProps.getArray().cells, toPlotIndices);
			//(void)getPlotMPCost(fromPlotProps, toPlotProps);
			//(void)pathCost(fromNode, toNode, 0, mUnit->getGroup(), &fastar);
			//(void)getStepCost(fromState, fromPlotIndices, toPlotIndices, adjI);

			

			(void)calc_pathCost(*mGroup, mPathingFlags, fromTurns, fromMP, fromCoord, toCoord, INVALID_PLOT_COORD);
			(void)calc_pathValid(*mGroup, mStartCoord, mPathingFlags, fromTurns, fromMP, fromCoord, toCoord);

			if (((expectedFromValidityPlotProps ^ actualFromValidityPlotProps) & TeamPathingPlotPropsCache::getPlotPropMask(kValidityPlotPropBitIndex_BorderPlotDanger)))
			{
				std::clog << "Border plot danger changed." << std::endl;
				(void)mPlotProps->calcValidityPlotPropsForVerification(fromCoord, mGroup->getOwnerINLINE());
			}

			if (((expectedFromValidityPlotProps ^ actualFromValidityPlotProps) & TeamPathingPlotPropsCache::getPlotPropMask(kValidityPlotPropBitIndex_UnitPlotDanger)))
			{
				std::clog << "Unit plot danger changed." << std::endl;
				(void)mPlotProps->calcValidityPlotPropsForVerification(fromCoord, mGroup->getOwnerINLINE());
			}

			//mPlotProps->verify();

			mMap->verify(mGroup->getTeam());

			std::abort();
		}
	}
}

//std::array<VectorisedPathStepFunction::StateVector, VectorisedPathStepFunction::kCoordVectorLength> VectorisedPathStepFunction::gatherPlotProps(ivec2 plotCoord) const
//{
//	const iaabb2 blkRect = iaabb2::sized(plotCoord, kCoordVectorLength);
//	std::array<StateVector, kCoordVectorLength> plotProps{};
//	const ivec2 mapDim = mPlotProps->getArray().dim;
//	const std::span plotPropsMap = mPlotProps->getArray().cells;
//
//	const auto loadPlotPropsRow = [blkRect, plotPropsMap, mapDim](int dy) {
//		return StateVector(std::span(plotPropsMap).subspan(blkRect.min.x + (blkRect.min.y + dy) * mapDim.x).subspan<0, kCoordVectorLength>());
//		};
//
//	if (blkRect.contains(iaabb2::sized({}, mapDim)))
//	{
//		for (const int dy : range(kCoordVectorLength))
//			plotProps[dy] = loadPlotPropsRow(dy);
//	}
//	else
//	{
//		// Going out of bounds. Use UINT32_MAX as default plot props value.
//		constexpr uint32_t defaultPlotProps = UINT32_MAX;
//
//		const int numRowsUntilBottomOfMap = std::min<int>(kCoordVectorLength, mapDim.y - blkRect.min.y);
//
//		if (blkRect.max.x > mapDim.x)
//		{
//			const StateVector::Mask rowInRangeMask = StateVector::Mask(heck::makeLowNBitsSet<StateVector::kNumElements>(blkRect.max.x - mapDim.x));
//			// Load the first rows, which may include elements from the next row, then mask them out.
//			// TeamPathingPlotPropsCache allocates extra, so loading the last row won't go out of bounds.
//			for (const int dy : range(numRowsUntilBottomOfMap))
//				plotProps[dy] = vselect(rowInRangeMask, loadPlotPropsRow(dy), defaultPlotProps);
//		}
//		else
//		{
//			// No masking needed.
//			for (const int dy : range(numRowsUntilBottomOfMap))
//				plotProps[dy] = loadPlotPropsRow(dy);
//		}
//
//		// Remaining rows are defaultPlotProps.
//		for (const int dy : range(numRowsUntilBottomOfMap, (int)kCoordVectorLength))
//			plotProps[dy] = defaultPlotProps;
//	}
//	return plotProps;
//}

void VectorisedPathStepFunction::verifyAt(ivec2 coord, unsigned int state) const
{
	// Avoid accidental double verification.
	if constexpr (kEnableStepVerification)
		std::abort();

	const CivMapGeometry mapGeom(GC.getMapINLINE());

	const auto [fromMP, fromTurns] = splitMPTurns(state);

	StepFromInfo scratch;

	for (const int dy : range(-1, 2))
		for (const int dx : range(-1, 2))
		{
			const ivec2 fromCoordUnwrapped = coord + ivec2(dx, dy);
			if (mapGeom.isValidCoord(fromCoordUnwrapped))
			{
				const ivec2 fromCoord = mapGeom.wrapCoord(fromCoordUnwrapped);
				const int fromPlotI = toPlotIndex(fromCoord, mapGeom.dim.x);
				//const StepFromInfo& stepFromInfo = scratch = makeStepFromInfo(state, fromPlotI, std::move(scratch));

				const heck::vec2<Vector> toCoordsUnwrapped = heck::vec2<Vector>(fromCoord) + heck::SimdAStarConfig::kAdjCoordsVector;
				const heck::vec2<Vector> toCoords = mapGeom.wrapOrClampCoords(toCoordsUnwrapped);
				const Vector toCoordIndices = toCoords.x + toCoords.y * mapGeom.dim.x;

				const heck::ISimdAStarGraph::Step step = getStepCost(StateVector(state), Vector(fromPlotI), toCoordIndices,
					vmin(Vector(heck::simd::iotaArray<int32_t, heck::SimdAStarConfig::kCoordVectorLength>()), static_cast<int>(std::size(kAdj) - 1)));

				for (const size_t adjI : range(std::size(kAdj)))
				{
					const int actualCost = step.cost.getElement(adjI);
					const auto [actualMP, actualTurns] = splitMPTurns(step.state.getElement(adjI));
					const bool actualValid = actualCost > 0;

					const ivec2 toCoord(toCoords.x.getElement(adjI), toCoords.y.getElement(adjI));

					const bool expectedValid = calc_pathValid(*mGroup, mStartCoord, mPathingFlags, fromTurns, fromMP, fromCoord, toCoord);
					// Using INVALID_PLOT_COORD for the goal. We do goal-oblivious path costs here.
					const int expectedCost = expectedValid ? calc_pathCost(*mGroup, mPathingFlags, fromTurns, fromMP, fromCoord, toCoord, INVALID_PLOT_COORD) : 0;
					const auto [expectedMP, expectedTurns] = calc_pathAdd(*mGroup, mPathingFlags, fromTurns, fromMP, fromCoord, toCoord);

					if (actualValid != expectedValid || actualCost != expectedCost || (expectedValid && (expectedMP != actualMP || expectedTurns != actualTurns)))
					{
						CvMap& map = GC.getMapINLINE();

						// For multithreaded unit update...
						static std::mutex mutex;
						const std::lock_guard lock(mutex);

						std::clog << "VectorisedPathfinderGraph step mismatch!" << std::endl;
						std::wclog << L"Group at: " << getFullPlotDescription(*mGroup->plot()) << std::endl;
						std::wclog << L"Start   : " << getFullPlotDescription(*map.plot(mStartCoord.x, mStartCoord.y)) << std::endl; // Can be different due to parallel unit update
						std::wclog << L"From: " << getFullPlotDescription(*map.plot(fromCoord.x, fromCoord.y)) << std::endl;
						std::wclog << L"To  : " << getFullPlotDescription(*map.plot(toCoord.x, toCoord.y)) << std::endl;
						std::clog << "Live valid = " << expectedValid << ", cost = " << expectedCost << std::endl;
						std::clog << "Our  valid = " << actualValid   << ", cost = " << actualCost << " (difference=" << actualCost - expectedCost << ")" << std::endl;
						std::clog << "From    Turns:MP=" << fromTurns << ':' << fromMP << std::endl;
						std::clog << "Live To Turns:MP=" << expectedTurns << ':' << expectedMP << std::endl;
						std::clog << "Our  To Turns:MP=" << actualTurns << ':' << actualMP << std::endl;

						const uint32_t actualFromCostPlotProps = mPlotProps->getCostPlotPropsArray()[fromCoord];
						const uint32_t actualFromValidityPlotProps = mPlotProps->getValidityPlotPropsArray()[fromCoord];
						const uint32_t actualToCostPlotProps = mPlotProps->getCostPlotPropsArray()[toCoord];
						const uint32_t expectedFromCostPlotProps = mPlotProps->calcCostPlotProps(fromCoord);
						const uint32_t expectedFromValidityPlotProps = mPlotProps->calcValidityPlotPropsForVerification(fromCoord, mGroup->getOwnerINLINE());
						const uint32_t expectedToCostPlotProps = mPlotProps->calcCostPlotProps(toCoord);
						std::clog << "flags=" << mPathingFlags << std::endl;
						std::clog << "adjI=" << adjI << std::endl;
						std::clog << "Validity                 " << TeamPathingPlotPropsCache::kValidityPlotPropsDescString << std::endl;
						std::clog << "Cached From plot props : " << std::bitset<32>(actualFromValidityPlotProps) << std::endl;
						std::clog << "Live   From plot props : " << std::bitset<32>(expectedFromValidityPlotProps) << std::endl;
						std::clog << "Cost                     " << TeamPathingPlotPropsCache::kCostPlotPropsDescString << std::endl;
						std::clog << "Cached From plot props : " << std::bitset<32>(actualFromCostPlotProps) << std::endl;
						std::clog << "Live   From plot props : " << std::bitset<32>(expectedFromCostPlotProps) << std::endl;
						std::clog << "Cached To plot props   : " << std::bitset<32>(actualToCostPlotProps) << std::endl;
						std::clog << "Live   To plot props   : " << std::bitset<32>(expectedToCostPlotProps) << std::endl;

						for (const UnitEvalConfig& config : mUnitEvalConfigs)
						{
							std::wclog << L"Unit: " << config.unit->getName() << std::endl;
							std::clog << "\tValidity                  " << TeamPathingPlotPropsCache::kValidityPlotPropsDescString << std::endl;
							std::clog << "\tPath valid false mask -A: " << std::bitset<32>(config.pathValidCanMoveIntoWithoutAttackFalseMask) << std::endl;
							std::clog << "\tPath valid false mask +A: " << std::bitset<32>(config.pathValidCanMoveIntoWithAttackFalseMask) << std::endl;
							//for (const int i : range(kCoordVectorLength))
							//	std::clog << "\tEvaluated route cost " << i << ": " << config.evaluatedRouteCosts.getElement(i) << std::endl;
						}
						(void)calc_pathCost(*mGroup, mPathingFlags, fromTurns, fromMP, fromCoord, toCoord, INVALID_PLOT_COORD);
						(void)calc_pathValid(*mGroup, mStartCoord, mPathingFlags, fromTurns, fromMP, fromCoord, toCoord);

						if (((expectedFromValidityPlotProps ^ actualFromValidityPlotProps) & TeamPathingPlotPropsCache::getPlotPropMask(kValidityPlotPropBitIndex_BorderPlotDanger)))
						{
							std::clog << "Border plot danger changed." << std::endl;
							(void)mPlotProps->calcValidityPlotPropsForVerification(fromCoord, mGroup->getOwnerINLINE());
						}

						if (((expectedFromValidityPlotProps ^ actualFromValidityPlotProps) & TeamPathingPlotPropsCache::getPlotPropMask(kValidityPlotPropBitIndex_UnitPlotDanger)))
						{
							std::clog << "Unit plot danger changed." << std::endl;
							(void)mPlotProps->calcValidityPlotPropsForVerification(fromCoord, mGroup->getOwnerINLINE());
						}

						//mPlotProps->verify();

						mMap->verify(mGroup->getTeam());

						std::abort();
					}
				}
				
			}
		}
}

#endif