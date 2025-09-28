#include "../CommonConfig.h"

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS

#include "VectorisedPathfinderMap.h"
#include "Util.h"
#include "UnitPathingUtil.h"

#include <CommonStuff/PQCoordSimdAStar.h>

#include "../CvInfos.h"
#include "../CvUnit.h"
#include "../CvPlayerAI.h"
#include "../CvTeamAI.h"
#include "../CvDLLInterfaceIFaceBase.h"
#include "../CvDLLUtilityIFaceBase.h"

#include <CommonStuff/ParallelExt.h>

#include <chrono>
#include <unordered_set>
#include <iostream>
#include <future>

using namespace GiganticMapsOptimisationsLib;

static constexpr uint8_t kHeuristicPlotMask_RegularCost = 0b11; // [1..3]
static constexpr uint8_t kHeuristicPlotFlag_InDomain = 1 << 2;
static constexpr uint8_t kHeuristicPlotFlag_Forest = 1 << 3;
static constexpr uint8_t kHeuristicPlotFlag_Hills = 1 << 4;
static constexpr uint8_t kHeuristicPlotFlag_HasRoute = 1 << 5;
static constexpr int kHeuristicPlotShift_RouteType = 6;
static constexpr unsigned int kRouteTypeIndexWidth = std::bit_width(kNumRouteTypes - 1);
static constexpr int kHeuristicPlotMask_RouteType = ((1 << kRouteTypeIndexWidth) - 1) << kHeuristicPlotShift_RouteType;

// TODO: Heuristic instances are actually invalidated rather frequently. +/-30 plot changes every turn. Any way to stop this except by jacking up the threshold?
[[maybe_unused]] static constexpr int kUpdateStaleHeuristicBiasThresholdMP = 64 * MOVE_DENOMINATOR * PATH_MOVEMENT_WEIGHT; // About X turns of MP.

static constexpr int kGarbageCollectionGCTimeThreshold = 10;

// Landmarks disabled for now.
// The asynchronous background update makes the game non-deterministic.
// A determinstic update is possible by simply using the previous turn's heuristic (for known movement types), but that makes the heuristic part of game state!
//static constexpr bool kEnableLandmarkHeuristics = false;
#define CV4MINIENGINE_ENABLE_LANDMARK_HEURISTICS 0

#if CV4MINIENGINE_ENABLE_LANDMARK_HEURISTICS
static constexpr int kMinAreaSize = 100;
#endif

static constexpr bool kEnableHeuristicBiasLogging = false;

std::string VectorisedPathfinderMap::HeuristicUnitType::toString() const
{
	return std::format("MovementType{{ mp={}, routeChanges=[{}, {}], doubleForest={}, doubleHills={} water={} }}",
		maxMP, teamRouteCostChanges[0], teamRouteCostChanges[1], doubleForestMovement, doubleHillsMovement, water
	);
}

#if CV4MINIENGINE_ENABLE_LANDMARK_HEURISTICS
static std::vector<ivec2> gatherMapSeedPlots(const CvMap& map, bool water)
{
	std::unordered_set<const CvArea*> areas;
	std::vector<ivec2> seeds;
	seeds.reserve(map.numPlotsINLINE());
	for (const int y : range(map.getGridHeightINLINE()))
		for (const int x : range(map.getGridWidthINLINE()))
			if (const CvPlot& plot = *map.plot(x, y); water == plot.isWater() && plot.area()->getNumTiles() >= kMinAreaSize && areas.insert(plot.area()).second)
				seeds.emplace_back(x, y);
	return seeds;
}
#endif

DynamicArray2D<VectorisedPathfinderMap::HeuristicPlot> VectorisedPathfinderMap::buildHeuristicMap(const CvMap& map, bool water)
{
	DynamicArray2D<HeuristicPlot> plots{ CivMapGeometry(map).dim };
	for (const int y : range(plots.dim.y))
		for (const int x : range(plots.dim.x))
			plots[{ x, y }] = calcHeuristicPlot(map, { x, y }, water);
	return plots;
}

VectorisedPathfinderMap::HeuristicPlot VectorisedPathfinderMap::calcHeuristicPlot(const CvMap& map, ivec2 coord, bool water)
{
	const CvPlot& plot = *map.plot(coord.x, coord.y);
	HeuristicPlot bits{};
	if (water)
	{
		// Water units are really simple. Just 60MP for every domain plot.
		if ((plot.isWater() || plot.isCoastalLand()) && !plot.isImpassable())
			bits = 1 | kHeuristicPlotFlag_InDomain;
	}
	else
	{
		if (!plot.isWater() && !plot.isImpassable())
			bits |= kHeuristicPlotFlag_InDomain;

		// CvPlot::movementCost
		int iRegularCost = ((plot.getFeatureType() == NO_FEATURE) ? GC.getTerrainInfo(plot.getTerrainType()).getMovementCost() : GC.getFeatureInfo(plot.getFeatureType()).getMovementCost());
		if (plot.isHills())
			iRegularCost += GC.getHILLS_EXTRA_MOVEMENT();
		if (iRegularCost > 3)
			std::abort();
		if (bits & kHeuristicPlotFlag_InDomain) // zero cost for non-valid plots.
			bits |= (uint8_t)iRegularCost;
		bits |= isForest(plot.getFeatureType()) ? kHeuristicPlotFlag_Forest : 0;
		bits |= plot.isHills() ? kHeuristicPlotFlag_Hills : 0;
		if (plot.isRoute())
			bits |= kHeuristicPlotFlag_HasRoute;
		bits |= std::max<int>(0, plot.getRouteType()) << kHeuristicPlotShift_RouteType;
	}
	return bits;
}

