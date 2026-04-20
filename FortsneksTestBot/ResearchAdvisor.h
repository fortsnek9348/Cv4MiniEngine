#pragma once

#include <PlayerBotGameBinding/GameStructs.h>
#include <PlayerBotGameBinding/IGame.h>
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
			//std::array<EState, cvbot::ETech::Num> techs{};

			// Persistently valid. Once a civ has a tech, they always have it.
			std::bitset<cvbot::ETech::Num> techsKnownHas{};
			// Only valid for the current turn. A civ can always get a tech later.
			std::bitset<cvbot::ETech::Num> techsKnownHasNot{};
		};

		std::vector<RivalTeamState> rivalTeams; // [team]

		int numAliveTeams = 0;
		std::array<cvbot::Interval, cvbot::ETech::Num> techHasCountIntervals{};
		std::array<cvbot::Interval, cvbot::ETech::Num> techHasNotCountIntervals{};

		int breakevenSliderResearch{};

		// If we're researching the same tech as the previous turn, then overflow is guarenteed zero (and applied to the current tech), even during anarchy.
		//cvbot::ETech prevTech = cvbot::ETech::None;

		//cvbot::ETech prevTechDecision = cvbot::ETech::None;

		// NOTE: Teams are supported in RivalTeamState, but active player research rates do not support teams.
		// NOTE: Keep civState slider at max for more accurate data (unless bar truncation occurs).
		[[nodiscard]] cvbot::ETech update(
			const cvbot::CivState& civState,
			std::span<const std::optional<cvbot::Player>> revealedPlayers,
			cvbot::Span2D<const cvbot::Plot> plots,
			std::span<const cvbot::City> myCities,
			int barbThreatTurn,
			const cvbot::GlobalInfo& globalInfo,
			const cvbot::GlobalInfoData& infos,
			const cvbot::IGame& game
		);

	private:
		void updateBreakevenSliderResearch(const cvbot::CivState& civState, std::span<const cvbot::City> myCities);
		void updateAllRivalStates(cvbot::ETeam activeTeamI, const cvbot::CivState& civState, const cvbot::GlobalInfoData& infos, std::span<const std::optional<cvbot::Player>> revealedPlayers);
		void updateRivalState(const cvbot::GlobalInfoData& infos, RivalTeamState& rivalState, const cvbot::Player& player);
	};


	
}