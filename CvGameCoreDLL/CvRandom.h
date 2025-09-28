#pragma once

#include "CommonShared.h"

class FDataStreamBase;

// fortsnek: Longs changed to int for Linux

class
#ifndef _WIN32
	DllExportForInterface
#endif
	CvRandom
{

public:

	DllExport CvRandom();
	DllExport virtual ~CvRandom();

	DllExport void init(unsigned int ulSeed);
	void uninit();
	void reset(unsigned int ulSeed = 0);

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS
	void disableThreadCheck();
#endif

	DllExport unsigned short get(unsigned short usNum, const TCHAR* pszLog = nullptr);  //  Returns value from 0 to num-1 inclusive.
	DllExport float getFloat();

	void reseed(unsigned int ulNewValue);
	DllExport unsigned int getSeed();

	// for serialization
	void read(FDataStreamBase* pStream);
	void write(FDataStreamBase* pStream);

protected:

	unsigned int m_ulRandomSeed;

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS
private:
	bool mThreadCheckEnabled = true;
#endif
};
