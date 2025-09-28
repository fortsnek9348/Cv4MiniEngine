#pragma once

// Nothing except this!
#include "..\..\CvEnums.h"

#include "..\Util.h"

namespace GiganticMapsOptimisationsLib
{
	/// All game data accessed through a defined interface (including global constant data).

	struct FoundValueCalculationLocalDataInterface
	{
		enum class EArea : int;

		// All functions are implicitly relative to this coordinate.
		const ivec2 foundValueCoord{};

		EArea getAreaId() const;
		EArea getCityPlotAreaId(int cityPlotI) const;
		bool isCityPlotSameArea(int cityPlotI) const;

		bool canFound(PlayerTypes playerI) const;
		bool isOceanicCoastalLand() const; // pPlot->isCoastalLand(GC.getMIN_WATER_SIZE_FOR_OCEAN());
		bool isHills() const; // if (pPlot->isHills())
		bool isCityPlotWater(int cityPlotI) const;
		bool isCityPlotRiver(int cityPlotI) const;
		bool isFreshWater() const; // if (pPlot->isFreshWater())

		bool getCityPlotHasAnyFeature(int cityPlotI) const;

		bool isPlayerAIPlotCitySite(PlayerTypes playerI) const; // player.AI_isPlotCitySite(pPlot)
		//int getPlayerAINumCitySites(PlayerTypes playerI) const; // player.AI_getNumCitySites()
		//ivec2 getPlayerAICitySiteCoord(PlayerTypes playerI, int index) const; // player.AI_getCitySite(iJ)
		bool isCityPlotOutOfBounds(int cityPlotI) const;
		std::optional<i16vec2> tryGetIndexedCityPlotCoord(ivec2 cityCoord, int index) const; // plotCity(iX, iY, iI)
		bool isCityPlotWithinPlayerAICitySites(PlayerTypes playerI, int cityPlotI) const; // data.plotDistance(*pLoopPlot, pCitySitePlot.x) <= CITY_PLOTS_RADIUS
		bool hasHostileRivalUnitWithinRange(PlayerTypes playerI, int range) const; // pLoopPlot->plotCheck(PUF_isOtherTeam, player.getID()) != nullptr
		bool isCityPlotOwnedByOtherTeamOrOutOfBounds(PlayerTypes playerI, int cityPlotI) const; // if (pLoopPlot->isOwned() && pLoopPlot->getTeam() != player.getTeam())

		// Include div by 2 at the end.
		// Also has minor global dependency on revealed bonuses.
		// And also, this can be be local per-plot because it depends on found plot isCoastalLand too.
		int getTotalBadTileCount(PlayerTypes playerI) const;

		int getAreaNumPlots(EArea) const;

		BonusTypes getCityPlotBonusType(PlayerTypes playerI, int cityPlotI) const; // pLoopPlot->getBonusType(player.getTeam());
		bool isCityPlotOwned(int cityPlotI) const; // pLoopPlot->isOwned
		bool isCityPlotOwnedByMe(int cityPlotI) const; // pLoopPlot->getOwnerINLINE() == player.getID()
		bool hasForeignPlayerOwner(int cityPlotI) const; // !(!locals.isCityPlotOwned(iI) || (pLoopPlot->getOwnerINLINE() == player.getID()))
		int getOurCultureMultiplierOfCityPlot(int cityPlotI) const; // depends on our culture and foreign culture, and iClaimThreshold

		bool isCityPlotCityRadius(int cityPlotI) const;
		FeatureTypes getCityPlotFeatureType(int cityPlotI) const;

		//int getCityPlotYield(int cityPlotI, YieldTypes yieldI) const;
		
		std::array<int, NUM_YIELD_TYPES> getHomePlotValuationYield(bool withBonus) const;
		std::array<int, NUM_YIELD_TYPES> getCityRadiusPlotValuationYield(int cityPlotI, bool withBonus) const;

		int getCityPlotYieldValueScale(int cityPlotI, YieldTypes yieldI) const;

		int8_t getCityPlotFeatureHealth(int cityPlotI) const;

		// pLoopPlot->calculateBestNatureYield(YIELD_FOOD, player.getTeam()) + GC.getImprovementInfo(eBonusImprovement).getImprovementBonusYield(eBonus, YIELD_FOOD);
		std::array<int, NUM_YIELD_TYPES> getCityPlotBestNatureYieldWithImprovement(int cityPlotI, bool withBonus) const;