ivec2 VectorisedPathfinderMap::pickNewLandmarkLocation(const HeuristicMap& plotMap, std::span<const i16vec2> seeds) const
{
	// Actually using unsigned values, but signed lets us use std::ranges::max_element to get max dist.

	const DynamicArray2D<HeuristicPlot>& map = plotMap.plots;

	const CivMapGeometry mapSizeInfo(GC.getMapINLINE());

	DynamicArray2D<int32_t> distanceField(map.dim, -1);

	std::vector<i16vec2> queue;
	size_t queueI = 0;
	queue.reserve(map.cells.size());

	for (const ivec2 seed : seeds)
	{
		distanceField[seed] = 0;
		queue.push_back(seed);
	}

	ivec2 best = seeds[0];

	while (queueI < queue.size())
	{
		const ivec2 thisCoord = queue[queueI++];
		const uint32_t thisDist = distanceField[thisCoord];

		for (const ivec2 adjD : kAdj)
		{
			const ivec2 adjCoordUnwrapped = thisCoord + adjD;
			if (mapSizeInfo.isValidCoord(adjCoordUnwrapped))
			{
				const ivec2 adjCoord = mapSizeInfo.wrapCoord(adjCoordUnwrapped);

				if (thisDist + 1 < (uint32_t)distanceField[adjCoord] && (map[adjCoord] & kHeuristicPlotFlag_InDomain) != 0)
				{
					distanceField[adjCoord] = int32_t(thisDist + 1);
					best = adjCoord; // Distance is always increasing, the most distant should simply be the last visited coord.
					queue.push_back(adjCoord);
				}
			}
		}
	}

	//const size_t index = std::ranges::max_element(distanceField.cells) - distanceField.cells.begin();
	//return {
	//	int(index) % map.dim.x,
	//	int(index) / map.dim.x,
	//};
	return best;
}

std::array<i16vec2, VectorisedPathfinderMap::kNumLandmarks> VectorisedPathfinderMap::computeLandmarkLocations(const HeuristicMap& plotMap, ivec2 seed) const
{
	std::array<i16vec2, kNumLandmarks> p{ (i16vec2)seed };
	p[0] = pickNewLandmarkLocation(plotMap, std::span(p).subspan(0, 1));
	for (const int i : range(1, kNumLandmarks))
		p[i] = pickNewLandmarkLocation(plotMap, std::span(p).subspan(0, i));
	return p;
}

struct VectorisedPathfinderMap::LandmarkHeuristicGraph : heck::ISimdAStarGraph
{
	const DynamicArray2D<HeuristicPlot>& map;
	HeuristicUnitType unitType{};
	uint8_t halfCostMask = 0;
	Vector evaluatedRouteCosts;

	explicit LandmarkHeuristicGraph(HeuristicUnitType unitType, const DynamicArray2D<HeuristicPlot>& map, heck::ISimdAStarGraph::Vector evaluatedRouteCosts)
		: map(map), unitType(unitType), evaluatedRouteCosts(evaluatedRouteCosts)
	{
		halfCostMask |= unitType.doubleForestMovement ? 0b01 : 0;
		halfCostMask |= unitType.doubleHillsMovement ? 0b10 : 0;

		if (GC.getNumRouteInfos() != 2)
			std::abort();

		// TODO: Use minimum route change of all teams.
		//std::array<int32_t, LandmarkHeuristicGraph::Vector::kNumElements> routeCosts{};
		//std::ranges::copy(calcRouteCostCombinations(NO_TEAM, unitType.maxMP), routeCosts.begin());
		//evaluatedRouteCosts = LandmarkHeuristicGraph::Vector(routeCosts);
	}

	// Ignores routes.
	static int getPlotMovementRegularCost(HeuristicUnitType movementType, HeuristicPlot toPlotProps)
	{
		const unsigned int halfCostMask =
			(movementType.doubleForestMovement ? 0b01 : 0) |
			(movementType.doubleHillsMovement ? 0b10 : 0);

		int regularCost = (toPlotProps & kHeuristicPlotMask_RegularCost) * MOVE_DENOMINATOR;
		//Vector regularCost = Vector(std::array<int, 16>{ 0, 60, 60*2, 60*3, 60*4 }).permute(toPlotProps & kHeuristicPlotMask_RegularCost);
		regularCost = std::min(regularCost, movementType.maxMP);
		//regularCost = vselect(vtest(toPlotProps, halfCostMask), regularCost >> std::integral_constant<size_t, 1>(), regularCost);
		if (toPlotProps & halfCostMask)
			regularCost >>= 1;

		return regularCost;
	}

	Step getStepCostWithProps(StateVector fromState, Vector fromPlotProps, Vector toPlotProps) const
	{
		const StateVector fromMP = vselect(vcmpeq(fromState, 0), unitType.maxMP, fromState);

		Vector regularCost = (toPlotProps & kHeuristicPlotMask_RegularCost) * MOVE_DENOMINATOR;
		//Vector regularCost = Vector(std::array<int, 16>{ 0, 60, 60*2, 60*3, 60*4 }).permute(toPlotProps & kHeuristicPlotMask_RegularCost);
		regularCost = vmin(regularCost, unitType.maxMP);
		//regularCost = vselect(vtest(toPlotProps, halfCostMask), regularCost >> std::integral_constant<size_t, 1>(), regularCost);
		regularCost = vmaskedshr(vtest(toPlotProps, halfCostMask), regularCost, std::integral_constant<size_t, 1>());

		//if ((vtest(fromPlotProps, kRouteMask) & vtest(toPlotProps, kRouteMask)).asBits())
		{
			const Vector::Mask usableRoutes = vtest(fromPlotProps & toPlotProps, kHeuristicPlotFlag_HasRoute);
			const Vector routeSel = ((fromPlotProps & kHeuristicPlotMask_RouteType) | ((toPlotProps & kHeuristicPlotMask_RouteType) << std::integral_constant<size_t, 1>())) >> std::integral_constant<size_t, kHeuristicPlotShift_RouteType>();
			const Vector routeCosts = vselect(usableRoutes, evaluatedRouteCosts.permute(routeSel), INT32_MAX);
			regularCost = vmin(regularCost, routeCosts);
		}

		const Vector cost = vmin(Vector(fromMP), regularCost);
		const StateVector toMP = fromMP - StateVector(cost);

		//constexpr int kInDomainMask = 1 << 2;

		// The from or to plot for the domain check? `pathValid` uses from plot.
		// If we use `to` plot, we stay strictly in the domain. The only downside is that some heuristic values will be crazy high?
		//return { vselect(vtest(toPlotProps, kInDomainMask), cost, 0), toMP };
		return { cost * PATH_MOVEMENT_WEIGHT, toMP };
	}

