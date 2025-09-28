#include "../CommonConfig.h"

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS

#include "PlotDangerCache.h"
#include "Util.h"

#include <CommonStuff/ParallelExt.h>

#include "../CvUnit.h"
#include "../CvGameCoreUtils.h"
#include "../CvPlayerAI.h"

// Debugging
#include "../CvInfos.h"
#include "../CvTeamAI.h"

#include <iostream>

using namespace GiganticMapsOptimisationsLib;

static constexpr int kDangerRadius = 4; // AI_getPlotDanger DANGER_RANGE

static constexpr bool kEnableDebugLogging = false;
static constexpr TeamTypes kDebugLogDefenderTeam = std::bit_cast<TeamTypes>(16);
static constexpr ivec2 kDebugLogDefenderCoord{ 21, 17 };
[[maybe_unused]] static constexpr ivec2 kDebugLogAttackerCoord{ 471, 79 };

static constexpr bool kEnableLightVerification = false;

static int getMoveRadius(const CvUnit& unit)
{
	return std::min(kDangerRadius, unit.baseMoves() + unit.plot()->isValidRoute(&unit));
}

static auto getWarBits(TeamTypes defenderTeamI)
{
	std::bitset<MAX_TEAMS> warBits{};
	for (const auto otherTeamI : range<TeamTypes>(MAX_TEAMS))
		warBits[otherTeamI] = atWar(defenderTeamI, otherTeamI);
	return warBits;
}

// AI_getPlotDanger, but ignore border danger.
static const CvUnit* findAnyPlotDangerAttackUnit(TeamTypes defenderTeamI, ivec2 coord)
{
	const int radius = kDangerRadius;
	CvMap& map = GC.getMapINLINE();
	const CvPlot& plot = *map.plot(coord.x, coord.y);
	//std::bitset<MAX_TEAMS> defenderDangerBits{};
	const CvArea* const area = plot.area();
	for (const int dy : range(-radius, radius + 1))
	{
		for (const int dx : range(-radius, radius + 1))
		{
			const int dist = stepDistance(0, 0, dx, dy);
			if (const CvPlot* const unitPlot = map.plot(coord.x + dx, coord.y + dy); unitPlot && unitPlot->area() == area)
			{
				for (auto* unitNode = unitPlot->headUnitNode(); unitNode; unitNode = unitPlot->nextUnitNode(unitNode))
				{
					CvUnit* const enemyUnit = getUnit(unitNode->m_data);

					if (!enemyUnit->canAttack())
						continue;

					if (enemyUnit->alwaysInvisible())
						continue;

					if (!enemyUnit->isEnemy(defenderTeamI))
						continue;

					if (dist <= getMoveRadius(*enemyUnit))
						if (!enemyUnit->isInvisible(defenderTeamI, false) && enemyUnit->canMoveOrAttackInto(&plot))
							return enemyUnit;
				}
			}
		}
	}

	return nullptr;
}

static std::bitset<MAX_TEAMS> alwaysHostileEnemies(TeamTypes ownerTeam)
{
	std::bitset<MAX_TEAMS> bits{};
	bits[ownerTeam] = true;
	return ~bits;
}

std::bitset<MAX_TEAMS> UnitPlotDangerCache::computePlotDangerFromAttacker(const CvUnit& attacker, const CvPlot& plot) const
{
	if (!attacker.canAttack())
		return {};

	if (attacker.alwaysInvisible())
		return {};

	if (attacker.getArea() != plot.getArea())
		return {};

	// Things that isAlwaysHostile depends on:
	//     bool(getPlotCity())
	//     Improvement type (if GC.getImprovementInfo(getImprovementType()).isActsAsCity())
	//     Vassalage of attacker team to plot team
	//     Vassalage of plot team to attacker team
	//     Open borders of attacker to plot team iff attacker is a vassal of plot team.
	const std::bitset<MAX_TEAMS> defenderMask =
		attacker.isAlwaysHostile(attacker.plot())
		? alwaysHostileEnemies(attacker.getTeam())
		: mWarState[attacker.getTeam()];

	// Probably not worth the early-out.
	//if (enemyUnit->getInvisibleType() != NO_INVISIBLE)
	//{
	//	bool visibleToAny = false;
	//	for (const auto teamI : range<TeamTypes>(MAX_TEAMS))
	//		if (defenderMask[teamI] && unitPlot->isInvisibleVisible(teamI, enemyUnit->getInvisibleType()))
	//		{
	//			visibleToAny = true;
	//			break;
	//		}
	//	if (!visibleToAny)
	//		continue;
	//}

	const int iDangerRange = getMoveRadius(attacker);

	std::bitset<MAX_TEAMS> reachableBits{};

	const int dist = stepDistance(attacker.getX_INLINE(), attacker.getY_INLINE(), plot.getX_INLINE(), plot.getY_INLINE());
	if (dist <= iDangerRange && attacker.canMoveOrAttackInto(&plot))
	{
		// Fast path
		if (attacker.getInvisibleType() == NO_INVISIBLE)
		{
			if (/*!attacker.alwaysInvisible() &&*/ !attacker.isCargo())
				reachableBits = ~reachableBits;
		}
		else
		{
			for (const auto defenderTeamI : range<TeamTypes>(MAX_TEAMS))
				reachableBits[defenderTeamI] = !attacker.isInvisible(defenderTeamI, false);
		}
	}

	const std::bitset<MAX_TEAMS> result = defenderMask & reachableBits;

	if constexpr (kEnableLightVerification)
	{
		if (result.any())
		{
			for (const auto defenderTeamI : range<TeamTypes>(MAX_TEAMS))
				if (result[defenderTeamI] && !attacker.isEnemy(defenderTeamI))
				{
					for (const auto warCheckTeamI : range<TeamTypes>(MAX_TEAMS))
					{
						if (mWarState[attacker.getTeam()][warCheckTeamI] != ::atWar(attacker.getTeam(), warCheckTeamI))
							std::abort();
						if (mWarState[attacker.getTeam()][warCheckTeamI] != mWarState[warCheckTeamI][attacker.getTeam()])
							std::abort();
					}

					(void)attacker.isEnemy(defenderTeamI);
					std::abort();
				}
		}
	}

	return result;
}

