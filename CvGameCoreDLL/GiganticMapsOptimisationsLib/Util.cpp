#include "../CommonConfig.h"

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS

#include "Util.h"

#include <CommonStuff/Hashing.h>
#include <CommonStuff/PPM.h>

#include "../CvTeamAI.h"
#include "../CvUnit.h"
#include "../CvGameCoreUtils.h"
#include "../CvSelectionGroup.h"
#include "../CvInfos.h"
#include "../CvRandom.h"
#include "../CvPlayerAI.h"

#include <fstream>
#include <mutex>

using namespace GiganticMapsOptimisationsLib;

//CoarseHeuristicUnitPathingCostTraits::CoarseHeuristicUnitPathingCostTraits(const CvUnit& unit)
//{
//	assert(unit.baseMoves() <= UINT8_MAX);
//	assert(unit.getExtraMoveDiscount() <= 1);
//	const int routeChange = GET_TEAM(unit.getTeam()).getRouteChange(kRouteType_Road);
//	assert(routeChange <= UINT8_MAX);
//	maxMoves = (uint8_t)unit.baseMoves();
//	isForestDoubleMove = unit.isFeatureDoubleMove(FeatureTypes(1));
//	isHillDoubleMove = unit.isHillsDoubleMove();
//	ignoresTerrainMovementCost = unit.ignoreTerrainCost();
//	mobilityPromotion = (bool)unit.getExtraMoveDiscount();
//	playerRoadMovementChange = (uint8_t)routeChange;
//}
//
//UnitPathingCostTraits::UnitPathingCostTraits(const CvUnit& unit) : CoarseHeuristicUnitPathingCostTraits(unit), initialMP(unit.movesLeft())
//{
//}

//UnitPathingValidityTraits::UnitPathingValidityTraits(const CvUnit& unit)
//	: canFight(unit.canFight())
//	, canExploreRivalTerritory(unit.isRivalTerritory())
//	, isAlwaysInvisible(unit.alwaysInvisible())
//	, isHiddenNationality(unit.getUnitInfo().isHiddenNationality())
//	, isNoCapture(unit.isNoCapture())
//	, canAttack(unit.canAttack())
//{
//}

//static bool combine(bool a, bool b, bool def)
//{
//	return a == b ? a : def;
//}

//UnitPathingValidityTraits::UnitPathingValidityTraits(UnitPathingValidityTraits a, UnitPathingValidityTraits b, bool def)
//	: canFight(combine(a.canFight, b.canFight, def))
//	, canExploreRivalTerritory(combine(a.canExploreRivalTerritory, b.canExploreRivalTerritory, def))
//	, isAlwaysInvisible(combine(a.isAlwaysInvisible, b.isAlwaysInvisible, def))
//	, isHiddenNationality(combine(a.isHiddenNationality, b.isHiddenNationality, def))
//	, isNoCapture(combine(a.isNoCapture, b.isNoCapture, def))
//	, canAttack(combine(a.canAttack, b.canAttack, def))
//{
//}
//
//GroupPathingValidityTraits::GroupPathingValidityTraits(UnitPathingValidityTraits a) : a(a), b(a)
//{
//}
//
//GroupPathingValidityTraits::GroupPathingValidityTraits(GroupPathingValidityTraits ga, GroupPathingValidityTraits gb)
//	: a(UnitPathingValidityTraits(ga.a, ga.b, false), UnitPathingValidityTraits(gb.a, gb.b, false), false)
//	, b(UnitPathingValidityTraits(ga.a, ga.b, true), UnitPathingValidityTraits(gb.a, gb.b, true), true)
//{
//}
//
//TeamDiploState::TeamDiploState(TeamTypes team)
//{
//	const CvTeam& teamObj = GET_TEAM(team);
//	for (const TeamTypes other : range<TeamTypes>(MAX_TEAMS))
//	{
//		wars[other] = atWar(team, other);
//		foreignTerritoryAllowPassage[other] = wars[other] || teamObj.isOpenBorders(other) || teamObj.isFriendlyTerritory(other);
//		//foreignUnitsAllowPassage[other]
//	}
//}

