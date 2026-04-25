#include "ResearchAdvisor.h"

#include <PlayerBotGameBinding/EnumDefs.h>
#include <PlayerBotGameBinding/Infos.h>
#include <PlayerBotGameBinding/EnumStrings.h>

#include <CommonStuff/div.h>
#include <CommonStuff/range.h>

#include <algorithm>
#include <ranges>
#include <fstream>
#include <sstream>
#include <numbers>
#include <functional>

using namespace mybot;

namespace
{
	constexpr int kOrderEnumerationDepth = 4;
	constexpr int kEvaluationAdditionalTurnsAtNormalSpeed = 25; // Will be scaled.

	constexpr auto kTechIndices = heck::range<ETech>(ETech::Num);

	struct ResearchTreePreprocessed
	{
		const GlobalInfoData* globalInfoData{};
		const GlobalInfo* globalInfo{};
		std::span<const TechInfo> infos;
		std::vector<std::vector<ETech>> dependencies;

		explicit ResearchTreePreprocessed(const GlobalInfoData* globalInfoData, const GlobalInfo* globalInfo)
			: globalInfoData(globalInfoData), globalInfo(globalInfo), infos(globalInfoData->techs), dependencies(infos.size())
		{
			for (const auto i : heck::range<ETech>(infos.size()))
			{
				for (const auto t : infos[i].prereqAndTechs)
					dependencies[t].push_back(i);
				for (const auto t : infos[i].prereqOrTechs)
					dependencies[t].push_back(i);
			}
		}
	};

	// CvPlayer::pushResearch
	std::bitset<ETech::Num> getResearchListForTech(const GlobalInfoData& infos, const CivState& civState, ETech tech)
	{
		struct Visitor
		{
			const GlobalInfoData& infos;
			const CivState& civState;
			std::bitset<ETech::Num> bits;

			explicit Visitor(const GlobalInfoData& infos, const CivState& civState) : infos(infos), civState(civState)
			{
				for (const int i : kTechIndices)
					bits[i] = civState.techs[i].isResearched;
			}

			//int accCost(ETech tech, std::bitset<ETech::Num>& accBits) const
			//{
			//	if (accBits[tech])
			//		return 0;
			//
			//	int result = 0;
			//	for (const auto prereq : infos.techs[tech].prereqAndTechs)
			//		result += accCost(prereq, accBits);
			//
			//	ETech bestChoice = ETech::None;
			//	int bestCost = 0;
			//	for (const auto prereq : infos.techs[tech].prereqOrTechs)
			//	{
			//		std::bitset<ETech::Num> prereqAccBits = accBits;
			//		const int branchCost = accCost(prereq, prereqAccBits);
			//		if (branchCost < bestCost || bestChoice == ETech::None)
			//		{
			//			bestChoice = prereq;
			//			bestCost = branchCost;
			//		}
			//	}
			//	if (bestChoice != ETech::None)
			//		result += accCost(bestChoice, accBits);
			//	result += civState.techs[tech].cost - civState.techs[tech].progress;
			//	return result;
			//}

			int operator()(ETech tech)
			{
				if (bits[tech])
					return 0;

				int cost = 0;
				for (const auto prereq : infos.techs[tech].prereqAndTechs)
					cost += (*this)(prereq);

				ETech bestChoice = ETech::None;
				int bestCost = INT_MAX;
				for (const auto prereq : infos.techs[tech].prereqOrTechs)
				{
					const std::bitset<ETech::Num> saved = bits;
					const int branchCost = (*this)(prereq);
					if (branchCost < bestCost)
					{
						bestChoice = prereq;
						bestCost = branchCost;
					}
					bits = saved;
				}
				if (bestChoice != ETech::None)
					cost += (*this)(bestChoice);
				cost += civState.techs[tech].cost - civState.techs[tech].progress;
				bits[tech] = true;
				return cost;
			}
		};

		Visitor visitor(infos, civState);
		visitor(tech);
		return visitor.bits;
	}


	

	struct ResearchChoicesState
	{
		const ResearchTreePreprocessed* preprocessedTree{};
		std::bitset<ETech::Num> hasTech;
		std::vector<ETech> choices;

		explicit ResearchChoicesState(const ResearchTreePreprocessed* preprocessedTree, std::span<const TechState> initialState)
			: preprocessedTree(preprocessedTree)
		{
			for (const auto i : kTechIndices)
				hasTech[i] = initialState[i].isResearched;

			for (const auto i : kTechIndices)
			{
				const bool canResearch = calculateCanResearch(i);
				if (canResearch != initialState[i].canResearch)
					throw BotFailure("Internal research implementation got tech choices wrong.");
				if (canResearch)
					choices.push_back(i);
			}
		}

		void set(ETech t, bool has)
		{
			if (hasTech[t] == has)
				return;

			hasTech[t] = has;
			if (has)
				std::erase(choices, t);
			else
				choices.push_back(t);

			for (const ETech dependent : preprocessedTree->dependencies[t])
			{
				if (calculateCanResearch(dependent))
				{
					if (!std::ranges::contains(choices, dependent))
						choices.push_back(dependent);
				}
				else
					std::erase(choices, dependent);
			}
		}

		void generateOrders(
			std::vector<ETech>& order,
			std::vector<ETech>& localChoicesScratch,
			size_t maxDepth,
			std::vector<std::vector<ETech>>& output)
		{
			if (choices.empty() || order.size() >= maxDepth)
			{
				output.push_back(order);
				return;
			}

			const size_t localChoicesScratchBase = localChoicesScratch.size();
			localChoicesScratch.append_range(choices);
			for (const ETech t : std::span(localChoicesScratch).subspan(localChoicesScratchBase))
			{
				order.push_back(t);
				set(t, true);
				generateOrders(order, localChoicesScratch, maxDepth, output);
				set(t, false);
				order.pop_back();
			}
			//std::ranges::sort(localChoices);
			//std::vector<ETech> localChoices2 = choices;
			//std::ranges::sort(localChoices2);
			//assert(localChoices == localChoices2);

			localChoicesScratch.erase(localChoicesScratch.begin() + localChoicesScratchBase, localChoicesScratch.end());
		}