static std::string getAttackConditionString(TeamTypes defenderTeamI, ivec2 coord, const CvUnit* enemyUnit)
{
	std::string str;
	str += enemyUnit->canAttack() ? 'A' : '-';
	str += enemyUnit->alwaysInvisible() ? 'I' : '-';
	str += enemyUnit->isEnemy(defenderTeamI) ? 'E' : '-';
	const int iDangerRange = getMoveRadius(*enemyUnit);
	str += std::to_string(iDangerRange);
	str += enemyUnit->isInvisible(defenderTeamI, false) ? 'I' : '-';
	const CvPlot& plot = *GC.getMapINLINE().plotINLINE(coord.x, coord.y);
	str += enemyUnit->canMoveOrAttackInto(&plot) ? 'M' : '-';
	
	// What we do is basically copy `canMoveInto` and evaluate all its conditions.

	enum ECanMoveInfoCondition
	{
		FeatureImpassable,
		TerrainImpassable,
		DomainSeaCantMoveOntoCity,
		DomainAirNoAvailAirCapacity,
		DomainLandCantMoveOntoWater,
		Immobile,
		AnimalOwnedPlot,
		AnimalBonusPlot,
		AnimalImprovementPlot,
		AnimalHasUnitsPlot,
		NoCaptureCity,
		MadeAttack,
		CanAirStrike,
		UnitInTheWay,
		NoUnitToAttack,
		CantAttack,
		CantAttackUnit,
		CantAttackAndTheresACityInTheWay,
		CantAttackAndTheresAUnitInTheWay,
		CantEnterAreaCantDeclareWar,
		CantEnterAreaHumanWontDeclareWar,
		CantEnterAreaAISneakAttackReadyButGroupWontDeclareWar,
		CantEnterAreaAISneakAttackNotReady,

		NumCanMoveIntoConditions,
	};

	static constexpr const char* kConditionNames[]{
		"FeatureImpassable",
		"TerrainImpassable",
		"DomainSeaCantMoveOntoCity",
		"DomainAirNoAvailAirCapacity",
		"DomainLandCantMoveOntoWater",
		"Immobile",
		"AnimalOwnedPlot",
		"AnimalBonusPlot",
		"AnimalImprovementPlot",
		"AnimalHasUnitsPlot",
		"NoCaptureCity",
		"MadeAttack",
		"CanAirStrike",
		"UnitInTheWay",
		"NoUnitToAttack",
		"CantAttackUnit",
		"CantAttackAndTheresACityInTheWay",
		"CantAttackAndTheresAUnitInTheWay",
		"CantEnterAreaCantDeclareWar",
		"CantEnterAreaHumanWontDeclareWar",
		"CantEnterAreaAISneakAttackReadyButGroupWontDeclareWar",
		"CantEnterAreaAISneakAttackNotReady",
	};
	const auto evaluateCanMoveInto = [&](bool bAttack)
		{
			std::bitset<NumCanMoveIntoConditions> canMoveIntoConditions{};

			const TeamTypes enemyTeamI = enemyUnit->getTeam();
			const CvTeam& enemyTeam = GET_TEAM(enemyUnit->getTeam());
			const CvUnitInfo& unitInfo = enemyUnit->getUnitInfo();
			const CvArea* const pPlotArea = plot.area();
			TeamTypes ePlotTeam = plot.getTeam();
			const DomainTypes enemyDomainType = enemyUnit->getDomainType();
			bool bCanEnterArea = enemyUnit->canEnterArea(ePlotTeam, pPlotArea);
			if (bCanEnterArea)
			{
				if (plot.getFeatureType() != NO_FEATURE)
				{
					if (unitInfo.getFeatureImpassable(plot.getFeatureType()))
					{
						TechTypes eTech = (TechTypes)unitInfo.getFeaturePassableTech(plot.getFeatureType());
						if (NO_TECH == eTech || !enemyTeam.isHasTech(eTech))
						{
							if (DOMAIN_SEA != enemyDomainType || plot.getTeam() != enemyTeamI)  // sea units can enter impassable in own cultural borders
							{
								canMoveIntoConditions[FeatureImpassable] = true;
							}
						}
					}
				}
				else
				{
					if (unitInfo.getTerrainImpassable(plot.getTerrainType()))
					{
						TechTypes eTech = (TechTypes)unitInfo.getTerrainPassableTech(plot.getTerrainType());
						if (NO_TECH == eTech || !enemyTeam.isHasTech(eTech))
						{
							if (DOMAIN_SEA != enemyDomainType || plot.getTeam() != enemyTeamI)  // sea units can enter impassable in own cultural borders
							{
								canMoveIntoConditions[TerrainImpassable] = true;
							}
						}
					}
				}
			}

			switch (enemyDomainType)
			{
			case DOMAIN_SEA:
				if (!plot.isWater() && !enemyUnit->canMoveAllTerrain())
				{
					if (!plot.isFriendlyCity(*enemyUnit, true) || !plot.isCoastalLand())
					{
						canMoveIntoConditions[DomainSeaCantMoveOntoCity] = true;
					}
				}
				break;

			case DOMAIN_AIR:
				if (!bAttack)
				{
					bool bValid = false;

					if (plot.isFriendlyCity(*enemyUnit, true))
					{
						bValid = true;

						if (unitInfo.getAirUnitCap() > 0)
						{
							if (plot.airUnitSpaceAvailable(enemyTeamI) <= 0)
							{
								bValid = false;
							}
						}
					}

					if (!bValid)
					{
						canMoveIntoConditions[DomainAirNoAvailAirCapacity] = true;
					}
				}

				break;

			case DOMAIN_LAND:
				if (plot.isWater() && !enemyUnit->canMoveAllTerrain())
				{
					if (!plot.isCity() || 0 == GC.getDefineINT("LAND_UNITS_CAN_ATTACK_WATER_CITIES"))
					{
						canMoveIntoConditions[DomainLandCantMoveOntoWater] = true;
					}
				}
				break;

			case DOMAIN_IMMOBILE:
				canMoveIntoConditions[Immobile] = true;
				break;

			default:
				FAssert(false);
				break;
			}

			if (enemyUnit->isAnimal())
			{
				if (plot.isOwned())
				{
					canMoveIntoConditions[AnimalOwnedPlot] = true;
				}

				if (!bAttack)
				{
					if (plot.getBonusType() != NO_BONUS)
					{
						canMoveIntoConditions[AnimalBonusPlot] = true;
					}

					if (plot.getImprovementType() != NO_IMPROVEMENT)
					{
						canMoveIntoConditions[AnimalImprovementPlot] = true;
					}

					if (plot.getNumUnits() > 0)
					{
						canMoveIntoConditions[AnimalHasUnitsPlot] = true;
					}
				}
			}

			if (enemyUnit->isNoCapture())
			{
				if (!bAttack)
				{
					if (plot.isEnemyCity(*enemyUnit))
					{
						canMoveIntoConditions[NoCaptureCity] = true;
					}
				}
			}

			if (bAttack)
			{
				if (enemyUnit->isMadeAttack() && !enemyUnit->isBlitz())
				{
					canMoveIntoConditions[MadeAttack] = true;
				}
			}

			if (enemyDomainType == DOMAIN_AIR)
			{
				if (bAttack)
				{
					struct Hack : CvUnit
					{
						static consteval auto get()
						{
							return &Hack::canAirStrike;
						}
					};
					if (!(enemyUnit->*Hack::get())(&plot))
					{
						canMoveIntoConditions[CanAirStrike] = true;
					}
				}
			}
			else
			{
				if (enemyUnit->canAttack())
				{
					if (bAttack || !enemyUnit->canCoexistWithEnemyUnit(NO_TEAM))
					{
						if (!enemyUnit->isHuman() || plot.isVisible(enemyTeamI, false))
						{
							if (plot.isVisibleEnemyUnit(enemyUnit) != bAttack)
							{
								canMoveIntoConditions[UnitInTheWay] = true;
							}
						}
					}

					if (bAttack)
					{
						CvUnit* pDefender = plot.getBestDefender(NO_PLAYER, enemyUnit->getOwnerINLINE(), enemyUnit, true);
						if (nullptr != pDefender)
						{
							if (!enemyUnit->canAttack(*pDefender))
							{
								canMoveIntoConditions[CantAttackUnit] = true;
							}
						}
					}
				}
				else
				{
					if (bAttack)
					{
						canMoveIntoConditions[CantAttack] = true;
					}
					else if (!enemyUnit->canCoexistWithEnemyUnit(NO_TEAM))
					{
						if (!enemyUnit->isHuman() || plot.isVisible(enemyTeamI, false))
						{
							if (plot.isEnemyCity(*enemyUnit))
							{
								canMoveIntoConditions[CantAttackAndTheresACityInTheWay] = true;
							}

							if (plot.isVisibleEnemyUnit(enemyUnit))
							{
								canMoveIntoConditions[CantAttackAndTheresAUnitInTheWay] = true;
							}
						}
					}
				}

				if (enemyUnit->isHuman())
				{
					ePlotTeam = plot.getRevealedTeam(enemyTeamI, false);
					bCanEnterArea = enemyUnit->canEnterArea(ePlotTeam, pPlotArea);
				}

				if (!bCanEnterArea)
				{
					FAssert(ePlotTeam != NO_TEAM);

					if (!enemyTeam.canDeclareWar(ePlotTeam))
					{
						canMoveIntoConditions[CantEnterAreaCantDeclareWar] = true;
					}

					if (enemyUnit->isHuman())
					{
						canMoveIntoConditions[CantEnterAreaHumanWontDeclareWar] = true;
					}
					else
					{
						if (enemyTeam.AI_isSneakAttackReady(ePlotTeam))
						{
							if (!enemyUnit->getGroup()->AI_isDeclareWar(&plot))
							{
								canMoveIntoConditions[CantEnterAreaAISneakAttackReadyButGroupWontDeclareWar] = true;
							}
						}
						else
						{
							canMoveIntoConditions[CantEnterAreaAISneakAttackNotReady] = true;
						}
					}
				}
			}

			str += '{';
			for (const size_t i : range(NumCanMoveIntoConditions))
			{
				if (canMoveIntoConditions[i])
				{
					str += '[';
					str += kConditionNames[i];
					str += ']';
				}
			}
			str += '}';
		};

		evaluateCanMoveInto(false);
		evaluateCanMoveInto(true);
	return str;
}