// bool CvSelectionGroupAI::AI_isDeclareWar(const CvPlot* pPlot)
//static bool isGroupOnWarMarch(const CvSelectionGroup& group, TeamTypes other)
//{
//	// Can't use function directly. Rewritten here.
//	switch (group.getHeadUnitAI())
//	{
//	case UNITAI_ATTACK_CITY:
//	case UNITAI_ATTACK_CITY_LEMMING:
//		return true;
//
//	case UNITAI_ATTACK:
//	case UNITAI_COLLATERAL:
//	case UNITAI_PILLAGE:
//	case UNITAI_ATTACK_SEA:
//	case UNITAI_RESERVE_SEA:
//	case UNITAI_ESCORT_SEA:
//		return GET_TEAM(group.getTeam()).AI_getWarPlan(other) == WARPLAN_LIMITED;
//
//	case UNITAI_ASSAULT_SEA:
//		return group.hasCargo();
//
//	default:
//		return false;
//	}
//}

//TeamDiploPathingInfo::TeamDiploPathingInfo(TeamTypes team, const CvSelectionGroup& group) : TeamDiploState(team)
//{
//	// bool CvUnit::canMoveInto(const CvPlot* pPlot, bool bAttack, bool bDeclareWar, bool bIgnoreLoad) const
//	const CvTeam& teamObj = GET_TEAM(team);
//	for (const TeamTypes other : range<TeamTypes>(MAX_TEAMS))
//		foreignTerritoryAllowPassage[other] = foreignTerritoryAllowPassage[other] || (teamObj.canDeclareWar(other) && teamObj.AI_isSneakAttackReady(other) && isGroupOnWarMarch(group, other));
//}

template<class T>
	requires (std::has_unique_object_representations_v<T> && std::is_trivially_copyable_v<T>)
static std::array<std::byte, sizeof(T)> asHashableBytes(const T& arg)
{
	return std::bit_cast<std::array<std::byte, sizeof(T)>>(arg);
}

template<size_t... kN>
static std::array<std::byte, (kN + ...)> combineArrays(const std::array<std::byte, kN>&... args)
{
	std::array<std::byte, (kN + ...)> blob;
	size_t offset = 0;
	((std::copy_n(args.begin(), kN, blob.begin() + offset), offset += kN), ...);
	return blob;
}

template<class... T>
static size_t hashTrivial(const T&... args)
{
	const auto blob = combineArrays(asHashableBytes(args)...);
	return std::hash<std::string_view>()({ reinterpret_cast<const char*>(blob.data()), blob.size() });
}

//size_t PathingStateStaticKey::Hasher::operator()(const PathingStateStaticKey& x) const noexcept
//{
//	return hashTrivial(
//		x.startPlotCoord,
//		x.optDestPlotCoord,
//		x.team,
//		x.diploInfo,
//		x.unitPathingCostTraits,
//		x.unitPathingValidityTraits,
//		x.flags,
//		x.isReversePathing
//	);
//}

uint16_t DeterministicParallelPlotHashRNG::get(uint16_t size, int plotI, const char*) const
{
	return uint16_t(((heck::rxprime64((uint64_t(mSeed) << 32) | plotI) & UINT32_MAX) * size) >> 32);
}

DeterministicParallelPlotHashRNG::DeterministicParallelPlotHashRNG(CvRandom& rng, const char* text) : mSeed(rng.get(UINT16_MAX, text))
{
	// This is [0..0xFFFEFFFE].
	mSeed |= uint32_t(rng.get(UINT16_MAX, text)) << 16;
}

DeterministicParallelSeedingRNG::DeterministicParallelSeedingRNG(CvRandom& rng, const char* text) : mRootSeed(rng.get(UINT16_MAX, text))
{
	// More bits.
	mRootSeed |= uint32_t(rng.get(UINT16_MAX, text)) << 16;
}

