//
// Python wrapper class for CvGame 
// 

#include "CvGameCoreDLL.h"
#include "CyDeal.h"
#include "CvDeal.h"

CyDeal::CyDeal(CvDeal* pDeal) :
	m_pDeal(pDeal)
{

}

CyDeal::~CyDeal()
{
}

bool CyDeal::isNone()
{ 
	return (nullptr == m_pDeal); 
}

int CyDeal::getID() const
{
	return (m_pDeal ? m_pDeal->getID() : -1);
}

int CyDeal::getInitialGameTurn() const
{
	return (m_pDeal ? m_pDeal->getInitialGameTurn() : -1);
}

int CyDeal::getFirstPlayer() const
{
	return (m_pDeal ? m_pDeal->getFirstPlayer() : -1);
}

int CyDeal::getSecondPlayer() const
{
	return (m_pDeal ? m_pDeal->getSecondPlayer() : -1);
}

int CyDeal::getLengthFirstTrades() const
{
	return (m_pDeal ? m_pDeal->getLengthFirstTrades() : 0);
}

int CyDeal::getLengthSecondTrades() const
{
	return (m_pDeal ? m_pDeal->getLengthSecondTrades() : 0);
}

TradeData* CyDeal::getFirstTrade(int i) const
{
	if (i < getLengthFirstTrades() && nullptr != m_pDeal && nullptr != m_pDeal->getFirstTrades())
	{
		const CLinkList<TradeData>& listTradeData = *(m_pDeal->getFirstTrades());
		int iCount = 0;
		for (CLLNode<TradeData>* pNode = listTradeData.head(); nullptr != pNode; pNode = listTradeData.next(pNode))
		{
			if (iCount == i)
			{
				return &(pNode->m_data);
			}
			iCount++;
		}
	}
	return (nullptr);
}

TradeData* CyDeal::getSecondTrade(int i) const
{
	if (i < getLengthSecondTrades() && nullptr != m_pDeal && nullptr != m_pDeal->getSecondTrades())
	{
		const CLinkList<TradeData>& listTradeData = *(m_pDeal->getSecondTrades());
		int iCount = 0;
		for (CLLNode<TradeData>* pNode = listTradeData.head(); nullptr != pNode; pNode = listTradeData.next(pNode))
		{
			if (iCount == i)
			{
				return &(pNode->m_data);
			}
			iCount++;
		}
	}
	return (nullptr);
}

void CyDeal::kill()
{
	if (nullptr != m_pDeal)
	{
		m_pDeal->kill();
	}
}