	//virtual Step HECK_VECTORCALL getStepCost(StateVector fromState, Vector fromPlotIndices, Vector toPlotIndices, [[maybe_unused]] Vector adjI) const override
	//{
	//	const std::span cells = map.cells; // Local copy to help optimiser
	//	const Vector fromPlotProps(heck::simd::kOutOfBoundsGather, cells, fromPlotIndices);
	//	const Vector toPlotProps(heck::simd::kOutOfBoundsGather, cells, toPlotIndices);
	//	return getStepCostWithProps(fromState, fromPlotProps, toPlotProps);
	//}

	StateVector storedFromState{};
	Vector storedFromPlotProps{};

	virtual void HECK_VECTORCALL setStepFrom(StateVector fromState, Vector fromPlotIndices) override
	{
		const std::span cells = map.cells; // Local copy to help optimiser
		storedFromState = fromState;
		storedFromPlotProps = Vector(heck::simd::kOutOfBoundsGather, cells, fromPlotIndices);
	}
	virtual Step HECK_VECTORCALL getStepCostTo(Vector toPlotIndices, [[maybe_unused]] Vector adjI) const override
	{
		const std::span cells = map.cells; // Local copy to help optimiser
		const Vector toPlotProps(heck::simd::kOutOfBoundsGather, cells, toPlotIndices);
		return getStepCostWithProps(storedFromState, storedFromPlotProps, toPlotProps);
	}
	virtual Vector HECK_VECTORCALL getHeuristic(Vector) const override
	{
		return {};
	}

	virtual int getSingleHeuristic(int) const override
	{
		return 0;
	}
};

//static int toPlotIndex(i16vec2 coord, ivec2 dim)
//{
//	return coord.x + coord.y * dim.x;
//}

VectorisedPathfinderMap::MultiLandmarkDistanceField VectorisedPathfinderMap::computeLandmarkHeuristicInstance(
	std::span<const std::array<i16vec2, kNumLandmarks>> areas,
	const DynamicArray2D<HeuristicPlot>& heuristicMap,
	HeuristicUnitType unit,
	heck::ISimdAStarGraph::Vector evaluatedRouteCosts
)
{
	MultiLandmarkDistanceField field(heuristicMap.dim);

	LandmarkHeuristicGraph graph(unit, heuristicMap, evaluatedRouteCosts);

	std::vector<heck::PQCoordSimdAStar> astarInstances;
	astarInstances.reserve(kNumLandmarks);
	for (const int landmarkI : range(kNumLandmarks))
	{
		std::vector<i16vec2> startingPlotCoords;
		for (const auto& area : areas)
			startingPlotCoords.push_back((i16vec2)area[landmarkI]);
		astarInstances.emplace_back(heuristicMap.dim, GC.getMapINLINE().isWrapXINLINE(), GC.getMapINLINE().isWrapYINLINE()).addStart(startingPlotCoords);
	}

	// Compute landmarks in parallel.
	// If we put the areas in the inner loop, A* state can be reused as visit sets should not overlap.
	heck::parallelForEachN(kNumLandmarks, [&](int landmarkI) {
		//std::ranges::for_each_n(std::views::iota(0).begin(), kNumLandmarks, [&](int landmarkI) {
		heck::PQCoordSimdAStar& astar = astarInstances[landmarkI];
		//for (const auto& area : areas)
		{

			//astar.reuseForNonOverlappingVisitSet(toPlotIndex(area[landmarkI], plots.dim));
			(void)astar.search(-1, graph);
			//(void)astar.searchUsingPriorityQueue(-1, graph);
		}
		});

	//std::vector<std::future<void>> futures;
	//for (const int landmarkI : range(kNumLandmarks))
	//{
	//	futures.push_back(std::async(std::launch::async, [&, landmarkI] {
	//		heck::SimdAStar& astar = astarInstances[landmarkI];
	//		for (const auto& area : areas)
	//		{
	//
	//			astar.reuseForNonOverlappingVisitSet(toPlotIndex(area[landmarkI], plots.dim));
	//			(void)astar.search(-1, graph);
	//		}
	//		}));
	//	
	//}
	//for (auto& f : futures)
	//	f.wait();

	//for (heck::SimdAStar& astar : astarInstances)
	//	astar.dumpStats(std::clog);

	// Copy values into the field. Can't really do *landmarks* in parallel as simd::Vector may write the whole vector when setting an element.
	const ivec2 dim = heuristicMap.dim;
	for (const int y : range(dim.y))
		for (const int x : range(dim.x))
			for (const int landmarkI : range(kNumLandmarks))
				field[{ x, y }].setElement(landmarkI, astarInstances[landmarkI].getGCost({ x, y }));

	return field;
}


