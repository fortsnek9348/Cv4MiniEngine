#pragma once

#include <CvGameCoreDLL/CvGlobals.h>

#include <chrono>
#include <vector>

struct GameRunScope
{
	const int maxCivPlayers;
	const int startTurn;

	struct ExtraPlayerInfo
	{
		int turnLastSeenAlive = -1;
	};
	std::vector<ExtraPlayerInfo> extraPlayerInfos;

	using Clock = std::chrono::steady_clock;
	const Clock::time_point t0;

	GameRunScope();

	void update();

	void dumpSummary() const;
};