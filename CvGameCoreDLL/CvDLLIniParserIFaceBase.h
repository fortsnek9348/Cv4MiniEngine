#pragma once

#ifdef _WIN32
#include <tchar.h>
#else
#include "CommonShared.h"
#endif

//
// abstract interface for FIniParser functions used by DLL
//
class FIniParser;
class CvDLLIniParserIFaceBase
{
public:
	virtual FIniParser* create(const char* szFile) = 0;
	virtual void destroy(FIniParser*& pParser, bool bSafeDelete=true) = 0;
	virtual bool SetGroupKey(FIniParser* pParser, const TCHAR* pGroupKey) = 0;
	virtual bool GetKeyValue(FIniParser* pParser, const TCHAR* szKey, bool  *iValue) = 0;
	virtual bool GetKeyValue(FIniParser* pParser, const TCHAR* szKey, short *iValue) = 0;
	virtual bool GetKeyValue(FIniParser* pParser, const TCHAR* szKey, int   *iValue) = 0;
	virtual bool GetKeyValue(FIniParser* pParser, const TCHAR* szKey, float *fValue) = 0;
	// fortsnek: Use TCHAR directly instead of LPTSTR.
	virtual bool GetKeyValue(FIniParser* pParser, const TCHAR* szKey, TCHAR* szValue) = 0;

};