VectorisedPathfinderMap::VectorisedPathfinderMap()
	: mLandHeuristicMap(std::make_shared<HeuristicMap>(0, buildHeuristicMap(GC.getMapINLINE(), false)))
	//, mWaterHeuristicMap(std::make_shared<HeuristicMap>(0, buildHeuristicMap(GC.getMapINLINE(), true)))
{
	for (const auto i : range<PromotionTypes>(GC.getNumPromotionInfos()))
	{
		const auto& info = GC.getPromotionInfo(i);
		if (info.isHillsDoubleMove())
		{
			if (mHillsMovementPromotionI != NO_PROMOTION)
				std::abort();
			mHillsMovementPromotionI = i;
		}
		const size_t forestDefaultMoveCount = std::ranges::count_if(kForestFeatureTypes, std::bind_front(&CvPromotionInfo::getFeatureDoubleMove, std::ref(info)));
		if (forestDefaultMoveCount > 0)
		{
			if (forestDefaultMoveCount == std::size(kForestFeatureTypes))
			{
				if (mForestMovementPromotionI != NO_PROMOTION)
					std::abort();
				mForestMovementPromotionI = i;
			}
			else
				std::abort(); // Only applied to some features.
		}
	}

	if (mHillsMovementPromotionI == NO_PROMOTION || mForestMovementPromotionI == NO_PROMOTION)
		std::abort();

	mTeamPlotProps.reserve(MAX_TEAMS);
	//mTeamBorderPlotDanger.reserve(MAX_TEAMS);
	for (const auto i : range<TeamTypes>(MAX_TEAMS))
	{
		mTeamPlotProps.emplace_back(i, &mAllBorderPlotDangerSourceCache, &mUnitPlotDanger);
		//mTeamBorderPlotDanger.emplace_back(i);
	}

#if CV4MINIENGINE_ENABLE_LANDMARK_HEURISTICS
	{
		const std::vector<ivec2> landSeedPlots = gatherMapSeedPlots(GC.getMapINLINE(), false);
		const std::vector<ivec2> waterSeedPlots = gatherMapSeedPlots(GC.getMapINLINE(), true);

		using Clock = std::chrono::steady_clock;
		auto t0 = Clock::now();

		mLandAreaLandmarks.assign_range(landSeedPlots | std::views::transform([this](ivec2 seed) {
			return computeLandmarkLocations(*mLandHeuristicMap, seed);
			}));

		std::clog << "Land landmark locations computed in " << std::chrono::duration<double, std::milli>(Clock::now() - t0) << std::endl;
	}
#else
	std::clog << "Landmarks disabled." << std::endl;
#endif

	//t0 = Clock::now();

	//mWaterAreaLandmarks.assign_range(waterSeedPlots | std::views::transform([this](ivec2 seed) {
	//	return computeLandmarkLocations(*mWaterHeuristicMap, seed);
	//	}));
	//
	//std::clog << "Water landmark locations computed in " << std::chrono::duration<double, std::milli>(Clock::now() - t0) << std::endl;

	//heck::Image img = renderMap();
	//for (const auto& landmarkArea : areaLandmarks)
	//	for (const i16vec2 p : landmarkArea)
	//	{
	//		img.drawPixel(p, { 255, 255, 255 }, 255);
	//		img.drawRect(iaabb2::sized(p, { 1, 1 }).expanded({ 3, 3 }), { 255, 255, 255 }, 255);
	//	}
	//img.saveAsPPM("PathfinderMap Landmarks.ppm");
	
	//static constexpr HeuristicUnitType kUnitTypes[]{
	//	{ 1 * MOVE_DENOMINATOR, false, false },
	//	{ 1 * MOVE_DENOMINATOR, true, false }, // warrior
	//	{ 1 * MOVE_DENOMINATOR, false, true }, // archer
	//	{ 2 * MOVE_DENOMINATOR, false, false }, // horse
	//	{ 2 * MOVE_DENOMINATOR, true, false }, // explorer
	//	{ 2 * MOVE_DENOMINATOR, false, true }, // explorer
	//	{ 2 * MOVE_DENOMINATOR, true, true }, // explorer
	//};
	//
	//for (const HeuristicUnitType unitType : kUnitTypes)
	//{
	//	t0 = Clock::now();
	//	mMultiLandmarkDistanceField.emplace(unitType, computeLandmarkHeuristicInstance(areaLandmarks, unitType));
	//	std::clog << "Landmark heuristic for unit type computed in " << std::chrono::duration<double, std::milli>(Clock::now() - t0) << std::endl;
	//}

	//t0 = Clock::now();
	//std::for_each(std::execution::par, std::begin(kUnitTypes), std::end(kUnitTypes), [&, mutex = std::mutex()](HeuristicUnitType unitType) mutable {
	//	const auto t0 = Clock::now();
	//	PathfinderMultiLandmarkDistanceField field = computeLandmarkHeuristicInstance(mHeuristicPlots, areaLandmarks, unitType);
	//	std::scoped_lock lock(mutex);
	//	mMultiLandmarkDistanceField.emplace(unitType, std::move(field));
	//	std::clog << "Landmark heuristic for unit type computed in " << std::chrono::duration<double, std::milli>(Clock::now() - t0) << std::endl;
	//	});
	//std::clog << "Landmark heuristic computed in " << std::chrono::duration<double, std::milli>(Clock::now() - t0) << std::endl;

	mFortImprovementI = (ImprovementTypes)GC.getInfoTypeForString("IMPROVEMENT_FORT");
}


