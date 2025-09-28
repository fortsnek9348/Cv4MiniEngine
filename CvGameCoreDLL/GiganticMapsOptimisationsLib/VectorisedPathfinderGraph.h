#pragma once

#include "VectorisedPathStep.h"
#include "VectorisedPathfinderMap.h"

namespace GiganticMapsOptimisationsLib
{
	class VectorisedPathfinderGraph : public heck::ISimdAStarGraph
	{
	public:
		
		explicit VectorisedPathfinderGraph(const VectorisedPathStepFunction& pathStepFunction, VectorisedPathfinderMap::LandmarkDistanceFieldWithBias heuristicInstance);

		const VectorisedPathStepFunction& getStepFunction() const;

		void setStartPlot(ivec2 coord);
		void setGoal(ivec2 destCoord);

		// For the final step of goal-dependent path costs pathing, we'll need to path to all the adjacent plots.
		// Sets the landmark values to the minimum of all adjacent plots around the goal plot.
		void setGoalTo3x3Around(ivec2 destCoord);

		uint32_t getTeamCostPlotPropsReadMask() const;
		uint32_t getTeamValidityPlotPropsReadMask() const;

		ivec2 getDim() const;

		void verifyAt(ivec2 coord, uint32_t state) const;

		//virtual Step HECK_VECTORCALL getStepCost(StateVector fromState, Vector fromPlotIndices, Vector toPlotIndices, Vector adjI) const override;
		virtual void HECK_VECTORCALL setStepFrom(StateVector fromState, Vector fromPlotIndices) override;
		virtual Step HECK_VECTORCALL getStepCostTo(Vector toPlotIndices, Vector adjI) const override;
		virtual Vector HECK_VECTORCALL getHeuristic(Vector fromPlotIndices) const override;
		virtual int32_t getSingleHeuristic(int plotI) const override;
	
	private:
		VectorisedPathStepFunction mPathStepFunction;
		VectorisedPathfinderMap::LandmarkDistanceFieldWithBias mHeuristic{};
		VectorisedPathfinderMap::MultiLandmarkDistanceVector mGoalLandmarkVector{};

		VectorisedPathStepFunction::StepFromInfo mStepFromInfo{};
	};
}