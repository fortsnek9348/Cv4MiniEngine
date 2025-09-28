#include "../CommonConfig.h"

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS

#include "VectorisedPathfinder.h"
#include "VectorisedPathfinderGraph.h"
#include "VectorisedPathfinderMap.h"
#include "TeamPathingPlotPropsCache.h"
#include "VectorisedPathfinderGenericPathReconstruction.h"

#include <CommonStuff/PQCoordSimdAStar.h>

#include <fstream>

using namespace GiganticMapsOptimisationsLib;

namespace
{
	struct CoordPlotwiseAStarVectorisedPathfinder : IVectorisedPathfinder
	{
		VectorisedPathfinderMap& map;
		std::optional<VectorisedPathfinderGraph> graph;
		heck::PQCoordSimdAStar astar{ CivMapGeometry(GC.getMapINLINE()).dim, GC.getMapINLINE().isWrapXINLINE(), GC.getMapINLINE().isWrapYINLINE() };
		ivec2 currentStartCoord{};
		ivec2 currentGoalCoord{};

		explicit CoordPlotwiseAStarVectorisedPathfinder(VectorisedPathfinderMap& map) : map(map)
		{

		}

		virtual void setConfig(const CvSelectionGroup* group, uint32_t flags) override
		{
			graph.emplace(VectorisedPathStepFunction(&map, group, flags, currentStartCoord), map.tryGetMultiLandmarkDistanceField(*group));
		}

		virtual i16aabb2 reset([[maybe_unused]] uint32_t visitRectPlotPropsMask) override
		{
			const i16aabb2 rect = astar.reset();
			return visitRectPlotPropsMask ? rect : i16aabb2();
		}

		virtual void start(ivec2 startCoord, uint32_t startState) override
		{
			if (graph)
				graph->setStartPlot(startCoord);
			currentStartCoord = startCoord;
			astar.addStart(startCoord, startState);
		}

		virtual void setHeuristicGoal(ivec2 goal) override
		{
			currentGoalCoord = goal;
			graph->setGoal(goal);
		}
		virtual void setHeuristicGoalTo3x3Around(ivec2 goal) override
		{
			currentGoalCoord = goal;
			graph->setGoalTo3x3Around(goal);
		}

		virtual void updateOpenSetHeuristicValues() override
		{
			astar.updateOpenSetHeuristicValues(*graph);
		}

		// Expand open set until goal has a G cost assigned or until F cost limit is reached.
		virtual bool search(ivec2 goal, int maxFCost, [[maybe_unused]] bool isDebugPathingRequest) override
		{
			return astar.search(goal, *graph, maxFCost);
		}


		virtual std::vector<i16vec2> reconstructPath(ivec2 start, ivec2 goal) const override
		{
			//const ivec2 dim = astar.getPitch();
			//return astar.reconstructPath(toPlotIndex(start, dim), toPlotIndex(goal, dim), *graph);

			

			std::vector<i16vec2> path = GiganticMapsOptimisationsLib::reconstructPath(start, goal, graph->getStepFunction(), [this](ivec2 toCoord) {
				return astar.getGCostsAndUnitStatesAroundPlot(toCoord);
				}, [this](ivec2 toCoord) {
					return astar.getGCost(toCoord);
					}, [this](ivec2 toCoord) {
						return astar.getGState(toCoord);
						});

					//if (path.size() > 300)
					//	astar.dumpStats(std::clog);

					return path;
		}

		virtual int32_t getGCostAt(ivec2 coord) const noexcept override
		{
			return astar.getGCost(coord);
		}

		virtual uint32_t getUnitStateAt(ivec2 coord) const noexcept override
		{
			return astar.getGState(coord);
		}

		virtual uint32_t getTeamCostPlotPropsReadMask() const noexcept override
		{
			return graph ? graph->getTeamCostPlotPropsReadMask() : 0;
			//return graph->getTeamPlotPropsReadMask();
		}

		virtual uint32_t getTeamValidityPlotPropsReadMask() const noexcept override
		{
			return graph ? graph->getTeamValidityPlotPropsReadMask() : 0;
			//return graph->getTeamPlotPropsReadMask();
		}

		// Thread-safe.
		virtual void liteVerifyAt(ivec2 coord, uint32_t state) const override
		{
			graph->verifyAt(coord, state);
		}

		virtual void debugDump() const override
		{
			{
				heck::Image img = renderMap(false);

				for (const int y : range(graph->getDim().y))
					for (const int x : range(graph->getDim().x))
						if (astar.getGCost({ x, y }) < astar.kUnreachableCost)
							img.drawPixel({ x, y }, { 255, 0, 255 }, 128);

				img.drawPixel(currentStartCoord, { 255, 255, 0 }, 128);
				img.drawPixel(currentGoalCoord, { 255, 255, 255 }, 128);

				img.saveAsPPM("CoordPlotwiseAStarVectorisedPathfinder debugDump visitation.ppm");
			}

			{
				// Dump full A* state to ~~CSV~~ ✨HTML✨.
				std::ofstream dump("CoordPlotwiseAStarVectorisedPathfinder debugDump.html");

				// https://www.jacobparis.com/content/html-optional-closing-elements

				dump << R"(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8" />
<style>
td { text-align: center; vertical-align: middle; }
.S { border: 1px solid green; }
.G { border: 1px solid red; }
</style>
</head>
<body>
<table>
)";

				dump << "<tr><th>";
				for (const int x : range(graph->getDim().x))
					dump << "<th>" << x;
				dump << '\n';

				for (const int y : range(graph->getDim().y))
				{
					dump << "<tr><th>" << y;

					for (const int x : range(graph->getDim().x))
					{
						std::string classes;
						if (ivec2(x, y) == currentStartCoord)
							classes += "S ";
						if (ivec2(x, y) == currentGoalCoord)
							classes += "G ";
						dump << "<td title='" << x << ',' << y << '\'';
						if (!classes.empty())
							dump << " class='" << classes << '\'';
						dump << '>';

						const int cost = astar.getGCost({ x, y });
						if (cost < astar.kUnreachableCost)
						{
							dump << cost;

							dump << "<br/>";

							const uint32_t state = astar.getGState({ x, y });
							const auto [mp, turns] = VectorisedPathStepFunction::splitMPTurns(state);
							dump << turns << ';' << mp;

							const int parentRelAdj = astar.getParentRelAdj({ x, y });
							if (parentRelAdj >= 0)
							{
								static constexpr std::array kInvAdjIndices{ 7, 6, 5, 4, 3, 2, 1, 0 };
								const int adjToParent = kInvAdjIndices[parentRelAdj];
								//dump << "\\|/--/|\\"[adjToParent];
								dump << ' ' << std::to_array<std::string_view>({ (const char*)u8"⇖", (const char*)u8"⇑", (const char*)u8"⇗", (const char*)u8"⇐", (const char*)u8"⇒", (const char*)u8"⇙", (const char*)u8"⇓", (const char*)u8"⇘" })[adjToParent];
							}
							// else, parent pointers not enabled
						}
					}
					dump << '\n';
				}

				dump << R"(</table>
</body>
</html>)";
			}
		}
	};
}

std::unique_ptr<IVectorisedPathfinder> GiganticMapsOptimisationsLib::createCoordPlotwiseAStarVectorisedPathfinder(VectorisedPathfinderMap& map)
{
	return std::make_unique<CoordPlotwiseAStarVectorisedPathfinder>(map);
}

#endif