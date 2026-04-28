#include "inc/PlayerBotGameBinding/GameStructs.h"
#include "inc/PlayerBotGameBinding/Infos.h"

#include <climits>

using namespace cvbot;

// Avoiding dependency on CommonStuff.
// Returns ceil(x / y).
int cvbot::cdiv(int x, std::same_as<std::uint32_t> auto y)
{
	const int q = int(x / (int64_t)y);
	return q + (x % (int64_t)y > 0);
}

template int cvbot::cdiv(int, std::uint32_t);

// Which values when divided by `denom` will result in `b`?
// b = a / denom. a = ?
// Example:
// [4..6] = a / 3
// a = [4 * 3 .. 6 * 3 + 2] = [12..20]
static std::optional<Interval> invFloorDiv(std::optional<Interval> b, int denom)
{
	assert(denom > 0);
	if (!b)
		return b;
	else
		return Interval{ b->min * denom, b->imax * denom + (denom - 1) };
}

// Which values when multiplied by `factor` will result in `b`?
// b = a * factor. a = ?
// Example:
// [10, 20] = a * 3
// a = [ceil(10 / 3) .. floor(20 / 3)] = [4..6]
// [10, 10] = a * 3
// a = [4..3] = nullopt
static std::optional<Interval> invMul(std::optional<Interval> b, Interval factor)
{
	assert(factor.min >= 0);
	if (!b)
		return b;
	else if (factor.min <= 0)
		return Interval{ INT_MIN, INT_MAX };
	else
		return Interval{ cdiv(b->min, static_cast<unsigned int>(factor.imax)), b->imax / factor.min }.optional();
}

std::optional<Interval> TechState::guessBaseOverflow(int overflow, const GlobalInfoData& globalInfoData, int sliderOutput, int numPrereqsKnown, Interval numKnownByMetRivals, int numRivalsAlive)
{
	const int techCostTotalKnownTeamModifier = globalInfoData.techCostTotalKnownTeamModifier;
	const int techCostKnownPrereqModifier = globalInfoData.techCostKnownPrereqModifier;
	const int baseResearchRate = globalInfoData.baseResearchRate;

	// Always added on.
	sliderOutput += baseResearchRate;

	// int baseOverflow{};
	// 
	Interval modifier = 100;
	modifier += techCostTotalKnownTeamModifier * numKnownByMetRivals / numRivalsAlive;
	modifier += numPrereqsKnown * techCostKnownPrereqModifier;
	// 
	//const Interval iResearchRate = sliderOutput * modifier / 100;
	// int iOverflow = (baseOverflow * modifier) / 100;
	// 
	// finalRate = iResearchRate + iOverflow;

	// Now reverse all that to get baseOverflow.
	// You can plug this into a CAS and solve for baseOverflow, but that assumes the divisions don't round.

	// What we really want are lower and upper bounds due to rounding.
	// At least we know the exact value of `modifier` and 'iResearchRate', given inputs.

	// finalRate = iResearchRate + iOverflow
	// finalRate = iResearchRate + baseOverflow * modifier // 100
	// finalRate - iResearchRate = baseOverflow * modifier // 100

	std::optional<Interval> lhs = Interval{ overflow };
	//*lhs -= iResearchRate;
	lhs = invFloorDiv(lhs, 100);
	lhs = invMul(lhs, modifier);
	return lhs;
}



std::optional<Interval> TechState::guessNumKnownByMetRivals(int rate, const GlobalInfoData& globalInfoData, int sliderOutput, int numPrereqsKnown, int numRivalsAlive)
{
	const int techCostTotalKnownTeamModifier = globalInfoData.techCostTotalKnownTeamModifier;
	const int techCostKnownPrereqModifier = globalInfoData.techCostKnownPrereqModifier;
	const int baseResearchRate = globalInfoData.baseResearchRate;

	// Always added on.
	sliderOutput += baseResearchRate;

	// We let baseOverflow = 0.

	// int baseOverflow{};
	// 
	const int modifier = 100 + numPrereqsKnown * techCostKnownPrereqModifier;
	//modifier += techCostTotalKnownTeamModifier * numKnownByMetRivals / numRivalsAlive;
	// 
	//const int iResearchRate = sliderOutput * modifier / 100;
	//int iOverflow = 0;
	// 
	// finalRate = iResearchRate + iOverflow;

	// finalRate = sliderOutput * modifier // 100 + iOverflow
	// finalRate = sliderOutput * (baseModifier + techCostTotalKnownTeamModifier * numKnownByMetRivals // numRivalsAlive) // 100 + iOverflow
	//
	// finalRate - iOverflow = sliderOutput * (baseModifier + techCostTotalKnownTeamModifier * numKnownByMetRivals // numRivalsAlive) // 100
	// (finalRate - iOverflow) invFloorDiv 100 = sliderOutput * (baseModifier + techCostTotalKnownTeamModifier * numKnownByMetRivals // numRivalsAlive)
	// (finalRate - iOverflow) invFloorDiv 100 invMul sliderOutput = baseModifier + techCostTotalKnownTeamModifier * numKnownByMetRivals // numRivalsAlive
	// (finalRate - iOverflow) invFloorDiv 100 invMul sliderOutput - baseModifier = techCostTotalKnownTeamModifier * numKnownByMetRivals // numRivalsAlive
	// ((finalRate - iOverflow) invFloorDiv 100 invMul sliderOutput - baseModifier) invFloorDiv numRivalsAlive = techCostTotalKnownTeamModifier * numKnownByMetRivals
	// ((finalRate - iOverflow) invFloorDiv 100 invMul sliderOutput - baseModifier) invFloorDiv numRivalsAlive invMul techCostTotalKnownTeamModifier = numKnownByMetRivals

	//finalRate -= overflow;

	std::optional<Interval> lhs = Interval{ rate };
	lhs = invFloorDiv(lhs, 100);
	lhs = invMul(lhs, sliderOutput);
	if (lhs)
		*lhs -= modifier;
	lhs = invFloorDiv(lhs, numRivalsAlive);
	lhs = invMul(lhs, techCostTotalKnownTeamModifier);
	return lhs;
}