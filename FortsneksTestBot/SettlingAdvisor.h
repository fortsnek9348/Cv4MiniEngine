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
			const mybot::DynamicArray2D<cvbot::Plot>& map,
			const mybot::DynamicArray2D<mybot::MultipleSourceDistanceFieldCell>& pathLengthAnalysis,
			const mybot::DynamicArray2D<mybot::MultipleSourceDistanceFieldCell>& stepDistanceAnalysis,
			const cvbot::IGame& game
		);
		int getSettlerDemand() const;
	};
}