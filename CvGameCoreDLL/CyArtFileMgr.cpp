//
// Python wrapper class for CvArtFileMgr 
// 
#include "CvGameCoreDLL.h"
#include "CyArtFileMgr.h"
#include "CvArtFileMgr.h"
#include "CyGlobalContext.h"

CyArtFileMgr::CyArtFileMgr() : m_pArtFileMgr(nullptr)
{
	m_pArtFileMgr = &ARTFILEMGR;
}

CyArtFileMgr::CyArtFileMgr(CvArtFileMgr* pArtFileMgr) : m_pArtFileMgr(pArtFileMgr)
{}

void CyArtFileMgr::Reset()
{
	if (m_pArtFileMgr)
	{
		m_pArtFileMgr->Reset();
	}
}

void CyArtFileMgr::buildArtFileInfoMaps()
{
	if (m_pArtFileMgr)
	{
		m_pArtFileMgr->buildArtFileInfoMaps();
	}
}

CvArtInfoInterface* CyArtFileMgr::getInterfaceArtInfo( const char * szArtDefineTag ) const
{
	return m_pArtFileMgr ? m_pArtFileMgr->getInterfaceArtInfo( szArtDefineTag ) : nullptr;
}

CvArtInfoMovie* CyArtFileMgr::getMovieArtInfo( const char * szArtDefineTag ) const
{
	return m_pArtFileMgr ? m_pArtFileMgr->getMovieArtInfo( szArtDefineTag ) : nullptr;
}

CvArtInfoMisc* CyArtFileMgr::getMiscArtInfo( const char * szArtDefineTag ) const
{
	return m_pArtFileMgr ? m_pArtFileMgr->getMiscArtInfo( szArtDefineTag ) : nullptr;
}

CvArtInfoUnit* CyArtFileMgr::getUnitArtInfo( const char * szArtDefineTag ) const
{
	return m_pArtFileMgr ? m_pArtFileMgr->getUnitArtInfo( szArtDefineTag ) : nullptr;
}

CvArtInfoBuilding* CyArtFileMgr::getBuildingArtInfo( const char * szArtDefineTag ) const
{
	return m_pArtFileMgr ? m_pArtFileMgr->getBuildingArtInfo( szArtDefineTag ) : nullptr;
}

CvArtInfoCivilization* CyArtFileMgr::getCivilizationArtInfo( const char * szArtDefineTag ) const
{
	return m_pArtFileMgr ? m_pArtFileMgr->getCivilizationArtInfo( szArtDefineTag ) : nullptr;
}

CvArtInfoLeaderhead* CyArtFileMgr::getLeaderheadArtInfo( const char * szArtDefineTag ) const
{
	return m_pArtFileMgr ? m_pArtFileMgr->getLeaderheadArtInfo( szArtDefineTag ) : nullptr;
}

CvArtInfoBonus* CyArtFileMgr::getBonusArtInfo( const char * szArtDefineTag ) const
{
	return m_pArtFileMgr ? m_pArtFileMgr->getBonusArtInfo( szArtDefineTag ) : nullptr;
}

CvArtInfoImprovement* CyArtFileMgr::getImprovementArtInfo( const char * szArtDefineTag ) const
{
	return m_pArtFileMgr ? m_pArtFileMgr->getImprovementArtInfo( szArtDefineTag ) : nullptr;
}

CvArtInfoTerrain* CyArtFileMgr::getTerrainArtInfo( const char * szArtDefineTag ) const
{
	return m_pArtFileMgr ? m_pArtFileMgr->getTerrainArtInfo( szArtDefineTag ) : nullptr;
}

CvArtInfoFeature* CyArtFileMgr::getFeatureArtInfo( const char * szArtDefineTag ) const
{
	return m_pArtFileMgr ? m_pArtFileMgr->getFeatureArtInfo( szArtDefineTag ) : nullptr;
}
