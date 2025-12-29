#include "../../CommonConfig.h"

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS

#include "FoundValueVectorisedCalculation.h"

#include "../Util.h"

#include <iostream>

using namespace GiganticMapsOptimisationsLib;
using namespace GiganticMapsOptimisationsLib::FoundValueSystem;

#define HECK_FOUNDVALUE_LOG(x) do { if constexpr (kEnableLogOutput) *logOutput << __LINE__ << ": " << x << '\n'; } while(0)

// Turn off to avoid compiling a second version of the function.
static constexpr bool kEnableLoggingAbility = false;

namespace
{

	//I16Vector HECK_VECTORCALL divSignedBy2(I16Vector a)
	//{
	//	return (a - (a >> heck::simd::imm<15>)) >> heck::simd::imm<1>;
	//}

	I32Vector HECK_VECTORCALL divSignedBy2(I32Vector a)
	{
		return (a - (a >> heck::simd::imm<31>)) >> heck::simd::imm<1>;
		//return a / heck::simd::imm<2>;
	}

	I32Vector HECK_VECTORCALL divSignedBy3(I32Vector a)
	{
		return vmulhi(a, 0x55555556) - (a >> heck::simd::imm<31>);
	}

	I16Vector HECK_VECTORCALL divSignedBy5(I16Vector a)
	{
		return (vmulhi(a, 0x6667) >> heck::simd::imm<1>) - (a >> heck::simd::imm<15>);
	}

	I32Vector HECK_VECTORCALL divSignedBy100(I32Vector a)
	{
		return (vmulhi(a, 0x51EB851F) >> heck::simd::imm<5>) - (a >> heck::simd::imm<31>);
	}

	// Used to handle bits in a bonuses bit vector, but you have kVectorWidth bit vectors.
	// Assumes that most accesses will be at bit < 32, which should always happen in BTS.
	struct VectorisedBitset
	{
		using U32Vector = heck::simd::Vector<uint32_t, kVectorWidth>;

		// [bitI / 32][elementI].bit[bitI % 32]
		std::vector<uint32_t> words;

		explicit VectorisedBitset(size_t numBitsPerElement) : words(heck::cdiv(numBitsPerElement, 32u) * kVectorWidth)
		{
		}

		void clear()
		{
			std::ranges::fill(words, 0);
		}

		I32Mask HECK_VECTORCALL testAndSet(I16Vector bitIndices, I16Mask accessMask)
		{
			const I16Vector wordIndices = bitIndices >> heck::simd::imm<5>;
				
			if ((vcmpeq(wordIndices, 0) | ~accessMask).all())
			{
				// All word accesses within the first vector. We can avoid the gather/scatter.
				U32Vector accessedWords{ std::span(words).subspan<0, kVectorWidth>() };
					
				const I32Mask result = testAndSetVector(accessedWords, bitIndices, accessMask);

				std::ranges::copy(accessedWords.toArray(), words.begin());

				return result;
			}
			else
			{
				// Need to gather/scatter.
				U32Vector accessedWords(words, static_cast<I32Vector>(wordIndices));

				const I32Mask result = testAndSetVector(accessedWords, bitIndices, accessMask);

				scatter(std::span(words), static_cast<I32Vector>(wordIndices), static_cast<U32Vector>(accessedWords), static_cast<I32Mask>(accessMask));

				return result;
			}
		}

	private:
		static I32Mask HECK_VECTORCALL testAndSetVector(U32Vector& accessedWords, I16Vector bitIndices, I16Mask accessMask)
		{
			const U32Vector mask = static_cast<U32Vector>(vselect(accessMask, I16Vector(1), 0)) << static_cast<U32Vector>(bitIndices & (32 - 1));
			const I32Mask result = vtest(accessedWords, mask);
			accessedWords |= mask;
			return result;
		}
	};

	struct VectorisedFoundValueCalculationLogger
	{
		std::ostringstream os;

		using This = VectorisedFoundValueCalculationLogger;

		This& operator<<(char s)
		{
			os << s;
			return *this;
		}

		This& operator<<(int s)
		{
			os << s;
			return *this;
		}

		This& operator<<(const char* s)
		{
			os << s;
			return *this;
		}

		This& operator<<(ivec2 v)
		{
			os << v;
			return *this;
		}

		template<class T, size_t k>
		This& operator<<(const std::array<T, k>& a)
		{
			os << '[';
			for (size_t i = 0; i < k; ++i)
			{
				if (i)
					os << ", ";
				os << i << ':' << a[i];
			}
			os << ']';
			return *this;
		}

		template<size_t k>
		This& operator<<(const std::bitset<k>& a)
		{
			os << '[';
			for (size_t i = 0; i < k; ++i)
			{
				if (i)
					os << ", ";
				os << i << ':' << a[i];
			}
			os << ']';
			return *this;
		}

		template<class T, size_t k>
		This& operator<<(const heck::simd::Vector<T, k>& v)
		{
			*this << v.toArray();
			return *this;
		}

		template<class SingleMask, size_t k>
		This& operator<<(const heck::simd::GenericMask<SingleMask, k>& v)
		{
			*this << v.asBitset();
			return *this;
		}
	};

