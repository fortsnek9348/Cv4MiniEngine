#pragma once

// ivec2, CITY_PLOT_RADIUS... Not much is used from here
#include "../Util.h"
#include "FoundValuePlayerNearestCityCache.h"

#include <CommonStuff/IntegerDivisonByConstant.h>
#include <CommonStuff/Simd.h>

namespace GiganticMapsOptimisationsLib::FoundValueSystem
{
	/// Vectorised found value computation.
	/// All game data is accessed through a defined interface (including global constant data).

	// 16 is nice, but how about 32?
	// A lot of found value calculation only needs 16-bit integers (32 elements), but a bunch of stuff needs 32-bit too (16 elements).
	// Hopefully, doing 16-bit most of the time with a few conversions here and there is a win over all.
	inline constexpr unsigned int kVectorWidth = 32;

	// Vectorised found value calculation is going to extensively use the SIMD abstraction.

	using I32Vector = heck::simd::Vector<int32_t, kVectorWidth>;
	using U32Vector = heck::simd::Vector<uint32_t, kVectorWidth>;
	// Useful for parts of calculation that don't need more than 16 bits. Only one AVX512 register needed.
	using I16Vector = heck::simd::Vector<int16_t, kVectorWidth>;

	// Internal use.
	using I8Vector = heck::simd::Vector<int8_t, kVectorWidth>;
	using U8Vector = heck::simd::Vector<uint8_t, kVectorWidth>;
	using U16Vector = heck::simd::Vector<uint16_t, kVectorWidth>;
	
	// Used as a data interface return type, but expected to be casted to I16Vector immediately.
	//using I8Vector = heck::simd::Vector<int8_t, kVectorWidth>;

	using I32Mask = I32Vector::Mask;
	using I16Mask = I16Vector::Mask;

	//static_assert(std::same_as<I32Mask, heck::simd::Mask<heck::simd::AvxRegMask<heck::simd::Avx256IntegerReg, 4u>, 4u>>);

	using AreaVector = I32Vector;

	// NOTE: There are 35 bonuses.
	//       76(!) in Realism Invictus. So a 64-bit mask won't be enough in that theoretical case.
	using BonusTypeVector = I16Vector;

	// Bonuses are relabelled for a faster bonus visit check. The most common bonuses will be reindexed to < 16, and then rest will use gather/scatter.
	//using RelabelledBonusTypeVector = BonusTypeVector;

	// Vectorised evaluation calculates the found value for kVectorWidth plots along a row (or maybe we could do 8x4 which would work better for islands).
	// The first plot is aligned to vector width.

	//using U16Vector = heck::simd::Vector<int16_t, kVectorWidth>;
	//using U32Vector = heck::simd::Vector<int32_t, kVectorWidth>;

	// You can get these from <https://www.cse.scu.edu/~dlewis/book3/tools/DivideByConstant.shtml> or compiler explorer.

	// Even with forceinline everywhere, there's no stopping MSVC's optimiser from calling trivial functions. It just really really wants to.

	// aiCityPlotX, aiCityPlotY
	inline constexpr std::array<ivec2, NUM_CITY_PLOTS> kCityPlotDeltaCoords{ {
		{ 0,	 0,	 },
		{ 0,	 1,	 },
		{ 1,	 1,	 },
		{ 1,	 0,	 },
		{ 1,	 -1, },
		{ 0,	 -1, },
		{ -1,	 -1, },
		{ -1,	 0,	 },
		{ -1,	 1,	 },
		{ 0,	 2,	 },
		{ 1,	 2,	 },
		{ 2,	 1,	 },
		{ 2,	 0,	 },
		{ 2,	 -1, },
		{ 1,	 -2, },
		{ 0,	 -2, },
		{ -1,	 -2, },
		{ -2,	 -1, },
		{ -2,	 0,	 },
		{ -2,	 1,	 },
		{ -1,	 2,	 },
	} };
	inline constexpr std::array<int, NUM_CITY_PLOTS> kCityPlotRingNumbers{
		0,
		1, 1, 1, 1, 1, 1, 1, 1,
		2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	};
	inline constexpr int kCityDiameter = 5;