		// if (eFeature != NO_FEATURE && GC.getFeatureInfo(eFeature).getYieldChange(YIELD_FOOD) < 0)
		bool getCityPlotFeatureFoodPenaltyCondition(int cityPlotI) const;

		// std::max(0, (8 - plotDistance(iX, iY, pNearestCity->getX_INLINE(), pNearestCity->getY_INLINE()))) * 200;
		// For barbarians.
		int getAnyNearestCityDistanceOnThisArea() const;

		int getMyNearestCityDistanceOnThisArea() const;
		bool isMyNearestCityACapital() const; // if (pNearestCity->isCapital())

		// For barbarians.
		// plotDistance(iX, iY, pNearestCity->getX_INLINE(), pNearestCity->getY_INLINE());
		// std::min(500 * iDistance, (8000 * iDistance) / GC.getMapINLINE().maxPlotDistance());
		// Returns 0 if no cities.
		int getGlobalAnyCityNearestDistanceValuation() const;
		// plotDistance(iX, iY, pNearestCity->getX_INLINE(), pNearestCity->getY_INLINE());
		// std::min(500 * iDistance, (8000 * iDistance) / GC.getMapINLINE().maxPlotDistance());
		// Returns 0 if no cities.
		int getGlobalMyCityNearestDistanceValuation() const;

		// paiBonusCount, iBonusCount
		int getCityRadiusBonusCount() const;
		// paiBonusCount, iUniqueBonusCount
		int getCityRadiusUniqueBonusCount() const;
		// int iDeadLockCount = player.AI_countDeadlockedBonuses(pPlot);
		int getDeadlockedBonusesCount() const;
		
		//static int plotDistance(ivec2, ivec2);

		static bool isCityPlotInFirstRing(int cityPlotI);
		static bool isCityPlotBeyondFirstRing(int cityPlotI);
	};

	struct FoundValueCalculationGlobalDataInterface
	{
		// Here, we assume that an area is practically global, and not just a few plots.

		using EArea = FoundValueCalculationLocalDataInterface::EArea;

		int getAreaPlayerNumCities(EArea area, PlayerTypes playerI) const; // pArea->getCitiesPerPlayer(player.getID());
		bool hasMyCitiesOnArea(EArea area) const; // pLoopPlot->area()->getCitiesPerPlayer(player.getID()) != 0
		bool hasAnyCitiesOnArea(EArea area) const;

		bool getGoodBonusCondition(PlayerTypes playerI, BonusTypes bonusI) const; // if ((player.getNumTradeableBonuses(eBonus) == 0) || (player.AI_bonusVal(eBonus) > 10) || (GC.getBonusInfo(eBonus).getYieldChange(YIELD_FOOD) > 0))

		bool getAccessibleBonusPlotCondition(PlayerTypes playerI, EArea area) const; // if (pLoopPlot->isWater() || (pLoopPlot->area() == pArea) || (pLoopPlot->area()->getCitiesPerPlayer(player.getID()) > 0))

		int getPlayerGreedValue(PlayerTypes playerI) const;
		int getPlayerClaimThreshold(PlayerTypes playerI) const;

		ImprovementTypes getBonusImprovement(BonusTypes bonusI) const; // First where CvImprovementInfo.isImprovementBonusMakesValid

		// hasSingleBonusInCityRadius = paiBonusCount[eBonus] == 1
		int getPlayerBonusValuation(BonusTypes bonusI, bool hasSingleBonusInCityRadius) const; // (player.AI_bonusVal(eBonus) * (((player.getNumTradeableBonuses(eBonus) == 0) && (paiBonusCount[eBonus] == 1)) ? 80 : 20));

		bool isBarbarian() const;

		int getPlayerNumCities() const;
		bool hasPlayerAnyCities() const;

		bool hasCapital() const; // else if (player.getCapitalCity() != nullptr)

		int getMaxDistanceFromCapital() const; // iMaxDistanceFromCapital = std::max(iMaxDistanceFromCapital, plotDistance(iCapitalX, iCapitalY, pLoopCity->getX_INLINE(), pLoopCity->getY_INLINE()));

		int doTeamAreaCityCountValueScaling(EArea area, int iValue) const;