	private:
		// CvPlayer::canResearch, typical case
		bool calculateCanResearch(ETech t) const
		{
			if (hasTech[t])
				return false;

			bool bFoundValid = false;

			const TechInfo& info = preprocessedTree->infos[t];

			for (const ETech prereq : info.prereqOrTechs)
			{
				if (hasTech[prereq])
				{
					bFoundValid = true;
					break;
				}
			}

			if (!info.prereqOrTechs.empty() && !bFoundValid)
				return false;

			for (const ETech prereq : info.prereqAndTechs)
				if (!hasTech[prereq])
					return false;
			
			return true;
		}
	};

	std::vector<std::vector<ETech>> generateResearchOrders(const ResearchTreePreprocessed& preprocessedTree, std::span<const TechState> initialState, size_t depth)
	{
		ResearchChoicesState state(&preprocessedTree, initialState);
		std::vector<ETech> order;
		std::vector<ETech> localChoicesScratch;
		std::vector<std::vector<ETech>> orders;
		order.reserve(depth);
		localChoicesScratch.reserve(200);
		orders.reserve(100);
		state.generateOrders(order, localChoicesScratch, depth, orders);
		return orders;
	}

	// CvPlayer::calculateResearchModifier
	int calculateResearchModifier(const GlobalInfoData& infos, int numKnownByAliveTeamsMet, int numPrereqOrsResearched, int numRivalsAlive)
	{
		int iModifier = 100;

		if (numRivalsAlive > 0)
			iModifier += (infos.techCostTotalKnownTeamModifier * numKnownByAliveTeamsMet) / numRivalsAlive;

		iModifier += numPrereqOrsResearched * infos.techCostKnownPrereqModifier;

		return iModifier;
	}

	// NOTE: The isHasMet check in CvPlayer::calculateResearchModifier includes ourselves.
	int calculateResearchRate(const GlobalInfoData& infos, int sliderOutput, int numPrereqsKnown, int numKnownByAliveTeamsMet, int numRivalsAlive)
	{
		const int modifier = calculateResearchModifier(infos, numKnownByAliveTeamsMet, numPrereqsKnown, numRivalsAlive);
		const int researchRate = (sliderOutput + infos.baseResearchRate) * modifier / 100;
		return researchRate;
	}

	int getNumPrereqOrsKnown(std::span<const TechState> initialTechStates, ETech tech, const GlobalInfoData& infos)
	{
		int n{};
		for (const auto prereq : infos.techs[tech].prereqOrTechs)
			n += initialTechStates[prereq].isResearched;
		return n;
	}

	// This is needed because research rates for techs vary.
	struct TechResearchRateEstimation
	{
		int remaining{}; // >= 0
		int firstTurnOverflow{};
		int numKnownByRivals{};
	};
	struct ResearchRateEstimation
	{
		int sliderOutputWithBaseResearchRate{};
		int baseOverflow{};
		std::vector<TechResearchRateEstimation> techs{};

		explicit ResearchRateEstimation(const GlobalInfoData& infos, int breakevenSliderResearch, int currentResearchSliderOutput, std::span<const TechState> initialTechStates,
			const std::array<Interval, ETech::Num>& techHasCountIntervals, int numAliveCivTeams)
			: sliderOutputWithBaseResearchRate(breakevenSliderResearch + infos.baseResearchRate)
		{
			// Perform research rate estimations.

			techs.resize(ETech::Num);

			//const bool isZeroOverflow = prevTech != ETech::None && prevTech == civState.optCurrentResearch;

			for (const auto i : heck::range(ETech::Num))
			{
				if (!initialTechStates[i].isResearched)
				{
					int numKnownByTeams = techHasCountIntervals[i].center();

					if (initialTechStates[i].canResearch)
					{
						const int numPrereqOrsResearched = getNumPrereqOrsKnown(initialTechStates, i, infos);

						// We could guess by reversing the math, but trial and error sounds better.
						//const int overflow = isZeroOverflow ? 0 : initialTechStates[i].overflow;
						//const std::optional<Interval> optNumKnownByTeamsUsingOverflow = TechState::guessNumKnownByMetRivals(
						//	initialTechStates[i].overflow + initialTechStates[i].researchRate,
						//	infos,
						//	currentSliderOutput,
						//	initialTechStates[i].overflow,
						//	numPrereqOrsResearched,
						//	numAliveTeams
						//);

						// If the researchj rate is not truncated, use it to refine numKnownByTeams.
						if (!initialTechStates[i].isResearchRateTruncated)
						{
							const int expectedResearchRate = initialTechStates[i].researchRate;

							// Refine approximation by trial and error.
							int calculatedResearchRate = calculateResearchRate(infos, currentResearchSliderOutput, numPrereqOrsResearched, numKnownByTeams, numAliveCivTeams);
							for (;;)
							{
								if (numKnownByTeams > techHasCountIntervals[i].min)
								{
									const int trialResult = calculateResearchRate(infos, currentResearchSliderOutput, numPrereqOrsResearched, numKnownByTeams - 1, numAliveCivTeams);
									// When equal, bias towards lesser values.
									if (std::abs(trialResult - expectedResearchRate) <= std::abs(calculatedResearchRate - expectedResearchRate))
									{
										--numKnownByTeams;
										calculatedResearchRate = trialResult;
										continue;
									}
								}

								if (numKnownByTeams < techHasCountIntervals[i].imax)
								{
									const int trialResult = calculateResearchRate(infos, currentResearchSliderOutput, numPrereqOrsResearched, numKnownByTeams + 1, numAliveCivTeams);
									if (std::abs(trialResult - expectedResearchRate) < std::abs(calculatedResearchRate - expectedResearchRate))
									{
										++numKnownByTeams;
										calculatedResearchRate = trialResult;
										continue;
									}
								}

								break;
							}
						}


					}

					techs[i] = {
						.remaining = initialTechStates[i].cost - initialTechStates[i].progress,
						.firstTurnOverflow = initialTechStates[i].overflow,
						.numKnownByRivals = numKnownByTeams,
					};
				}
			}

			// Take a guess at baseOverflow.
			// If there are non-truncated overflow values, calculate baseOverflow for all of them and take the most constrained value.
			// Otherwise, calculate baseOverflow from the maximum overflow value.
			bool foundNonTruncatedOverflow = false;
			Interval nontruncatedBaseOverflowInterval{ INT_MIN, INT_MAX };
			int maxTruncatedBaseOverflow{};
			for (const ETech i : kTechIndices)
			{
				const TechState state = initialTechStates[i];
				if (state.canResearch)
				{
					const Interval numKnownByTeams = techHasCountIntervals[i];

					int numPrereqOrsResearched{};
					for (const auto prereq : infos.techs[i].prereqOrTechs)
						numPrereqOrsResearched += initialTechStates[prereq].isResearched;

					const std::optional<Interval> optBaseOverflowInterval = TechState::guessBaseOverflow(state.overflow, infos, currentResearchSliderOutput, numPrereqOrsResearched, numKnownByTeams, numAliveCivTeams);

					if (optBaseOverflowInterval)
					{
						if (!state.isOverflowTruncated)
						{
							foundNonTruncatedOverflow = true;
							// Doesn't matter if the interval becomes empty.
							nontruncatedBaseOverflowInterval.min = std::max(nontruncatedBaseOverflowInterval.min, optBaseOverflowInterval->min);
							nontruncatedBaseOverflowInterval.imax = std::min(nontruncatedBaseOverflowInterval.imax, optBaseOverflowInterval->imax);
						}
						else
						{
							maxTruncatedBaseOverflow = std::max(maxTruncatedBaseOverflow, optBaseOverflowInterval->min);
						}
					}
				}
			}

			if (foundNonTruncatedOverflow)
				baseOverflow = nontruncatedBaseOverflowInterval.center();
			else
				baseOverflow = maxTruncatedBaseOverflow;
		}
	};

	

