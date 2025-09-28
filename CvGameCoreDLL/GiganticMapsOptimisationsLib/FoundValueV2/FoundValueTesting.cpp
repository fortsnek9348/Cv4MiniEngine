#include "../../CommonConfig.h"

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS

#include "FoundValueVectorisedCalculation.h"
#include "FoundValueCommonCache.h"
#include "FoundValuePlayerCache.h"
#include "FoundValueDeadlockedBonusesTracker.h"
#include "FoundValueTesting.h"

#include "../../CvPlayerAI.h"

#include <iostream>
#include <fstream>
#include <chrono>

using namespace GiganticMapsOptimisationsLib;

using Clock = std::chrono::steady_clock;
using Microseconds = std::chrono::duration<double, std::micro>;
using Milliseconds = std::chrono::duration<double, std::milli>;

void FoundValueSystem::testPerformance([[maybe_unused]] bool wholeMapVerify)
{
	using namespace FoundValueSystem;
	CvMap& map = GC.getMapINLINE();
	const AreaMap areaMap(map);
	const CivMapGeometry mapGeom(map);

	//const PlayerTypes playerI = PlayerTypes(1);
	const PlayerTypes playerI = BARBARIAN_PLAYER;
	const bool enableHostileRangeCheck = playerI == BARBARIAN_PLAYER;
	//const ivec2 coord{ 21, 17 };

	const int minHostileUnitRange = enableHostileRangeCheck ? GC.getDefineINT("MIN_BARBARIAN_CITY_STARTING_DISTANCE") : -1;

	CommonCache commonCache(mapGeom, areaMap);
	commonCache.update(playerI);
	FoundValueDeadlockedBonusesTracker deadlockedBonusesTracker(mapGeom);
	deadlockedBonusesTracker.verify();
	PlayerCache playerCache(mapGeom, areaMap, playerI, deadlockedBonusesTracker);
	playerCache.update();

	const auto commonGlobalsInterface = commonCache.createCommonGlobalDataInterface();

	const auto playerGlobalsInterface = playerCache.createPlayerGlobalsInterface();

	commonCache.verify(areaMap);


	for (const int y : range(mapGeom.dim.y))
	{
		for (const int x : range(heck::cdiv(mapGeom.dim.x, kVectorWidth)))
		{
			const ivec2 coord{ x * (int)kVectorWidth, y };
			const auto commonLocalsInterface = commonCache.createCommonMapInterface(coord);
			const auto playerLocalsInterface = playerCache.createPlayerMapInterface(coord);
			std::array<int, kVectorWidth> foundValuesLive{};
			//std::array<int, kVectorWidth> foundValuesLogged{};
			for (const int dx : range(kVectorWidth))
			{
				foundValuesLive[dx] = GET_PLAYER(playerI).AI_foundValue(coord.x + dx, coord.y, minHostileUnitRange);

				//std::ostringstream os;
				//foundValuesLogged[dx] = computeFoundValue_Logging(playerI, coord + ivec2(dx, 0), -1, os);
				//if (foundValuesLive[dx] != foundValuesLogged[dx])
				//	std::abort();
			}

			const I32Vector valuesCached = computeFoundValue_Vectorised(
				commonGlobalsInterface, playerGlobalsInterface, commonLocalsInterface, playerLocalsInterface, enableHostileRangeCheck, I16Mask::kAll);

			if (foundValuesLive != valuesCached.toArray())
			{
				const int dx = static_cast<int>(std::ranges::mismatch(foundValuesLive, valuesCached.toArray()).in1 - foundValuesLive.begin());

				std::cout << "Found value mismatch at dx = " << dx << std::endl;
				std::cout << "Live value = " << foundValuesLive[dx] << std::endl;
				std::cout << "Vectorised value = " << valuesCached.toArray()[dx] << std::endl;

				std::ofstream liveLog("FoundValueSystem test live log.txt");
				const int foundValueLogged = computeFoundValue_Logging(playerI, coord + ivec2(dx, 0), -1, liveLog);
				liveLog.close();

				if (foundValueLogged != foundValuesLive[dx])
					std::abort();

				std::string vectorisedLog;
				computeFoundValue_Vectorised(
					commonGlobalsInterface, playerGlobalsInterface, commonLocalsInterface, playerLocalsInterface, enableHostileRangeCheck, I16Mask::kAll, &vectorisedLog);

				std::ofstream("FoundValueSystem test vectorised log.txt") << vectorisedLog;


				std::abort();
			}
		}
	}

	std::cout << "Verification complete." << std::endl;

	{
		std::atomic_int sum{};

		const auto t0 = Clock::now();

		for (const int y : range(mapGeom.dim.y)) {
			//heck::parallelForEachN(mapGeom.dim.y, [&](int y) {
			for (const int x : range(mapGeom.dim.x))
				sum.fetch_add(GET_PLAYER(playerI).AI_foundValue(x, y), std::memory_order_relaxed);
		}

		const auto t1 = Clock::now();

		std::cout << sum << std::endl;

		std::cout << "Whole-map AI_foundValue computation took " << Milliseconds(t1 - t0)
			<< " (" << Microseconds(t1 - t0) / (mapGeom.dim.x * mapGeom.dim.y) << " per plot)." << std::endl;
	}

	{
		std::vector<I32Vector> rowSums(mapGeom.dim.y);

		const auto t0 = Clock::now();

		for (const int y : range(mapGeom.dim.y)) {

			//heck::parallelForEachN(mapGeom.dim.y, [&](int y) {
			//heck::parallelWorkStealingForEachN(mapGeom.dim.y, [&](size_t threadI, size_t y) {
			const ivec2 rowCoord{ 0 * (int)kVectorWidth, (int)y };
			auto commonLocalsInterface = commonCache.createCommonMapInterface(rowCoord);
			auto playerLocalsInterface = playerCache.createPlayerMapInterface(rowCoord);

			I32Vector sum;

			for ([[maybe_unused]] const int x : range(heck::cdiv(mapGeom.dim.x, kVectorWidth)))
			{
				const I32Vector valuesCached = computeFoundValue_Vectorised(commonGlobalsInterface, playerGlobalsInterface, commonLocalsInterface, playerLocalsInterface, false, I16Mask::kAll);

				sum += valuesCached;

				commonLocalsInterface.addressingCoord.x += kVectorWidth;
				playerLocalsInterface.addressingCoord.x += kVectorWidth;
			}

			rowSums[y] = sum;
		}

		I32Vector sum{};
		for (const int y : range(mapGeom.dim.y))
			sum += rowSums[y];

		const auto t1 = Clock::now();

		std::cout << hmax(sum) << std::endl;

		std::cout << "Whole-map vectorised found value computation took " << Milliseconds(t1 - t0)
			<< " (" << Microseconds(t1 - t0) / (mapGeom.dim.x * mapGeom.dim.y) << " per plot)." << std::endl;
	}

	std::abort();
}

#endif