		static int getFOOD_CONSUMPTION_PER_POPULATION();
		static int getFRESH_WATER_HEALTH_CHANGE();
	};

	

	inline int computeFoundValue_explicitDependencies(
		const FoundValueCalculationGlobalDataInterface& globals,
		const FoundValueCalculationLocalDataInterface& locals,
		PlayerTypes playerI,int iMinRivalRange)
	{
		//static constexpr bool bStartingLoc = false;

		//const CvPlot* const pPlot = GC.getMapINLINE().plotINLINE(iX, iY);

		if (!locals.canFound(playerI))
			return 0;

		const bool bIsCoastal = locals.isOceanicCoastalLand();
		const FoundValueCalculationLocalDataInterface::EArea pArea = locals.getAreaId();
		const bool hasMyCitiesOnThisArea = globals.hasMyCitiesOnArea(pArea);

		//const bool bAdvancedStart = (player.getAdvancedStartPoints() >= 0);
		constexpr bool bAdvancedStart = false;

		if (!bIsCoastal && !hasMyCitiesOnThisArea) // fortsnek: AI_foundValue_dependency
			return 0;

		std::bitset<NUM_CITY_PLOTS> abCitySiteRadius{};

		if (!locals.isPlayerAIPlotCitySite(playerI)) // fortsnek: AI_foundValue_dependency
			for (int iI = 0; iI < NUM_CITY_PLOTS; iI++)
				abCitySiteRadius[iI] = locals.isCityPlotWithinPlayerAICitySites(playerI, iI);

		thread_local std::vector<int> paiBonusCount;
		paiBonusCount.clear();
		paiBonusCount.resize(GC.getNumBonusInfos());

		if (iMinRivalRange != -1 && locals.hasHostileRivalUnitWithinRange(playerI, iMinRivalRange))
			return 0;
		

		int iOwnedTiles = 0;

		for (int iI = 0; iI < NUM_CITY_PLOTS; iI++)
			iOwnedTiles += locals.isCityPlotOwnedByOtherTeamOrOutOfBounds(playerI, iI);

		if (iOwnedTiles > NUM_CITY_PLOTS / 3)
			return 0;

		const int iBadTile = locals.getTotalBadTileCount(playerI);

		if (iBadTile > NUM_CITY_PLOTS / 2 || locals.getAreaNumPlots(pArea) <= 2)
		{
			bool bHasGoodBonus = false;

			for (int iI = 0; iI < NUM_CITY_PLOTS; iI++)
			{
				const BonusTypes eBonus = locals.getCityPlotBonusType(playerI, iI);

				if (eBonus != NO_BONUS && !locals.isCityPlotOwned(iI))
				{
					if (locals.getCityPlotAreaId(iI) == pArea || locals.isCityPlotWater(iI) || globals.hasMyCitiesOnArea(locals.getCityPlotAreaId(iI)))
					{
						if (globals.getGoodBonusCondition(playerI, eBonus))
						{
							bHasGoodBonus = true;
							break;
						}
					}
				}
			}

			if (!bHasGoodBonus)
			{
				return 0;
			}
		}

		int iTakenTiles = 0;
		int iTeammateTakenTiles = 0;
		int iHealth = 0;
		int iValue = 1000;

		const int iGreed = globals.getPlayerGreedValue(playerI);
		const int iClaimThreshold = globals.getPlayerClaimThreshold(playerI);

		int iResourceValue = 0;
		int iSpecialFood = 0;
		int iSpecialFoodPlus = 0;
		int iSpecialFoodMinus = 0;
		int iSpecialProduction = 0;
		int iSpecialCommerce = 0;
		bool bNeutralTerritory = true;

		for (int iI = 0; iI < NUM_CITY_PLOTS; iI++)
		{
			if (locals.isCityPlotOutOfBounds(iI))
			{
				iTakenTiles++;
			}
			else if (locals.isCityPlotCityRadius(iI) || abCitySiteRadius[iI])
			{
				iTakenTiles++;

				if (abCitySiteRadius[iI])
					iTeammateTakenTiles++;
			}
			else
			{
				int iTempValue = 0;

				const BonusTypes eBonus = locals.getCityPlotBonusType(playerI, iI);

				const int iCultureMultiplier = locals.hasForeignPlayerOwner(iI)
					? bNeutralTerritory = false, locals.getOurCultureMultiplierOfCityPlot(iI)
					: 100;

				// Depends on found value coord
				if (iCultureMultiplier < (hasMyCitiesOnThisArea ? 25 : 50))
					iTakenTiles += hasMyCitiesOnThisArea ? 1 : 2;

				const ImprovementTypes eBonusImprovement = eBonus != NO_BONUS ? globals.getBonusImprovement(eBonus) : NO_IMPROVEMENT;

				const std::array<int, NUM_YIELD_TYPES> aiYield = iI == CITY_HOME_PLOT
					? locals.getHomePlotValuationYield(eBonus != NO_BONUS)
					: locals.getCityRadiusPlotValuationYield(iI, eBonus != NO_BONUS);

				if (iI == CITY_HOME_PLOT)
				{
					iTempValue += aiYield[YIELD_FOOD] * 60;
					iTempValue += aiYield[YIELD_PRODUCTION] * 60;
					iTempValue += aiYield[YIELD_COMMERCE] * 40;
				}
				else
				{
					iTempValue += aiYield[YIELD_FOOD] * locals.getCityPlotYieldValueScale(iI, YIELD_FOOD);
					iTempValue += aiYield[YIELD_PRODUCTION] * locals.getCityPlotYieldValueScale(iI, YIELD_PRODUCTION);
					iTempValue += aiYield[YIELD_COMMERCE] * locals.getCityPlotYieldValueScale(iI, YIELD_COMMERCE);
				}

				const bool isWater = locals.isCityPlotWater(iI);
					
				if (isWater && aiYield[YIELD_COMMERCE] > 1)
				{
					// Depends on found value coord
					iTempValue += bIsCoastal ? 30 : -20;
					if (bIsCoastal && aiYield[YIELD_FOOD] >= GC.getFOOD_CONSUMPTION_PER_POPULATION())
						iSpecialFoodPlus += 1;
				}

				if (locals.isCityPlotRiver(iI))
					iTempValue += 10;

				if (iI == CITY_HOME_PLOT)
					iTempValue *= 2;
				// Depends on found value coord
				else if (locals.isCityPlotOwnedByMe(iI) || locals.isCityPlotInFirstRing(iI))
				{
					iTempValue *= 3;
					iTempValue /= 2;
				}
				else
				{
					iTempValue *= iGreed;
					iTempValue /= 100;
				}

				iTempValue *= iCultureMultiplier;
				iTempValue /= 100;

				iValue += iTempValue;

				if (iCultureMultiplier > 33)
				{
					if (iI != CITY_HOME_PLOT && locals.getCityPlotHasAnyFeature(iI))
					{
						iHealth += locals.getCityPlotFeatureHealth(iI);
						iSpecialFoodPlus += std::max(0, aiYield[YIELD_FOOD] - GC.getFOOD_CONSUMPTION_PER_POPULATION());
					}

					// Depends on found value coord
					if (eBonus != NO_BONUS && (locals.isCityPlotSameArea(iI) || globals.hasMyCitiesOnArea(locals.getCityPlotAreaId(iI))))
					{
						paiBonusCount[eBonus]++;

						iTempValue = globals.getPlayerBonusValuation(eBonus, paiBonusCount[eBonus] == 1) * iGreed / 100;

						if (iI != CITY_HOME_PLOT)
						{
							// Depends on found value coord
							if (!locals.isCityPlotOwnedByMe(iI) && locals.isCityPlotBeyondFirstRing(iI))
							{
								iTempValue = iTempValue * 2 / 3;
								iTempValue = iTempValue * std::min(150, iGreed) / 100;
							}

							if (eBonusImprovement != NO_IMPROVEMENT)
							{
								const std::array<int, NUM_YIELD_TYPES> specialYieldTemp = locals.getCityPlotBestNatureYieldWithImprovement(iI, eBonus != NO_BONUS);

								int iSpecialFoodTemp = specialYieldTemp[YIELD_FOOD];

								iSpecialFood += iSpecialFoodTemp;

								iSpecialFoodTemp -= globals.getFOOD_CONSUMPTION_PER_POPULATION();

								iSpecialFoodPlus += std::max(0, iSpecialFoodTemp);
								iSpecialFoodMinus -= std::min(0, iSpecialFoodTemp);
								iSpecialProduction += specialYieldTemp[YIELD_PRODUCTION];
								iSpecialCommerce += specialYieldTemp[YIELD_COMMERCE];
							}

							if (locals.getCityPlotFeatureFoodPenaltyCondition(iI))
								iResourceValue -= 30;

							if (isWater)
								iValue += bIsCoastal ? 100 : -800;
						}

						iValue += iTempValue + 10;
					}
				}
			}
		}

		iResourceValue += iSpecialFood * 50;
		iResourceValue += iSpecialProduction * 50;
		iResourceValue += iSpecialCommerce * 50;

		iValue += std::max(0, iResourceValue);

		if (iTakenTiles > NUM_CITY_PLOTS / 3 && iResourceValue < 250)
			return 0;

		if (iTeammateTakenTiles > 1)
			return 0;

		iValue += iHealth / 5;

		if (bIsCoastal)
		{
			if (!globals.hasMyCitiesOnArea(pArea))
			{
				if (bNeutralTerritory)
					iValue += iResourceValue > 0 ? 800 : 100;
			}
			else
			{
				iValue += 400;
			}
		}

		if (locals.isHills())
			iValue += 200;

		if (locals.isCityPlotRiver(CITY_HOME_PLOT))
			iValue += 40;

		if (locals.isFreshWater())
			iValue += 40 + globals.getFRESH_WATER_HEALTH_CHANGE() * 30;

		if (globals.isBarbarian() ? globals.hasAnyCitiesOnArea(pArea) : hasMyCitiesOnThisArea)
		{
			if (globals.isBarbarian())
			{
				iValue -= std::max(0, 8 - locals.getAnyNearestCityDistanceOnThisArea()) * 200;
			}
			else
			{
				const int iDistance = locals.getMyNearestCityDistanceOnThisArea();

				if (iDistance > 5)
					iValue -= (iDistance - 5) * 500;
				else if (iDistance < 4)
					iValue -= (4 - iDistance) * 2000;

				// fortsnek: This is a function that tends towards 1.0: iValue *= [0.0..1.0].
				//           Except when distance is very low.
				//           This function also exponentially decays towards zero as distance increases.
				const int iNumCities = globals.getPlayerNumCities();
				iValue = iValue * (8 + iNumCities * 4) / (2 + (iNumCities * 4) + iDistance);

				if (locals.isMyNearestCityACapital())
				{
					iValue = iValue * 150 / 100;
				}
				else if (globals.hasCapital())
				{
					const int iMaxDistanceFromCapital = globals.getMaxDistanceFromCapital();

					// fortsnek: At iDistance < iMaxDistanceFromCapital, this is a lerp from 1.5 to 1.
					//           Else, this does nothing.
					iValue *= 100 + (50 * std::max(0, iMaxDistanceFromCapital - iDistance)) / iMaxDistanceFromCapital;
					iValue /= 100;
				}
			}
		}
		else
		{
			iValue -= globals.isBarbarian() ? locals.getGlobalAnyCityNearestDistanceValuation() : locals.getGlobalMyCityNearestDistanceValuation();
		}

		if (iValue <= 0)
			return 1;

		iValue = globals.doTeamAreaCityCountValueScaling(pArea, iValue);

		{
			int iFoodSurplus = std::max(0, iSpecialFoodPlus - iSpecialFoodMinus);
			int iFoodDeficit = std::max(0, iSpecialFoodMinus - iSpecialFoodPlus);

			iValue *= 100 + 20 * std::max(0, std::min(iFoodSurplus, 2 * globals.getFOOD_CONSUMPTION_PER_POPULATION()));
			iValue /= 100 + 20 * std::max(0, iFoodDeficit);
		}

		if (globals.hasPlayerAnyCities())
		{
			const int iBonusCount = locals.getCityRadiusBonusCount();
			const int iUniqueBonusCount = locals.getCityRadiusUniqueBonusCount();
			if (iBonusCount > 4)
			{
				iValue *= 5;
				iValue /= (1 + iBonusCount);
			}
			else if (iUniqueBonusCount > 2)
			{
				iValue *= 5;
				iValue /= (3 + iUniqueBonusCount);
			}
		}

		
		iValue /= 1 + locals.getDeadlockedBonusesCount();
		
		iValue /= std::max(0, iBadTile - NUM_CITY_PLOTS / 4) + 3;

		return std::max(1, iValue);
	}
}