UnitPlotDangerCache::UnitPlotDangerCache()
	: mDefenderInvalidationState(CivMapGeometry(GC.getMapINLINE()))
	, mPlotDanger(mDefenderInvalidationState.getMapSizeInfo().dim)
	, mPlotDangerVersioning(mDefenderInvalidationState.getMapSizeInfo().dim)
{
	for (const auto teamA : range<TeamTypes>(MAX_TEAMS))
		for (const auto teamB : range<TeamTypes>(MAX_TEAMS))
			if (atWar(teamA, teamB))
				mWarState[teamA][teamB] = true;
	// This has to be all teams, to include always-hostile.
	mDirtyEnemyTeams = ~std::bitset<MAX_TEAMS>();

	//mDefenderInvalidationState.totallyInvalidated = true;
}

// This may be called from multiple threads due to parallel unit update.
	// Just use a simple mutex for now.
static std::mutex mutex;


//void UnitPlotDangerCache::onUnitRadiusChange(EPlotDangerRadiusChange change, const CvUnit& unit)
//{
//	onPlotTeamRadiusChange(change, *unit.plot(), unit.getTeam(), NO_TEAM);
//}
//void UnitPlotDangerCache::onPlotRadiusChange(EPlotDangerRadiusChange change, const CvPlot& plot)
//{
//	onPlotTeamRadiusChange(change, plot, plot.getTeam(), NO_TEAM);
//}
void UnitPlotDangerCache::onPlotChange(EGamePlotChangeEvent e, const CvPlot& plot, [[maybe_unused]] int oldValue, [[maybe_unused]] int newValue)
{
	const ivec2 position = getPlotCoord(plot);

	const std::lock_guard lock(mutex);

	switch (e)
	{
		// Rule of thumb: Invalidate full radius unless proven otherwise.
		using enum EGamePlotChangeEvent;
		// Name                      // Radius | Scope                                 | Source (relative to AI_getPlotDanger)
	case Owner:                   // single | canEnterArea change, all cities       | canMoveOrAttackInto: canEnterArea, if (pPlot->isEnemyCity(*this)); maxCombatStr: if (pPlot->isCity(true, getTeam())), if (pAttackedPlot->isCity(true, getTeam()))
		//                                // moves  | Our+enemy units.                      | .../defenseModifier: if (eDefender != NO_TEAM && (getTeam() == NO_TEAM || GET_TEAM(eDefender).isFriendlyTerritory(getTeam())))
	case RouteType:                   // moves  | Plots with our units                  | iDangerRange += ((pLoopPlot->isValidRoute(pLoopUnit)) ? 1 : 0);
	case InvisibilitySight: // moves  | Visibility change on plots with units | canMoveOrAttackInto: if isVisibleEnemyUnit
	case Terrain:                 // single | Enemy units.                          | .../maxCombatStr: iExtraModifier = pPlot->defenseModifier(getTeam(), (pAttacker != nullptr) ? pAttacker->ignoreBuildingDefense() : true);
	case Feature:                 // single | Enemy units.                          | .../maxCombatStr: iExtraModifier = pPlot->defenseModifier(getTeam(), (pAttacker != nullptr) ? pAttacker->ignoreBuildingDefense() : true);
	//case IsFort:                  // single | Enemy units.                          | .../maxCombatStr: if (pPlot->isCity(true, getTeam())), iExtraModifier = pPlot->defenseModifier(getTeam(), (pAttacker != nullptr) ? pAttacker->ignoreBuildingDefense() : true);
	case Improvement:
		// getCombatOwner depends on forts
	case CityOccupation:              // single | All cities.                           | .../maxCombatStr/defenseModifier/CvCity::getDefenseModifier: if (isOccupation())
	case CityBuildingDefence:         // single | All cities.                           | .../maxCombatStr/defenseModifier/CvCity::getDefenseModifier: CvCity::getTotalDefense
	case CityCultureLevel:            // single | All cities.                           | .../maxCombatStr/defenseModifier/CvCity::getDefenseModifier/getNaturalDefense: return GC.getCultureLevelInfo(getCultureLevel()).getCityDefenseModifier();
		//case PlayerCityDefenceModifier:   // single | Cities of all players.                | .../maxCombatStr/defenseModifier/CvCity::getDefenseModifier: return (std::max(((bIgnoreBuilding) ? 0 : getBuildingDefense()), getNaturalDefense()) + GET_PLAYER(getOwnerINLINE()).getCityDefenseModifier());
		invalidateAroundPlot(position, kDangerRadius);
		break;
	case RevealedByTeam:            // single | Revealed by us                        | canMoveOrAttackInto: ePlotTeam = pPlot->getRevealedTeam(getTeam(), false);
	case IsVisibleByTeam:               // single | Human                                 | canMoveOrAttackInto: if (!isHuman() || (pPlot->isVisible(getTeam(), false)))
	case HasBonus:         // single | Animals                               | canMoveOrAttackInto: if animal
	//case HasImprovementChanged:   // single | Animals                               | canMoveOrAttackInto: if animal
	case HasCity:                  // single | All cities.                           | canMoveOrAttackInto: if (pPlot->isEnemyCity(*this)); maxCombatStr: if (pPlot->isCity(true, getTeam())), if (pAttackedPlot->isCity(true, getTeam()))
		// getCombatOwner depends on this too.
		invalidateSinglePlot(position);
		break;

	case HasUnits:
		break;

	default:
		break;
	}
}
void UnitPlotDangerCache::onUnitChange(EGameUnitChangeEvent e, const CvUnit& unit)
{
	const CvPlot& plot = *unit.plot();
	const ivec2 position = getPlotCoord(plot);

	switch (e)
	{
		// Rule of thumb: Invalidate full radius unless proven otherwise.
		using enum EGameUnitChangeEvent;
		// Name                      // Radius | Scope                                 | Source (relative to AI_getPlotDanger)
		//case MoveFrom:             // moves  | Our+enemy units.                      | Unit loop, canMoveOrAttackInto
		//case MoveTo:              // moves  | Our+enemy units.                      | Unit loop, canMoveOrAttackInto
		case Created:            // moves  | Our+enemy units.                      | Unit loop, canMoveOrAttackInto
		case Destroyed:           // moves  | Our+enemy units.                      | Unit loop, canMoveOrAttackInto
		case IsMadeAttack:            // moves  | Our units.                            | canMoveOrAttackInto: if (isMadeAttack() && !isBlitz())
		case BlitzPromotion:          // moves  | Our units.                            | canMoveOrAttackInto: if (isMadeAttack() && !isBlitz())
		case Damage:                  // full   | Our+enemy units.                      | canMoveOrAttackInto: getBestDefender(NO_PLAYER, getOwnerINLINE(), this, true), if (!canAttack(*pDefender)); 
		case BaseCombatStr:           // single | Enemy units.                          | canMoveOrAttackInto/getBestDefender/isBetterDefenderThan/currCombatStr/maxCombatStr: if (baseCombatStr() == 0), pCombatDetails->iBaseCombatStr = baseCombatStr();
		case ExtraCombatPercent:      // single | Enemy units.                          | .../maxCombatStr: iExtraModifier = getExtraCombatPercent();
		case Fortify:                 // single | Enemy units.                          | .../maxCombatStr: iExtraModifier = fortifyModifier();
		case CityDefence:             // single | Enemy units.                          | .../maxCombatStr/cityDefenseModifier: getExtraCityDefensePercent
		case HillsDefence:            // single | Enemy units.                          | .../maxCombatStr/hillsDefenseModifier: getExtraHillsDefensePercent
		case FeatureDefence:          // single | Enemy units.                          | .../maxCombatStr/featureDefenseModifier: iExtraModifier = featureDefenseModifier(pPlot->getFeatureType());
		case TerrainDefence:          // single | Enemy units.                          | .../maxCombatStr/terrainDefenseModifier: iExtraModifier = terrainDefenseModifier(pPlot->getTerrainType());
		case CityAttack:              // moves  | Our units.                            | .../maxCombatStr: iExtraModifier = -pAttacker->cityAttackModifier();
		case HillsAttack:             // moves  | Our units.                            | .../maxCombatStr: iExtraModifier = -pAttacker->hillsAttackModifier();
		case FeatureAttack:           // moves  | Our units.                            | .../maxCombatStr: iExtraModifier = -pAttacker->featureAttackModifier(pAttackedPlot->getFeatureType());
		case TerrainAttack:           // moves  | Our units.                            | .../maxCombatStr: iExtraModifier = -pAttacker->terrainAttackModifier(pAttackedPlot->getTerrainType());
		case DomainModifier:          // full   | Our+enemy units.                      | .../maxCombatStr: iExtraModifier = domainModifier(pAttacker->getDomainType()), iExtraModifier = -pAttacker->domainModifier(getDomainType());
		case RiverPromotion:          // moves  | Our units.                            | .../maxCombatStr: if (!(pAttacker->isRiver()))
		case AmphibPromotion:         // moves  | Our units.                            | .../maxCombatStr: if (!(pAttacker->isAmphib()))
		case KamikazePercent:         // moves  | Our units.                            | .../maxCombatStr: if (pAttacker->getKamikazePercent() != 0)
		//case PlayerCityDefenceModifier:   // single | Cities of all players.                | .../maxCombatStr/defenseModifier/CvCity::getDefenseModifier: return (std::max(((bIgnoreBuilding) ? 0 : getBuildingDefense()), getNaturalDefense()) + GET_PLAYER(getOwnerINLINE()).getCityDefenseModifier());
		case ImmuneToFirstStrikesCount: // full | Our+enemy units.                      | canMoveOrAttackInto/getBestDefender/isBetterDefenderThan: if (!(pAttacker->immuneToFirstStrikes())), if (immuneToFirstStrikes())
		case ExtraChanceFirstStrikes: // full   | Our+enemy units.                      | canMoveOrAttackInto/getBestDefender/isBetterDefenderThan: chanceFirstStrikes()
		case ExtraFirstStrikes:       // full   | Our+enemy units.                      | canMoveOrAttackInto/getBestDefender/isBetterDefenderThan: firstStrikes()
			invalidateAroundPlot(position, std::min(kDangerRadius, unit.baseMoves() + plot.isValidRoute(&unit)));
			break;
			// Stuff that changes range.
		case BaseMoves:               // full   | Our units.                            | int iDangerRange = pLoopUnit->baseMoves();
		case EnemyRoutesPromotion:    // moves  | Our units.                            | iDangerRange += ((pLoopPlot->isValidRoute(pLoopUnit)) ? 1 : 0);
			invalidateAroundPlot(position, kDangerRadius);
			break;
			// Not really relevant for land plot danger, but could be used later.
		case CargoUnits:              // single | Enemy units.                          | canMoveOrAttackInto/getBestDefender/isBetterDefenderThan: pDefender->getCargoUnits(aCargoUnits);
		case LeaderType:              // single | Enemy units.                          | canMoveOrAttackInto/getBestDefender/isBetterDefenderThan: if (NO_UNIT == getLeaderUnitType() && NO_UNIT != pDefender->getLeaderUnitType())
		case Level:                       // single | Enemy units.                          | canMoveOrAttackInto/getBestDefender/isBetterDefenderThan/isBeforeUnitCycle: if (pFirstUnit->getLevel() != pSecondUnit->getLevel())
		case Experience:                  // single | Enemy units.                          | canMoveOrAttackInto/getBestDefender/isBetterDefenderThan/isBeforeUnitCycle: if (pFirstUnit->getExperience() != pSecondUnit->getExperience())
			invalidateSinglePlot(position);
			break;
		
		default:
			break;
	}
}
void UnitPlotDangerCache::onUnitMoved(const CvUnit& unit, ivec2 from, ivec2 to) // Thread-safe
{
	const std::lock_guard lock(mutex);
	const auto invalidateCoord = [&map = GC.getMapINLINE(), &unit, this](ivec2 position) {
		if (position.x >= 0)
		{
			const CvPlot& plot = *map.plot(position.x, position.y);
			invalidateAroundPlot(position, std::min(kDangerRadius, unit.baseMoves() + plot.isValidRoute(&unit)));
		}
		};
	invalidateCoord(from);
	invalidateCoord(to);
}
void UnitPlotDangerCache::onDiploChange(EPlotDangerDiploChange change, TeamTypes teamA, TeamTypes teamB)
{
	switch (change)
	{
	case EPlotDangerDiploChange::AnyWar:
		mDirtyDefenderTeams[teamA] = true;
		mDirtyDefenderTeams[teamB] = true;
		mDirtyDefenderTeams |= mWarState[teamA];
		mDirtyDefenderTeams |= mWarState[teamB];
		mWarState[teamA][teamB] = mWarState[teamB][teamA] = atWar(teamA, teamB);
		invalidateMoveRadiusAroundDangerousUnits(teamA);
		invalidateMoveRadiusAroundDangerousUnits(teamB);
		break;

		// This changes canEnterArea for the master.
		
	case EPlotDangerDiploChange::AnyVassal:
		mDirtyDefenderTeams |= mWarState[teamA];
		invalidateMoveRadiusAroundDangerousUnits(teamA);

		// Also changes "combat owner" for always hostile units in forts.
		mDirtyDefenderTeams = {};
		mDirtyDefenderTeams = ~mDirtyDefenderTeams;
		invalidateMoveRadiusAroundDangerousUnits(teamB, true);
		break;
		// This changes canEnterArea for both teams.
	case EPlotDangerDiploChange::OpenBorders:
		mDirtyDefenderTeams |= mWarState[teamA];
		mDirtyDefenderTeams |= mWarState[teamB];
		invalidateMoveRadiusAroundDangerousUnits(teamA);
		invalidateMoveRadiusAroundDangerousUnits(teamB);

		// Also changes "combat owner" for always hostile units in forts.
		mDirtyDefenderTeams = {};
		mDirtyDefenderTeams = ~mDirtyDefenderTeams;
		break;
		// Various checks at the end of canMoveInto.
	case EPlotDangerDiploChange::CanDeclareWar:
	case EPlotDangerDiploChange::AI_isSneakAttackReady:
	case EPlotDangerDiploChange::AI_isDeclareWar:
		mDirtyDefenderTeams |= mWarState[teamA];
		invalidateMoveRadiusAroundDangerousUnits(teamA);
		break;

	case EPlotDangerDiploChange::AreaIsBorderObstacleChanged:
		mDirtyDefenderTeams = {};
		mDirtyDefenderTeams = ~mDirtyDefenderTeams;
		invalidateMoveRadiusAroundDangerousUnits(BARBARIAN_TEAM);
		break;
	case EPlotDangerDiploChange::PermAlliance:
	case EPlotDangerDiploChange::PlayerCityDefenceModifier:
		// Just do a full invalidation.
		mDirtyDefenderTeams = {};
		mDirtyDefenderTeams = ~mDirtyDefenderTeams;
		mDefenderInvalidationState.invalidateAll();
		break;

	default:
		std::abort();
	}
}