	inline constexpr uint16_t kNearestCityIsCapitalFlag = 0x8000;

	inline I16Vector HECK_VECTORCALL loadByteVector(std::span<const int8_t> map, ivec2 coord, int pitch)
	{
		return static_cast<I16Vector>(I8Vector(map.subspan(coord.x + coord.y * pitch).subspan<0, kVectorWidth>()));
	}

	inline I16Vector HECK_VECTORCALL loadByteVector(std::span<const uint8_t> map, ivec2 coord, int pitch)
	{
		return static_cast<I16Vector>(U8Vector(map.subspan(coord.x + coord.y * pitch).subspan<0, kVectorWidth>()));
	}

	inline I16Vector HECK_VECTORCALL loadI16Vector(std::span<const int16_t> map, ivec2 coord, int pitch)
	{
		return I16Vector(map.subspan(coord.x + coord.y * pitch).subspan<0, kVectorWidth>());
	}

	inline U16Vector HECK_VECTORCALL loadU16Vector(std::span<const uint16_t> map, ivec2 coord, int pitch)
	{
		return U16Vector(map.subspan(coord.x + coord.y * pitch).subspan<0, kVectorWidth>());
	}

	inline I32Vector HECK_VECTORCALL loadI32Vector(std::span<const int32_t> map, ivec2 coord, int pitch)
	{
		return I32Vector(map.subspan(coord.x + coord.y * pitch).subspan<0, kVectorWidth>());
	}

	inline U32Vector HECK_VECTORCALL loadU32Vector(std::span<const uint32_t> map, ivec2 coord, int pitch)
	{
		return U32Vector(map.subspan(coord.x + coord.y * pitch).subspan<0, kVectorWidth>());
	}

	struct FoundValueCalculationCommonMapInterface
	{
		struct CityPlotInfo
		{
			I16Vector flags{};

			I16Mask isCityPlotWater() const { return vtest(flags, kCommonMapFlag_isCityPlotWater); }
			I16Mask isCityPlotRiver() const { return vtest(flags, kCommonMapFlag_isCityPlotRiver); }
			I16Mask getCityPlotHasAnyFeature() const { return vtest(flags, kCommonMapFlag_getCityPlotHasAnyFeature); }
			I16Mask isCityPlotOutOfBounds() const { return vtest(flags, kCommonMapFlag_isCityPlotOutOfBounds); }
			I16Mask isCityPlotOwned() const { return vtest(flags, kCommonMapFlag_isCityPlotOwned); }
			//I32Mask isCityPlotOwnedByMe() const { return static_cast<I32Mask>(vtest(flags, kCommonMapFlag_isCityPlotOwnedByMe)); }
			//I16Mask hasForeignPlayerOwner() const { return vtest(flags, kCommonMapFlag_hasForeignPlayerOwner); }
			I16Mask isCityPlotCityRadius() const { return vtest(flags, kCommonMapFlag_isCityPlotCityRadius); }
			I16Mask getCityPlotFeatureFoodPenaltyCondition() const { return vtest(flags, kCommonMapFlag_getCityPlotFeatureFoodPenaltyCondition); }
			I16Mask isBarbariansCivUnitAvoidance() const { return vtest(flags, kCommonMapFlag_isBarbarianCivUnitAvoidance); }
			I16Mask hasAnyCitiesOnArea() const { return vtest(flags, kCommonMapFlag_hasAnyCitiesOnArea); }
			I16Mask isOutOfBoundsOrImpassable() const { return vtest(flags, kCommonMapFlag_isCityPlotOutOfBounds | kCommonMapFlag_isImpassable); }
			I16Mask isHillsOrFreshWater() const { return vtest(flags, kCommonMapFlag_isHills | kCommonMapFlag_isFreshWater); }

