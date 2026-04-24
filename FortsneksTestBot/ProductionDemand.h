#pragma once

#include "Common.h"

#include <PlayerBotGameBinding/GameStructs.h>

namespace mybot
{
	enum class EProductionUrgency
	{
		None,
		MilitaryPolice, // Just make something cheap for hereditary rule happiness.
		NonCombat, // We settling or need to improve some plots. Don't take time away from important production.
		Escort, // Need a decent defender to escort a settler. Produce in a city that has nothing better to do.
		WorldWonder, // We would very much like to get this building, as long as we don't have other problems.
		ShowOfForce, // Make sure we don't fall behind in Power too much.
		MilitaryBuildUp, // Prepping for war, but not time-critcal. Build in multiple cities. Deprioritise buildings, except barracks.
		UndefendedCity, // Get a cheap defender for this city. Produce in a city that has nothing better to do.
		TerritoryDefence, // Something's about to get pillaged. Cities not threatened, but get unit out asap.
		Survival, // Units needed to keep city. Get this unit out asap, ignore all other priorities. Use inefficient whips if necessary.

		Num,
	};

	struct ProductionDemand
	{
		ProductionChoice thing{};
		// Anything that can move has a target, even scouts and workers.
		i16vec2 target{};
		// Try to produce in N turns.
		uint16_t turns{};
		uint16_t count{};
		EProductionUrgency urgency{};
	};
}