void UnitPlotDangerCache::invalidateMoveRadiusAroundDangerousUnits(TeamTypes teamI, bool alwaysHostileOnly)
{
	for (const auto playerI : range<PlayerTypes>(MAX_PLAYERS))
	{
		CvPlayer& player = GET_PLAYER(playerI);
		if (player.getTeam() == teamI)
		{
			int index{};
			for (auto* group = player.firstSelectionGroup(&index); group; group = player.nextSelectionGroup(&index))
				for (auto* unitNode = group->headUnitNode(); unitNode; unitNode = group->nextUnitNode(unitNode))
				{
					const CvUnit& unit = *getUnit(unitNode->m_data);
					if (unit.canAttack() && !unit.alwaysInvisible() && (!alwaysHostileOnly || unit.isAlwaysHostile(nullptr)))
						mDefenderInvalidationState.invalidateStepRadius(getPlotCoord(*unit.plot()), getMoveRadius(unit));
				}
		}
	}
}

void UnitPlotDangerCache::update()
{
	std::atomic_bool changed{};

	if (mDefenderInvalidationState.isAllDirty())
	{
		mDirtyDefenderTeams = {};
		mDirtyDefenderTeams = ~mDirtyDefenderTeams;
	}

	if (mDirtyDefenderTeams.any())
	{
		//if (!mDefenderInvalidationState.totallyInvalidated)
		//{
		//	// Expand invalidation around defender units.
		//	const auto originalPlotList = mDefenderInvalidationState.invalidationPlotList;
		//	for (const ivec2 coord : originalPlotList)
		//	{
		//		mDefenderInvalidationState.invalidateStepRadius(coord, kDangerRadius);
		//		if (mDefenderInvalidationState.totallyInvalidated)
		//			break;
		//	}
		//}

		if (mDefenderInvalidationState.isAllDirty())
		{
			// Clear out all plot danger.
			for (const int y : range(mDefenderInvalidationState.getMapSizeInfo().dim.y))
				for (const int x : range(mDefenderInvalidationState.getMapSizeInfo().dim.x))
					if (mPlotDanger[{ x, y }].any())
					{
						mPlotDanger[{ x, y }] = {};
						mPlotDangerVersioning.updateMultithreaded({ x, y });
					}

			// Update from enemy units.
			mDirtyEnemyTeams = ~std::bitset<MAX_TEAMS>();

			changed.store(true, std::memory_order_relaxed);

			// Slower than going through enemy units, as expected.
			//std::atomic_bool changed{};
			//heck::parallelForEachN(mDefenderInvalidationState.mapSizeInfo.dim.y, [&changed, &map = GC.getMapINLINE(), this](int y) {
			//	bool localChanged = false;
			//	for (const int x : range(mDefenderInvalidationState.mapSizeInfo.dim.x))
			//	{
			//		const std::bitset<MAX_TEAMS> bits = computePlotDanger(*map.plot(x, y));
			//		const ivec2 coord(x, y);
			//		if (bits != mPlotDanger[coord])
			//		{
			//			mPlotDanger[coord] = bits;
			//			mPlotDangerVersioning.updateMultithreaded(coord);
			//			localChanged = true;
			//		}
			//	}
			//	if (localChanged)
			//		changed.store(true, std::memory_order_relaxed);
			//	});
			//if (changed.load(std::memory_order_relaxed))
			//	mPlotDangerVersioning.bumpVersionAfterMultithreadedUpdate();
		}
		else
		{
			// Doing this quickly is important after all those barb animals moved.
			CvMap& map = GC.getMapINLINE();
			//for (const ivec2 coord : mDefenderInvalidationState.invalidationPlotList)
			//	updatePlotDangerForDefenderPlot(*map.plot(coord.x, coord.y), changed);
			const std::span dirtyPlotList = mDefenderInvalidationState.getDirtyPlotList();
			std::for_each(heck::kDefaultParallelExecution, dirtyPlotList.begin(), dirtyPlotList.end(),
			//std::for_each(dirtyPlotList.begin(), dirtyPlotList.end(),
				[&](ivec2 coord) {
					//for (const int dy : range(-inv.maxStepRadius, inv.maxStepRadius + 1))
						//for (const int dx : range(-inv.maxStepRadius, inv.maxStepRadius + 1))
					if (const CvPlot* const plot = map.plotINLINE(coord.x, coord.y))
						updatePlotDangerForDefenderPlot(*plot, changed);
					if constexpr (kEnableDebugLogging)
						if (coord == kDebugLogDefenderCoord)
							std::clog << "UnitPlotDanger: " << kDebugLogDefenderCoord << " updated by dirtyPlotList.\n";
				});
		}
	}
	else
	{
		if (mDefenderInvalidationState.hasAnyDirty())
			std::abort();
	}

	if (mDirtyEnemyTeams.any())
	{
		for (const auto teamI : range<TeamTypes>(MAX_TEAMS))
			if (mDirtyEnemyTeams[teamI])
				setPlotDangerFromEnemyTeam(teamI, changed);
		mDirtyEnemyTeams = {};
	}

	if (changed.load(std::memory_order_relaxed))
		mPlotDangerVersioning.bumpVersionAfterMultithreadedUpdate();

	mDefenderInvalidationState.reset();

	// SLOW
	//verifyAll();

	if constexpr (kEnableDebugLogging)
		debugFindAttacker();
}