CvRandom DeterministicParallelSeedingRNG::createTaskRNGFor(uint32_t taskSeed) const
{
	CvRandom rng;
	rng.disableThreadCheck();
	rng.reseed(heck::rxprime64(taskSeed | (uint64_t(mRootSeed) << 32)) & UINT32_MAX);
	return rng;
}

heck::Image GiganticMapsOptimisationsLib::renderMap(bool showTerrainAndFeatures)
{
	const CvMap& map = GC.getMapINLINE();
	const CivMapGeometry mapSizeInfo(map);

	heck::Image img(mapSizeInfo.dim);

	for (const int y : range(mapSizeInfo.dim.y))
	{
		for (const int x : range(mapSizeInfo.dim.x))
		{
			heck::RGB8 colour{};

			const CvPlot& plot = *map.plot(x, y);

			if (plot.isWater())
				colour = { 17, 45, 84 };
			else if (!plot.isImpassable())
			{
				colour = { 85, 85, 85 };
				if (showTerrainAndFeatures)
				{
					if (plot.getRouteType() != NO_ROUTE)
						colour[2] = 255;
					else
					{
						if (plot.isHills())
							colour[0] = 255;
						if (isForest(plot.getFeatureType()))
							colour[1] = 255;
					}
				}
			}

			// Show all barbs.
			//if (plot.isVisibleEnemyUnit(PlayerTypes(0)))
			//	colour = { 255, 255, 255 };

			img[{ x, y }] = colour;
		}
	}

	return img;
}

void GiganticMapsOptimisationsLib::dumpMapExperimentFile(const char* path)
{
	const CvMap& map = GC.getMapINLINE();
	const CivMapGeometry mapSizeInfo(map);

	DynamicArray2D<uint8_t> a(mapSizeInfo.dim);

	for (const int y : range(a.dim.y))
	{
		for (const int x : range(a.dim.x))
		{
			const CvPlot& plot = *map.plot(x, y);

			uint8_t flags = 0;
			if (!plot.isWater() && !plot.isImpassable())
			{
				flags |= 1;
				if (plot.isHills())
					flags |= 2;
				if (isForest(plot.getFeatureType()))
					flags |= 4;
				if (plot.getRouteType() != NO_ROUTE)
					flags |= 8;
			}
			a[{ x, y }] = flags;
		}
	}

	std::ofstream dump(path, std::ios::binary);

	dump.write(reinterpret_cast<const char*>(&a.dim), sizeof(a.dim));
	dump.write(reinterpret_cast<const char*>(a.cells.data()), std::span(a.cells).size_bytes());

	if (!dump)
		std::abort();
}

static std::mutex gMutex;

