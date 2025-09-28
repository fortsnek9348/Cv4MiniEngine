//  $Header: //depot/main/Civilization4/CvGameCoreDLL/CvDLLFlagEntityIFaceBase.h#3 $
//------------------------------------------------------------------------------------------------
//
//  ***************** CIV4 GAME ENGINE   ********************
//
//! \file		CvDLLFlagEntityIFaceBase.h
//! \author		Bart Muzzin -- 4-12-2005
//! \brief		DLL Stub interface for CvFlagEntity
//
//------------------------------------------------------------------------------------------------
//  Copyright (c) 2005 Firaxis Games, Inc. All rights reserved.
//------------------------------------------------------------------------------------------------
	
#pragma once

#include "CvDLLEntityIFaceBase.h"
#include "CvDLLUtilityIFaceBase.h"
//#include "CvGlobals.h"	// for gDLL
#include "CommonConfig.h"

class CvPlot;
class CvFlagEntity;

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  CLASS:      CvDLLFlagEntityIFaceBase
//!  \brief		abstract interface for CvFlagEntity functions used by DLL
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
class DllExport CvDLLFlagEntityIFaceBase //: public CvDLLEntityIFaceBase
{
	public:

		virtual CvFlagEntity * create( PlayerTypes ePlayer) = 0;

		virtual PlayerTypes getPlayer(CvFlagEntity * pkFlag) const = 0;
		virtual CvPlot* getPlot( CvFlagEntity * pkFlag ) const = 0;
		virtual void setPlot( CvFlagEntity * pkFlag, CvPlot * pkPlot, bool bOffset ) = 0;
		virtual void updateUnitInfo( CvFlagEntity * pkFlag, const CvPlot * pkPlot, bool bOffset ) = 0;
		virtual void updateGraphicEra(CvFlagEntity *pkFlag) = 0;
		// fortsnek: moved to implementation file
		virtual void setVisible(CvFlagEntity* pEnt, bool bVis);
		virtual void destroy(CvFlagEntity*& pImp, bool bSafeDelete=true) = 0;
};