void UnitPlotDangerCache::invalidateAroundPlot(ivec2 position, int radius)
{
	mDirtyDefenderTeams = {};
	mDirtyDefenderTeams = ~mDirtyDefenderTeams;
	mDefenderInvalidationState.invalidateStepRadius(position, radius);
}

void UnitPlotDangerCache::invalidateSinglePlot(ivec2 position)
{
	mDirtyDefenderTeams = {};
	mDirtyDefenderTeams = ~mDirtyDefenderTeams;
	mDefenderInvalidationState.invalidateSingle(position);
}

void UnitPlotDangerCache::setPlotDangerFromEnemyTeam(TeamTypes teamI, std::atomic_bool& changed)
{
	std::vector<const CvSelectionGroup*> groups;
	for (const auto playerI : range<PlayerTypes>(MAX_PLAYERS))
	{
		CvPlayer& player = GET_PLAYER(playerI);
		if (player.getTeam() == teamI)
		{
			//int index{};
			//for (auto* group = player.firstSelectionGroup(&index); group; group = player.nextSelectionGroup(&index))
			//	setPlotDangerFromEnemyGroup(*group, changed);

			
			groups.reserve(player.getNumSelectionGroups());

			int index{};
			for (auto* group = player.firstSelectionGroup(&index); group; group = player.nextSelectionGroup(&index))
				groups.push_back(group);
		}
	}

	//std::bitset<MAX_TEAMS> warMask{};
	//for (const auto i : range<TeamTypes>(MAX_TEAMS))
	//	if (::atWar(i, teamI))
	//		warMask[i] = true;
	//
	//std::bitset<MAX_TEAMS> alwaysHostileMask{};
	//alwaysHostileMask = ~alwaysHostileMask;
	//alwaysHostileMask[teamI] = false;

	if constexpr (kEnableLightVerification)
		for (const auto i : range<TeamTypes>(MAX_TEAMS))
			for (const auto j : range<TeamTypes>(MAX_TEAMS))
				if (mWarState[i][j] != ::atWar(i, j))
					std::abort();

	std::atomic_bool changedDebugPlot{};

	// Multithreaded version helps a little. For all those 3000 barbs.
	std::atomic_bool& changedAtomic = changed;
	std::for_each(heck::kDefaultParallelExecution, groups.begin(), groups.end(), [&changedDebugPlot, &changedAtomic, &map = GC.getMapINLINE(), mapSizeInfo = mDefenderInvalidationState.getMapSizeInfo(), this](const CvSelectionGroup* group) {
	//std::for_each(groups.begin(), groups.end(), [&changedAtomic, &map = GC.getMapINLINE(), mapSizeInfo = mDefenderInvalidationState.getMapSizeInfo(), this](const CvSelectionGroup* group) {
		(void)changedDebugPlot; // can't use [[maybe_unused]]

		bool localChanged = false;

		// Empty group. Happened with "UN - Fluffy AD-1858 After UN.CivBeyondSwordSave".
		if (!group->plot())
			return;

		const ivec2 centerCoord = getPlotCoord(*group->plot());

		//if (centerCoord == kDebugLogAttackerCoord)
		//	__debugbreak();

		for (auto* node = group->headUnitNode(); node; node = group->nextUnitNode(node))
		{
			const CvUnit& unit = *::getUnit(node->m_data);
			const int radius = getMoveRadius(unit);

			for (const int dy : range(-radius, radius + 1))
				for (const int dx : range(-radius, radius + 1))
				{
					const ivec2 coordUnwrapped = centerCoord + ivec2(dx, dy);
					if (mapSizeInfo.isValidCoord(coordUnwrapped))
					{
						const ivec2 coord = mapSizeInfo.wrapCoord(coordUnwrapped);
						//const std::bitset<MAX_TEAMS> bits = computePlotDanger(*map.plot(coord.x, coord.y));

						const CvPlot& plot = *map.plotINLINE(coord.x, coord.y);
						const std::bitset<MAX_TEAMS> bits = computePlotDangerFromAttacker(unit, plot);

						std::atomic_ref ref(mPlotDanger[coord]);
						//if (ref.exchange(bits, std::memory_order_relaxed) != bits)
						
						auto localValue = ref.load(std::memory_order_relaxed);
						if ((bits | localValue) != localValue)
						{
							while ((bits | localValue) != localValue && !ref.compare_exchange_weak(localValue, localValue | bits, std::memory_order_acq_rel))
								;
							
							if constexpr (kEnableLightVerification)
							{
								const std::bitset<MAX_TEAMS> defenderBits = computePlotDanger(plot);
								// No way should we have computed plot danger set for a team if `computePlotDanger` says no.
								if ((bits & ~defenderBits).any())
								{
									(void)computePlotDangerFromAttacker(unit, plot);
									std::abort();
								}
								// If one of the bits is set, `findAnyPlotDangerAttackUnit` should find an attacker unit.
								for (const auto teamI : range<TeamTypes>(MAX_TEAMS))
									if (bits[teamI])
										if (!findAnyPlotDangerAttackUnit(teamI, coord))
										{
											(void)computePlotDangerFromAttacker(unit, plot);
											(void)findAnyPlotDangerAttackUnit(teamI, coord);
											std::abort();
										}

								if (coord == kDebugLogDefenderCoord)
									changedDebugPlot.store(true, std::memory_order_relaxed);
							}
							localChanged = true;
							mPlotDangerVersioning.updateMultithreaded(coord);
						}

						//if constexpr (kEnableDebugLogging)
						//{
						//	//if (coord == kDebugLogDefenderCoord && centerCoord == kDebugLogAttackerCoord)
						//	if (centerCoord == kDebugLogAttackerCoord)
						//	{
						//		std::clog << "UnitPlotDangerCache::setPlotDangerFromEnemyTeam: oldValue = " << localValue << ", newValue = " << (bits | localValue) << std::endl;
						//	}
						//}
					}
				}
		}
		if (localChanged && !changedAtomic.load(std::memory_order_relaxed)) // Load first so that we don't get cacheline sharing (afaik).
			changedAtomic.store(true, std::memory_order_relaxed);
		});
	//changed |= changedAtomic.load(std::memory_order_relaxed);

	if constexpr (kEnableDebugLogging)
		if (changedDebugPlot.load(std::memory_order_relaxed))
			std::clog << "UnitPlotDanger: " << kDebugLogDefenderCoord << " updated by setPlotDangerFromEnemyTeam.\n";
}
void UnitPlotDangerCache::setPlotDangerFromEnemyGroup(const CvSelectionGroup& group, std::atomic_bool& changed)
{
	// If there's more than one unit, then just do kDangerRadius.
	const int radius = group.getNumUnits() == 1 ? std::min(kDangerRadius, getMoveRadius(*group.getHeadUnit())) : kDangerRadius;//std::min(kDangerRadius, getMoveRadius(unit));
	CvMap& map = GC.getMapINLINE();
	const ivec2 coord = getPlotCoord(*group.plot());
	const CivMapGeometry mapSizeInfo = mDefenderInvalidationState.getMapSizeInfo();
	for (const int dy : range(-radius, radius + 1))
		for (const int dx : range(-radius, radius + 1))
			if (mapSizeInfo.isValidCoord(coord + ivec2(dx, dy)))
				updatePlotDangerForDefenderPlot(*map.plot(coord.x + dx, coord.y + dy), changed);
}
//void UnitPlotDangerCache::updatePlotDangerForDefenderTeam(TeamTypes, bool& changed)
//{
//}
void UnitPlotDangerCache::updatePlotDangerForDefenderPlot(const CvPlot& plot, std::atomic_bool& changed)
{
	const std::bitset<MAX_TEAMS> bits = computePlotDanger(plot);
	const ivec2 coord = getPlotCoord(plot);
	if (bits != std::atomic_ref(mPlotDanger[coord]).exchange(bits, std::memory_order_relaxed))
	{
		if (!changed.load(std::memory_order_relaxed))
			changed.store(true, std::memory_order_relaxed);
		mPlotDangerVersioning.updateMultithreaded(coord);
	}

	if constexpr (kEnableDebugLogging)
	{
		if (coord == kDebugLogDefenderCoord)
		{
			debugFindAttacker();
		}
	}
}



