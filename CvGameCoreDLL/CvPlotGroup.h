#pragma once

#include "CvStructs.h"
#include "CvEnums.h"
#include "LinkedList.h"


class CvPlot;

class CvPlotGroup
{
public:
	CvPlotGroup();
	virtual ~CvPlotGroup();

	void reset(int iID = 0, PlayerTypes eOwner = NO_PLAYER, bool bConstructorCall = false);
	void uninit();

	void init(int iID, PlayerTypes eOwner, CvPlot* pPlot);

	void addPlot(CvPlot* pPlot);
	void removePlot(CvPlot* pPlot);
	void recalculatePlots();

	int getID() const;
	void setID(int iID);

	PlayerTypes getOwner() const;
#ifdef _USRDLL
	inline PlayerTypes getOwnerINLINE() const
	{
		return m_eOwner;
	}
#endif
	int getNumBonuses(BonusTypes eBonus) const;
	bool hasBonus(BonusTypes eBonus);										
	void changeNumBonuses(BonusTypes eBonus, int iChange);

	void insertAtEndPlots(XYCoords xy);
	int getLengthPlots();
	CLLNode<XYCoords>* deletePlotsNode(CLLNode<XYCoords>* pNode);
	CLLNode<XYCoords>* nextPlotsNode(CLLNode<XYCoords>* pNode);
	CLLNode<XYCoords>* headPlotsNode();

	// for serialization
	void read(FDataStreamBase* pStream);

	void write(FDataStreamBase* pStream);

protected:

	int m_iID;

	PlayerTypes m_eOwner;

	int* m_paiNumBonuses;

	CLinkList<XYCoords> m_plots;
};
