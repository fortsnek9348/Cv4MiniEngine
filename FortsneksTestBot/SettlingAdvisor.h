#pragma once

#include "Pathing.h"

#include <PlayerBotGameBinding/IGame.h>

namespace mybot
{
	struct SettlingAdvisor
	{
		std::optional<ivec2> optTarget;

		void update(
			MapGeometry geom,
			const DynamicArray2D<Plot>& map,
			const DynamicArray2D<PathingPlot>& pathingMap,
			std::span<const ivec2> myCityCoords,
			const DynamicArray2D<MultipleSourceDistanceFieldCell>& stepDistanceAnalysis,
			const IGame& game
		);
		int getSettlerDemand() const;
	};
}