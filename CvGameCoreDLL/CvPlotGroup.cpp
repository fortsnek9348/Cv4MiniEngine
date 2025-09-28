// plotGroup.cpp

#include "CvGameCoreDLL.h"
#include "CvPlotGroup.h"
#include "CvPlot.h"
#include "CvGlobals.h"
#include "CvPlayerAI.h"
#include "CvMap.h"
#include "CvCity.h"
#include "CvDLLFAStarIFaceBase.h"
#include "FProfiler.h"

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS
#include "GiganticMapsOptimisationsLib/GameContext.h"
#endif

// Public Functions...

CvPlotGroup::CvPlotGroup()
{
	m_paiNumBonuses = nullptr;

	reset(0, NO_PLAYER, true);
}


CvPlotGroup::~CvPlotGroup()
{
	uninit();
}

void CvPlotGroup::uninit()
{
	SAFE_DELETE_ARRAY(m_paiNumBonuses);

	m_plots.clear();
}

// FUNCTION: reset()
// Initializes data members that are serialized.
void CvPlotGroup::reset(int iID, PlayerTypes eOwner, bool bConstructorCall)
{
	int iI;

	//--------------------------------
	// Uninit class
	uninit();

	m_iID = iID;
	m_eOwner = eOwner;

	if (!bConstructorCall)
	{
		FAssertMsg((0 < GC.getNumBonusInfos()), "GC.getNumBonusInfos() is not greater than zero but an array is being allocated in CvPlotGroup::reset");
		m_paiNumBonuses = new int[GC.getNumBonusInfos()];
		for (iI = 0; iI < GC.getNumBonusInfos(); iI++)
		{
			m_paiNumBonuses[iI] = 0;
		}
	}
}

void CvPlotGroup::init(int iID, PlayerTypes eOwner, CvPlot* pPlot)
{
	//--------------------------------
	// Init saved data
	reset(iID, eOwner);

	//--------------------------------
	// Init non-saved data

	//--------------------------------
	// Init other game data
	addPlot(pPlot);
}

void CvPlotGroup::addPlot(CvPlot* pPlot)
{
	XYCoords xy;

	xy.iX = pPlot->getX_INLINE();
	xy.iY = pPlot->getY_INLINE();

	insertAtEndPlots(xy);

	pPlot->setPlotGroup(getOwnerINLINE(), this);
}

void CvPlotGroup::removePlot(CvPlot* pPlot)
{
	CLLNode<XYCoords>* pPlotNode;
	pPlotNode = headPlotsNode();

	while (pPlotNode != nullptr)
	{
		if (GC.getMapINLINE().plotSorenINLINE(pPlotNode->m_data.iX, pPlotNode->m_data.iY) == pPlot)
		{
			pPlot->setPlotGroup(getOwnerINLINE(), nullptr);

			pPlotNode = deletePlotsNode(pPlotNode); // can delete this PlotGroup...
			break;
		}
		else
		{
			pPlotNode = nextPlotsNode(pPlotNode);
		}
	}
}


void CvPlotGroup::recalculatePlots()
{
	PROFILE_FUNC();

	CvPlot* pPlot;
	PlayerTypes eOwner;
	int iCount;

	eOwner = getOwnerINLINE();

	CLLNode<XYCoords>* pPlotNode = headPlotsNode();
	if (pPlotNode != nullptr)
	{
		pPlot = GC.getMapINLINE().plotSorenINLINE(pPlotNode->m_data.iX, pPlotNode->m_data.iY);

		iCount = 0;

#if ENABLE_GAMECOREDLL_ENHANCEMENTS
		// Another bug. Plot group plot counting is incorrect when starting at a plot that has become !isTradeNetwork.
		// This happens because the start plot is always valid in the path finder.
		if (gGlobals.enhancedDLLConfig[EEnhancedDLLConfigOption::GeneralFixes] ? pPlot->isTradeNetwork(GET_PLAYER(getOwnerINLINE()).getTeam()) : true)
		{
			if (gGlobals.enhancedDLLConfig[EEnhancedDLLConfigOption::Fast_CvPlotGroup_recalculatePlots])
				iCount = GiganticMapsOptimisationsLib::countTradeReachablePlots(eOwner, pPlot->getX_INLINE(), pPlot->getY_INLINE(), getLengthPlots() + 1);
			else
			{
				gDLL->getFAStarIFace()->SetData(&GC.getPlotGroupFinder(), &iCount);
				gDLL->getFAStarIFace()->GeneratePath(&GC.getPlotGroupFinder(), pPlot->getX_INLINE(), pPlot->getY_INLINE(), -1, -1, false, eOwner);
			}
		}
#else
		gDLL->getFAStarIFace()->SetData(&GC.getPlotGroupFinder(), &iCount);
		gDLL->getFAStarIFace()->GeneratePath(&GC.getPlotGroupFinder(), pPlot->getX_INLINE(), pPlot->getY_INLINE(), -1, -1, false, eOwner);
#endif

		if (iCount == getLengthPlots())
		{
			return;
		}
	}

	CLinkList<XYCoords> oldPlotGroup;
	XYCoords xy;

	oldPlotGroup.clear();

	pPlotNode = headPlotsNode();

	while (pPlotNode != nullptr)
	{
		pPlot = GC.getMapINLINE().plotSorenINLINE(pPlotNode->m_data.iX, pPlotNode->m_data.iY);

		FAssertMsg(pPlot != nullptr, "Plot is not assigned a valid value");

		xy.iX = pPlot->getX_INLINE();
		xy.iY = pPlot->getY_INLINE();

		oldPlotGroup.insertAtEnd(xy);

		pPlot->setPlotGroup(eOwner, nullptr);

		pPlotNode = deletePlotsNode(pPlotNode); // will delete this PlotGroup...
	}

	pPlotNode = oldPlotGroup.head();

	while (pPlotNode != nullptr)
	{
		pPlot = GC.getMapINLINE().plotSorenINLINE(pPlotNode->m_data.iX, pPlotNode->m_data.iY);

		FAssertMsg(pPlot != nullptr, "Plot is not assigned a valid value");

		pPlot->updatePlotGroup(eOwner, true);

		pPlotNode = oldPlotGroup.deleteNode(pPlotNode);
	}
}