			I16Mask isOceanicCoastalLand() const { return vtest(flags, kCommonMapFlag_isOceanicCoastalLand); }
			I16Mask isHills() const { return vtest(flags, kCommonMapFlag_isHills); }
			I16Mask isFreshWater() const { return vtest(flags, kCommonMapFlag_isFreshWater); }
			I16Mask isAreaNumPlotsAtMostTwo() const { return vtest(flags, kCommonMapFlag_isAreaNumPlotsAtMostTwo); }
			
		};

		using PackedYieldVector = I32Vector;

		// Aligned to kVectorWidth. Extra columns are added that contain the correct data.
		int pitch = 0;

		// All functions are implicitly relative to this coordinate.
		ivec2 addressingCoord{};
		

		// 2 + 4 + 4*2 + 4 + 1 + 4*2 + 2 + 1 = 30 bytes per plot = 19,968,000 bytes per 1040x640 map
		// (expluding wrap padding)
		//std::array<std::span<const int16_t>, kCityDiameter> commonMapFlagRows;
		std::span<const int16_t> flags;
		std::span<const int32_t> areaIdsMap;
		std::array<std::span<const int32_t>, 2> homePlotYieldValuationMaps; // no bonus, with bonus
		std::span<const int32_t> plotYieldMap;
		std::span<const int8_t> featureHealthPercentMap;
		static constexpr int kAssumedMaxNaturalYieldWithImprovement = 20;
		std::array<std::span<const int32_t>, 2> bestNatureYieldWithImprovementMap; // no bonus, with bonus
		std::array<std::span<const int32_t>, 2> bestNatureYieldMap; // no bonus, with bonus
		// For barbarians.
		// If this area has a city on it, then distance to nearest city on area:
		//     plotDistance(iX, iY, pNearestCity->getX_INLINE(), pNearestCity->getY_INLINE()))
		// Otherwise, valuation of distance to nearest city on map:
		//     iDistance = plotDistance(iX, iY, pNearestCity->getX_INLINE(), pNearestCity->getY_INLINE());
		//     std::min(500 * iDistance, (8000 * iDistance) / GC.getMapINLINE().maxPlotDistance());
		std::span<const uint16_t> barbarianNearestCityDistanceValue;

		std::span<const int8_t> plotBonuses;

		//CityPlotInfo commonMapFlagsAtFoundValuePlot = getCityPlotFlags(CITY_HOME_PLOT);

		AreaVector HECK_VECTORCALL getAreaId() const
		{
			return getCityPlotAreaIds(CITY_HOME_PLOT);
		}
		AreaVector HECK_VECTORCALL getCityPlotAreaIds(int cityPlotI) const
		{
			return loadI32Vector(areaIdsMap, addressingCoord + kCityPlotDeltaCoords[cityPlotI], pitch);
		}

		I32Mask isCityPlotSameArea(int cityPlotI) const
		{
			return vcmpeq(getAreaId(), getCityPlotAreaIds(cityPlotI));
		}

		static constexpr uint16_t kCommonMapFlag_isOceanicCoastalLand                   /**/ = 1 << 0; // pPlot->isCoastalLand(GC.getMIN_WATER_SIZE_FOR_OCEAN());
		static constexpr uint16_t kCommonMapFlag_isHills                                /**/ = 1 << 1; // if (pPlot->isHills())
		static constexpr uint16_t kCommonMapFlag_isCityPlotWater                        /**/ = 1 << 2;
		static constexpr uint16_t kCommonMapFlag_isCityPlotRiver                        /**/ = 1 << 3;
		static constexpr uint16_t kCommonMapFlag_isFreshWater                           /**/ = 1 << 4; // if (pPlot->isFreshWater())
		static constexpr uint16_t kCommonMapFlag_getCityPlotHasAnyFeature               /**/ = 1 << 5;
		static constexpr uint16_t kCommonMapFlag_isCityPlotOutOfBounds                  /**/ = 1 << 6;
		static constexpr uint16_t kCommonMapFlag_isAreaNumPlotsAtMostTwo                /**/ = 1 << 7;
		static constexpr uint16_t kCommonMapFlag_isCityPlotOwned                        /**/ = 1 << 8; // pLoopPlot->isOwned
		static constexpr uint16_t kCommonMapFlag_isCityPlotCityRadius                   /**/ = 1 << 9; // In any existing city's radius.
		static constexpr uint16_t kCommonMapFlag_getCityPlotFeatureFoodPenaltyCondition /**/ = 1 << 10; // if (eFeature != NO_FEATURE && GC.getFeatureInfo(eFeature).getYieldChange(YIELD_FOOD) < 0)
		static constexpr uint16_t kCommonMapFlag_isBarbarianCivUnitAvoidance            /**/ = 1 << 11;
		static constexpr uint16_t kCommonMapFlag_hasAnyCitiesOnArea                     /**/ = 1 << 12;
		static constexpr uint16_t kCommonMapFlag_InternalHasCivUnit                     /**/ = 1 << 13;
		// Bad tile counting
		static constexpr uint16_t kCommonMapFlag_isImpassable                           /**/ = 1 << 14;