	template<bool kEnableLogOutput>
	I32Vector HECK_VECTORCALL _computeFoundValue_Vectorised(
		const FoundValueCalculationCommonGlobalDataInterface commonGlobals,
		const FoundValueCalculationPlayerGlobalDataInterface playerGlobals,
		const FoundValueCalculationCommonMapInterface commonLocals,
		const FoundValueCalculationPlayerMapInterface playerLocals,
		int iMinRivalRange,
		I16Mask inputActiveMask,
		VectorisedFoundValueCalculationLogger* logOutput
	)
	{
		//static constexpr bool bStartingLoc = false;
		//static constexpr bool bAdvancedStart = false;

		//const CvPlot* const pPlot = GC.getMapINLINE().plotINLINE(iX, iY);

		HECK_FOUNDVALUE_LOG("Player " << (int)playerLocals.playerI);

		const FoundValueCalculationCommonMapInterface::CityPlotInfo homePlotCommonInfo = commonLocals.getCityPlotFlags(CITY_HOME_PLOT);
		const FoundValueCalculationPlayerMapInterface::PlayerPlotInfo homePlotPlayerInfo = playerLocals.getPlayerPlotInfo(CITY_HOME_PLOT);

		I16Mask active = inputActiveMask & homePlotPlayerInfo.canFound();
		HECK_FOUNDVALUE_LOG("active = " << active);
		if (active.none())
		{
			HECK_FOUNDVALUE_LOG("EXIT active.none()");
			return {};
		}

		const I16Mask bIsCoastal = homePlotCommonInfo.isOceanicCoastalLand();
		const AreaVector pArea = commonLocals.getAreaId();
		const I16Mask hasMyCitiesOnThisArea = homePlotPlayerInfo.hasMyCitiesOnArea();

		HECK_FOUNDVALUE_LOG("bIsCoastal = " << bIsCoastal);
		HECK_FOUNDVALUE_LOG("hasMyCitiesOnThisArea = " << hasMyCitiesOnThisArea);

		active &= ~(~bIsCoastal & ~hasMyCitiesOnThisArea);
		HECK_FOUNDVALUE_LOG("bIsCoastal/hasMyCitiesOnThisArea active = " << active);
		if (active.none())
		{
			HECK_FOUNDVALUE_LOG("EXIT active.none()");
			return 0;
		}

		

		std::array<FoundValueCalculationCommonMapInterface::CityPlotInfo, NUM_CITY_PLOTS> commonCityPlotInfos{};
		for (int i = 0; i < NUM_CITY_PLOTS; ++i)
			commonCityPlotInfos[i] = i == CITY_HOME_PLOT ? homePlotCommonInfo : commonLocals.getCityPlotFlags(i);

		if (iMinRivalRange >= 0)
		{
			//active &= ~homePlotCommonInfo.isBarbariansCivUnitAvoidance();

			// NOTE: iMinRivalRange is the same as city plot range, which is same as the padding around the cached plot prop map.
			assert(iMinRivalRange <= CITY_PLOTS_RADIUS);
			// NOTE: From a quick test, doing this isn't much slower than testing a single flag as above. Within variance.
			// Check check radius first, as these values need to be loaded anyway.
			for (int iI = 0; iI < NUM_CITY_PLOTS; iI++)
				active &= ~commonCityPlotInfos[iI].isBarbariansCivUnitAvoidance();
			// Then check outside city radius. These plot props are only needed here.
			for (int iDX = -(iMinRivalRange); iDX <= iMinRivalRange; iDX++)
			{
				for (int iDY = -(iMinRivalRange); iDY <= iMinRivalRange; iDY++)
				{
					if (getUnwrappedPlotDistance({}, { iDX, iDY }) > CITY_PLOTS_RADIUS)
						active &= ~commonLocals.getCityPlotFlagsAtRelCoord({ iDX, iDY }).isBarbariansCivUnitAvoidance();
				}
			}

			HECK_FOUNDVALUE_LOG("isBarbariansCivUnitAvoidance active = " << active);
			if (active.none())
			{
				HECK_FOUNDVALUE_LOG("EXIT active.none()");
				return 0;
			}
		}

		std::array<FoundValueCalculationPlayerMapInterface::PlayerPlotInfo, NUM_CITY_PLOTS> playerCityPlotInfos{};
		for (int i = 0; i < NUM_CITY_PLOTS; ++i)
			playerCityPlotInfos[i] = i == CITY_HOME_PLOT ? homePlotPlayerInfo : playerLocals.getPlayerPlotInfo(i);

		std::array<I16Mask, NUM_CITY_PLOTS> abCitySiteRadius{};

		{
			const I16Mask isNotPlayerAIPlotCitySite = ~homePlotPlayerInfo.isPlayerAIPlotCitySite();
			for (int iI = 0; iI < NUM_CITY_PLOTS; iI++)
			{
				abCitySiteRadius[iI] = playerCityPlotInfos[iI].isCityPlotWithinPlayerAICitySites() & isNotPlayerAIPlotCitySite;
				if (abCitySiteRadius[iI].any())
					HECK_FOUNDVALUE_LOG("abCitySiteRadius " << iI << " = " << abCitySiteRadius[iI]);
			}
		}

		I16Vector iOwnedTiles{};

		for (int iI = 0; iI < NUM_CITY_PLOTS; iI++)
		{
			const I16Mask foreignOrOutOfBounds = playerCityPlotInfos[iI].hasForeignOwnerOrOutOfBounds();
			iOwnedTiles = vmaskedadd(foreignOrOutOfBounds, iOwnedTiles, 1);
		}

		HECK_FOUNDVALUE_LOG("iOwnedTiles = " << iOwnedTiles);

		active &= vcmple(iOwnedTiles, NUM_CITY_PLOTS / 3);
		HECK_FOUNDVALUE_LOG("iOwnedTiles active = " << active);
		if (active.none())
		{
			HECK_FOUNDVALUE_LOG("EXIT iOwnedTiles");
			return 0;
		}

		

		I16Vector iBadTile{};

		// TODO: Could cache iBadTile instead, but would it be worth the complexity?
		//       Or, evaluate with byte vectors to do 64 at a time.
		//const I16Vector iBadTile = playerLocals.getTotalBadTileCount();
		for (const int iI : range(NUM_CITY_PLOTS))
		{
			if (iI == CITY_HOME_PLOT)
				continue;

			const auto commonPlotInfo = commonCityPlotInfos[iI];
			const auto playerPlotInfo = playerCityPlotInfos[iI];
			const I16Mask cnd0 = commonPlotInfo.isOutOfBoundsOrImpassable();
			const I16Mask cnd1 = ~commonPlotInfo.isHillsOrFreshWater();
			const std::array<I16Vector, NUM_YIELD_TYPES> yields = commonLocals.getCityPlotBestNatureYield(iI, playerPlotInfo.hasRevealedBonus());
			const I16Mask cnd2 = vcmpeq(yields[YIELD_FOOD], 0) | vcmple(yields[YIELD_FOOD] + yields[YIELD_PRODUCTION] + yields[YIELD_COMMERCE], 1);
			const I16Mask cnd3 = ~bIsCoastal & commonPlotInfo.isCityPlotWater() & vcmple(yields[YIELD_FOOD], 1);
			const I16Mask cnd4 = playerPlotInfo.isOwnedByOurTeam() & (commonPlotInfo.isCityPlotCityRadius() | playerPlotInfo.isCityPlotWithinPlayerAICitySites());
			iBadTile = vmaskedadd(cnd0 | (cnd1 & cnd2), iBadTile, 2);
			iBadTile = vmaskedadd(~cnd0 & ((cnd1 & ~cnd2 & cnd3) | (~cnd1 & cnd4)), iBadTile, 1);
		}

		HECK_FOUNDVALUE_LOG("iBadTile = " << iBadTile << " / 2");

		iBadTile >>= heck::simd::imm<1>;

		if (const I16Mask goodBonusLoopMask = active & (vcmpgt(iBadTile, NUM_CITY_PLOTS / 2) | homePlotCommonInfo.isAreaNumPlotsAtMostTwo()); goodBonusLoopMask.any())
		{
			I16Mask bHasGoodBonus{};

			for (int iI = 0; iI < NUM_CITY_PLOTS; iI++)
			{
				const I16Mask hasRevealedBonus = playerCityPlotInfos[iI].hasRevealedBonus();
				const BonusTypeVector eBonus = vselect(hasRevealedBonus, commonLocals.getPlotBonuses(iI), -1);

				const I16Mask hasBonusToGrab = active & hasRevealedBonus & ~commonCityPlotInfos[iI].isCityPlotOwned();
				const I16Mask isOnSameAreaOrWater = static_cast<I16Mask>(vcmpeq(commonLocals.getCityPlotAreaIds(iI), pArea)) | commonCityPlotInfos[iI].isCityPlotWater();
				// Desired mask: Only need values of `hasMyCitiesOnArea` that can change the value of the expression.
				const I16Mask goodBonusMaskAtCityPlot = playerCityPlotInfos[iI].hasMyCitiesOnArea();

				const I16Mask cityPlotGoodBonusMask = (hasBonusToGrab & (isOnSameAreaOrWater | goodBonusMaskAtCityPlot)) & playerGlobals.playerGoodBonusConditionFlags.get(eBonus);

				bHasGoodBonus |= cityPlotGoodBonusMask;
				HECK_FOUNDVALUE_LOG("bHasGoodBonus at " << iI << " = " << bHasGoodBonus);
			}

			const I16Mask goodBonusActiveMask = ~goodBonusLoopMask | bHasGoodBonus;
			active &= goodBonusActiveMask;
			HECK_FOUNDVALUE_LOG("goodBonusActiveMask = " << goodBonusActiveMask << ", active = " << active);
			if (active.none())
			{
				HECK_FOUNDVALUE_LOG("EXIT goodBonusActiveMask");
				return 0;
			}
		}

		I16Vector iTakenTiles{};
		I16Vector iTeammateTakenTiles{};
		I16Vector iHealth{};
		I32Vector iValue = 1000;


		I16Vector iResourceValue16{};
		I16Vector iSpecialFood{};
		I16Vector iSpecialFoodPlus{};
		I16Vector iSpecialFoodMinus{};
		I16Vector iSpecialProduction{};;
		I16Vector iSpecialCommerce{};
		I16Mask bNeutralTerritory = I16Mask::kAll;

		HECK_FOUNDVALUE_LOG("iGreed = " << playerGlobals.iGreed);

		// A bit for each bonus for each SIMD element. This will cover all plot bonuses in BTS.
		thread_local VectorisedBitset tlsBonusSeen(GC.getNumBonusInfos());
		tlsBonusSeen.clear();

		I16Vector bonusCount{};
		I16Vector uniqueBonusCount{};

		for (int iI = 0; iI < NUM_CITY_PLOTS; iI++)
		{
			HECK_FOUNDVALUE_LOG("=== Yield valuation loop [" << iI << "]");

			const I16Mask case0 = commonCityPlotInfos[iI].isCityPlotOutOfBounds();
			const I16Mask case1 = ~case0 & (commonCityPlotInfos[iI].isCityPlotCityRadius() | abCitySiteRadius[iI]);
			const I16Mask case2 = active & ~case0 & ~case1;

			iTakenTiles = vmaskedadd(case0 | case1, iTakenTiles, 1);
			iTeammateTakenTiles = vmaskedadd(case1 & abCitySiteRadius[iI], iTeammateTakenTiles, 1);

			HECK_FOUNDVALUE_LOG("case0 = " << case0);
			HECK_FOUNDVALUE_LOG("case1 = " << case1);
			HECK_FOUNDVALUE_LOG("case2 = " << case2);
			HECK_FOUNDVALUE_LOG("isCityPlotCityRadius = " << commonCityPlotInfos[iI].isCityPlotCityRadius());
			HECK_FOUNDVALUE_LOG("iTakenTiles = " << iTakenTiles);
			HECK_FOUNDVALUE_LOG("iTeammateTakenTiles = " << iTeammateTakenTiles);

			if (case2.none()) // On the off chance... probably out of bounds.
				continue;

			// Remember that changes to variables outside this loop where `active` is true must be conditional on `case2`!

			const I16Mask hasRevealedBonus = active & playerCityPlotInfos[iI].hasRevealedBonus();
			const BonusTypeVector eBonus = vselect(hasRevealedBonus, commonLocals.getPlotBonuses(iI), -1);

			HECK_FOUNDVALUE_LOG("eBonus = " << eBonus);

			// Wrapping bug in PaddedArray2D...
			//if (playerLocals.playerI == 17 && iI == 17 && commonLocals.addressingCoord == ivec2(0, 26) + 2)
			//{
			//	VectorisedFoundValueCalculationLogger foo;
			//	foo << "Raw plot bonus = " << commonLocals.getPlotBonuses(iI) << '\n';
			//	foo << "Has revealed bonus = " << playerCityPlotInfos[iI].hasRevealedBonus() << '\n';
			//	const ivec2 cityPlotCoordUnwrapped = kCityPlotDeltaCoords[iI] + commonLocals.addressingCoord;
			//	const ivec2 cityPlotCoordWrapped = cityPlotCoordUnwrapped + ivec2(gGlobals.getMapINLINE().getGridWidthINLINE(), 0);
			//	foo << cityPlotCoordUnwrapped << '\n';
			//	foo << cityPlotCoordWrapped << '\n';
			//	foo << commonLocals.addressingCoord << '\n';
			//	foo << playerLocals.addressingCoord << '\n';
			//	foo << "Raw plot bonus = " << loadByteVector(commonLocals.plotBonuses, cityPlotCoordWrapped, commonLocals.pitch) << '\n';
			//	foo << "Raw flags = " << loadByteVector(playerLocals.flags, cityPlotCoordWrapped, playerLocals.pitch) << '\n';
			//	foo << "Raw flags = " << loadByteVector(playerLocals.flags, cityPlotCoordUnwrapped, playerLocals.pitch) << '\n';
			//	std::clog << foo.os.str();
			//	std::abort();
			//}

			const I16Mask hasForeignPlayerOwner = case2 & playerCityPlotInfos[iI].hasForeignOwnerOrOutOfBounds(); // Not out of bounds inside case2.
			bNeutralTerritory &= ~hasForeignPlayerOwner;
			//const I16Vector iCultureMultiplier = vselect(hasForeignPlayerOwner, playerLocals.getOurCultureMultiplierOfCityPlot(iI), 100);
			const I16Vector iCultureMultiplier = playerLocals.getOurCultureMultiplierOfCityPlot(iI);

			HECK_FOUNDVALUE_LOG("hasForeignPlayerOwner = " << hasForeignPlayerOwner);
			HECK_FOUNDVALUE_LOG("iCultureMultiplier = " << iCultureMultiplier);

			// Depends on found value coord
			//if (iCultureMultiplier < (hasMyCitiesOnThisArea ? 25 : 50))
			//	iTakenTiles += hasMyCitiesOnThisArea ? 1 : 2;
			iTakenTiles = vmaskedadd(case2 & vcmplt(iCultureMultiplier, vselect(hasMyCitiesOnThisArea, I16Vector(25), 50)),
				iTakenTiles, vselect(hasMyCitiesOnThisArea, I16Vector(1), 2));

			HECK_FOUNDVALUE_LOG("iTakenTiles (after culture condition) = " << iTakenTiles);

			const I16Mask hasBonusImprovement = commonGlobals.hasBonusImprovement(eBonus);

			HECK_FOUNDVALUE_LOG("hasBonusImprovement = " << hasBonusImprovement);

			const std::array<I16Vector, NUM_YIELD_TYPES> aiYield = iI == CITY_HOME_PLOT
				? commonLocals.getHomePlotValuationYield(hasRevealedBonus)
				: commonLocals.getCityRadiusPlotValuationYield(iI);

			HECK_FOUNDVALUE_LOG("aiYield[0] = " << aiYield[0]);
			HECK_FOUNDVALUE_LOG("aiYield[1] = " << aiYield[1]);
			HECK_FOUNDVALUE_LOG("aiYield[2] = " << aiYield[2]);

			I16Vector iTempI16Value{};

			// Careful this doesn't overflow!
			// Assume max yield 20. Max value 3200 after this `if` block.
			if (iI == CITY_HOME_PLOT)
			{
				iTempI16Value = aiYield[YIELD_FOOD] * 60 + aiYield[YIELD_PRODUCTION] * 60 + aiYield[YIELD_COMMERCE] * 40;
			}
			else
			{
				// aiYield[YIELD_FOOD] >= GC.getFOOD_CONSUMPTION_PER_POPULATION()     => 0
				// aiYield[YIELD_FOOD] == GC.getFOOD_CONSUMPTION_PER_POPULATION() - 1 => 1
				// else                                                               => 2
				const I16Vector scaleSelector = vmax(0, vmin(2, static_cast<int16_t>(commonGlobals.FOOD_CONSUMPTION_PER_POPULATION) - aiYield[YIELD_FOOD]));
				static constexpr I16Vector kFoodProdScales = I16Vector::Array{ 40, 25, 15 };
				static constexpr I16Vector kCommScales = I16Vector::Array{ 30, 20, 10 };
				const I16Vector foodProdScale = kFoodProdScales.permute(scaleSelector);
				iTempI16Value = aiYield[YIELD_FOOD] * foodProdScale;
				iTempI16Value += aiYield[YIELD_PRODUCTION] * foodProdScale;
				iTempI16Value += aiYield[YIELD_COMMERCE] * kCommScales.permute(scaleSelector);
			}

			HECK_FOUNDVALUE_LOG("iTempI16Value after yield scaling = " << iTempI16Value);

			const I16Mask isWater = commonCityPlotInfos[iI].isCityPlotWater();

			{
				const I16Mask richWaterMask = case2 & isWater & vcmpgt(aiYield[YIELD_COMMERCE], 1);
				iTempI16Value = vmaskedadd(richWaterMask, iTempI16Value, vselect(bIsCoastal, I16Vector(30), -20));
				iSpecialFoodPlus = vmaskedadd(richWaterMask & bIsCoastal & vcmpge(aiYield[YIELD_FOOD], static_cast<int16_t>(commonGlobals.FOOD_CONSUMPTION_PER_POPULATION)),
					iSpecialFoodPlus, 1);
			}

			HECK_FOUNDVALUE_LOG("iTempValue after water commerce = " << iTempI16Value << ", iSpecialFoodPlus = " << iSpecialFoodPlus);

			iTempI16Value = vmaskedadd(commonCityPlotInfos[iI].isCityPlotRiver(), iTempI16Value, 10);

			// Moving to 32-bit. Hopefully, none of the above has overflowed.
			I32Vector iTempI32Value(iTempI16Value);

			// Only needed in the second ring.
			const I32Mask secondRingIsCityPlotOwnedByMe = commonLocals.isCityPlotBeyondFirstRing(iI) ? static_cast<I32Mask>(playerCityPlotInfos[iI].isOwnedByThisPlayer()) : I32Mask();

			if (iI == CITY_HOME_PLOT)
				iTempI32Value <<= heck::simd::imm<1>;
			else if (commonLocals.isCityPlotInFirstRing(iI))
				iTempI32Value = divSignedBy2(iTempI32Value * 3); // v = v * 3 / 2
			else
			{
				// locals.isCityPlotOwnedByMe(iI) ? v * 3 / 2 : v * iGreed / 100
				iTempI32Value = vselect(secondRingIsCityPlotOwnedByMe, divSignedBy2(iTempI32Value * 3), divSignedBy100(iTempI32Value * playerGlobals.iGreed));
			}

			HECK_FOUNDVALUE_LOG("iTempI32Value = " << iTempI32Value);

			iValue = vmaskedadd(static_cast<I32Mask>(case2), iValue, divSignedBy100(iTempI32Value * I32Vector(iCultureMultiplier)));

			HECK_FOUNDVALUE_LOG("iValue = " << iValue);

			if (const I16Mask decentCultureMultiplierMask = case2 & vcmpgt(iCultureMultiplier, 33); decentCultureMultiplierMask.any())
			{
				if (iI != CITY_HOME_PLOT)
				{
					const I16Mask hasAnyFeature = decentCultureMultiplierMask & commonCityPlotInfos[iI].getCityPlotHasAnyFeature();
					iHealth = vmaskedadd(hasAnyFeature, iHealth, commonLocals.getCityPlotFeatureHealthPercent(iI));
					iSpecialFoodPlus = vmaskedadd(hasAnyFeature, iSpecialFoodPlus, vmax(0, aiYield[YIELD_FOOD] - static_cast<int16_t>(commonGlobals.FOOD_CONSUMPTION_PER_POPULATION)));
					if (hasAnyFeature.any())
					{
						HECK_FOUNDVALUE_LOG("iHealth = " << iHealth);
						HECK_FOUNDVALUE_LOG("iHealth iSpecialFoodPlus = " << iSpecialFoodPlus);
					}
				}

				const I16Mask activeBonusMask = decentCultureMultiplierMask & vcmple(0, eBonus);
				if (const I16Mask bonusAccessCnd = activeBonusMask & (static_cast<I16Mask>(commonLocals.isCityPlotSameArea(iI)) | playerCityPlotInfos[iI].hasMyCitiesOnArea());
					bonusAccessCnd.any())
				{
					const I32Mask bonusAccessCnd32 = static_cast<I32Mask>(bonusAccessCnd);

					//paiBonusCount[eBonus]++;
					// bonusFirstSeenCondition is paiBonusCount[eBonus] == 1.
					const I32Mask bonusIsFirstSeenHere = ~tlsBonusSeen.testAndSet(eBonus, bonusAccessCnd);

					bonusCount = vmaskedadd(bonusAccessCnd, bonusCount, 1);
					uniqueBonusCount = vmaskedadd(bonusAccessCnd & static_cast<I16Mask>(bonusIsFirstSeenHere), uniqueBonusCount, 1);

					HECK_FOUNDVALUE_LOG("bonusAccessCnd = " << bonusAccessCnd);
					HECK_FOUNDVALUE_LOG("isCityPlotSameArea = " << commonLocals.isCityPlotSameArea(iI));
					HECK_FOUNDVALUE_LOG("isCityPlotSameArea = " << static_cast<I16Mask>(commonLocals.isCityPlotSameArea(iI)));
					HECK_FOUNDVALUE_LOG("hasMyCitiesOnArea = " << playerCityPlotInfos[iI].hasMyCitiesOnArea());
					HECK_FOUNDVALUE_LOG("bonusIsFirstSeenHere = " << bonusIsFirstSeenHere);

					iTempI32Value = divSignedBy100(I32Vector(playerGlobals.getPlayerBonusValuation(eBonus, static_cast<I16Mask>(bonusIsFirstSeenHere))) * playerGlobals.iGreed);

					HECK_FOUNDVALUE_LOG("iTempI32Value = " << iTempI32Value);

					if (iI != CITY_HOME_PLOT)
					{
						// Depends on found value coord
						if (commonLocals.isCityPlotBeyondFirstRing(iI))
						{
							//if ((I32Mask(bonusAccessCnd) & ~secondRingIsCityPlotOwnedByMe).any())
							{
								I32Vector greedAdjusted = iTempI32Value;
								greedAdjusted = divSignedBy3(greedAdjusted << heck::simd::imm<1>);
								greedAdjusted = divSignedBy100(greedAdjusted * std::min(150, playerGlobals.iGreed));
								iTempI32Value = vselect(secondRingIsCityPlotOwnedByMe, iTempI32Value, greedAdjusted);
							}
						}

						HECK_FOUNDVALUE_LOG("greed adjusted iTempI32Value = " << iTempI32Value);

						if (const I16Mask specialCnd = bonusAccessCnd & hasBonusImprovement; specialCnd.any()) // if (eBonusImprovement != NO_IMPROVEMENT)
						{
							HECK_FOUNDVALUE_LOG("specialCnd = " << specialCnd);

							const std::array<I16Vector, NUM_YIELD_TYPES> specialYieldTemp = commonLocals.getCityPlotBestNatureYieldWithImprovement(iI, hasRevealedBonus);

							I16Vector iSpecialFoodTemp = specialYieldTemp[YIELD_FOOD];

							iSpecialFood = vmaskedadd(specialCnd, iSpecialFood, iSpecialFoodTemp);
							iSpecialFoodTemp = vmaskedsub(specialCnd, iSpecialFoodTemp, static_cast<int16_t>(commonGlobals.FOOD_CONSUMPTION_PER_POPULATION));
							iSpecialFoodPlus = vmaskedadd(specialCnd, iSpecialFoodPlus, vmax(0, iSpecialFoodTemp));
							iSpecialFoodMinus = vmaskedsub(specialCnd, iSpecialFoodMinus, vmin(0, iSpecialFoodTemp));
							iSpecialProduction = vmaskedadd(specialCnd, iSpecialProduction, specialYieldTemp[YIELD_PRODUCTION]);
							iSpecialCommerce = vmaskedadd(specialCnd, iSpecialCommerce, specialYieldTemp[YIELD_COMMERCE]);

							HECK_FOUNDVALUE_LOG("iSpecialFoodTemp = " << iSpecialFoodTemp);
							HECK_FOUNDVALUE_LOG("iSpecialFood = " << iSpecialFood);
							HECK_FOUNDVALUE_LOG("iSpecialFoodPlus = " << iSpecialFoodPlus);
							HECK_FOUNDVALUE_LOG("iSpecialFoodMinus = " << iSpecialFoodMinus);
							HECK_FOUNDVALUE_LOG("iSpecialProduction = " << iSpecialProduction);
							HECK_FOUNDVALUE_LOG("iSpecialCommerce = " << iSpecialCommerce);
						}

						// if (locals.getCityPlotFeatureFoodPenaltyCondition(iI))
						iResourceValue16 = vmaskedsub(bonusAccessCnd & commonCityPlotInfos[iI].getCityPlotFeatureFoodPenaltyCondition(), iResourceValue16, 30);

						HECK_FOUNDVALUE_LOG("iResourceValue = " << iResourceValue16);

						//if (isWater)
						//	iValue += bIsCoastal ? 100 : -800;
						iValue = vmaskedadd(bonusAccessCnd32, iValue, static_cast<I32Vector>(vselect(isWater, vselect(bIsCoastal, I16Vector(100), -800), 0)));

						HECK_FOUNDVALUE_LOG("Water iValue = " << iValue);
					}

					iValue = vmaskedadd(bonusAccessCnd32, iValue, iTempI32Value + 10);
				}
			} // decentCultureMultiplierMask

			HECK_FOUNDVALUE_LOG("Final city plot yield valuation iValue = " << iValue);
		}

		// Bit too dangerous to add these up as 16-bit...
		// Assuming a max yield of 20 (kAssumedMaxNaturalYieldWithImprovement), the special values after multiplication should end up at a max of 21000, less than 2^15.
		I32Vector iResourceValue = I32Vector(iResourceValue16)
			+ I32Vector(iSpecialFood * 50)
			+ I32Vector(iSpecialProduction * 50)
			+ I32Vector(iSpecialCommerce * 50)
			;

		iValue += vmax(0, iResourceValue);

		HECK_FOUNDVALUE_LOG("iValue = " << iValue);
		HECK_FOUNDVALUE_LOG("iResourceValue = " << iResourceValue);

		const auto takenTilesExitMask = (vcmpgt(iTakenTiles, NUM_CITY_PLOTS / 3) & static_cast<I16Mask>(vcmplt(iResourceValue, 250))) | vcmpgt(iTeammateTakenTiles, 1);
		active &= ~takenTilesExitMask;
		HECK_FOUNDVALUE_LOG("takenTilesExitMask = " << takenTilesExitMask << ", active = " << active);
		if (active.none())
		{
			HECK_FOUNDVALUE_LOG("EXIT takenTilesExitMask");
			return 0;
		}

		// A few extras where 16 bits is enough.
		I16Vector valueExtras = divSignedBy5(iHealth);
		HECK_FOUNDVALUE_LOG("iHealth iValue = " << (iValue + static_cast<I32Vector>(valueExtras)));
		valueExtras = vmaskedadd(bIsCoastal & ~hasMyCitiesOnThisArea & bNeutralTerritory, valueExtras, vselect(static_cast<I16Mask>(vcmpgt(iResourceValue, 0)), I16Vector(800), 100));
		HECK_FOUNDVALUE_LOG("bIsCoastal unsettled bNeutralTerritory iValue = " << (iValue + static_cast<I32Vector>(valueExtras)));
		valueExtras = vmaskedadd(bIsCoastal & hasMyCitiesOnThisArea, valueExtras, 400);
		HECK_FOUNDVALUE_LOG("bIsCoastal iValue = " << (iValue + static_cast<I32Vector>(valueExtras)));
		valueExtras = vmaskedadd(homePlotCommonInfo.isHills(), valueExtras, 200);
		HECK_FOUNDVALUE_LOG("isHills iValue = " << (iValue + static_cast<I32Vector>(valueExtras)));
		//HECK_FOUNDVALUE_LOG("isHills = " << commonLocals.isHills());
		valueExtras = vmaskedadd(commonCityPlotInfos[CITY_HOME_PLOT].isCityPlotRiver(), valueExtras, 40);
		HECK_FOUNDVALUE_LOG("isRiver iValue = " << (iValue + static_cast<I32Vector>(valueExtras)));
		valueExtras = vmaskedadd(homePlotCommonInfo.isFreshWater(), valueExtras, static_cast<int16_t>(40 + commonGlobals.FRESH_WATER_HEALTH_CHANGE * 30));
		// Unfortunately, we can't continue this into distance valuation.
		iValue += static_cast<I32Vector>(valueExtras);

		HECK_FOUNDVALUE_LOG("isFreshWater iValue = " << iValue);

		if (playerGlobals.weHaveCityDistances)
		{
			const bool isBarbarian = playerGlobals.isBarbarian;
			const I32Mask distanceAdjustmentBranch0 = static_cast<I32Mask>(isBarbarian ? homePlotCommonInfo.hasAnyCitiesOnArea() : hasMyCitiesOnThisArea);
			const I32Mask distanceAdjustmentBranch1 = static_cast<I32Mask>(active) & ~distanceAdjustmentBranch0;
			const I16Vector nearestCityDistanceValues = isBarbarian ? commonLocals.getBarbarianNearestCityDistanceValue() : playerLocals.getNearestCityDistanceValue();
			const I16Mask isNearestCityMyCapital = playerLocals.getNearestCityIsMyCapital(); // Not used by barbarians.
			HECK_FOUNDVALUE_LOG("distanceAdjustmentBranch0 = " << distanceAdjustmentBranch0);
			HECK_FOUNDVALUE_LOG("distanceAdjustmentBranch1 = " << distanceAdjustmentBranch1);
			HECK_FOUNDVALUE_LOG("nearestCityDistanceValues = " << nearestCityDistanceValues);
			HECK_FOUNDVALUE_LOG("isNearestCityMyCapital = " << isNearestCityMyCapital);
			if (distanceAdjustmentBranch0.any())
			{
				if (isBarbarian)
				{
					valueExtras = vmax(0, 8 - nearestCityDistanceValues) * 200;
					iValue = vmaskedsub(distanceAdjustmentBranch0, iValue, static_cast<I32Vector>(valueExtras));
					HECK_FOUNDVALUE_LOG("dist = " << nearestCityDistanceValues << ", iValue = " << iValue);
				}
				else
				{
					const I16Vector iDistance16 = nearestCityDistanceValues;
					const I32Vector iDistance32 = static_cast<I32Vector>(iDistance16);

					I32Vector distanceAdjustedValue = iValue;

					distanceAdjustedValue = vmaskedsub(distanceAdjustmentBranch0 & vcmpgt(iDistance32, 5), distanceAdjustedValue,
						(iDistance32 - 5) * 500);
					distanceAdjustedValue = vmaskedsub(distanceAdjustmentBranch0 & vcmplt(iDistance32, 4), distanceAdjustedValue,
						(4 - iDistance32) * 2000);

					// fortsnek: This is a function that tends towards 1.0: iValue *= [0.0..1.0].
					//           Except when distance is very low.
					//           This function also exponentially decays towards zero as distance increases.
					const int iNumCities = playerGlobals.playerNumCities;

					/// SIMD INTEGER DIVISION
					distanceAdjustedValue = vdiv(distanceAdjustedValue * (8 + iNumCities * 4), 2 + (iNumCities * 4) + iDistance32);

					HECK_FOUNDVALUE_LOG("dist = " << iDistance16);
					HECK_FOUNDVALUE_LOG("iNumCities = " << iNumCities);
					HECK_FOUNDVALUE_LOG("distanceAdjustedValue = " << distanceAdjustedValue);


					I32Vector isNearestCityMyCapitalValue = distanceAdjustedValue;
					I32Vector playerHasCapitalValue = distanceAdjustedValue;

					if (!isNearestCityMyCapital.all() && playerGlobals.playerHasCapital)
					{
						const int iMaxDistanceFromCapital = playerGlobals.iMaxDistanceFromCapital;

						/// SIMD INTEGER DIVISION
						// Note that this could be replaced with divide by constant as there's only one iMaxDistanceFromCapital per player...
						playerHasCapitalValue *= 100 + vdiv((50 * vmax(0, iMaxDistanceFromCapital - iDistance32)), iMaxDistanceFromCapital);
						playerHasCapitalValue = divSignedBy100(playerHasCapitalValue);
					}

					if (isNearestCityMyCapital.any())
						isNearestCityMyCapitalValue = divSignedBy100(distanceAdjustedValue * 150);

					iValue = vselect(distanceAdjustmentBranch0,
						vselect(static_cast<I32Mask>(isNearestCityMyCapital), isNearestCityMyCapitalValue, playerHasCapitalValue),
						iValue
					);

					HECK_FOUNDVALUE_LOG("Player city distance iValue = " << iValue);
				}
			}

			if (distanceAdjustmentBranch1.any())
			{
				const I32Vector iDistance = static_cast<I32Vector>(nearestCityDistanceValues);
				const I32Vector distanceValuation = vmin(500 * iDistance, commonGlobals.divByMaxPlotDistance(8000 * iDistance));
				iValue = vmaskedsub(distanceAdjustmentBranch1, iValue, distanceValuation);
				HECK_FOUNDVALUE_LOG("Player unsettled area iValue = " << iValue);
			}
		}


		// Note that the below is only multiplicative. If iValue is zero, it will stay zero.

		//if (iValue <= 0)
		//	return 1;
		const I32Mask onesMask = static_cast<I32Mask>(active) & vcmple(iValue, 0);

		HECK_FOUNDVALUE_LOG("onesMask = " << onesMask);

		const I32Mask active32 = static_cast<I32Mask>(active) & ~onesMask;

		HECK_FOUNDVALUE_LOG("active32 = " << active32);

		if (active32.any())
		{
			const auto playerStage2Data = playerLocals.getStage2Data();

			// Team are city count value scaling.
			{
				const I32Mask caseNotEmptyArea = static_cast<I32Mask>(homePlotCommonInfo.hasAnyCitiesOnArea());
				const I32Mask caseBit0 = playerStage2Data.getTeamAreaCityCountCaseBit0();
				const I32Mask caseBit1 = playerStage2Data.getTeamAreaCityCountCaseBit1();

				HECK_FOUNDVALUE_LOG("Team city counts scaling caseBit0 = " << caseBit0);
				HECK_FOUNDVALUE_LOG("Team city counts scaling caseBit1 = " << caseBit1);

				if (caseBit1.all())
				{
					// Common case for everybody on the same continent.
					iValue = vselect(caseNotEmptyArea,
						vselect(caseBit0, iValue, (iValue * 5) >> heck::simd::imm<2>),
						iValue << heck::simd::imm<1>
					);
				}
				else
				{
					iValue = vselect(caseNotEmptyArea,
						vselect(caseBit1,
							vselect(caseBit0, iValue, (iValue * 5) >> heck::simd::imm<2>),
							vselect(caseBit0, divSignedBy3(iValue << heck::simd::imm<2>), (iValue * 3) >> heck::simd::imm<1>)
						),
						iValue << heck::simd::imm<1>
					);
				}
			}

			HECK_FOUNDVALUE_LOG("Team city counts scaling. iValue = " << iValue);

			{
				const I16Vector iFoodSurplus = vmax(0, iSpecialFoodPlus - iSpecialFoodMinus);
				const I16Vector iFoodDeficit = vmax(0, iSpecialFoodMinus - iSpecialFoodPlus);

				iValue *= static_cast<I32Vector>(100 + 20 * vmax(0, vmin(iFoodSurplus, static_cast<int16_t>(2 * commonGlobals.FOOD_CONSUMPTION_PER_POPULATION))));
				/// SIMD INTEGER DIVISION
				iValue = vdiv(iValue, static_cast<I32Vector>(100 + 20 * vmax(0, iFoodDeficit)));
			}

			HECK_FOUNDVALUE_LOG("Food scaling. iValue = " << iValue);

			// TODO: The divisions below, despite not being constant, could be optimised using the division by constant algorithm. Because they are small divisors, they can be looked up in a table.
			if (playerGlobals.playerNumCities != 0)
			{
				//const I32Vector iBonusCount = playerStage2BonusData.getCityRadiusRevealedBonusCount();
				//const I32Vector iUniqueBonusCount = playerStage2BonusData.getCityRadiusUniqueRevealedBonusCount();
				const I32Vector iBonusCount = static_cast<I32Vector>(bonusCount);
				const I32Vector iUniqueBonusCount = static_cast<I32Vector>(uniqueBonusCount);

				HECK_FOUNDVALUE_LOG("iBonusCount = " << iBonusCount);
				HECK_FOUNDVALUE_LOG("iUniqueBonusCount = " << iUniqueBonusCount);

				const I32Mask branch0 = vcmpgt(iBonusCount, 4);
				const I32Mask branch1 = ~branch0 & vcmpgt(iUniqueBonusCount, 2);

				/// SIMD INTEGER DIVISION
				if ((active32 & branch0).any())
					iValue = vselect(static_cast<I32Mask>(branch0), vdiv(iValue * 5, static_cast<I32Vector>(1 + iBonusCount)), iValue);

				/// SIMD INTEGER DIVISION
				if ((active32 & branch1).any())
					iValue = vselect(static_cast<I32Mask>(branch1), vdiv(iValue * 5, static_cast<I32Vector>(3 + iUniqueBonusCount)), iValue);
			}

			HECK_FOUNDVALUE_LOG("Bonus scaling. iValue = " << iValue);

			/// SIMD INTEGER DIVISION
			iValue = vdiv(iValue, 1 + playerStage2Data.getCityRadiusDeadlockedBonusesCount());

			HECK_FOUNDVALUE_LOG("iDeadLockCount = " << playerStage2Data.getCityRadiusDeadlockedBonusesCount() << ", iValue = " << iValue);

			/// SIMD INTEGER DIVISION
			iValue = vdiv(iValue, static_cast<I32Vector>(vmax(0, iBadTile - NUM_CITY_PLOTS / 4) + 3));

			
		}

		const I32Vector finalValue = vselect(active32, vmax(1, iValue), vselect(onesMask, I32Vector(1), 0));

		HECK_FOUNDVALUE_LOG("Final iValue = " << finalValue);

		return finalValue;
	}
}

FoundValueSystem::I32Vector HECK_VECTORCALL FoundValueSystem::computeFoundValue_Vectorised(
	const FoundValueCalculationCommonGlobalDataInterface commonGlobals,
	const FoundValueCalculationPlayerGlobalDataInterface playerGlobals,
	const FoundValueCalculationCommonMapInterface commonLocals,
	const FoundValueCalculationPlayerMapInterface playerLocals,
	int iMinRivalRange,
	I16Mask inputActiveMask,
	std::string* logOutput
)
{
	if constexpr (kEnableLoggingAbility)
	{
		if (logOutput) [[unlikely]]
		{
			VectorisedFoundValueCalculationLogger logger;
			const I32Vector values = _computeFoundValue_Vectorised<true>(commonGlobals, playerGlobals, commonLocals, playerLocals, iMinRivalRange, inputActiveMask, &logger);
			*logOutput = std::move(logger.os).str();
			return values;
		}
	}

	return _computeFoundValue_Vectorised<false>(commonGlobals, playerGlobals, commonLocals, playerLocals, iMinRivalRange, inputActiveMask, nullptr);
}

#endif