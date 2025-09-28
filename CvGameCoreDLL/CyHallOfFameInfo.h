#pragma once

#include "CvHallOfFameInfo.h"

class CyReplayInfo;

class CyHallOfFameInfo
{
public:
	CyHallOfFameInfo();

	void loadReplays();
	int getNumGames() const;
	CyReplayInfo* getReplayInfo(int i);

private:
	CvHallOfFameInfo m_hallOfFame;
};