		CityPlotInfo HECK_VECTORCALL getCityPlotFlagsAtRelCoord(ivec2 relCoord) const
		{
			// Must not go beyond padding.
			assert(std::abs(relCoord.x) <= CITY_PLOTS_RADIUS && std::abs(relCoord.y) <= CITY_PLOTS_RADIUS);
			return {
				loadI16Vector(flags, relCoord + addressingCoord, pitch),
			};
		}

		CityPlotInfo HECK_VECTORCALL getCityPlotFlags(int cityPlotI) const
		{
			return {
				loadI16Vector(flags, kCityPlotDeltaCoords[cityPlotI] + addressingCoord, pitch),
			};
		}

		//I16Mask isOceanicCoastalLand() const { return vtest(commonMapFlagsAtFoundValuePlot.flags, kCommonMapFlag_isOceanicCoastalLand); }
		//I16Mask isHills() const { return vtest(commonMapFlagsAtFoundValuePlot.flags, kCommonMapFlag_isHills); }
		//I16Mask isFreshWater() const { return vtest(commonMapFlagsAtFoundValuePlot.flags, kCommonMapFlag_isFreshWater); }
		//I16Mask isAreaNumPlotsAtMostTwo() const { return vtest(commonMapFlagsAtFoundValuePlot.flags, kCommonMapFlag_isAreaNumPlotsAtMostTwo); }
		
		

		I16Vector HECK_VECTORCALL loadByteVector(std::span<const int8_t> map, ivec2 coord) const
		{
			return FoundValueSystem::loadByteVector(map, coord, pitch);
		}
		
		I16Vector HECK_VECTORCALL loadByteVector(std::span<const int8_t> map, int cityPlotI) const
		{
			return loadByteVector(map, kCityPlotDeltaCoords[cityPlotI] + addressingCoord);
		}

		static std::array<I16Vector, NUM_YIELD_TYPES> HECK_VECTORCALL unpackYieldVector(PackedYieldVector yields)
		{
			//     3    2    1    0
			// [---- COMM PROD FOOD]
			return {
				static_cast<I16Vector>(static_cast<I8Vector>(yields)),
				static_cast<I16Vector>(static_cast<I8Vector>(yields >> heck::simd::imm<8>)),
				static_cast<I16Vector>(static_cast<I8Vector>(yields >> heck::simd::imm<16>)),
			};
		}

		std::array<I16Vector, NUM_YIELD_TYPES> HECK_VECTORCALL vselectYields(I16Mask maskWithBonus, const std::array<std::span<const int32_t>, 2>& map, int cityPlotI) const
		{
			const PackedYieldVector withNoBonus = loadI32Vector(map[0], addressingCoord + kCityPlotDeltaCoords[cityPlotI], pitch);
			const PackedYieldVector withBonus = loadI32Vector(map[1], addressingCoord + kCityPlotDeltaCoords[cityPlotI], pitch);
			[[maybe_unused]] const auto bitsA = maskWithBonus.asBitset();
			[[maybe_unused]] const auto bitsB = static_cast<I32Mask>(maskWithBonus).asBitset();
			assert(bitsA == bitsB);
			return unpackYieldVector(vselect(static_cast<I32Mask>(maskWithBonus), withBonus, withNoBonus));
		}