int CvPlotGroup::getID() const
{
	return m_iID;
}


void CvPlotGroup::setID(int iID)
{
	m_iID = iID;
}


PlayerTypes CvPlotGroup::getOwner() const
{
	return getOwnerINLINE();
}


int CvPlotGroup::getNumBonuses(BonusTypes eBonus) const
{
	FAssertMsg(eBonus >= 0, "eBonus is expected to be non-negative (invalid Index)");
	FAssertMsg(eBonus < GC.getNumBonusInfos(), "eBonus is expected to be within maximum bounds (invalid Index)");
	return m_paiNumBonuses[eBonus];
}


bool CvPlotGroup::hasBonus(BonusTypes eBonus)
{
	return(getNumBonuses(eBonus) > 0);
}


void CvPlotGroup::changeNumBonuses(BonusTypes eBonus, int iChange)
{
	CvCity* pCity;
	[[maybe_unused]] int iOldNumBonuses;

	FAssertMsg(eBonus >= 0, "eBonus is expected to be non-negative (invalid Index)");
	FAssertMsg(eBonus < GC.getNumBonusInfos(), "eBonus is expected to be within maximum bounds (invalid Index)");

	if (iChange != 0)
	{
		iOldNumBonuses = getNumBonuses(eBonus);

		m_paiNumBonuses[eBonus] = (m_paiNumBonuses[eBonus] + iChange);

		//FAssertMsg(m_paiNumBonuses[eBonus] >= 0, "m_paiNumBonuses[eBonus] is expected to be non-negative (invalid Index)"); XXX

		CLLNode<XYCoords>* pPlotNode = headPlotsNode();

		while (pPlotNode != nullptr)
		{
			pCity = GC.getMapINLINE().plotSorenINLINE(pPlotNode->m_data.iX, pPlotNode->m_data.iY)->getPlotCity();

			if (pCity != nullptr)
			{
				if (pCity->getOwnerINLINE() == getOwnerINLINE())
				{
					pCity->changeNumBonuses(eBonus, iChange);
				}
			}

			pPlotNode = nextPlotsNode(pPlotNode);
		}
	}
}


void CvPlotGroup::insertAtEndPlots(XYCoords xy)
{
	m_plots.insertAtEnd(xy);
}

CLLNode<XYCoords>* CvPlotGroup::deletePlotsNode(CLLNode<XYCoords>* pNode)
{
	CLLNode<XYCoords>* pPlotNode;

	pPlotNode = m_plots.deleteNode(pNode);

	if (getLengthPlots() == 0)
	{
		GET_PLAYER(getOwnerINLINE()).deletePlotGroup(getID());
	}

  return pPlotNode;
}


CLLNode<XYCoords>* CvPlotGroup::nextPlotsNode(CLLNode<XYCoords>* pNode)
{
	return m_plots.next(pNode);
}

int CvPlotGroup::getLengthPlots()
{
	return m_plots.getLength();
}

CLLNode<XYCoords>* CvPlotGroup::headPlotsNode()
{
	return m_plots.head();
}

void CvPlotGroup::read(FDataStreamBase* pStream)
{
	// Init saved data
	reset();

	unsigned int uiFlag=0;
	pStream->Read(&uiFlag);	// flags for expansion

	pStream->Read(&m_iID);

	pStream->Read((int*)&m_eOwner);

	FAssertMsg((0 < GC.getNumBonusInfos()), "GC.getNumBonusInfos() is not greater than zero but an array is being allocated in CvPlotGroup::read");
	pStream->Read(GC.getNumBonusInfos(), m_paiNumBonuses);

	m_plots.Read(pStream);
}

void CvPlotGroup::write(FDataStreamBase* pStream)
{
	unsigned int uiFlag=0;
	pStream->Write(uiFlag);		// flag for expansion

	pStream->Write(m_iID);

	pStream->Write(m_eOwner);

	FAssertMsg((0 < GC.getNumBonusInfos()), "GC.getNumBonusInfos() is not greater than zero but an array is being allocated in CvPlotGroup::write");
	pStream->Write(GC.getNumBonusInfos(), m_paiNumBonuses);

	m_plots.Write(pStream);
}
