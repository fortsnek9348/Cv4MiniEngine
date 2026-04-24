#pragma once

#include "Common.h"

#include <PlayerBotGameBinding/GameStructs.h>

#include <map>
#include <span>
#include <vector>

namespace mybot
{
	struct MapUnitLookup
	{
		std::map<i16vec2, std::vector<const Unit*>> byCoord;

		explicit MapUnitLookup(std::span<const Unit> units)
		{
			for (const Unit& unit : units)
				byCoord[unit.coord].push_back(&unit);
		}

		std::span<const Unit* const> getUnitsAt(i16vec2 coord) const
		{
			if (const auto it = byCoord.find(coord); it != byCoord.end())
				return it->second;
			else
				return {};
		}
	};
}