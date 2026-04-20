#pragma once

#include "Common.h"

#include <PlayerBotGameBinding/GameStructs.h>
#include <PlayerBotGameBinding/EnumDefs.h>

//#include <optional>

namespace mybot
{
	struct ResearchAdvisor
	{
		struct RivalTeamState
		{
			//enum EState : uint8_t
			//{
			//	Unknown,
			//	HasNot,
			//	Has,
			//};

			
			bool isAliveAndMet{};
			//std::array<EState, ETech::Num> techs{};

			// Persistently valid. Once a civ has a tech, they always have it.
			std::bitset<ETech::Num> techsKnownHas{};
			// Only valid for the current turn. A civ can always get a tech later.
			std::bitset<ETech::Num> techsKnownHasNot{};
		};

		std::vector<RivalTeamState> rivalTeams; // [team]

		int numAliveTeams = 0;
		std::array<Interval, ETech::Num> techHasCountIntervals{};
		std::array<Interval, ETech::Num> techHasNotCountIntervals{};

		int breakevenSliderResearch{};

		// If we're researching the same tech as the previous turn, then overflow is guarenteed zero (and applied to the current tech), even during anarchy.
		//ETech prevTech = ETech::None;

		//ETech prevTechDecision = ETech::None;

		// NOTE: Teams are supported in RivalTeamState, but active player research rates do not support teams.
		// NOTE: Keep civState slider at max for more accurate data (unless bar truncation occurs).
		[[nodiscard]] ETech update(
			const CivState& civState,
			std::span<const std::optional<Player>> revealedPlayers,
			Span2D<const Plot> plots,
			std::span<const City> myCities,
			int barbThreatTurn,
			const GlobalInfo& globalInfo,
			const GlobalInfoData& infos,
			const IGame& game
		);

	private:
		void updateBreakevenSliderResearch(const CivState& civState, std::span<const City> myCities);
		void updateAllRivalStates(ETeam activeTeamI, const CivState& civState, const GlobalInfoData& infos, std::span<const std::optional<Player>> revealedPlayers);
		void updateRivalState(const GlobalInfoData& infos, RivalTeamState& rivalState, const Player& player);
	};


	
}