#include "../CommonConfig.h"

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS

#include "VectorisedPathfinderGraph.h"

using namespace GiganticMapsOptimisationsLib;

VectorisedPathfinderGraph::VectorisedPathfinderGraph(const VectorisedPathStepFunction& pathStepFunction, VectorisedPathfinderMap::LandmarkDistanceFieldWithBias heuristicInstance)
	: mPathStepFunction(pathStepFunction)
	, mHeuristic(std::move(heuristicInstance))
{
}

const VectorisedPathStepFunction& VectorisedPathfinderGraph::getStepFunction() const
{
	return mPathStepFunction;
}

void VectorisedPathfinderGraph::setStartPlot(ivec2 coord)
{
	mPathStepFunction.setStartPlot(coord);
}

void VectorisedPathfinderGraph::setGoal(ivec2 destCoord)
{
	// Heuristic bias due to change in MP during pathfinding.
	mGoalLandmarkVector = mHeuristic.distanceField ? (*mHeuristic.distanceField)[destCoord] - mPathStepFunction.getMaxMP() * PATH_MOVEMENT_WEIGHT - mHeuristic.bias : 0;
}

void VectorisedPathfinderGraph::setGoalTo3x3Around(ivec2 destCoord)
{
	if (mHeuristic.distanceField)
	{
		const CvMap& map = GC.getMapINLINE();
		const CivMapGeometry mapSizeInfo(map);
		const int maxMP = mPathStepFunction.getMaxMP();
		for (const ivec2 adjD : kAdj)
			if (const ivec2 coord = destCoord + adjD; mapSizeInfo.isValidCoord(coord))
				mGoalLandmarkVector = vmin(mGoalLandmarkVector, (*mHeuristic.distanceField)[mapSizeInfo.wrapCoord(coord)] - maxMP * PATH_MOVEMENT_WEIGHT - mHeuristic.bias);
	}
}

uint32_t VectorisedPathfinderGraph::getTeamCostPlotPropsReadMask() const
{
	return mPathStepFunction.getTeamCostPlotPropsReadMask();
}

uint32_t VectorisedPathfinderGraph::getTeamValidityPlotPropsReadMask() const
{
	return mPathStepFunction.getTeamValidityPlotPropsReadMask();
}

ivec2 VectorisedPathfinderGraph::getDim() const
{
	return mPathStepFunction.getDim();
}

void VectorisedPathfinderGraph::verifyAt(ivec2 coord, uint32_t state) const
{
	mPathStepFunction.verifyAt(coord, state);
}

//VectorisedPathfinderGraph::Step HECK_VECTORCALL VectorisedPathfinderGraph::getStepCost(StateVector fromState, Vector fromPlotIndices, Vector toPlotIndices, Vector adjI) const
//{
//	return mPathStepFunction.getStepCost(fromState, fromPlotIndices, toPlotIndices, adjI);
//}

void HECK_VECTORCALL VectorisedPathfinderGraph::setStepFrom(StateVector fromState, Vector fromPlotIndices)
{
	mStepFromInfo = mPathStepFunction.makeStepFromInfo(fromState, fromPlotIndices, std::move(mStepFromInfo));
}
VectorisedPathfinderGraph::Step HECK_VECTORCALL VectorisedPathfinderGraph::getStepCostTo(Vector toPlotIndices, Vector adjI) const
{
	return mPathStepFunction.getStepCost(mStepFromInfo, toPlotIndices, adjI);
}

VectorisedPathfinderGraph::Vector HECK_VECTORCALL VectorisedPathfinderGraph::getHeuristic([[maybe_unused]] Vector fromPlotIndices) const
{
	if (mHeuristic.distanceField)
	{
		const std::span cells = mHeuristic.distanceField->cells;
		alignas(VectorisedPathfinderGraph::Vector) std::array<int, kCoordVectorLength> landmarkRegs{};
		for (const int i : range(kCoordVectorLength))
			landmarkRegs[i] = hmax(mGoalLandmarkVector - cells[fromPlotIndices.getElement(i)]);
		return vmax(0, VectorisedPathfinderGraph::Vector(landmarkRegs));
	}
	else
	{
		return {};
	}
}

int32_t VectorisedPathfinderGraph::getSingleHeuristic(int plotI) const
{
	if (mHeuristic.distanceField)
	{
		const std::span cells = mHeuristic.distanceField->cells;
		return std::max(0, hmax(mGoalLandmarkVector - cells[plotI]));
	}
	else
	{
		return 0;
	}
}



#endif