std::bitset<MAX_TEAMS> UnitPlotDangerCache::computePlotDanger(const CvPlot& plot) const
{
	const int radius = kDangerRadius;
	CvMap& map = GC.getMapINLINE();
	const ivec2 coord = getPlotCoord(plot);
	std::bitset<MAX_TEAMS> defenderDangerBits{};
	const CvArea* const area = plot.area();
	for (const int dy : range(-radius, radius + 1))
	{
		for (const int dx : range(-radius, radius + 1))
		{
			//const int dist = stepDistance(0, 0, dx, dy);
			if (const CvPlot* const unitPlot = map.plotINLINE(coord.x + dx, coord.y + dy); unitPlot && unitPlot->area() == area)
			{
				for (auto* unitNode = unitPlot->headUnitNode(); unitNode; unitNode = unitPlot->nextUnitNode(unitNode))
				{
					CvUnit* const enemyUnit = getUnit(unitNode->m_data);
					defenderDangerBits |= computePlotDangerFromAttacker(*enemyUnit, plot);
				}
			}
		}
	}

	return defenderDangerBits;
}



uint64_t UnitPlotDangerCache::getVersionSerial([[maybe_unused]] TeamTypes defenderTeamI) const
{
	return mPlotDangerVersioning.getVersionSerial();
}

