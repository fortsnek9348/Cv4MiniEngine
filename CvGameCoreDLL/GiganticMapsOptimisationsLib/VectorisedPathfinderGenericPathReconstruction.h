#pragma once

#include "VectorisedPathStep.h"

#include <vector>
#include <iostream>

namespace GiganticMapsOptimisationsLib
{
	// Generic implementation for path reconstruction as A* state is stored differently for plotwise and blockwise A*.
	// Step = getAdjPlotAStarState(ivec2 toCoord)
	inline std::vector<i16vec2> reconstructPath(ivec2 startCoord, ivec2 goalCoord, const VectorisedPathStepFunction& stepFunction,
		auto getAdjPlotAStarState, auto getPlotGCost, auto getPlotGState)
	{
		using namespace heck::SimdAStarConfig;

		static constexpr int32_t kUnreachableCost = INT32_MAX;

		static constexpr Vector kInvAdjIndices{ Vector::Array{ 7, 6, 5, 4, 3, 2, 1, 0 } };

		static constexpr heck::vec2<Vector> kAdjDeltaCoords{
			Vector(std::array{
				kAdj[0].x,
				kAdj[1].x,
				kAdj[2].x,
				kAdj[3].x,
				kAdj[4].x,
				kAdj[5].x,
				kAdj[6].x,
				kAdj[7].x,
			}, kAdj[7].x), // Fill with valid adj.
			Vector(std::array{
				kAdj[0].y,
				kAdj[1].y,
				kAdj[2].y,
				kAdj[3].y,
				kAdj[4].y,
				kAdj[5].y,
				kAdj[6].y,
				kAdj[7].y,
			}, kAdj[7].y),
		};

		std::vector<i16vec2> path;
		path.reserve(20);
		path.push_back(goalCoord);

		ivec2 toCoord = goalCoord;

		const CivMapGeometry mapSizeInfo(GC.getMapINLINE());
		const int pitch = mapSizeInfo.dim.x;

		while (toCoord != startCoord)
		{
			const Step fromAStarState = getAdjPlotAStarState(toCoord);

			// getPlotGCost does not wrap, for now.
			//for (int i = 0; i < 8; ++i)
			//	assert(fromAStarState.cost.getElement(i) == getPlotGCost(toCoord + kAdj[i]));

			const heck::vec2<Vector> fromPlotCoordsUnwrapped = toCoord + kAdjDeltaCoords;
			const Vector::Mask fromPlotCoordsIsValid = mapSizeInfo.isValidCoord(fromPlotCoordsUnwrapped);
			const heck::vec2<Vector> fromPlotCoordsWrapped = mapSizeInfo.wrapOrClampCoords(fromPlotCoordsUnwrapped);
			const Vector fromPlotIndices = fromPlotCoordsWrapped.x + fromPlotCoordsWrapped.y * pitch;

			const int toPlotI = toPlotIndex(toCoord, pitch);

			// For debug checking. Note that because the Civ4 cost function depends on unit state, this might not match up exactly.
			[[maybe_unused]] const int toPlotExpectedCost = getPlotGCost(toCoord);

			// Disabling verification. It's largely redundant.
			const Step step = stepFunction.getStepCost(fromAStarState.state, fromPlotIndices, toPlotI, kInvAdjIndices, true);

			const Vector::Mask mask = vcmplt(fromAStarState.cost, kUnreachableCost) & vcmplt(0, step.cost)
				& fromPlotCoordsIsValid & Vector::Mask(std::bitset<kCoordVectorLength>((1 << std::size(kAdj)) - 1));

			const Vector steppedToCost = vselect(mask, fromAStarState.cost + step.cost, kUnreachableCost);

			const int adjI = static_cast<int>(vcmpeq(steppedToCost, hmin(steppedToCost)).countr_zero());

			if (steppedToCost.getElement(adjI) < 0 || steppedToCost.getElement(adjI) >= kUnreachableCost)
			{
				// No path to this plot.
				//path.clear();
				//return path;

				// But there should be a path if this function was called.
				std::abort();
			}
			
			const ivec2 fromCoord = ivec2{
				fromPlotCoordsWrapped.x.getElement(adjI),
				fromPlotCoordsWrapped.y.getElement(adjI),
			};

			path.push_back(fromCoord);

			// Sanity check
			if (path.size() > 1'000'000)
			{
				// Pretty debug dump
				std::clog << "Infinite loop in path reconstruction!" << std::endl;

				// If you see unusual G costs in the grid, check that the vectorised pathfinder's cost array is being reset properly.

				std::clog << "Start = " << startCoord << std::endl;
				std::clog << "Goal  = " << goalCoord << std::endl;
				std::clog << "toCoord  = " << toCoord << std::endl;
				std::clog << "fromCoord  = " << fromCoord << std::endl;
				std::clog << "adjI = " << adjI << std::endl;
				std::clog << "fromCost = " << fromAStarState.cost.getElement(adjI) << std::endl;
				std::clog << "step cost to = " << step.cost.getElement(adjI) << std::endl;
				std::clog << "steppedToCost = " << steppedToCost.getElement(adjI) << std::endl;

				std::clog << "fromPlotExpectedCost = " << getPlotGCost(fromCoord) << std::endl;
				std::clog << "toPlotExpectedCost = " << toPlotExpectedCost << std::endl;

				const int invAdjI = kInvAdjIndices.getElement(adjI);
				const Step toAStarState = getAdjPlotAStarState(fromCoord);

				std::clog << "invAdjI = " << invAdjI << std::endl;
				std::clog << "toCost = " << toAStarState.cost.getElement(invAdjI) << std::endl;

				for (const int dy : range(-4, 4 + 1))
				{
					for (const int dx : range(-4, 4 + 1))
					{
						const int g = getPlotGCost(toCoord + ivec2(dx, dy));
						std::clog.width(10);
						if (g >= kUnreachableCost)
							std::clog << '-';
						else
							std::clog << g;
						std::clog.width(0);
						std::clog << ' ';
					}
					std::clog << std::endl;
				}

				for (const int dy : range(-4, 4 + 1))
				{
					for (const int dx : range(-4, 4 + 1))
					{
						const int g = getPlotGState(toCoord + ivec2(dx, dy));
						std::clog.width(10);
						if (g >= kUnreachableCost)
							std::clog << '-';
						else
							std::clog << g;
						std::clog.width(0);
						std::clog << ' ';
					}
					std::clog << std::endl;
				}

				//_CrtDbgBreak();
				std::abort();
			}

			toCoord = fromCoord;
		}

		std::ranges::reverse(path);

		return path;
	}
}