	struct ResearchSim
	{
		ResearchChoicesState choicesState;
		int turn = 0;
		int baseOverflow{}; // Before modifier.

		struct LogEntry
		{
			int finishTurn{}; // The turn that we got the tech.
			ETech tech{};
		};
		std::vector<LogEntry> log;

		explicit ResearchSim(ResearchChoicesState choicesState, int baseOverflow)
			: choicesState(std::move(choicesState))
			, baseOverflow(baseOverflow)
		{
			log.reserve(20);
		}

		

		void research(ETech tech, const ResearchRateEstimation& researchRateEstimation)
		{
			if (tech == ETech::None)
				return;

			assert(std::ranges::contains(choicesState.choices, tech));
			std::erase(choicesState.choices, tech);

			const auto [numTurns, baseOverflow2] = getTechFinishResult(tech, researchRateEstimation);

			turn += numTurns;
			baseOverflow = baseOverflow2;

			log.push_back({
				.finishTurn = turn,
				.tech = tech
			});

			choicesState.set(tech, true);
		}

		ETech pick(int endTurn, std::span<const int> techValues, const ResearchRateEstimation& researchRateEstimation) const
		{
			if (choicesState.choices.empty())
				return ETech::None;

			return std::ranges::max(choicesState.choices, std::less(), [&](ETech tech) {
				const int numTurns = getTechFinishResult(tech, researchRateEstimation).numTurns;
				if (turn + numTurns < endTurn)
					return (endTurn - (turn + numTurns)) * techValues[tech];
				else
					return techValues[tech] / 10;
				});
		}

		int evaluate(int endTurn, std::span<const int> techValues) const
		{
			int sum = 0;
			for (const LogEntry& entry : log)
				if (entry.finishTurn < endTurn)
					sum += (endTurn - entry.finishTurn) * techValues[entry.tech];
			return sum;
		}

		ETech getFirstTech() const
		{
			return log.front().tech;
		}

	private:
		struct TechFinishResult
		{
			int numTurns{};
			int baseOverflow{};
		};

		TechFinishResult getTechFinishResult(ETech tech, const ResearchRateEstimation& researchRateEstimation) const
		{
			int numPrereqOrsResearched{};
			for (const auto prereq : choicesState.preprocessedTree->infos[tech].prereqOrTechs)
				numPrereqOrsResearched += choicesState.hasTech[prereq];

			const TechResearchRateEstimation rateInfo = researchRateEstimation.techs[tech];

			const int modifier = calculateResearchModifier(*choicesState.preprocessedTree->globalInfoData, rateInfo.numKnownByRivals, numPrereqOrsResearched,
				choicesState.preprocessedTree->globalInfo->numAliveCivTeams);

			// If this is the first turn, use the overflow value from CivState.
			const int overflow = turn == 0 ? rateInfo.firstTurnOverflow : baseOverflow * modifier / 100;
			// CvPlayer::calculateResearchRate
			const int rate = researchRateEstimation.sliderOutputWithBaseResearchRate * modifier / 100;
			const int firstTurnBeakers = overflow + rate;

			// firstTurnBeakers + rate * N >= remaining
			// N >= (remaining - firstTurnBeakers) / rate

			const int numTurns = 1 + cdiv(std::max(0, rateInfo.remaining - firstTurnBeakers), static_cast<unsigned int>(rate));

			const int beakerOverflow = firstTurnBeakers + rate * (numTurns - 1) - rateInfo.remaining;

			// CvTeam::setResearchProgress, conversion of overflow
			return { .numTurns = numTurns, .baseOverflow = beakerOverflow * 100 / modifier };
		}
	};

	struct EvalInput
	{
		const CivState& civState{};
		const ELeaderhead leaderhead{};
		std::array<int, EBonus::Num> bonusCounts{};
		bool hasLandOil{};
		bool hasOceanOil{}; // Plastics
		// Remember to not use counts raw because their importance should be relative to number of cities.
		int numForests{};
		int numJungles{};
		int numRiversidePlots{};
		int numCoastalCities{};
		int numCottagablePlots{};
		int numCottagablePlotsUnderForest{};
		int numVillagesOrTowns{};
		int numFarms{};
		int barbThreatTurn{};
		int numCityReligions{};
		int numCitiesWithStateReligion{};
		int numCitiesOnRivalBorder{};
		int numCities{};
		int totalEmancipationUnhappiness{};
		int totalSignedHealth{};
		int totalSignedHappy{};
		int totalMaintainenceCents{};
		int researchStartTurn{}; // For when evaluating in the future.
		int researchRate{};
		std::bitset<ETech::Num> techsGone{}; // True iff /anybody/ has researched the tech and we know about it.
		std::bitset<EBuildingClass::Num> wondersGone{};
		std::bitset<EProject::Num> projectsGone{};
		const GlobalInfoData& infos{};
		//const IGame& game;
	};