std::generator<iaabb2> UnitPlotDangerCache::getDefenderDirtyRects([[maybe_unused]] TeamTypes defenderTeamI, uint64_t myOldVersion) const
{
	return mPlotDangerVersioning.getUserDirtyRects(myOldVersion);
}

bool UnitPlotDangerCache::getDefenderPlotDanger(TeamTypes myTeamI, ivec2 coord) const
{
	return mPlotDanger[coord][myTeamI];
}


void UnitPlotDangerCache::verify(TeamTypes defenderTeamI, ivec2 coord) const
{
	const CvUnit* const livePlotDanger = findAnyPlotDangerAttackUnit(defenderTeamI, coord);
	const bool cachedPlotDanger = getDefenderPlotDanger(defenderTeamI, coord);

	if ((bool)livePlotDanger != cachedPlotDanger)
	{
		std::wclog << L"Cached unit plot danger for team " << (int)defenderTeamI << L" at plot " << getFullPlotDescription(*GC.getMap().plot(coord.x, coord.y)) << L" is wrong!" << std::endl;
		std::wclog << L"cachedPlotDanger = " << cachedPlotDanger << std::endl;
		std::wclog << L"livePlotDanger plot = ";
		if (livePlotDanger)
			std::wclog << getFullPlotDescription(*livePlotDanger->plot());
		else
			std::wclog << L"None";

		std::wclog << std::endl;

		// Put the attacker coord here that was found for kDebugLogDefenderCoord above.

		if (const CvUnit* const attacker = ::getUnit(mDebugAttackerUnitID))
		{
			std::wclog << L"Debug attacker info\n";
			std::wclog << L'\t' << getFullPlotDescription(*attacker->plot()) << L'\n';
			std::wclog << L"\tDefender warBits = " << getWarBits(defenderTeamI) << std::endl;
			std::clog << "\tgetAttackConditionString: " << getAttackConditionString(kDebugLogDefenderTeam, coord, attacker) << std::endl;
		}
		else
			std::wclog << L"Debug attacker not found" << std::endl;

		std::abort();
	}

	//std::wclog << L"Cached unit plot danger is correct." << std::endl;
}

