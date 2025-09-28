#pragma once

#include "TeamPathingPlotPropsCache.h"

#include <CommonStuff/SimdVector.h>
#include <CommonStuff/SimdAStarGraph.h>

#include <functional>

class CvUnit;

namespace GiganticMapsOptimisationsLib
{
	class VectorisedPathfinderMap;

	class VectorisedPathStepFunction
	{
	public:
		static constexpr size_t kCoordVectorLength = heck::ISimdAStarGraph::kCoordVectorLength;
		using Vector = heck::ISimdAStarGraph::Vector;
		using StateVector = heck::ISimdAStarGraph::StateVector;

		static constexpr uint32_t kUnitStateTurnsShift = 12;
		static constexpr uint32_t kUnitStateMPMask = (1 << kUnitStateTurnsShift) - 1;

		static constexpr std::pair<int, int> splitMPTurns(uint32_t state)
		{
			return { int(state & kUnitStateMPMask), int(state >> kUnitStateTurnsShift) };
		}

		static constexpr uint32_t joinMPTurns(int mp, int turns)
		{
			return static_cast<uint32_t>(mp | (turns << kUnitStateTurnsShift));
		}

		static bool isSiegeUnit(const CvUnit& unit) noexcept;

		explicit VectorisedPathStepFunction(const VectorisedPathfinderMap* map, const CvSelectionGroup* group, uint32_t pathingFlags, ivec2 startCoord);

		// To avoid recomputing step eval config.
		void setStartPlot(ivec2 coord);

		uint32_t getTeamCostPlotPropsReadMask() const;
		uint32_t getTeamValidityPlotPropsReadMask() const;

		int getMaxMP() const;

		ivec2 getDim() const;

		heck::ISimdAStarGraph::Step HECK_VECTORCALL getStepCost(StateVector fromState, Vector fromPlotIndices, Vector toPlotIndices, Vector adjI, bool disableVerify = false) const;

		struct AnyUnitStepFromInfo
		{
			Vector fromPlotIndices;
			StateVector fromCostPlotProps{};
			StateVector fromValidityPlotProps{};
			Vector fromMP{};
			Vector fromTurns{};
		};

		struct PerUnitStepFromInfo
		{
			// This is unit-specific.
			Vector::Mask validityMask{};
		};

		struct StepFromInfo
		{
			AnyUnitStepFromInfo anyUnit{};
			std::vector<PerUnitStepFromInfo> perUnit{};
		};

		StepFromInfo HECK_VECTORCALL makeStepFromInfo(StateVector fromState, Vector fromPlotIndices, StepFromInfo&& scratch) const;

		heck::ISimdAStarGraph::Step HECK_VECTORCALL getStepCost(const StepFromInfo&, Vector toPlotIndices, Vector adjI, bool disableVerify = false) const;
		
		heck::ISimdAStarGraph::Step HECK_VECTORCALL getStepCostWithProps(
			const StepFromInfo& stepFromInfo, Vector toPlotIndices, Vector adjI, StateVector toCostPlotProps, bool disableVerify = false) const;

		//std::array<StateVector, kCoordVectorLength> gatherPlotProps(ivec2 plotCoord) const;

		void verifyAt(ivec2 coord, unsigned int state) const;
		
	private:
		const TeamPathingPlotPropsCache* mPlotProps = nullptr;
		const VectorisedPathfinderMap* mMap = nullptr; // For verification.
		const CvSelectionGroup* mGroup = nullptr; // For verification.
		ivec2 mStartCoord{}; // For verification.
		uint32_t mPathingFlags{};
		int mStartPlotI{};
		bool mIsWater = false;
		bool mUsePlotDanger = false;

		struct UnitEvalConfig
		{
			const CvUnit* unit = nullptr; // For verification.

			bool canFight = false;

			uint32_t defenceMask = 0;
			int extraMoveDiscount = 0;
			int maxMP = 0;
			TeamPathingPlotPropsCache::PlotProps halfCostMovementMask = 0;
			TeamPathingPlotPropsCache::PlotProps pathValidCanMoveIntoWithoutAttackFalseMask = 0;
			TeamPathingPlotPropsCache::PlotProps pathValidCanMoveIntoWithAttackFalseMask = 0;

			StateVector unusableRoutesMaskFrom{};
			StateVector unusableRoutesMaskTo{};
			Vector evaluatedRouteCosts{};
		};

		std::vector<UnitEvalConfig> mUnitEvalConfigs;

		Vector HECK_VECTORCALL getPlotMPCost(StateVector fromPlotProps, StateVector toPlotProps, Vector adjI, const UnitEvalConfig& config) const;
		//Vector::Mask HECK_VECTORCALL getPlotValidMask(Vector fromTurns, Vector fromMP, Vector fromPlotIndices, StateVector fromPlotProps, Vector adjI, const UnitEvalConfig& config) const;
		Vector::Mask HECK_VECTORCALL getPlotValidMask(const AnyUnitStepFromInfo& stepFromInfo, const PerUnitStepFromInfo& perUnitStepFromInfo, Vector adjI) const;
		Vector::Mask HECK_VECTORCALL getPlotDangerValidMask(const StepFromInfo& stepFromInfo) const;

		void verifyPathingStep(
			Vector fromTurns, Vector fromMP,
			Vector fromPlotIndices,
			Vector toPlotIndices, Vector adjI, Vector cost, Vector toTurns, Vector toMP) const;
	};
}