	int calculateTechValue(const EvalInput& input, ETech tech)
	{
		using enum EBonus;
		const int turnsNeeded = ((input.civState.techs[tech].cost - input.civState.techs[tech].progress) + input.researchRate - 1) / input.researchRate;
		const int finishTurn = input.researchStartTurn + turnsNeeded;
		switch (tech)
		{
			using enum ETech;

			// Techs by column.

		case Fishing: return 100 + (input.bonusCounts[Clam] * 2 + input.bonusCounts[Crab] * 2 + input.bonusCounts[Fish] * 3) * 100 / input.numCities;
		case Agriculture: return 100 + (input.bonusCounts[Corn] * 2 + input.bonusCounts[Rice] + input.bonusCounts[Wheat] * 2) * 100 / input.numCities;
		case Hunting: return 100 + (input.bonusCounts[Deer] * 2 + input.bonusCounts[Fur] * 3 + input.bonusCounts[Ivory] * 3) * 100 / input.numCities;
			// happy bonus from monument, more than one city and we don't have creative, rival border tension
		case Mysticism: return std::ranges::contains(input.infos.leaders[input.leaderhead].traits, ELeaderTrait::Charismatic) * input.numCities * 100
			+ (input.numCities > 1 && !std::ranges::contains(input.infos.leaders[input.leaderhead].traits, ELeaderTrait::Creative) ? (input.numCities - 1) * 100 + input.numCitiesOnRivalBorder * 50 : 0)
			;
		case Mining: return 100;

		case Sailing: return input.numCoastalCities * 300 / input.numCities;
		case Pottery: return (input.numCottagablePlots + input.techsGone[BronzeWorking] * input.numCottagablePlotsUnderForest) * 50 / input.numCities;
		case AnimalHusbandry: return 100 + (input.bonusCounts[Cow] * 1 + input.bonusCounts[Pig] * 3 + input.bonusCounts[Sheep] * 2) * 100 / input.numCities;
		case Archery:
		{
			int urgency = std::clamp((finishTurn - input.barbThreatTurn) * 100, 0, 1000); // Using absolute turn values, but maybe that's better for barbs anyway.
			if ((input.bonusCounts[Copper] || input.bonusCounts[Iron]) && input.techsGone[BronzeWorking])
				urgency /= 10;
			return 50 + urgency;
		}

		case Meditation: return input.numCityReligions * 50 / input.numCities;
		case Polytheism: return 30;
		case Masonry: return (input.bonusCounts[Stone] + input.bonusCounts[Marble]) * 200 / input.numCities;

		case HorsebackRiding: return 30; // We don't plan on going to war for now...
		case Priesthood: return input.numCityReligions * 70 / input.numCities + !input.wondersGone[EBuildingClass::Oracle] * 200;
		case Monotheism: return input.numCitiesWithStateReligion * 50 / input.numCities;
		case BronzeWorking:
		{
			int urgency = std::clamp((finishTurn - input.barbThreatTurn) * 100, 0, 1000);
			if (input.techsGone[Archery])
				urgency /= 5;
			return 50 + input.numForests * 20 / input.numCities + urgency;
		}

		case Writing: return 300;
		case MetalCasting: return 200;
		case IronWorking: return (input.bonusCounts[Copper] ? 50 : 200) + input.numJungles * 20 / input.numCities;

		case Aesthetics: return 100;
		case Mathematics: return 100 + input.numForests * 40 / input.numCities;
		case Alphabet: return 100;
		case Monarchy: return cdiv(-input.totalSignedHappy * 200, static_cast<unsigned int>(input.numCities)) + 100 + (input.bonusCounts[Wine] ? 100 : 0) + (input.bonusCounts[Wine] * 50 / input.numCities);
		case Compass: return 50 + input.numCoastalCities * 50 / input.numCities;

		case Literature: return 100;
		case Calendar: return (input.bonusCounts[Banana] * 2 + input.bonusCounts[Dye] * 3 + input.bonusCounts[Incense] * 3 + input.bonusCounts[Silk] * 2 + input.bonusCounts[Spices] * 2 + input.bonusCounts[Sugar] * 2) * 100 / input.numCities;
		case Construction: return 200;
		case Currency: return 250;
		case Machinery: return 150;

		case Drama: return input.numCitiesOnRivalBorder * 100 / input.numCities;
		case Engineering: return 100;
		case CodeOfLaws: return 200 + input.totalMaintainenceCents / (3 * input.numCities);
		case Feudalism: return 100;
		case Optics: return 50;

		case Music: return input.techsGone[tech] ? 50 : 200;
		case Philosophy: return 200;
		case CivilService: return 300;
		case Theology: return 50;

		case DivineRight: return 0;
		case Paper: return 200;
		case Guilds: return 100;

		case Nationalism: return 100;// +(!wondersGone[TajMahal] && !isLiberalismGone) * 100;
		case PrintingPress: return input.numVillagesOrTowns * 50 / input.numCities;
		case Education: return 300;
		case Banking: return 200;

		case Constitution: return 200;
		case MilitaryTradition: return 100;
		case ReplaceableParts: return 150;
		case Liberalism: return input.techsGone[tech] ? 100 : 1000;
		case Economics: return input.techsGone[tech] ? 100 : 500;
		case Gunpowder: return 50;

		case Democracy: return input.totalEmancipationUnhappiness * 50 / input.numCities;
		case Rifling: return 500;
		case Astronomy: return 100;
		case Corporation: return 100;
		case Chemistry: return 200;

		case SteamPower: return 50 + input.numRiversidePlots * 30;
		case ScientificMethod: return 40;
		case MilitaryScience: return 30;
		case Steel: return 200;

		case AssemblyLine: return 450;
		case Utopia: return input.techsGone[tech] ? 50 : 400;
		case Physics: return input.techsGone[tech] ? 200 : 500;
		case Biology: return input.numFarms * 50 / input.numCities;
		case Railroad: return 300;

		case Flight: return 300;
		case Artillery: return 300;
		case Fascism: return input.techsGone[tech] ? 300 : 400;
		case Electricity: return 200;
		case Medicine: return (-input.totalSignedHealth * 200 / input.numCities) + 200;
		case Combustion: return input.bonusCounts[Oil] ? 400 : 50;

		case Rocketry: return 10'000; // Let's go for space victory.
		case Industrialism: return 300;
		case Fission: return 50;
		case Radio: return (!input.wondersGone[EBuildingClass::EiffelTower] * 2 + !input.wondersGone[EBuildingClass::Rocknroll] * 2 + !input.wondersGone[EBuildingClass::CristoRedentor]) * 200;
		case Refrigeration: return (-input.totalSignedHealth * 200 / input.numCities) + 200;

		case Satellites: return 700; // Let's go for space victory.
		case Plastics: return 100 + !input.wondersGone[EBuildingClass::GreatDam] * 100 + (!input.hasLandOil && input.hasOceanOil) * 200;
		case MassMedia: return 100 + !input.wondersGone[EBuildingClass::Hollywood] * 200;
		case Computers: return 100 + !input.projectsGone[EProject::TheInternet] * 100;

		case AdvancedFlight: return 100;
		case Laser: return 200;
		case Composites: return 1000; // Let's go for space victory.
		case Ecology: return (-input.totalSignedHealth * 200 / input.numCities) + 200;
		case Superconductors: return 1000; // Let's go for space victory.

		case Stealth: return 100;
		case FiberOptics: return 700; // Let's go for space victory.
		case Robotics: return 100;
		case Genetics: return (-input.totalSignedHealth * 200 / input.numCities) + 700; // Let's go for space victory.

		case Fusion: return 100 + !input.techsGone[tech] * 300 + 700; // Let's go for space victory.
		case FutureTech: return 100;

		default:
			return 100;
		}
	}

	

}

