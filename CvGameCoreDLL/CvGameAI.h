#pragma once

// gameAI.h

#ifndef CIV4_GAME_AI_H
#define CIV4_GAME_AI_H

#include "CvGame.h"

class CvGameAI : public CvGame
{

public:

	CvGameAI();
	virtual ~CvGameAI();

	void AI_init() override;
	void AI_uninit();
	void AI_reset() override;

	void AI_makeAssignWorkDirty() override;
	void AI_updateAssignWork() override;

	int AI_combatValue(UnitTypes eUnit) override;

	int AI_turnsPercent(int iTurns, int iPercent);

	virtual void read(FDataStreamBase* pStream) override;
	virtual void write(FDataStreamBase* pStream) override;

protected:

  int m_iPad;

};

#endif
