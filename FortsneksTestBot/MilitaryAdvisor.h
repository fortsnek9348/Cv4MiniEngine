#pragma once

#include "Pathing.h"

#include <PlayerBotGameBinding/GameStructs.h>
#include <PlayerBotGameBinding/IGame.h>

namespace mybot
{
	struct MilitaryAdvisor
	{
		std::vector<cvbot::EUnitType> barbUnitsSeen;
		std::vector<cvbot::EUnitType> rivalUnitsSeen;
		bool barbsHaveEnteredTerritory = false;
		bool canBarbsEnterTerritory = false;

		// Use them for scouting, extra defence, barb capturing, conquering.
		std::vector<cvbot::EUnitId> spareMilitaryUnits;

		//struct CityDefenceStatus
		//{
		//	cvbot::i16vec2 cityCoord{};
		//	// This measures how much defence strength is expected to be needed, including defence promotions.
		//	// If <= 1, only minimal defence is needed. Almost certainly will never be attacked.
		//	int defenceStrengthDemand = 0;
		//	// To attack roaming pillagers.
		//	int roamingBarbPillagersActiveDefenceDemand = 0;
		//};
		//
		//std::vector<CityDefenceStatus> cityDefenceStatuses;

		// Including active pillaging defence.
		int cityDefenceDemand{};
		int antiBarbDemand{};

		void update(
			std::span<const cvbot::Unit> myUnits,
			std::span<const cvbot::Unit> enemyUnits,
			std::optional<ivec2> futureSettlerEscortTarget,
			std::span<const cvbot::City> myCities,
			cvbot::MapGeometry mapGeom,
			cvbot::Span2D<const cvbot::Plot> plots,
			const mybot::DynamicArray2D<mybot::MultipleSourceDistanceFieldCell>& cityPathLengthAnalysis,
			std::span<const cvbot::TechState> techs,
			const cvbot::GlobalInfoData& infos,
			//const cvbot::GameSetup& setup,
			const cvbot::GlobalInfo& globalInfo,
			cvbot::IGame& game
		);
	};
}