ETech mybot::ResearchAdvisor::update(
	const CivState& civState,
	std::span<const std::optional<Player>> revealedPlayers,
	Span2D<const Plot> plots,
	std::span<const City> myCities,
	int barbThreatTurn,
	int rivalPowerRatioPercent,
	const GlobalInfo& globalInfo,
	const GlobalInfoData& infos,
	[[maybe_unused]] const IGame& game
)
{
	const auto activePlayerI = civState.activePlayerI;
	const auto activeTeamI = civState.activeTeamI;

	const int currentResearchSliderOutput = civState.techStatesReferenceSliderOutput;

	updateAllRivalStates(activeTeamI, civState, infos, revealedPlayers);
	updateBreakevenSliderResearch(civState, myCities);

	numAliveTeams = globalInfo.numAliveCivTeams;

	const std::span<const TechState> initialTechStates = civState.techs;

	// Gather up inputs to tech valuation
	std::array<int, EBonus::Num> revealedBonuses{};
	bool hasLandOil = false;
	bool hasOceanOil = false;
	int numForests{};
	int numJungles{};
	int numRiversidePlots{};
	int numCottagablePlots{};
	int numCottagablePlotsUnderForest{};
	int numVillagesOrTowns{};
	int numFarms{};
	for (int y = 0; y < plots.dim.y; ++y)
	{
		for (int x = 0; x < plots.dim.x; ++x)
		{
			const Plot& plot = plots[{ x, y }];
			if (plot.owner == activePlayerI)
			{
				if (const EBonus bonus = plot.bonus; bonus != EBonus::None)
				{
					++revealedBonuses[bonus];
					if (bonus == EBonus::Oil)
						(plot.type == EPlotType::Ocean ? hasOceanOil : hasLandOil) = true;
				}

				numForests += plot.feature == EFeature::Forest;
				numJungles += plot.feature == EFeature::Jungle;
				numRiversidePlots += plot.isRiverside;
				numCottagablePlots += plot.type == EPlotType::Land && (plot.feature == EFeature::None || plot.feature == EFeature::FloodPlains);
				numCottagablePlotsUnderForest += plot.type == EPlotType::Land && plot.feature == EFeature::Forest;
				numVillagesOrTowns += plot.improvement == EImprovement::Village || plot.improvement == EImprovement::Town;
				numFarms += plot.improvement == EImprovement::Farm;
			}
		}
	}

	int numCoastalCities{};
	int numCityReligions{};
	int numCitiesWithStateReligion{};
	int totalEmancipationUnhappiness{};
	int totalSignedHappy{};
	int totalSignedHealth{};
	int totalMaintainenceCents{};
	for (const City& city : myCities)
	{
		numCoastalCities += city.isCoastal;
		numCityReligions += static_cast<int>(city.religions.size());
		if (civState.optStateReligion)
			numCitiesWithStateReligion += std::ranges::contains(city.religions, *civState.optStateReligion);
		totalEmancipationUnhappiness += city.optInspectableCityInfo->percentAngerContributions[City::InspectableCityInfo::Emancipation];
		totalSignedHappy += city.optInspectableCityInfo->happiness;
		totalSignedHealth += city.optInspectableCityInfo->healthiness;
		totalMaintainenceCents += city.optInspectableCityInfo->maintainenceCents;
	}


	// TODO: Determine actual value. Count the closest cities of each plot that has neighbouring rival plot.
	const int numCitiesOnRivalBorder = std::min(static_cast<int>(myCities.size()), static_cast<int>(std::lround(std::sqrt(myCities.size() / std::numbers::pi) * 4)));

	
	std::bitset<ETech::Num> techsGone{};
	for (const size_t i : heck::range(techsGone.size()))
		techsGone[i] = civState.techs[i].isResearched || civState.techs[i].hasMissedOutOnFirstToResearch;

	std::bitset<EBuildingClass::Num> wondersGone{};
	for (const auto& builtWonder : globalInfo.builtWonders)
		wondersGone[builtWonder.buildingClass] = true;

	std::bitset<EProject::Num> projectsGone{};
	for (const size_t i : heck::range(projectsGone.size()))
		projectsGone[i] = globalInfo.projects[i].numBuiltTotal > 0;

	

	EvalInput input{
		.civState = civState,
		.leaderhead = revealedPlayers[activePlayerI]->leader,
		.bonusCounts = revealedBonuses,
		.hasLandOil = hasLandOil,
		.hasOceanOil = hasOceanOil,
		.numForests = numForests,
		.numJungles = numJungles,
		.numRiversidePlots = numRiversidePlots,
		.numCoastalCities = numCoastalCities,
		.numCottagablePlots = numCottagablePlots,
		.numCottagablePlotsUnderForest = numCottagablePlotsUnderForest,
		.numVillagesOrTowns = numVillagesOrTowns,
		.numFarms = numFarms,
		.barbThreatTurn = barbThreatTurn,
		.numCityReligions = numCityReligions,
		.numCitiesWithStateReligion = numCitiesWithStateReligion,
		.numCitiesOnRivalBorder = numCitiesOnRivalBorder,
		.numCities = static_cast<int>(myCities.size()),
		.totalEmancipationUnhappiness = totalEmancipationUnhappiness,
		.totalSignedHealth = totalSignedHealth,
		.totalSignedHappy = totalSignedHappy,
		.totalMaintainenceCents = totalMaintainenceCents,
		.researchStartTurn = 0,
		.researchRate = breakevenSliderResearch, // Note that this is not entirely accurate as research rate depends on tech.
		.techsGone = techsGone,
		.wondersGone = wondersGone,
		.projectsGone = projectsGone,
		.infos = infos,
	};

	const auto victoryTechsSet = civState.techs[ETech::Rocketry].isResearched ?
		getResearchListForTech(infos, civState, ETech::Satellites) |
		getResearchListForTech(infos, civState, ETech::Composites) |
		getResearchListForTech(infos, civState, ETech::Superconductors) |
		getResearchListForTech(infos, civState, ETech::FiberOptics) |
		getResearchListForTech(infos, civState, ETech::Genetics) |
		getResearchListForTech(infos, civState, ETech::Fusion)
		:
		getResearchListForTech(infos, civState, ETech::Rocketry)
		;

	const std::vector<int> techValues = kTechIndices | std::views::transform([&](ETech t) {
		int value = calculateTechValue(input, t);
		if (rivalPowerRatioPercent > 80 && victoryTechsSet[t])
			value += 500;
		return value;
		}) | std::ranges::to<std::vector>();


	

	// Perform research rate estimations.
	const ResearchRateEstimation estimation(infos, breakevenSliderResearch, currentResearchSliderOutput, initialTechStates, techHasCountIntervals, numAliveTeams);

	// What we do here is generate all possible tech orderings up to depth N, pad to end turn with depth 1 choices, accumulate the per-turn tech values, then see which order is best.

	const ResearchTreePreprocessed preprocessedTree(&infos, &globalInfo);
	const std::vector<std::vector<ETech>> candidateOrders = generateResearchOrders(preprocessedTree, initialTechStates, kOrderEnumerationDepth);

	if (candidateOrders.empty())
		return ETech::None;

	const ResearchChoicesState initialChoicesState(&preprocessedTree, initialTechStates);

	std::vector<ResearchSim> sims;
	sims.reserve(candidateOrders.size());

	// Simulate the research
	for (const auto& order : candidateOrders)
	{
		ResearchSim& sim = sims.emplace_back(initialChoicesState, estimation.baseOverflow);
		for (const ETech tech : order)
			sim.research(tech, estimation);
	}

	const int candidateOrdersEndTurn = std::ranges::max(sims, std::less(), &ResearchSim::turn).turn;
	const int researchSimEndTurn = candidateOrdersEndTurn + kEvaluationAdditionalTurnsAtNormalSpeed * infos.speedInfo.researchPercent / 100;

	// Pad sims with depth-1 choices.
	for (ResearchSim& sim : sims)
		while (sim.turn < researchSimEndTurn && !sim.choicesState.choices.empty())
			sim.research(sim.pick(researchSimEndTurn, techValues, estimation), estimation);

	// Pick the best
	const size_t bestI = std::ranges::max_element(sims, std::less(), std::bind_back(&ResearchSim::evaluate, researchSimEndTurn, techValues)) - sims.begin();
	const ResearchSim& bestSim = sims[bestI];

	//struct SimInfo
	//{
	//	int value{};
	//	std::vector<ETech> techs;
	//};
	//
	//[[maybe_unused]] const auto simValues = sims | std::views::transform([&](const ResearchSim& sim) {
	//	return SimInfo{ sim.evaluate(researchSimEndTurn, techValues), sim.log | std::views::transform(&ResearchSim::LogEntry::tech) | std::ranges::to<std::vector>() };
	//	}) | std::ranges::to<std::vector>();

	//std::ofstream file("BotDump.txt");
	//const auto& allKnowing = game.getAllKnowingGameInterface();
	//for (const ETech tech : kTechIndices)
	//{
	//	file << kTechNames[tech];
	//	if (initialTechStates[tech].isResearched)
	//		file << " (isResearched)";
	//	if (initialTechStates[tech].canResearch)
	//		file << " (canResearch)";
	//	file << '\n';
	//	file << "\tfirstTurnOverflow " << estimation.techs[tech].firstTurnOverflow << '\n';
	//	file << "\tnumKnownByRivals  " << estimation.techs[tech].numKnownByRivals << " (true value: " << allKnowing.getNumAliveMetTeamsKnowTech(tech) << ")\n";
	//	file << "\tremaining         " << estimation.techs[tech].remaining << '\n';
	//	file << "\tProgress rate     " << initialTechStates[tech].researchRate << '\n';
	//	const int numPrereqOrsKnown = getNumPrereqOrsKnown(initialTechStates, tech, infos);
	//	file << "\tCalculated rate   " << calculateResearchRate(infos, currentResearchSliderOutput, numPrereqOrsKnown, estimation.techs[tech].numKnownByRivals, numAliveTeams) << '\n';
	//	file << "\tKnown has         " << techHasCountIntervals[tech].min << ".." << techHasCountIntervals[tech].imax << '\n';
	//	file << "\tKnown has not     " << techHasNotCountIntervals[tech].min << ".." << techHasNotCountIntervals[tech].imax << '\n';
	//	//file << '\n';
	//}
	//for (const auto& [i, sim] : sims | std::views::enumerate)
	//{
	//	if (size_t(i) == bestI)
	//		file << '*';
	//	file << sim.evaluate(researchSimEndTurn, techValues) << ": ";
	//	int sum{};
	//	int valuePerTurn = 0;
	//	int prevTurn = 0;
	//	for (const auto& entry : sim.log)
	//	{
	//		sum += valuePerTurn * std::max(0, std::min(entry.finishTurn, researchSimEndTurn) - prevTurn);
	//		file << entry.finishTurn << '(' << sum << "):" << kTechNames[entry.tech] << '(' << techValues[entry.tech] << "),";
	//		valuePerTurn += techValues[entry.tech];
	//		prevTurn = entry.finishTurn;
	//		//if (researchSimEndTurn > entry.finishTurn)
	//		//	sum += (researchSimEndTurn - entry.finishTurn) * techValues[entry.tech];
	//	}
	//	file << '\n';
	//}
	//file.close();

	return bestSim.getFirstTech();
}