		std::array<I16Vector, NUM_YIELD_TYPES> HECK_VECTORCALL getHomePlotValuationYield(I16Mask withBonus) const
		{ 
			return vselectYields(withBonus, homePlotYieldValuationMaps, CITY_HOME_PLOT);
		}
		std::array<I16Vector, NUM_YIELD_TYPES> HECK_VECTORCALL getCityRadiusPlotValuationYield(int cityPlotI) const
		{
			//return vselectYields(withBonus, cityRadiusPlotYieldValuationMaps, cityPlotI);
			return unpackYieldVector(loadI32Vector(plotYieldMap, addressingCoord + kCityPlotDeltaCoords[cityPlotI], pitch));
		}

		

		I16Vector HECK_VECTORCALL getCityPlotFeatureHealthPercent(int cityPlotI) const
		{
			return loadByteVector(featureHealthPercentMap, cityPlotI);
		}

		


		// pLoopPlot->calculateBestNatureYield(YIELD_FOOD, player.getTeam()) + GC.getImprovementInfo(eBonusImprovement).getImprovementBonusYield(eBonus, YIELD_FOOD);
		// Ensure these are no more than 20!
		
		std::array<I16Vector, NUM_YIELD_TYPES> HECK_VECTORCALL getCityPlotBestNatureYieldWithImprovement(int cityPlotI, I16Mask withBonus) const
		{
			return vselectYields(withBonus, bestNatureYieldWithImprovementMap, cityPlotI);
		}

		std::array<I16Vector, NUM_YIELD_TYPES> HECK_VECTORCALL getCityPlotBestNatureYield(int cityPlotI, I16Mask withBonus) const
		{
			return vselectYields(withBonus, bestNatureYieldMap, cityPlotI);
		}

		// For barbarians.
		I16Vector HECK_VECTORCALL getBarbarianNearestCityDistanceValue() const
		{
			const U16Vector v = loadU16Vector(barbarianNearestCityDistanceValue, addressingCoord, pitch);
			// All values should be set if this is called.
			assert(vcmpeq(v, PlayerNearestCityCache::kNoCities).none());
			return static_cast<I16Vector>(v);
		}

		BonusTypeVector HECK_VECTORCALL getPlotBonuses(int cityPlotI) const
		{
			return static_cast<I16Vector>(loadByteVector(plotBonuses, cityPlotI));
		}

		static bool isCityPlotInFirstRing(int cityPlotI) { return kCityPlotRingNumbers[cityPlotI] == 1; }
		static bool isCityPlotBeyondFirstRing(int cityPlotI) { return kCityPlotRingNumbers[cityPlotI] > 1; }

		
	};

	//template<class T> using ptr = T*;
	//
	//using X = ptr<int>;
	//using Y = const X;
	//static_assert(std::same_as<const ptr<int>, Y>);

	struct FoundValueCalculationPlayerMapInterface
	{
		// Per-player data / no per-plot map data

		ivec2 addressingCoord{};
		PlayerTypes playerI = NO_PLAYER;
		int pitch = 0;

		// 1 + 1 + 2 + 1 = 5 bytes per plot =  3,328,000 bytes per 1040x640 map per player (59,904,000 bytes for 18 players)
		// (expluding wrap padding)