// SLOW
void UnitPlotDangerCache::verifyAll() const
{
	for (const int y : range(mDefenderInvalidationState.getMapSizeInfo().dim.y))
		for (const int x : range(mDefenderInvalidationState.getMapSizeInfo().dim.x))
			for (const auto teamA : range<TeamTypes>(MAX_TEAMS))
				verify(teamA, { x, y });
}

void UnitPlotDangerCache::debugFindAttacker()
{
	const ivec2 coord = kDebugLogDefenderCoord;
	const std::bitset bits = std::atomic_ref(mPlotDanger[coord]).load(std::memory_order_relaxed);

	if (kDebugLogDefenderTeam != NO_TEAM && bits[kDebugLogDefenderTeam])
	{
		const CvPlot& plot = *GC.getMapINLINE().plotINLINE(coord.x, coord.y);
		if (const CvUnit* const attacker = findAnyPlotDangerAttackUnit(kDebugLogDefenderTeam, coord))
		{
			if (attacker->getIDInfo() != mDebugAttackerUnitID)
			{
				mDebugAttackerUnitID = attacker->getIDInfo();
				std::wclog << L"\tDefender warBits = " << getWarBits(kDebugLogDefenderTeam) << std::endl;
				std::wclog << L"\tmPlotDanger[coord] = " << bits << std::endl;
				std::wclog << L"\tDefender plot: " << getFullPlotDescription(plot) << std::endl;
				std::wclog << L"\tAttack unit at " << getPlotCoord(*attacker->plot()) << ": " << attacker->getName() << L" (" << GET_PLAYER(attacker->getOwnerINLINE()).getName() << L')' << std::endl;
				std::wclog << L"\tPlot info: " << getFullPlotDescription(*attacker->plot()) << std::endl;
				std::clog << "\tgetAttackConditionString: " << getAttackConditionString(kDebugLogDefenderTeam, coord, attacker) << std::endl;
			}
		}
		else
		{
			if (mDebugAttackerUnitID != IDInfo())
				mDebugAttackerUnitID = {};

			std::wclog << L"No attacker found!" << std::endl;

			(void)computePlotDanger(plot);
			(void)findAnyPlotDangerAttackUnit(kDebugLogDefenderTeam, coord);

			// The bit was set, so there should be an attacker.
			std::abort();
		}
	}
}

#endif