std::wstring GiganticMapsOptimisationsLib::getBasicPlotDescription(const CvPlot& plot)
{
	std::wstringstream desc;

	const std::lock_guard lock(gMutex); // CvInfoBase has a bunch of `mutable` variables.

	desc << L"{ [" << plot.getX_INLINE() << L", " << plot.getY_INLINE() << L']';

	if (plot.isCity())
		desc << L" city of " << plot.getPlotCity()->getName();

	if (plot.getNumUnits() > 0)
	{
		desc << L" with ";
		desc << CvPlayerAI::getPlayerNonInl(::getUnit(plot.headUnitNode()->m_data)->getOwnerINLINE()).getName();
		desc << L' ' << ::getUnit(plot.headUnitNode()->m_data)->getName();
	}
	desc << L" }";

	return std::move(desc).str();
}
std::wstring GiganticMapsOptimisationsLib::getFullPlotDescription(const CvPlot& plot)
{
	std::wstringstream desc;

	const std::lock_guard lock(gMutex); // CvInfoBase has a bunch of `mutable` variables.

	desc << L"{ [" << plot.getX_INLINE() << L", " << plot.getY_INLINE() << L']';

	if (plot.isCity())
		desc << L" city of " << plot.getPlotCity()->getName();

	if (plot.getOwnerINLINE() != NO_PLAYER)
		desc << L" owned by " << GET_PLAYER(plot.getOwnerINLINE()).getName();

	if (plot.getNumUnits() > 0)
	{
		desc << L" with ";
		desc << CvPlayerAI::getPlayerNonInl(::getUnit(plot.headUnitNode()->m_data)->getOwnerINLINE()).getName();
		desc << L' ' << ::getUnit(plot.headUnitNode()->m_data)->getName();
	}

	desc << L' ' << std::to_array<std::wstring_view>({
			L"PLOT_PEAK",
			L"PLOT_HILLS",
			L"PLOT_LAND",
			L"PLOT_OCEAN",
		})[plot.getPlotType()];

	if (plot.getTerrainType() != NO_TERRAIN)
		desc << L' ' << GC.getTerrainInfo(plot.getTerrainType()).getDescription();

	if (plot.getFeatureType() != NO_FEATURE)
		desc << L' ' << GC.getFeatureInfo(plot.getFeatureType()).getDescription();

	if (plot.getRouteType() != NO_ROUTE)
		desc << L' ' << GC.getRouteInfo(plot.getRouteType()).getDescription();

	if (plot.getImprovementType() != NO_IMPROVEMENT)
		desc << L' ' << GC.getImprovementInfo(plot.getImprovementType()).getDescription();

	std::bitset<NUM_DIRECTION_TYPES> riverCrossings{};
	for (const auto i : range(NUM_DIRECTION_TYPES))
		riverCrossings[i] = plot.isRiverCrossing(i);
	if (riverCrossings.any())
		desc << L" riverCrossings=" << riverCrossings;

	desc << L" }";

	return std::move(desc).str();
}

std::string GiganticMapsOptimisationsLib::toString(UnitAITypes x)
{
	CvWString str;
	getUnitAIString(str, x);
	return heck::toAsciiString(str);
}

static_assert(
	rectdiv({ .min{ -8, 0 }, .max{ -1, 7 } }, 8) == iaabb2{ .min{ -1, 0 }, .max{ 0, 1 } } &&
	rectdiv({ .min{ -8, 0 }, .max{ +0, 8 } }, 8) == iaabb2{ .min{ -1, 0 }, .max{ 0, 1 } } &&
	rectdiv({ .min{ -8, 0 }, .max{ +1, 9 } }, 8) == iaabb2{ .min{ -1, 0 }, .max{ 1, 2 } }
	);

static void unionUnwrappedInterval(int dim, bool wrap, int& unwrappedMin, int& unwrappedMax, int wrappedX)
{
	if (unwrappedMin == unwrappedMax) [[unlikely]]
	{
		unwrappedMin = wrappedX;
		unwrappedMax = wrappedX + 1;
	}
	else if (wrap)
	{
		struct Choice
		{
			int min{};
			int max{};
			int range = max - min;
		};

		Choice a{ std::min(unwrappedMin, wrappedX - dim), std::max(unwrappedMax, wrappedX + 1 - dim) };
		Choice b{ std::min(unwrappedMin, wrappedX), std::max(unwrappedMax, wrappedX + 1) };
		const Choice c{ std::min(unwrappedMin, wrappedX + dim), std::max(unwrappedMax, wrappedX + 1 + dim) };

		// Pick the smallest range.
		if (b.range > c.range)
			b = c;
		if (a.range > b.range)
			a = b;

		unwrappedMin = a.min;
		unwrappedMax = a.max;
	}
	else
	{
		unwrappedMin = std::min(unwrappedMin, wrappedX);
		unwrappedMax = std::max(unwrappedMax, wrappedX + 1);
	}
}

void GiganticMapsOptimisationsLib::unionUnwrappedRect(const CivMapGeometry& mapGeom, iaabb2& unwrappedRect, ivec2 wrappedCoord) noexcept
{
	unionUnwrappedInterval(mapGeom.dim.x, mapGeom.wrapX, unwrappedRect.min.x, unwrappedRect.max.x, wrappedCoord.x);
	unionUnwrappedInterval(mapGeom.dim.y, mapGeom.wrapY, unwrappedRect.min.y, unwrappedRect.max.y, wrappedCoord.y);
}

#endif