void ResearchAdvisor::updateBreakevenSliderResearch(const CivState& civState, std::span<const City> myCities)
{
	// Research slider % at 0 gpt = gpt0 * 100 / (gpt0 - gpt100)
	// Research at 0 gpt = lerp(bpt0, bpt100, break-even % / 100)
	// You basically need to have the data to do CvCity::updateCommerce.

	int totalGPTAt0 = 0;
	int totalGPTAt100 = 0;
	int totalBPTAt0 = 0;
	int totalBPTAt100 = 0;

	for (const City& city : myCities)
	{
		const auto& info = *city.optInspectableCityInfo;

		std::array<int, ECommerce::Num> sliders{};

		const auto getCommerceFromPercent = [&](this auto&& self, ECommerce i, int yieldRate) -> int {
			if (i == ECommerce::Gold)
				return yieldRate - self(ECommerce::Research, yieldRate) - self(ECommerce::Culture, yieldRate) - self(ECommerce::Espionage, yieldRate);
			else
				return yieldRate * sliders[i] / 100;
			};

		const auto getBaseCommerceRateTimes100 = [&](ECommerce i) {
			int iBaseCommerceRate;
			iBaseCommerceRate = getCommerceFromPercent(i, info.yieldRates[EYield::Commerce] * 100);
			iBaseCommerceRate += info.baseCommerceRateTimes100WithZeroSlider;
			return iBaseCommerceRate;
			};

		const auto getCommerceRate = [&](ECommerce i) {
			// CvCity::updateCommerce
			int iNewCommerce = (getBaseCommerceRateTimes100(i) * info.commerceRateModifiers[i]) / 100;
			iNewCommerce += info.yieldRates[EYield::Production] * info.productionToCommerceModifiers[i];
			return iNewCommerce;
			};

		sliders[ECommerce::Gold] = 100;
		sliders[ECommerce::Research] = 0;
		totalGPTAt0 += getCommerceRate(ECommerce::Gold);
		totalBPTAt0 += getCommerceRate(ECommerce::Research);
		sliders[ECommerce::Gold] = 0;
		sliders[ECommerce::Research] = 100;
		totalGPTAt100 += getCommerceRate(ECommerce::Gold);
		totalBPTAt100 += getCommerceRate(ECommerce::Research);
	}

	totalGPTAt0 /= 100;
	totalGPTAt100 /= 100;
	totalBPTAt0 /= 100;
	totalBPTAt100 /= 100;

	const int baseGPT = civState.totalTradeGoldPerTurn - civState.inflatedCosts;
	totalGPTAt0 += baseGPT;
	totalGPTAt100 += baseGPT;

	//const int breakevenPercent = std::clamp((totalGPTAt0 * 100 + (totalGPTAt0 - totalGPTAt100) / 2) / (totalGPTAt0 - totalGPTAt100), 0, 100);
	//const int breakevenResearch = std::clamp(totalBPTAt0 + ((totalBPTAt100 - totalBPTAt0) * breakevenPercent + 100 / 2) / 100, 1, 10000);
	// Single division.
	breakevenSliderResearch = std::clamp(heck::rdiv(totalGPTAt0 * totalBPTAt100 - totalGPTAt100 * totalBPTAt0, static_cast<unsigned int>(std::max(1, totalGPTAt0 - totalGPTAt100))), 1, 100'000);

}

void ResearchAdvisor::updateAllRivalStates(ETeam activeTeamI, const CivState& civState, const GlobalInfoData& infos, std::span<const std::optional<Player>> revealedPlayers)
{
	rivalTeams.resize(std::ranges::max(revealedPlayers | std::views::transform([](const std::optional<Player>& player) {
		return player ? player->team + 1 : 0;
		})));

	// techsKnownHasNot needs to be refreshed.
	for (RivalTeamState& state : rivalTeams)
		state.techsKnownHasNot.reset();

	// Take into account "first to research" info.
	// Actually, don't do that here. rivalTeams[team].techs should only depend on trade info.
	for (const ETech j : kTechIndices)
	{
		if (!civState.techs[j].isResearched && infos.techs[j].hasFirstToResearchBonus)
		{
			if (!civState.techs[j].hasMissedOutOnFirstToResearch)
			{
				// No civ has it yet.
				for (RivalTeamState& state : rivalTeams)
					state.techsKnownHasNot[j] = true;
			}
		}
	}

	// If tech trading is not allowed, then they don't have alphabet.
	for (const auto& optPlayer : revealedPlayers)
		if (optPlayer && !optPlayer->tradeAdvisorData.isTechnologyTradingAllowed)
			rivalTeams[optPlayer->team].techsKnownHasNot[ETech::Alphabet] = true;
	
	for (RivalTeamState& state : rivalTeams)
		state.isAliveAndMet = false;

	// Make use of known free techs (CvGame::initFreeState)
	for (const auto& optPlayer : revealedPlayers)
	{
		if (optPlayer)
		{
			for (const ETech tech : infos.civs[optPlayer->civ].freeTechs)
				rivalTeams[optPlayer->team].techsKnownHas[tech] = true;
			for (const ETech tech : infos.handicap.freeTechs)
				rivalTeams[optPlayer->team].techsKnownHas[tech] = true;
			if (optPlayer->team != civState.activeTeamI)
				for (const ETech tech : infos.handicap.freeAITechs)
					rivalTeams[optPlayer->team].techsKnownHas[tech] = true;
		}
	}

	// Update tech states from trade data.
	for (const auto& optPlayer : revealedPlayers)
	{
		if (optPlayer)
		{
			const auto team = optPlayer->team;
			rivalTeams[team].isAliveAndMet = true;
			if (team == activeTeamI)
			{
				for (const size_t j : heck::range(ETech::Num))
				{
					rivalTeams[team].techsKnownHas[j] = civState.techs[j].isResearched;
					rivalTeams[team].techsKnownHasNot[j] = !civState.techs[j].isResearched;
				}
			}
			else
				updateRivalState(infos, rivalTeams[team], *optPlayer);
		}
	}

	// Bound tech known counts.
	std::array<int, ETech::Num> techCountsKnownHas{};
	std::array<int, ETech::Num> techCountsKnownHasNot{};
	std::array<int, ETech::Num> techCountsKnownUnknown{};
	for (const RivalTeamState& state : rivalTeams)
	{
		if (state.isAliveAndMet)
		{
			for (const size_t j : heck::range(ETech::Num))
			{
				techCountsKnownHas[j] += state.techsKnownHas[j];
				techCountsKnownHasNot[j] += state.techsKnownHasNot[j];
				techCountsKnownUnknown[j] += !state.techsKnownHas[j] && !state.techsKnownHasNot[j];
			}
		}
	}

	for (const size_t j : heck::range(ETech::Num))
	{
		// Take into account "first to research" info.
		if (!civState.techs[j].isResearched && infos.techs[j].hasFirstToResearchBonus)
		{
			if (civState.techs[j].hasMissedOutOnFirstToResearch)
			{
				// A rival civ has got the tech.
				if (techCountsKnownHas[j] == 0)
				{
					++techCountsKnownHas[j];
					--techCountsKnownUnknown[j];
				}
			}
			// `else` branch handled above.
		}

		techHasCountIntervals[j] = { techCountsKnownHas[j], techCountsKnownHas[j] + techCountsKnownUnknown[j] };
		techHasNotCountIntervals[j] = { techCountsKnownHasNot[j], techCountsKnownHasNot[j] + techCountsKnownUnknown[j] };
	}
}

void ResearchAdvisor::updateRivalState(const GlobalInfoData& infos, RivalTeamState& rivalState, const Player& player)
{
	struct Context
	{
		const GlobalInfoData& infos;
		RivalTeamState& state;

		bool markHas(ETech t)
		{
			if (state.techsKnownHas[t])
				return false;

			state.techsKnownHas[t] = true;
			state.techsKnownHasNot[t] = false;

			checkPrereqsKnownHas(t);

			return true;
		}

		bool markHasNot(ETech t)
		{
			if (state.techsKnownHasNot[t])
				return false;
			state.techsKnownHas[t] = false;
			state.techsKnownHasNot[t] = true;
			return true;
		}

		// If they don't have the prereqs for a tech, then we know they don't have it.
		bool checkHasNoPrereqs(ETech t)
		{
			if (state.techsKnownHas[t] || state.techsKnownHasNot[t])
				return false;

			for (const auto prereq : infos.techs[t].prereqAndTechs)
			{
				checkHasNoPrereqs(prereq);
				if (state.techsKnownHasNot[prereq])
					return markHasNot(t);
			}

			if (infos.techs[t].prereqOrTechs.empty())
				return false;

			for (const auto prereq : infos.techs[t].prereqOrTechs)
			{
				checkHasNoPrereqs(prereq);
				if (!state.techsKnownHasNot[prereq]) // Unknown or Has
					return false;
			}

			return markHasNot(t);
		}

		// If they do have the tech, then we know they have the prereqs.
		bool checkPrereqsKnownHas(ETech t)
		{
			if (!state.techsKnownHas[t]) // Unknown or HasNot
				return false;

			bool changed = false;

			for (const auto prereq : infos.techs[t].prereqAndTechs)
				changed |= markHas(prereq);

			size_t prereqOrsCertain = 0;
			size_t prereqOrsCertainHas = 0;
			for (const auto prereq : infos.techs[t].prereqOrTechs)
			{
				if (state.techsKnownHas[prereq] || state.techsKnownHasNot[prereq])
					++prereqOrsCertain;
				if (state.techsKnownHas[prereq])
					++prereqOrsCertainHas;
			}

			// If we know they don't have any of the prereq-or techs, except one, then we know they have that one.
			if (prereqOrsCertain + 1 == infos.techs[t].prereqOrTechs.size() && prereqOrsCertainHas == 0)
				for (const auto prereq : infos.techs[t].prereqOrTechs)
					if (!state.techsKnownHas[prereq] && !state.techsKnownHasNot[prereq])
						changed |= markHas(prereq);

			return changed;
		}
	};

	Context ctx{ infos, rivalState };

	for (const auto t : player.tradeAdvisorData.techsWants)
		ctx.markHasNot(t);
	for (const auto t : player.tradeAdvisorData.techsWantsButWeCantTrade)
		ctx.markHasNot(t);
	for (const auto t : player.tradeAdvisorData.techsCanResearch)
		ctx.markHasNot(t);

	for (const auto t : player.tradeAdvisorData.techsWillTrade)
		ctx.markHas(t);
	for (const auto t : player.tradeAdvisorData.techsWontTrade)
		ctx.markHas(t);
	for (const auto t : player.tradeAdvisorData.techsCantTrade)
		ctx.markHas(t);

	for (;;)
	{
		bool changed = false;
		for (const auto t : heck::range(ETech::Num))
		{
			changed |= ctx.checkPrereqsKnownHas(t);
			changed |= ctx.checkHasNoPrereqs(t);
		}
		if (!changed)
			break;
	}
}