		static constexpr uint8_t kPlayerMapFlag_canFound                          /**/ = 1 << 0;
		static constexpr uint8_t kPlayerMapFlag_isPlayerAIPlotCitySite            /**/ = 1 << 1; // player.AI_isPlotCitySite(pPlot)
		static constexpr uint8_t kPlayerMapFlag_isCityPlotWithinPlayerAICitySites /**/ = 1 << 2; // data.plotDistance(*pLoopPlot, pCitySitePlot.x) <= CITY_PLOTS_RADIUS
		static constexpr uint8_t kPlayerMapFlag_hasForeignOwnerOrOutOfBounds      /**/ = 1 << 3; // if (!pLoopPlot || pLoopPlot->isOwned() && pLoopPlot->getTeam() != player.getTeam())
		static constexpr uint8_t kPlayerMapFlag_hasMyCitiesOnArea                 /**/ = 1 << 4; // pLoopPlot->area()->getCitiesPerPlayer(player.getID()) != 0
		static constexpr uint8_t kPlayerMapFlag_isOwnedByThisPlayer               /**/ = 1 << 5;
		static constexpr uint8_t kPlayerMapFlag_hasRevealedBonus                  /**/ = 1 << 6; // 1 iff plot has a bonus and it's revealed.
		static constexpr uint8_t kPlayerMapFlag_isOwnedByOurTeam                  /**/ = 1 << 7; // if (pLoopPlot->isOwned() && pLoopPlot->getTeam() == player.getTeam())
		std::span<const uint8_t> flags;
		// These are needed for each city plot.
		// High bit is revealed bit.
		std::span<const uint8_t> revealedAndOurCultureMultipliers;
		// High bit: kNearestCityIsCapitalFlag
		std::span<const uint16_t> nearestCityDistanceValues;
		
		// 6 bits: cityRadiusDeadlockedBonusesCount (max: 81-21: 60)
		static_assert(NUM_CITY_PLOTS == 21);
		// 1 bit: teamAreaCityCountCaseBit0
		// 1 bit: teamAreaCityCountCaseBit1
		static constexpr int kStage2DataShift_cityRadiusDeadlockedBonusesCount = 0;
		static constexpr int kStage2DataShift_teamAreaCityCountCaseBit = 6;
		static constexpr uint8_t kStage2DataMask_cityRadiusDeadlockedBonusesCount /**/ = ((1 << 6) - 1) << kStage2DataShift_cityRadiusDeadlockedBonusesCount;
		static constexpr uint8_t kStage2DataMask_teamAreaCityCountCaseBit0        /**/ = 1 << kStage2DataShift_teamAreaCityCountCaseBit;
		static constexpr uint8_t kStage2DataMask_teamAreaCityCountCaseBit1        /**/ = 1 << (kStage2DataShift_teamAreaCityCountCaseBit + 1);
		std::span<const uint8_t> stage2Data;

		
		struct PlayerPlotInfo
		{
			I16Vector flags{};

			I16Mask canFound() const { return vtest(flags, kPlayerMapFlag_canFound); }
			I16Mask isPlayerAIPlotCitySite() const { return vtest(flags, kPlayerMapFlag_isPlayerAIPlotCitySite); }
			I16Mask isCityPlotWithinPlayerAICitySites() const { return vtest(flags, kPlayerMapFlag_isCityPlotWithinPlayerAICitySites); }
			I16Mask hasForeignOwnerOrOutOfBounds() const { return vtest(flags, kPlayerMapFlag_hasForeignOwnerOrOutOfBounds); }
			//I32Mask isMyNearestCityACapital() const { return static_cast<I32Mask>(vtest(flags, kPlayerMapFlag_isMyNearestCityACapital)); }
			I16Mask hasMyCitiesOnArea() const { return vtest(flags, kPlayerMapFlag_hasMyCitiesOnArea); }
			I16Mask isOwnedByThisPlayer() const { return vtest(flags, kPlayerMapFlag_isOwnedByThisPlayer); }
			I16Mask isOwnedByOurTeam() const { return vtest(flags, kPlayerMapFlag_isOwnedByOurTeam); }
			I16Mask hasRevealedBonus() const { return vtest(flags, kPlayerMapFlag_hasRevealedBonus); }
			
		};

		struct StageTwoData
		{
			I32Vector v;

			//I32Vector getNearestCityDistanceValue() const
			//{
			//	return static_cast<I32Vector>(v & ((1 << 14) - 1));
			//}

			//I32Vector HECK_VECTORCALL  getCityRadiusRevealedBonusCount() const
			//{
			//	return static_cast<I32Vector>(v & kStage2BonusDataMask_cityRadiusRevealedBonusCount);
			//}
			//
			//I32Vector HECK_VECTORCALL  getCityRadiusUniqueRevealedBonusCount() const
			//{
			//	return static_cast<I32Vector>((v & kStage2BonusDataMask_cityRadiusUniqueRevealedBonusCount) >> heck::simd::imm<kStage2BonusDataShift_cityRadiusUniqueRevealedBonusCount>);
			//}

