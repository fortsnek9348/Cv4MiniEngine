#include "../../CommonConfig.h"

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS

#include "FoundValueVectorisedCalculation.h"

#include "../../CvMap.h"
#include "../../CvPlayerAI.h"
#include "../../CvInfos.h"
#include "../../CvTeamAI.h"

#include <iostream>

using namespace GiganticMapsOptimisationsLib;

int FoundValueSystem::computeFoundValue_Logging(PlayerTypes playerI, ivec2 coord, int iMinRivalRange, std::ostream& os)
{
	//static constexpr bool bStartingLoc = false;

	const CvPlayerAI& player = GET_PLAYER(playerI);

	const int iX = coord.x;
	const int iY = coord.y;

	CvPlot* const pPlot = GC.getMapINLINE().plotINLINE(iX, iY);

	os << __LINE__ << ": Player " << (int)playerI << " coord " << coord << '\n';

	if (!player.canFound(iX, iY))
	{
		os << __LINE__ << ": Can't found.\n";
		return 0;
	}

	const bool bIsCoastal = pPlot->isCoastalLand(GC.getMIN_WATER_SIZE_FOR_OCEAN());
	const CvArea* const pArea = pPlot->area();
	const int iNumAreaCities = pArea->getCitiesPerPlayer(player.getID());

	//const bool bAdvancedStart = (player.getAdvancedStartPoints() >= 0);
	constexpr bool bAdvancedStart = false;

	os << __LINE__ << ": bIsCoastal = " << bIsCoastal << ", iNumAreaCities = " << iNumAreaCities << '\n';

	if (!bIsCoastal && iNumAreaCities == 0) // fortsnek: AI_foundValue_dependency
	{
		os << __LINE__ << ": EXIT !bIsCoastal && iNumAreaCities == 0\n";
		return 0;
	}

	std::vector<bool> abCitySiteRadius(NUM_CITY_PLOTS, false);

	{
		os << __LINE__ << ": AI_isPlotCitySite =  " << player.AI_isPlotCitySite(pPlot) << '\n';
		if (!player.AI_isPlotCitySite(pPlot))
		{
			for (int iI = 0; iI < NUM_CITY_PLOTS; iI++)
			{
				const CvPlot* const pLoopPlot = plotCity(iX, iY, iI);
				if (pLoopPlot != nullptr)
				{
					for (int iJ = 0; iJ < player.AI_getNumCitySites(); iJ++)
					{
						CvPlot* pCitySitePlot = player.AI_getCitySite(iJ);
						if (pCitySitePlot != pPlot)
						{
							if (plotDistance(pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE(), pCitySitePlot->getX_INLINE(), pCitySitePlot->getY_INLINE()) <= CITY_PLOTS_RADIUS)
							{
								os << __LINE__ << ": abCitySiteRadius " << iI << '\n';
								abCitySiteRadius[iI] = true;
								break;
							}
						}
					}
				}
			}
		}
	}

	std::vector<int> paiBonusCount;

	for (int iI = 0; iI < GC.getNumBonusInfos(); iI++)
	{
		paiBonusCount.push_back(0);
	}

	if (iMinRivalRange != -1)
	{
		for (int iDX = -(iMinRivalRange); iDX <= iMinRivalRange; iDX++)
		{
			for (int iDY = -(iMinRivalRange); iDY <= iMinRivalRange; iDY++)
			{
				const CvPlot* const pLoopPlot = plotXY(iX, iY, iDX, iDY);

				if (pLoopPlot != nullptr)
				{
					if (pLoopPlot->plotCheck(PUF_isOtherTeam, player.getID()) != nullptr)
					{
						os << __LINE__ << ": EXIT within iMinRivalRange " << iDX << ' ' << iDY << '\n';
						return 0;
					}
				}
			}
		}
	}

	int iOwnedTiles = 0;

	for (int iI = 0; iI < NUM_CITY_PLOTS; iI++)
	{
		const CvPlot* const pLoopPlot = plotCity(iX, iY, iI);

		if (pLoopPlot == nullptr)
		{
			iOwnedTiles++;
		}
		else if (pLoopPlot->isOwned()) // fortsnek: AI_foundValue_dependency
		{
			if (pLoopPlot->getTeam() != player.getTeam()) // fortsnek: AI_foundValue_dependency
			{
				iOwnedTiles++;
			}
		}
	}

	os << __LINE__ << ": iOwnedTiles = " << iOwnedTiles << '\n';

	if (iOwnedTiles > (NUM_CITY_PLOTS / 3))
	{
		os << __LINE__ << ": EXIT iOwnedTiles\n";
		return 0;
	}

	int iBadTile = 0;

	for (int iI = 0; iI < NUM_CITY_PLOTS; iI++)
	{
		const CvPlot* const pLoopPlot = plotCity(iX, iY, iI);

		if (iI != CITY_HOME_PLOT)
		{
			if ((pLoopPlot == nullptr) || pLoopPlot->isImpassable())
			{
				iBadTile += 2;
			}
			else if (!(pLoopPlot->isFreshWater()) && !(pLoopPlot->isHills()))
			{
				if ((pLoopPlot->calculateBestNatureYield(YIELD_FOOD, player.getTeam()) == 0) || (pLoopPlot->calculateTotalBestNatureYield(player.getTeam()) <= 1))
				{
					iBadTile += 2;
				}
				else if (pLoopPlot->isWater() && !bIsCoastal && (pLoopPlot->calculateBestNatureYield(YIELD_FOOD, player.getTeam()) <= 1))
				{
					iBadTile++;
				}
			}
			else if (pLoopPlot->isOwned()) // fortsnek: AI_foundValue_dependency
			{
				if (pLoopPlot->getTeam() == player.getTeam()) // fortsnek: AI_foundValue_dependency
				{
					if (pLoopPlot->isCityRadius() || abCitySiteRadius[iI]) // fortsnek: AI_foundValue_dependency
					{
						iBadTile += bAdvancedStart ? 2 : 1;
					}
				}
			}
		}
	}

	os << __LINE__ << ": iBadTile = " << iBadTile << " / 2\n";

	iBadTile /= 2;

	if ((iBadTile > (NUM_CITY_PLOTS / 2)) || (pArea->getNumTiles() <= 2))
	{
		bool bHasGoodBonus = false;

		for (int iI = 0; iI < NUM_CITY_PLOTS; iI++)
		{
			const CvPlot* const pLoopPlot = plotCity(iX, iY, iI);

			if (pLoopPlot != nullptr)
			{
				// fortsnek: AI_foundValue_dependency
				if (!(pLoopPlot->isOwned()))
				{
					// fortsnek: AI_foundValue_dependency
					if (pLoopPlot->isWater() || (pLoopPlot->area() == pArea) || (pLoopPlot->area()->getCitiesPerPlayer(player.getID()) > 0))
					{
						// fortsnek: AI_foundValue_dependency
						const BonusTypes eBonus = pLoopPlot->getBonusType(player.getTeam());

						if (eBonus != NO_BONUS)
						{
							// fortsnek: AI_foundValue_dependency
							if ((player.getNumTradeableBonuses(eBonus) == 0) || (player.AI_bonusVal(eBonus) > 10)
								|| (GC.getBonusInfo(eBonus).getYieldChange(YIELD_FOOD) > 0))
							{
								os << __LINE__ << ": bHasGoodBonus = true at " << iI << '\n';
								bHasGoodBonus = true;
								break;
							}
						}
					}
				}
			}
		}

		if (!bHasGoodBonus)
		{
			os << __LINE__ << ": EXIT !bHasGoodBonus\n";
			return 0;
		}

		os << __LINE__ << ": bHasGoodBonus = true\n";
	}

	int iTakenTiles = 0;
	int iTeammateTakenTiles = 0;
	int iHealth = 0;
	int iValue = 1000;

	int iGreed = 100;

	{
		for (int iI = 0; iI < GC.getNumTraitInfos(); iI++)
		{
			if (player.hasTrait((TraitTypes)iI))
			{
				iGreed += (GC.getTraitInfo((TraitTypes)iI).getUpkeepModifier() / 2);
				iGreed += 20 * (GC.getTraitInfo((TraitTypes)iI).getCommerceChange(COMMERCE_CULTURE));
			}
		}
	}

	int iClaimThreshold = GC.getGameINLINE().getCultureThreshold((CultureLevelTypes)(std::min(2, (GC.getNumCultureLevelInfos() - 1))));
	iClaimThreshold = std::max(1, iClaimThreshold);
	iClaimThreshold *= (std::max(100, iGreed));

	os << __LINE__ << ": iGreed = " << iGreed << ", iClaimThreshold = " << iClaimThreshold << '\n';

	int iResourceValue = 0;
	int iSpecialFood = 0;
	int iSpecialFoodPlus = 0;
	int iSpecialFoodMinus = 0;
	int iSpecialProduction = 0;
	int iSpecialCommerce = 0;
	bool bNeutralTerritory = true;

	for (int iI = 0; iI < NUM_CITY_PLOTS; iI++)
	{
		const CvPlot* const pLoopPlot = plotCity(iX, iY, iI);

		os << __LINE__ << ": Yield valuation loop [" << iI << "]\n";

		if (pLoopPlot == nullptr)
		{
			iTakenTiles++;
			os << __LINE__ << ": pLoopPlot == nullptr, iTakenTiles = " << iTakenTiles << '\n';
		}
		else if (pLoopPlot->isCityRadius() || abCitySiteRadius[iI])
		{
			iTakenTiles++;

			if (abCitySiteRadius[iI])
			{
				iTeammateTakenTiles++;
			}

			os << __LINE__ << ": pLoopPlot->isCityRadius() = " << pLoopPlot->isCityRadius() << " || abCitySiteRadius[iI] = " << abCitySiteRadius[iI] << ", iTakenTiles = " << iTakenTiles << '\n';
		}
		else
		{
			int iTempValue = 0;

			const FeatureTypes eFeature = pLoopPlot->getFeatureType();
			// fortsnek: AI_foundValue_dependency
			const BonusTypes eBonus = pLoopPlot->getBonusType(player.getTeam());
			ImprovementTypes eBonusImprovement = NO_IMPROVEMENT;

			os << __LINE__ << ": eBonus = " << (int)eBonus << '\n';

			int iCultureMultiplier;
			// fortsnek: AI_foundValue_dependency
			if (!pLoopPlot->isOwned() || (pLoopPlot->getOwnerINLINE() == player.getID()))
			{
				iCultureMultiplier = 100;
			}
			else
			{
				bNeutralTerritory = false;
				// fortsnek: AI_foundValue_dependency
				int iOurCulture = pLoopPlot->getCulture(player.getID());
				// fortsnek: AI_foundValue_dependency
				int iOtherCulture = std::max(1, pLoopPlot->getCulture(pLoopPlot->getOwnerINLINE()));
				iCultureMultiplier = 100 * (iOurCulture + iClaimThreshold);
				iCultureMultiplier /= (iOtherCulture + iClaimThreshold);
				iCultureMultiplier = std::min(100, iCultureMultiplier);
			}

			os << __LINE__ << ": has foreign owner = " << !(!pLoopPlot->isOwned() || (pLoopPlot->getOwnerINLINE() == player.getID())) << '\n';
			os << __LINE__ << ": iCultureMultiplier = " << iCultureMultiplier << '\n';

			if (iCultureMultiplier < ((iNumAreaCities > 0) ? 25 : 50))
			{
				iTakenTiles += (iNumAreaCities > 0) ? 1 : 2;
			}

			os << __LINE__ << ": iTakenTiles = " << iTakenTiles << '\n';

			if (eBonus != NO_BONUS)
			{
				for (int iImprovement = 0; iImprovement < GC.getNumImprovementInfos(); ++iImprovement)
				{
					CvImprovementInfo& kImprovement = GC.getImprovementInfo((ImprovementTypes)iImprovement);

					if (kImprovement.isImprovementBonusMakesValid(eBonus))
					{
						eBonusImprovement = (ImprovementTypes)iImprovement;
						break;
					}
				}
			}

			os << __LINE__ << ": eBonusImprovement = " << (int)eBonusImprovement << '\n';

			int aiYield[NUM_YIELD_TYPES];

			for (int iYieldType = 0; iYieldType < NUM_YIELD_TYPES; ++iYieldType)
			{
				YieldTypes eYield = (YieldTypes)iYieldType;
				// fortsnek: AI_foundValue_dependency local
				aiYield[eYield] = pLoopPlot->getYield(eYield);

				os << __LINE__ << ": raw plot yield " << iYieldType << " = " << aiYield[eYield] << '\n';

				if (iI == CITY_HOME_PLOT)
				{
					int iBasePlotYield = aiYield[eYield];
					aiYield[eYield] += GC.getYieldInfo(eYield).getCityChange();

					if (eFeature != NO_FEATURE)
					{
						aiYield[eYield] -= GC.getFeatureInfo(eFeature).getYieldChange(eYield);
						iBasePlotYield = std::max(iBasePlotYield, aiYield[eYield]);
					}

					if (eBonus == NO_BONUS)
					{
						aiYield[eYield] = std::max(aiYield[eYield], GC.getYieldInfo(eYield).getMinCity());
					}
					else
					{
						int iBonusYieldChange = GC.getBonusInfo(eBonus).getYieldChange(eYield);
						aiYield[eYield] += iBonusYieldChange;
						iBasePlotYield += iBonusYieldChange;

						aiYield[eYield] = std::max(aiYield[eYield], GC.getYieldInfo(eYield).getMinCity());
					}

					if (eBonusImprovement != NO_IMPROVEMENT)
					{
						iBasePlotYield += GC.getImprovementInfo(eBonusImprovement).getImprovementBonusYield(eBonus, eYield);

						if (iBasePlotYield > aiYield[eYield])
						{
							aiYield[eYield] -= 2 * (iBasePlotYield - aiYield[eYield]);
						}
						else
						{
							aiYield[eYield] += aiYield[eYield] - iBasePlotYield;
						}
					}

					os << __LINE__ << ": home plot yield = " << aiYield[eYield] << '\n';
				}
			}

			if (iI == CITY_HOME_PLOT)
			{
				iTempValue += aiYield[YIELD_FOOD] * 60;
				iTempValue += aiYield[YIELD_PRODUCTION] * 60;
				iTempValue += aiYield[YIELD_COMMERCE] * 40;
			}
			else if (aiYield[YIELD_FOOD] >= GC.getFOOD_CONSUMPTION_PER_POPULATION())
			{
				iTempValue += aiYield[YIELD_FOOD] * 40;
				iTempValue += aiYield[YIELD_PRODUCTION] * 40;
				iTempValue += aiYield[YIELD_COMMERCE] * 30;
			}
			else if (aiYield[YIELD_FOOD] == GC.getFOOD_CONSUMPTION_PER_POPULATION() - 1)
			{
				iTempValue += aiYield[YIELD_FOOD] * 25;
				iTempValue += aiYield[YIELD_PRODUCTION] * 25;
				iTempValue += aiYield[YIELD_COMMERCE] * 20;
			}
			else
			{
				iTempValue += aiYield[YIELD_FOOD] * 15;
				iTempValue += aiYield[YIELD_PRODUCTION] * 15;
				iTempValue += aiYield[YIELD_COMMERCE] * 10;
			}

			os << __LINE__ << ": iTempValue after yield scaling = " << iTempValue << '\n';

			if (pLoopPlot->isWater())
			{
				if (aiYield[YIELD_COMMERCE] > 1)
				{
					iTempValue += bIsCoastal ? 30 : -20;
					if (bIsCoastal && (aiYield[YIELD_FOOD] >= GC.getFOOD_CONSUMPTION_PER_POPULATION()))
					{
						iSpecialFoodPlus += 1;
					}
				}
			}

			os << __LINE__ << ": iTempValue after water commerce = " << iTempValue << ", iSpecialFoodPlus = " << iSpecialFoodPlus << '\n';

			if (pLoopPlot->isRiver())
			{
				iTempValue += 10;
			}

			if (iI == CITY_HOME_PLOT)
			{
				iTempValue *= 2;
			}
			else if ((pLoopPlot->getOwnerINLINE() == player.getID()) || (stepDistance(iX, iY, pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE()) == 1))
			{
				iTempValue *= 3;
				iTempValue /= 2;
			}
			else
			{
				iTempValue *= iGreed;
				iTempValue /= 100;
			}

			os << __LINE__ << ": iTempValue = " << iTempValue << '\n';

			iTempValue *= iCultureMultiplier;
			iTempValue /= 100;

			iValue += iTempValue;

			os << __LINE__ << ": iValue = " << iValue << '\n';

			if (iCultureMultiplier > 33) //ignore hopelessly entrenched tiles.
			{
				if (eFeature != NO_FEATURE)
				{
					if (iI != CITY_HOME_PLOT)
					{
						iHealth += GC.getFeatureInfo(eFeature).getHealthPercent();

						iSpecialFoodPlus += std::max(0, aiYield[YIELD_FOOD] - GC.getFOOD_CONSUMPTION_PER_POPULATION());

						os << __LINE__ << ": iHealth = " << iHealth << ", iSpecialFoodPlus = " << iSpecialFoodPlus << '\n';
					}
				}

				if ((eBonus != NO_BONUS) && ((pLoopPlot->area() == pPlot->area()) ||
					(pLoopPlot->area()->getCitiesPerPlayer(player.getID()) > 0)))
				{
					paiBonusCount[eBonus]++;
					FAssert(paiBonusCount[eBonus] > 0);

					os << __LINE__ << ": Bonus valuation\n";
					os << __LINE__ << ": paiBonusCount = " << paiBonusCount[eBonus] << '\n';

					iTempValue = (player.AI_bonusVal(eBonus) * (((player.getNumTradeableBonuses(eBonus) == 0) && (paiBonusCount[eBonus] == 1)) ? 80 : 20));
					iTempValue *= iGreed;
					iTempValue /= 100;

					os << __LINE__ << ": iTempValue = " << iTempValue << '\n';

					if (iI != CITY_HOME_PLOT)
					{
						if ((pLoopPlot->getOwnerINLINE() != player.getID()) && stepDistance(pPlot->getX_INLINE(), pPlot->getY_INLINE(), pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE()) > 1)
						{
							iTempValue *= 2;
							iTempValue /= 3;

							iTempValue *= std::min(150, iGreed);
							iTempValue /= 100;
						}
					}

					os << __LINE__ << ": iTempValue = " << iTempValue << '\n';

					iValue += (iTempValue + 10);

					os << __LINE__ << ": iValue = " << iValue << '\n';

					if (iI != CITY_HOME_PLOT)
					{
						if (eBonusImprovement != NO_IMPROVEMENT)
						{
							int iSpecialFoodTemp;
							iSpecialFoodTemp = pLoopPlot->calculateBestNatureYield(YIELD_FOOD, player.getTeam()) + GC.getImprovementInfo(eBonusImprovement).getImprovementBonusYield(eBonus, YIELD_FOOD);

							iSpecialFood += iSpecialFoodTemp;

							iSpecialFoodTemp -= GC.getFOOD_CONSUMPTION_PER_POPULATION();

							iSpecialFoodPlus += std::max(0, iSpecialFoodTemp);
							iSpecialFoodMinus -= std::min(0, iSpecialFoodTemp);
							iSpecialProduction += pLoopPlot->calculateBestNatureYield(YIELD_PRODUCTION, player.getTeam()) + GC.getImprovementInfo(eBonusImprovement).getImprovementBonusYield(eBonus, YIELD_PRODUCTION);
							iSpecialCommerce += pLoopPlot->calculateBestNatureYield(YIELD_COMMERCE, player.getTeam()) + GC.getImprovementInfo(eBonusImprovement).getImprovementBonusYield(eBonus, YIELD_COMMERCE);

							os << __LINE__ << ": iSpecialFoodTemp = " << iSpecialFoodTemp << '\n';
							os << __LINE__ << ": iSpecialFood = " << iSpecialFood << '\n';
							os << __LINE__ << ": iSpecialFoodPlus = " << iSpecialFoodPlus << '\n';
							os << __LINE__ << ": iSpecialFoodMinus = " << iSpecialFoodMinus << '\n';
							os << __LINE__ << ": iSpecialProduction = " << iSpecialProduction << '\n';
							os << __LINE__ << ": iSpecialCommerce = " << iSpecialCommerce << '\n';
						}

						if (eFeature != NO_FEATURE)
						{
							if (GC.getFeatureInfo(eFeature).getYieldChange(YIELD_FOOD) < 0)
							{
								iResourceValue -= 30;
								os << __LINE__ << ": iResourceValue = " << iResourceValue << '\n';
							}
						}

						if (pLoopPlot->isWater())
						{
							iValue += (bIsCoastal ? 100 : -800);
							os << __LINE__ << ": Water iValue = " << iValue << '\n';
						}
					}
				}
			}

			os << __LINE__ << ": Final city plot yield valuation iValue = " << iValue << '\n';
		}
	}

	iResourceValue += iSpecialFood * 50;
	iResourceValue += iSpecialProduction * 50;
	iResourceValue += iSpecialCommerce * 50;

	iValue += std::max(0, iResourceValue);

	os << __LINE__ << ": iValue = " << iValue << '\n';
	os << __LINE__ << ": iResourceValue = " << iResourceValue << '\n';

	if (iTakenTiles > (NUM_CITY_PLOTS / 3) && iResourceValue < 250)
	{
		os << __LINE__ << ": EXIT iTakenTiles > (NUM_CITY_PLOTS / 3) && iResourceValue < 250\n";
		return 0;
	}

	if (iTeammateTakenTiles > 1)
	{
		os << __LINE__ << ": EXIT iTeammateTakenTiles > 1\n";
		return 0;
	}

	iValue += (iHealth / 5);

	os << __LINE__ << ": iValue = " << iValue << '\n';

	if (bIsCoastal)
	{
		if (pArea->getCitiesPerPlayer(player.getID()) == 0)
		{
			if (bNeutralTerritory)
			{
				iValue += (iResourceValue > 0) ? 800 : 100;
				os << __LINE__ << ": bIsCoastal unsettled bNeutralTerritory iValue = " << iValue << '\n';
			}
		}
		else
		{
			iValue += 400;
			os << __LINE__ << ": bIsCoastal iValue = " << iValue << '\n';
		}
	}

	if (pPlot->isHills())
	{
		iValue += 200;
		os << __LINE__ << ": isHills iValue = " << iValue << '\n';
	}

	if (pPlot->isRiver())
	{
		iValue += 40;
		os << __LINE__ << ": isRiver iValue = " << iValue << '\n';
	}

	if (pPlot->isFreshWater())
	{
		iValue += 40;
		iValue += (GC.getDefineINT("FRESH_WATER_HEALTH_CHANGE") * 30);
		os << __LINE__ << ": isFreshWater iValue = " << iValue << '\n';
	}

	const CvCity* pNearestCity = GC.getMapINLINE().findCity(iX, iY, ((player.isBarbarian()) ? NO_PLAYER : player.getID()));

	if (pNearestCity != nullptr)
	{
		if (player.isBarbarian())
		{
			const int dist = plotDistance(iX, iY, pNearestCity->getX_INLINE(), pNearestCity->getY_INLINE());
			iValue -= (std::max(0, (8 - dist)) * 200);
			os << __LINE__ << ": dist = " << dist << ", iValue = " << iValue << '\n';
		}
		else
		{
			int iDistance = plotDistance(iX, iY, pNearestCity->getX_INLINE(), pNearestCity->getY_INLINE());
			// fortsnek: AI_foundValue_dependency global
			int iNumCities = player.getNumCities();
			if (iDistance > 5)
			{
				iValue -= (iDistance - 5) * 500;
			}
			else if (iDistance < 4)
			{
				iValue -= (4 - iDistance) * 2000;
			}

			// fortsnek: This is a function that tends towards 1.0: iValue *= [0.0..1.0].
			//           Except when distance is very low.
			//           This function also exponentially decays towards zero as distance increases.
			iValue *= (8 + iNumCities * 4);
			iValue /= (2 + (iNumCities * 4) + iDistance);

			os << __LINE__ << ": dist = " << iDistance << ", iNumCities = " << iNumCities << ", iValue = " << iValue << '\n';

			if (pNearestCity->isCapital())
			{
				iValue *= 150;
				iValue /= 100;
				os << __LINE__ << ": Nearest is capital. iValue = " << iValue << '\n';
			}
			else if (player.getCapitalCity() != nullptr)
			{
				CvCity* pLoopCity;
				int iLoop;
				int iMaxDistanceFromCapital = 0;

				int iCapitalX = player.getCapitalCity()->getX();
				int iCapitalY = player.getCapitalCity()->getY();

				// fortsnek: AI_foundValue_dependency global
				for (pLoopCity = player.firstCity(&iLoop); pLoopCity != nullptr; pLoopCity = player.nextCity(&iLoop))
				{
					iMaxDistanceFromCapital = std::max(iMaxDistanceFromCapital, plotDistance(iCapitalX, iCapitalY, pLoopCity->getX_INLINE(), pLoopCity->getY_INLINE()));
				}

				// fortsnek: At iDistance < iMaxDistanceFromCapital, this is a lerp from 1.5 to 1.
				//           Else, this does nothing.
				FAssert(iMaxDistanceFromCapital > 0);
				iValue *= 100 + (((bAdvancedStart ? 80 : 50) * std::max(0, (iMaxDistanceFromCapital - iDistance))) / iMaxDistanceFromCapital);
				iValue /= 100;

				os << __LINE__ << ": Player has capital. iMaxDistanceFromCapital = " << iMaxDistanceFromCapital
					<< ", iValue = " << iValue << '\n';
			}
		}
	}
	else
	{
		pNearestCity = GC.getMapINLINE().findCity(iX, iY, ((player.isBarbarian()) ? NO_PLAYER : player.getID()), ((player.isBarbarian()) ? NO_TEAM : player.getTeam()), false);
		if (pNearestCity != nullptr)
		{
			int iDistance = plotDistance(iX, iY, pNearestCity->getX_INLINE(), pNearestCity->getY_INLINE());
			iValue -= std::min(500 * iDistance, (8000 * iDistance) / GC.getMapINLINE().maxPlotDistance());
			os << __LINE__ << ": Nearest city on other area. dist = " << iDistance << ", iValue = " << iValue << '\n';
		}
		else
			os << __LINE__ << ": No nearest city anywhere.\n";
	}

	if (iValue <= 0)
	{
		os << __LINE__ << ": EXIT iValue <= 0\n";
		return 1;
	}

	if (pArea->getNumCities() == 0)
	{
		os << __LINE__ << ": Team city counts scaling case empty area.\n";
		iValue *= 2;
	}
	else
	{
		const int iTeamAreaCities = GET_TEAM(player.getTeam()).countNumCitiesByArea(pArea);

		if (pArea->getNumCities() == iTeamAreaCities)
		{
			os << __LINE__ << ": Team city counts scaling case 0.\n";
			iValue *= 3;
			iValue /= 2;
		}
		else if (pArea->getNumCities() == (iTeamAreaCities + GET_TEAM(BARBARIAN_TEAM).countNumCitiesByArea(pArea)))
		{
			os << __LINE__ << ": Team city counts scaling case 1.\n";
			iValue *= 4;
			iValue /= 3;
		}
		else if (iTeamAreaCities > 0)
		{
			os << __LINE__ << ": Team city counts scaling case 2.\n";
			iValue *= 5;
			iValue /= 4;
		}
		else
			os << __LINE__ << ": Team city counts scaling case 3.\n";
	}

	os << __LINE__ << ": Team city counts scaling. iValue = " << iValue << '\n';

	{
		int iFoodSurplus = std::max(0, iSpecialFoodPlus - iSpecialFoodMinus);
		int iFoodDeficit = std::max(0, iSpecialFoodMinus - iSpecialFoodPlus);

		iValue *= 100 + 20 * std::max(0, std::min(iFoodSurplus, 2 * GC.getFOOD_CONSUMPTION_PER_POPULATION()));
		iValue /= 100 + 20 * std::max(0, iFoodDeficit);
	}

	os << __LINE__ << ": Food scaling. iValue = " << iValue << '\n';

	if (player.getNumCities() > 0)
	{
		int iBonusCount = 0;
		int iUniqueBonusCount = 0;
		for (int iI = 0; iI < GC.getNumBonusInfos(); iI++)
		{
			iBonusCount += paiBonusCount[iI];
			iUniqueBonusCount += (paiBonusCount[iI] > 0) ? 1 : 0;
		}
		os << __LINE__ << ": iBonusCount = " << iBonusCount << ", iUniqueBonusCount = " << iUniqueBonusCount << '\n';
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

	os << __LINE__ << ": Bonus scaling. iValue = " << iValue << '\n';

	{
		int iDeadLockCount = player.AI_countDeadlockedBonuses(pPlot);
		if (bAdvancedStart && (iDeadLockCount > 0))
		{
			iDeadLockCount += 2;
		}
		iValue /= (1 + iDeadLockCount);
		os << __LINE__ << ": iDeadLockCount = " << iDeadLockCount << ", iValue = " << iValue << '\n';
	}

	iValue /= (std::max(0, (iBadTile - (NUM_CITY_PLOTS / 4))) + 3);

	os << __LINE__ << ": Final iValue = " << std::max(1, iValue) << std::endl;

	return std::max(1, iValue);
}

#endif