VectorisedPathfinderMap::LandmarkDistanceFieldWithBias VectorisedPathfinderMap::tryGetMultiLandmarkDistanceField(const CvSelectionGroup&)
{
#if CV4MINIENGINE_ENABLE_LANDMARK_HEURISTICS
	/// TODO: Need to support groups.
	if (unit.getDomainType() == DOMAIN_SEA)
		return {};
	if (!std::ranges::contains(std::array{ DOMAIN_LAND, DOMAIN_SEA }, unit.getDomainType()))
		std::abort();
	
	const CvTeam& team = GET_TEAM(unit.getTeam());
	const HeuristicUnitType unitType{
		.maxMP = std::min(unit.baseMoves(), 2) * MOVE_DENOMINATOR,
		.teamRouteCostChanges = {
			team.getRouteChange(kRouteType_Road),
			team.getRouteChange(kRouteType_Railroad),
		},
		.doubleForestMovement = unit.isHasPromotion(mForestMovementPromotionI),
		.doubleHillsMovement = unit.isHasPromotion(mHillsMovementPromotionI),
		.water = unit.getDomainType() == DOMAIN_SEA,
	};
	
	std::shared_ptr<HeuristicMap>& heuristicMapRef = unitType.water ? mWaterHeuristicMap : mLandHeuristicMap;
	
	
	// This is called by the parallel unit update.
	static std::mutex mutex;
	const std::lock_guard lock(mutex);
	
	HeuristicInstance& instance = mHeuristicInstances[unitType];
	
	if (instance.lastGCTimeUsed < 0)
	{
		// Init.
		instance.evaluatedRouteCosts = calcRouteCostCombinations(unitType.teamRouteCostChanges, unitType.maxMP);
	}
	
	instance.lastGCTimeUsed = mCurrentGCTime;
	
	//for (;;)
	{
		// Check deferred compute completion.
		if (instance.deferredDistanceField.valid() && instance.deferredDistanceField.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
		{
			instance.distanceField = instance.deferredDistanceField.get();
			instance.bias = instance.deferredBias;
			instance.deferredBias = 0;
		}
	
		// If we need an update, get it started.
		if (instance.distanceField.cells.empty() || instance.bias > kUpdateStaleHeuristicBiasThresholdMP)
		{
			if (!instance.deferredDistanceField.valid())
			{
				if (instance.distanceField.cells.empty())
					std::clog << std::format("New landmark heuristic instance for {} requested. Starting compute task.\n", unitType.toString());
				else
					std::clog << std::format("Landmark heuristic instance update for {} requested. firstUseGCTime={}, bias={}.\n", unitType.toString(), instance.firstUseGCTime, instance.bias);
	
				// Don't want the current plot array modified.
				heuristicMapRef->numConcurrentUsers.fetch_add(1, std::memory_order_acquire);
	
				// Reset this.
				instance.deferredBias = 0;
	
				auto& areaLandmarks = unitType.water ? mWaterAreaLandmarks : mLandAreaLandmarks;;
	
				// Start compute task.
				// Hopefully, this ends up in a threadpool rather than starting a whole new thread.
				// We'll take a shared ptr to the heuristic map, so it's zero-copy until we want to change it later while a concurrent compute task is still going.
				instance.deferredDistanceField = std::async(std::launch::async | std::launch::deferred,
					[unitType, map = heuristicMapRef, &areaLandmarks, evaluatedRouteCosts = instance.evaluatedRouteCosts, this] {
						const auto t0 = std::chrono::steady_clock::now();
						MultiLandmarkDistanceField distanceField = computeLandmarkHeuristicInstance(areaLandmarks, map->plots, unitType, evaluatedRouteCosts);
						const auto t1 = std::chrono::steady_clock::now();
	
						// TODO: Proper multithreaded logging.
						//gDLL->logMemState(std::format("Landmark heuristic instance for {} computed in {}.",
						//	unitType.toString(), std::chrono::duration<double, std::milli>(t1 - t0)
						//).c_str());
	
						map->numConcurrentUsers.fetch_sub(1, std::memory_order_release);
						return distanceField;
					});
				//instance.deferredDistanceField.wait();
			}
			// else, an update is already in-progress.
			else
			{
				//instance.deferredDistanceField.wait();
			}
		}
		//else
		//	break;
	}
	
	if (instance.distanceField.cells.empty())
		return {};
	else
	{
		instance.firstUseGCTime = std::min(instance.firstUseGCTime, instance.lastGCTimeUsed);
		return { &instance.distanceField, instance.bias };
	}
#else
	return {};
#endif
}

void VectorisedPathfinderMap::onTeamEvent(EGameTeamChangeEvent e, TeamTypes a, int b)
{
	switch (e)
	{
	case EGameTeamChangeEvent::War:
		for (auto& x : mTeamPlotProps)
			x.onWarChange();
		mUnitPlotDanger.onDiploChange(EPlotDangerDiploChange::AnyWar, a, static_cast<TeamTypes>(b));
		break;
	case EGameTeamChangeEvent::OpenBorders:
		mTeamPlotProps[a].onOpenBordersChange();
		mTeamPlotProps[b].onOpenBordersChange();
		mUnitPlotDanger.onDiploChange(EPlotDangerDiploChange::OpenBorders, a, static_cast<TeamTypes>(b));
		break;
	case EGameTeamChangeEvent::Vassal:
		for (auto& x : mTeamPlotProps)
			x.onAnyVassalChange();
		mUnitPlotDanger.onDiploChange(EPlotDangerDiploChange::AnyVassal, a, static_cast<TeamTypes>(b));
		break;
	case EGameTeamChangeEvent::AreaBorderObstacle:
		mTeamPlotProps[BARBARIAN_TEAM].onAreaBorderObstacle();
		mUnitPlotDanger.onDiploChange(EPlotDangerDiploChange::AreaIsBorderObstacleChanged, a, NO_TEAM);
		break;
	case EGameTeamChangeEvent::PlayerCityDefenceModifier:
		// Depends on city owner, not unit owner, so this affects all teams!
		for (auto& x : mTeamPlotProps)
			x.onPlayerCityDefenceModifierChanged();
		mUnitPlotDanger.onDiploChange(EPlotDangerDiploChange::PlayerCityDefenceModifier, a, NO_TEAM);
		break;
	case EGameTeamChangeEvent::BridgeBuilding:
		mTeamPlotProps[a].onBridgeBuilding();
		break;
	case EGameTeamChangeEvent::AI_isDeclareWar:
	case EGameTeamChangeEvent::AI_isSneakAttackReady:
	case EGameTeamChangeEvent::AI_setWarPlan:
	case EGameTeamChangeEvent::Alive:
	case EGameTeamChangeEvent::PermanentWarPeace:
	case EGameTeamChangeEvent::HasMet:
		mTeamPlotProps[a].onAI_getWarPlan_Changed();
		break;
	case EGameTeamChangeEvent::ForcePeace:
		mTeamPlotProps[a].onForcePeaceChanged();
		break;
	case EGameTeamChangeEvent::PermanentAlliance:
	default:
		break;
	}
}
void VectorisedPathfinderMap::onPlotEvent(EGamePlotChangeEvent e, const CvPlot& plot, int oldValue, int newValue)
{
	const auto forAllTeams = [&plot, this](TeamPathingPlotPropsCache::EPlotChange e) {
		for (auto& x : mTeamPlotProps)
			x.onPlotChange(plot, e);
		};

	// This may be called from multiple threads due to parallel unit update.
	// HasUnits, InvisibilitySight, RevealedByTeam
	// Team plot props and unit danger handle the parallelism.

	switch (e)
	{
	case EGamePlotChangeEvent::Owner:
		forAllTeams(TeamPathingPlotPropsCache::EPlotChange::Owner);
		mAllBorderPlotDangerSourceCache.onPlotOwnerChanged(plot);
		break;
	case EGamePlotChangeEvent::Terrain:
		forAllTeams(TeamPathingPlotPropsCache::EPlotChange::Terrain);

		if constexpr (kEnableHeuristicBiasLogging)
			std::wclog << L"Heuristic bias from Terrain change at " << getFullPlotDescription(plot) << std::endl;
		updateHeuristicPlot(getPlotCoord(plot));
		break;
	case EGamePlotChangeEvent::Feature:
		forAllTeams(TeamPathingPlotPropsCache::EPlotChange::Feature);
		if constexpr (kEnableHeuristicBiasLogging)
			std::wclog << L"Heuristic bias from Feature change at " << getFullPlotDescription(plot) << std::endl;
		updateHeuristicPlot(getPlotCoord(plot));
		break;
	case EGamePlotChangeEvent::RouteType:
		forAllTeams(TeamPathingPlotPropsCache::EPlotChange::RouteType);
		mAllBorderPlotDangerSourceCache.onPlotRouteTypeChanged(plot);
		if constexpr (kEnableHeuristicBiasLogging)
			std::wclog << L"Heuristic bias from RouteType change at " << getFullPlotDescription(plot) << std::endl;
		updateHeuristicPlot(getPlotCoord(plot));
		break;
	case EGamePlotChangeEvent::Improvement:
		forAllTeams(TeamPathingPlotPropsCache::EPlotChange::Improvement);
		break;
	case EGamePlotChangeEvent::HasCity:
		forAllTeams(TeamPathingPlotPropsCache::EPlotChange::HasCity);
		mAllBorderPlotDangerSourceCache.onPlotIsCityChanged(plot);
		break;

	case EGamePlotChangeEvent::CityIsOccupation:
		forAllTeams(TeamPathingPlotPropsCache::EPlotChange::CityIsOccupation);
		break;
	case EGamePlotChangeEvent::CityBuildingDefence:
		forAllTeams(TeamPathingPlotPropsCache::EPlotChange::CityBuildingDefense);
		break;
	case EGamePlotChangeEvent::CityCultureLevel:
	case EGamePlotChangeEvent::CityDefenceDamage:
	case EGamePlotChangeEvent::CityPlayerAddedDefence:
		forAllTeams(TeamPathingPlotPropsCache::EPlotChange::CityCultureLevel);
		break;

	//case EGamePlotChangeEvent::UnitMoveFrom:
	//case EGamePlotChangeEvent::UnitMoveTo:
	//case EGamePlotChangeEvent::UnitCreated:
	//case EGamePlotChangeEvent::UnitDestroyed:
	//	for (const auto i : range<TeamTypes>(MAX_TEAMS))
	//		if (newValue != i)
	//			mTeamPlotProps[i].onPlotChange(plot, TeamPathingPlotPropsCache::EPlotChange::UnitList);
	//	break;

	case EGamePlotChangeEvent::RevealedByTeam:
		mTeamPlotProps[newValue].onPlotChange(plot, TeamPathingPlotPropsCache::EPlotChange::Revealed);
		break;
	case EGamePlotChangeEvent::InvisibilitySight:
		mTeamPlotProps[newValue].onPlotChange(plot, TeamPathingPlotPropsCache::EPlotChange::InvisibilitySight);
		break;

	case EGamePlotChangeEvent::HasBonus:
		mTeamPlotProps[BARBARIAN_TEAM].onPlotChange(plot, TeamPathingPlotPropsCache::EPlotChange::HasBonus);
		break;
	case EGamePlotChangeEvent::HasUnits:
		mTeamPlotProps[BARBARIAN_TEAM].onPlotChange(plot, TeamPathingPlotPropsCache::EPlotChange::HasUnits);
		break;

	default:
		break;
	}

	// Plot danger
	mUnitPlotDanger.onPlotChange(e, plot, oldValue, newValue);
}

void VectorisedPathfinderMap::onUnitEvent(EGameUnitChangeEvent e, const CvUnit& unit)
{
	const CvPlot& plot = *unit.plot();

	switch (e)
	{
	case EGameUnitChangeEvent::Created:
	case EGameUnitChangeEvent::Destroyed:
		for (const auto i : range<TeamTypes>(MAX_TEAMS))
			mTeamPlotProps[i].onPlotChange(plot, TeamPathingPlotPropsCache::EPlotChange::UnitList);
		break;

	default:
		break;
	}

	mUnitPlotDanger.onUnitChange(e, unit);
}

void VectorisedPathfinderMap::onUnitMoved(const CvUnit& unit, ivec2 from, ivec2 to)
{
	if (from.x >= 0)
	{
		const CvPlot& plot = *GC.getMapINLINE().plot(from.x, from.y);
		for (const auto i : range<TeamTypes>(MAX_TEAMS))
			mTeamPlotProps[i].onPlotChange(plot, TeamPathingPlotPropsCache::EPlotChange::UnitList);
	}
	if (to.x >= 0)
	{
		const CvPlot& plot = *GC.getMapINLINE().plot(to.x, to.y);
		for (const auto i : range<TeamTypes>(MAX_TEAMS))
			mTeamPlotProps[i].onPlotChange(plot, TeamPathingPlotPropsCache::EPlotChange::UnitList);
	}
	mUnitPlotDanger.onUnitMoved(unit, from, to);
}

uint32_t VectorisedPathfinderMap::getOwnUnitMovementTeamPlotPropsInvalidationMask()
{
	return TeamPathingPlotPropsCache::getPlotPropMask(TeamPathingPlotPropsCache::kValidityPlotPropBitIndex_CanMoveInto_IsAnimalAvoid)
		| TeamPathingPlotPropsCache::getPlotPropMask(TeamPathingPlotPropsCache::kValidityPlotPropBitIndex_NotSafeTerritory)
		| TeamPathingPlotPropsCache::getPlotPropMask(TeamPathingPlotPropsCache::kValidityPlotPropBitIndex_UnitPlotDanger)
		;
}

uint32_t VectorisedPathfinderMap::getOwnUnitMovementTeamPlotPropsInvalidationMask(const CvSelectionGroup&)
{
	// TODO: Currently not implemented: Movement of invisibility-seeing units invalidating plot danger because they find an enemy sub.
	//assert(unit->getNumSeeInvisibleTypes() == 0);
	
	// Depends on plot: kPlotPropsBitIndex_Defence,
	// Depends on plot and isBridgeBuilding: kPlotPropsBitIndex_AdjNoRoute = kPlotPropsBitIndex_Defence + 8,
	// Depends on plot: kPlotPropsBitIndex_RouteType = kPlotPropsBitIndex_AdjNoRoute + 4,
	// Depends on owner: kPlotPropsBitIndex_NotMyTeam,
	// Depends on owner, war: kPlotPropsBitIndex_IsEnemyTeam,
	// Depends on plot: kPlotPropsBitIndex_RegularCost,
	// Depends on unit: kPlotPropsBitIndex_ForestHalfCostMovement = kPlotPropsBitIndex_HalfCostMovement,
	// Depends on unit: kPlotPropsBitIndex_HillsHalfCostMovement,
	// X Depends on REVEALED, and plot owner: kPlotPropsBitIndex_PathValid_NotSafeTerritory,
	// X Depends on invisibility sight: kPlotPropsBitIndex_PathValid_PlotDanger,
	// X Depends on plot unit count, bonus, owner, improvement: kPlotPropsBitIndex_PathValid_CanMoveInto_IsAnimalAvoid,
	// Depends on plot, owner: kPlotPropsBitIndex_PathValid_CanMoveInto_IsEnemyCity,
	// Depends on enemy units and invisibility sight: kPlotPropsBitIndex_PathValid_CanMoveInto_IsVisibleEnemyUnit,
	// Depends on enemy units and invisibility sight (but always hostile doesn't care): kPlotPropsBitIndex_PathValid_CanMoveInto_IsVisibleEnemyUnitForAlwaysHostile,
	// Depends on plot: kPlotPropsBitIndex_PathValid_CanMoveInto_IsImpassable,
	// Depends on diplo, owner, war plans: kPlotPropsBitIndex_PathValid_CanMoveInto_IsCantEnterArea_UnitWarAIType_False,
	// Depends on diplo, owner, war plans: kPlotPropsBitIndex_PathValid_CanMoveInto_IsCantEnterArea_UnitWarAIType_Limited,
	// Depends on diplo, owner, war plans: kPlotPropsBitIndex_PathValid_CanMoveInto_IsCantEnterArea_UnitWarAIType_True,

	// 'X' marks bits potentially invalidated by our own unit movement.
	// kPlotPropsBitIndex_PathValid_NotSafeTerritory is worst as all units can reveal plots. But what depends on it? Not much. And only happens at unrevealed plots.
	// Luckily, there's no(?) dependence on actual plot visibility (thanks, cheating AI).
	
	return getOwnUnitMovementTeamPlotPropsInvalidationMask();
}

void VectorisedPathfinderMap::onGameTurn()
{
	// NOTE: This may cause a wait if there are pending compute tasks. But unlikely as they would have to be in-progess for X turns...
	std::erase_if(mHeuristicInstances, [this](const decltype(mHeuristicInstances)::value_type& kv) {
		return mCurrentGCTime - kv.second.lastGCTimeUsed >= kGarbageCollectionGCTimeThreshold &&
			(std::clog << std::format("Landmark heuristic instance for {} garbage collected.\n", kv.first.toString()), true);
		});

	++mCurrentGCTime;
}

void VectorisedPathfinderMap::update(TeamTypes team)
{
	//for (auto& x : mTeamBorderPlotDanger)
	//	x.update();
	mAllBorderPlotDangerSourceCache.update();
	mUnitPlotDanger.update();
	mTeamPlotProps[team].update();
}

void VectorisedPathfinderMap::verify(TeamTypes team) const
{
	const ivec2 dim = CivMapGeometry(GC.getMapINLINE()).dim;
	for (const int y : range(dim.y))
		for (const int x : range(dim.x))
			mAllBorderPlotDangerSourceCache.verify({ x, y });
	for (const int y : range(dim.y))
		for (const int x : range(dim.x))
			mUnitPlotDanger.verify(team, { x, y });

	// Do this /after/ verifying plot danger caches.
	mTeamPlotProps[team].verify();
}

const TeamPathingPlotPropsCache& VectorisedPathfinderMap::getTeamPlotProps(TeamTypes team) const
{
	return mTeamPlotProps[team];
}

PromotionTypes VectorisedPathfinderMap::getForestMovementPromotion() const
{
	return mForestMovementPromotionI;
}

PromotionTypes VectorisedPathfinderMap::getHillsMovementPromotion() const
{
	return mHillsMovementPromotionI;
}

void VectorisedPathfinderMap::updateHeuristicPlot([[maybe_unused]] ivec2 coord)
{
#if CV4MINIENGINE_ENABLE_LANDMARK_HEURISTICS
	// Land only. I assume the water heuristic is constant.
	HeuristicMap& heuristicMap = *mLandHeuristicMap;
	const HeuristicPlot before = heuristicMap.plots[coord];
	const HeuristicPlot after = calcHeuristicPlot(GC.getMapINLINE(), coord, false);

	if (before == after)
		return;

	// Need to compute the max path cost reduction given the changes.
	// Max path cost reduction from entering and leaving this plot.

	const CivMapGeometry mapGeom(GC.getMapINLINE());

	const bool hasAdjRoute = std::ranges::count_if(kAdj, [mapGeom, coord, &heuristicMap](ivec2 adjD) {
		const ivec2 unwrappedCoord = coord + adjD;
		return mapGeom.isValidCoord(unwrappedCoord) && bool(heuristicMap.plots[mapGeom.wrapCoord(unwrappedCoord)] & kHeuristicPlotFlag_HasRoute);
		}) != 0;

	// Determine all possibilities of route traversal, so we don't use railroad costs in the early game.
	std::bitset<kNumRouteTypes> adjRouteTypes{};
	for (const ivec2 adjD : kAdj)
	{
		const ivec2 unwrappedCoord = coord + adjD;
		if (mapGeom.isValidCoord(unwrappedCoord))
		{
			const ivec2 wrappedCoord = mapGeom.wrapCoord(unwrappedCoord);
			if (heuristicMap.plots[wrappedCoord] & kHeuristicPlotFlag_HasRoute)
				adjRouteTypes[(heuristicMap.plots[wrappedCoord] & kHeuristicPlotMask_RouteType) >> kHeuristicPlotShift_RouteType] = true;
		}
	}
	// Indexed as [fromRouteType | (toRouteType << kRouteTypeIndexWidth)]
	// For all combinations: entering/leaving before/after
	std::bitset<heck::ISimdAStarGraph::kCoordVectorLength> evaluatedRouteCostsMaskBits{};
	const int beforeRouteType = (before & kHeuristicPlotMask_RouteType) >> kHeuristicPlotShift_RouteType;
	const int afterRouteType = (after & kHeuristicPlotMask_RouteType) >> kHeuristicPlotShift_RouteType;
	for (const int routeType : range(kNumRouteTypes))
	{
		if (adjRouteTypes[routeType])
		{
			// Entering
			evaluatedRouteCostsMaskBits[routeType | (beforeRouteType << kRouteTypeIndexWidth)] = true;
			evaluatedRouteCostsMaskBits[routeType | (afterRouteType << kRouteTypeIndexWidth)] = true;
			// Leaving
			evaluatedRouteCostsMaskBits[beforeRouteType | (routeType << kRouteTypeIndexWidth)] = true;
			evaluatedRouteCostsMaskBits[afterRouteType | (routeType << kRouteTypeIndexWidth)] = true;
		}
	}

	const heck::ISimdAStarGraph::Vector::Mask evaluatedRouteCostsMask(evaluatedRouteCostsMaskBits);

	const LandmarkHeuristicGraph::Vector adjOffsets(heck::ISimdAStarGraph::calcAdjOffsets(heuristicMap.plots.dim.x), 0);
	const int changedPlotI = toPlotIndex(coord, heuristicMap.plots.dim.x);
	const LandmarkHeuristicGraph::Vector adjPlotProps = LandmarkHeuristicGraph::Vector(heck::simd::kOutOfBoundsGather, std::span(std::as_const(heuristicMap.plots.cells)), changedPlotI + adjOffsets);

	for (auto& [movementType, heuristicInstance] : mHeuristicInstances)
	{
		// Start with these values.
		// These are the max movement costs, so the reduction can't be any greater than these.
		//const int maxPossibleReductionEntering = std::min(kHeuristicPlotMask_RegularCost * MOVE_DENOMINATOR, movementType.maxMP);
		//const int maxPossibleReductionLeaving = maxPossibleReductionEntering;

		const heck::ISimdAStarGraph::Vector possibleRouteCosts = vselect(evaluatedRouteCostsMask, heuristicInstance.evaluatedRouteCosts, INT32_MAX);

		// Calculate max reduction from entering and leaving, then add them together.
		// Can be calculated by computing movement cost interval before/after, then taking max(0, before max - after min) as the reduction, then add the reductions.

		int maxEnteringBefore = 0;
		int minEnteringAfter = 0;

		int maxLeavingBefore = 0;
		int minLeavingAfter = 0;

		// We assume it's always possible to enter this plot without taking a route.
		// The max cost can only be lower if all plots in the 3x3 are routed, which is unlikely.
		maxEnteringBefore = LandmarkHeuristicGraph::getPlotMovementRegularCost(movementType, before);

		if ((after & kHeuristicPlotFlag_HasRoute) && hasAdjRoute)
		{
			// Taking a route is possible, so use route costs.
			minEnteringAfter = hmin(possibleRouteCosts);
		}
		else
		{
			// Taking a route is not possible, so use regular cost.
			minEnteringAfter = maxEnteringBefore;
		}

		// Now, leaving is more complicated as there are many different regular cost possibilities.
		// But thanks to SIMD, we can just evaluate all of them in one go.
		// fromMP = 0, fromPlotCoord = coord, toPlotCoord = coord + adjD
		static_assert(heck::ISimdAStarGraph::kCoordVectorLength >= std::size(kAdj));
		const LandmarkHeuristicGraph graph(movementType, heuristicMap.plots, heuristicInstance.evaluatedRouteCosts);
		maxLeavingBefore = hmax(graph.getStepCostWithProps({}, before, adjPlotProps).cost);
		const LandmarkHeuristicGraph::Vector costsLeavingAfter = graph.getStepCostWithProps({}, after, adjPlotProps).cost;
		// Remember to check for domain...
		minLeavingAfter = hmin(vselect(vcmpeq(costsLeavingAfter, 0), INT32_MAX, costsLeavingAfter));

		const int maxReductionEntering = std::max(0, maxEnteringBefore - minEnteringAfter);
		const int maxReductionLeaving = std::max(0, maxLeavingBefore - minLeavingAfter);
		const int delta = maxReductionEntering * PATH_MOVEMENT_WEIGHT + maxReductionLeaving;

		heuristicInstance.bias += delta;

		if constexpr (kEnableHeuristicBiasLogging)
			std::clog << "Heuristic bias for " << movementType.toString() << ": Adding " << delta << " (" << heuristicInstance.bias  << '/' << kUpdateStaleHeuristicBiasThresholdMP << ')' << std::endl;

		
		// And apply to the deferred bias too!
		heuristicInstance.deferredBias += delta;
	}

	// If there are heuristic compute tasks in progress, we need to copy.
	if (heuristicMap.numConcurrentUsers.load(std::memory_order_acquire) != 0)
		mLandHeuristicMap = std::make_shared<HeuristicMap>(0, heuristicMap.plots);

	mLandHeuristicMap->plots[coord] = after;
#endif
}

#endif