			I32Vector HECK_VECTORCALL getCityRadiusDeadlockedBonusesCount() const
			{
				return v & kStage2DataMask_cityRadiusDeadlockedBonusesCount;
			}

			I32Mask getTeamAreaCityCountCaseBit0() const
			{
				return vtest(v, kStage2DataMask_teamAreaCityCountCaseBit0);
			}
			
			I32Mask getTeamAreaCityCountCaseBit1() const
			{
				return vtest(v, kStage2DataMask_teamAreaCityCountCaseBit1);
			}
		};
		

		PlayerPlotInfo HECK_VECTORCALL getPlayerPlotInfo(int cityPlotI) const
		{
			return { static_cast<I16Vector>(loadByteVector(flags, addressingCoord + kCityPlotDeltaCoords[cityPlotI], pitch)) };
		}

		

		I16Vector HECK_VECTORCALL getOurCultureMultiplierOfCityPlot(int cityPlotI) const // depends on our culture and foreign culture, and iClaimThreshold
		{
			return loadByteVector(revealedAndOurCultureMultipliers, addressingCoord + kCityPlotDeltaCoords[cityPlotI], pitch) & 0x7F;
		}

		// Not used by found value computation. Used by AI found value updates.
		I16Mask HECK_VECTORCALL getRevealedMask(int cityPlotI) const
		{
			return vtest(loadByteVector(revealedAndOurCultureMultipliers, addressingCoord + kCityPlotDeltaCoords[cityPlotI], pitch), 0x80);
		}

		// Include div by 2 at the end.
		// Also has minor global dependency on revealed bonuses.
		// And also, this can be local per-plot because it depends on found plot isCoastalLand too.
		
		I16Vector HECK_VECTORCALL getNearestCityDistanceValue() const
		{
			return static_cast<I16Vector>(loadU16Vector(nearestCityDistanceValues, addressingCoord, pitch) & static_cast<uint16_t>(~kNearestCityIsCapitalFlag));
		}

		I16Mask HECK_VECTORCALL getNearestCityIsMyCapital() const
		{
			return vtest(loadU16Vector(nearestCityDistanceValues, addressingCoord, pitch), kNearestCityIsCapitalFlag);
		}

		StageTwoData HECK_VECTORCALL getStage2Data() const
		{
			return { static_cast<I32Vector>(loadByteVector(stage2Data, addressingCoord, pitch)) };
		}
	};

	struct BonusFlags
	{
		// Using the whole register as a large bitmap. Bit-indexed by BonusType.
		// AVX is enough for 16 (max 256 bonuses)
		heck::simd::Vector<uint16_t, 16> fullRegisterBitmap;

		void set(size_t i, bool value)
		{
			const uint16_t mask = 1 << (i % 16);
			fullRegisterBitmap.setElement(i / 16, (fullRegisterBitmap.getElement(i / 16) & ~mask) | (value ? mask : 0));
		}

		I16Mask HECK_VECTORCALL get(BonusTypeVector bonusI) const
		{
			// Permute words, then shift by bit index within words.
			const U16Vector masks = fullRegisterBitmap.permute(bonusI >> heck::simd::imm<4>);
			return vtest(masks, static_cast<U16Vector>(1) << static_cast<U16Vector>(bonusI & (16 - 1)));
		}
	};

	struct FoundValueCalculationPlayerGlobalDataInterface
	{
		bool isBarbarian = false;
		bool playerHasCapital = false; // else if (player.getCapitalCity() != nullptr)
		bool weHaveCityDistances = false;
		int playerNumCities = 0;
		int iGreed = 0;
		int iMaxDistanceFromCapital = 0; // iMaxDistanceFromCapital = std::max(iMaxDistanceFromCapital, plotDistance(iCapitalX, iCapitalY, pLoopCity->getX_INLINE(), pLoopCity->getY_INLINE()));
		

