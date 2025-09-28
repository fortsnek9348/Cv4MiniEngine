// random.cpp

#include "CvGameCoreDLL.h"
#include "CvRandom.h"
#include "CvGlobals.h"
#include "CyArgsList.h"
#include "CvGameAI.h"
#include "CvDLLUtilityIFaceBase.h"

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS
//#define FORTSNEK_ENABLE_CVRANDOM_THREAD_CHECKS
#endif

#ifdef FORTSNEK_ENABLE_CVRANDOM_THREAD_CHECKS
#include <thread>
#endif

#define RANDOM_A      (1103515245)
#define RANDOM_C      (12345)
#define RANDOM_SHIFT  (16)

// Check main thread, because with all this multithreaded AI proc, you're bound to make RNG use non-determinsitic at some point...
#ifdef FORTSNEK_ENABLE_CVRANDOM_THREAD_CHECKS
static void checkMainThread()
{
	static const std::thread::id id = std::this_thread::get_id();
	if (std::this_thread::get_id() != id)
		std::abort();
}
#endif

CvRandom::CvRandom()
{
	reset();
}


CvRandom::~CvRandom()
{ 
	uninit();
}


void CvRandom::init(unsigned int ulSeed)
{
	//--------------------------------
	// Init saved data
	reset(ulSeed);

	//--------------------------------
	// Init non-saved data
}


void CvRandom::uninit()
{
}


// FUNCTION: reset()
// Initializes data members that are serialized.
void CvRandom::reset(unsigned int ulSeed)
{
	//--------------------------------
	// Uninit class
	uninit();

	m_ulRandomSeed = ulSeed;
}

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS
void CvRandom::disableThreadCheck()
{
	mThreadCheckEnabled = false;
}
#endif


unsigned short CvRandom::get(unsigned short usNum, const TCHAR* pszLog)
{
#ifdef FORTSNEK_ENABLE_CVRANDOM_THREAD_CHECKS
	if (mThreadCheckEnabled)
		checkMainThread();
#endif

	if (pszLog != nullptr)
	{
		if (GC.getLogging() && GC.getRandLogging())
		{
			if (GC.getGameINLINE().getTurnSlice() > 0)
			{
				TCHAR szOut[1024];
				// fortsnek: Log the rng choice too.
				CvGame& game = GC.getGameINLINE();
				sprintf(szOut, "Rand %s = %u on %d (%s)\n",
					this == &game.getMapRand() ? "map" :
					this == &game.getSorenRand() ? "sync" : "other", getSeed(), game.getTurnSlice(), pszLog);
				gDLL->messageControlLog(szOut);
			}
		}
	}

	m_ulRandomSeed = ((RANDOM_A * m_ulRandomSeed) + RANDOM_C);

	unsigned short us = ((unsigned short)((((m_ulRandomSeed >> RANDOM_SHIFT) & MAX_UNSIGNED_SHORT) * ((unsigned int)usNum)) / (MAX_UNSIGNED_SHORT + 1)));

	return us;
}


float CvRandom::getFloat()
{
	return (((float)(get(MAX_UNSIGNED_SHORT))) / ((float)MAX_UNSIGNED_SHORT));
}


void CvRandom::reseed(unsigned int ulNewValue)
{
	m_ulRandomSeed = ulNewValue;
}


unsigned int CvRandom::getSeed()
{
	return m_ulRandomSeed;
}


void CvRandom::read(FDataStreamBase* pStream)
{
	reset();

	pStream->Read(&m_ulRandomSeed);
}


void CvRandom::write(FDataStreamBase* pStream)
{
	pStream->Write(m_ulRandomSeed);
}