		// Per-player
		// if ((player.getNumTradeableBonuses(eBonus) == 0) || (player.AI_bonusVal(eBonus) > 10) || (GC.getBonusInfo(eBonus).getYieldChange(YIELD_FOOD) > 0))
		BonusFlags playerGoodBonusConditionFlags;

		// Allocate extra for out-of-bounds gather.
		// iTempValue = (player.AI_bonusVal(eBonus) * (((player.getNumTradeableBonuses(eBonus) == 0) && (paiBonusCount[eBonus] == 1)) ? 80 : 20));
		std::span<const int16_t, 128 + 2> playerBonusValuationFirstSeen;
		std::span<const int16_t, 128 + 2> playerBonusValuationExtra;

		I16Vector HECK_VECTORCALL getPlayerBonusValuation(BonusTypeVector bonusI, I16Mask isFirstBonusLookedAt) const
		{
			bonusI = vmax(0, bonusI);
			I16Vector valuationFirstSeen{};
			I16Vector valuationExtras{};
			if (vcmplt(bonusI, kVectorWidth).all())
			{
				// Fast path, no gathers.
				valuationFirstSeen = I16Vector(playerBonusValuationFirstSeen.subspan<0, kVectorWidth>()).permute(bonusI);
				valuationExtras = I16Vector(playerBonusValuationExtra.subspan<0, kVectorWidth>()).permute(bonusI);
			}
			else
			{
				const I32Vector bonus32 = static_cast<I32Vector>(bonusI);
				// 4 gathers here...
				valuationFirstSeen = static_cast<I16Vector>(I32Vector(std::as_bytes(playerBonusValuationFirstSeen), bonus32, heck::simd::imm<2>));
				valuationExtras = static_cast<I16Vector>(I32Vector(std::as_bytes(playerBonusValuationExtra), bonus32, heck::simd::imm<2>));
			}
			return vselect(isFirstBonusLookedAt, valuationFirstSeen, valuationExtras);
		}
	};

	struct FoundValueCalculationCommonGlobalDataInterface
	{
		BonusFlags bonusHasImprovement{};

		I16Mask HECK_VECTORCALL hasBonusImprovement(BonusTypeVector bonusI) const
		{
			return bonusHasImprovement.get(bonusI);
		}

		int FOOD_CONSUMPTION_PER_POPULATION = 0;
		int FRESH_WATER_HEALTH_CHANGE = 0;

		heck::IntegerDivisionByConstantParams maxPlotDistanceDivisor;
		int maxPlotDistance = 0;

		I32Vector HECK_VECTORCALL divByMaxPlotDistance(I32Vector x) const
		{
			// Note that the add could overflow in theory, but we assume `x` is not at the limit of integers.
			// TODO: Should probably update IntegerDivisionByConstantParams to handle signed...
			const U32Vector mulhi = vmulhi(static_cast<U32Vector>(x + maxPlotDistanceDivisor.add), maxPlotDistanceDivisor.M);
			const U32Vector result = mulhi >> maxPlotDistanceDivisor.shift;
			//for (const int i : range(kVectorWidth))
			//{
			//	//assert(mulhi.getElement(i) == ((int64_t(x.getElement(i)) + maxPlotDistanceDivisor.add) * maxPlotDistanceDivisor.M) >> 32);
			//	assert(result.getElement(i) == x.getElement(i) / maxPlotDistance);
			//}
			return static_cast<I32Vector>(result);
		}
	};

	

	I32Vector HECK_VECTORCALL computeFoundValue_Vectorised(
		const FoundValueCalculationCommonGlobalDataInterface commonGlobals,
		const FoundValueCalculationPlayerGlobalDataInterface playerGlobals,
		const FoundValueCalculationCommonMapInterface commonLocals,
		const FoundValueCalculationPlayerMapInterface playerLocals,
		int iMinRivalRange,
		I16Mask activeMask,
		std::string* logOutput = nullptr
	);

	int computeFoundValue_Logging(PlayerTypes playerI, ivec2 coord, int minRivalRange, std::ostream& os);
}