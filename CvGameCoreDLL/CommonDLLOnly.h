#pragma once

#include "CommonConfig.h"

#include <CommonStuff/CompilerMacros.h>

#include <stdint.h>

namespace NiAnimationKey
{
	enum KeyType
	{
		NOINTERP,
		LINKEY,
		BEZKEY,
		TCBKEY,
		EULERKEY,
		STEPKEY,
		NUMKEYTYPES
	};
};

typedef uint32_t dword;
typedef uint64_t qword;

#define MAX_CHAR                            (0x7f)
#define MIN_CHAR                            (0x80)
#define MAX_SHORT                           (0x7fff)
#define MIN_SHORT                           (0x8000)
#define MIN_INT                             (0x80000000)
#define MAX_UNSIGNED_CHAR                   (0xff)
#define MIN_UNSIGNED_CHAR                   (0x00)
#define MAX_UNSIGNED_SHORT                  (0xffff)
#define MIN_UNSIGNED_SHORT                  (0x0000)
#define MAX_UNSIGNED_INT                    (0xffffffff)
#define MIN_UNSIGNED_INT                    (0x00000000)


#ifndef SQR
#define SQR(x)      ( (x) * (x) )
#endif
#define DEGTORAD(x) ( (float)( (x) * (M_PI / 180) ))
#define LIMIT_RANGE(low, value, high) value = (value < low ? low : (value > high ? high : value));
#define M_PI       3.14159265358979323846
#define fM_PI		3.141592654f		//!< Pi (float)

//HECK_FORCEINLINE dword FtoDW(float f) { return std::_Bit_cast<dword>(f); }
//HECK_FORCEINLINE float DWtoF(dword n) { return std::_Bit_cast<float>(f); }
//HECK_FORCEINLINE float MaxFloat() { return DWtoF(0x7f7fffff); }

void startProfilingDLL();
void stopProfilingDLL();

// fortsnek
// For when things get really bad, enable this and configure frequency in CommonDLLOnly.cpp.
// This will hash important game state and perform some verification.
// Typically, compare logs with low-frequency logging, then compare a small range with high frequency logging.
//#define ENABLE_DIVERGENCE_CHECKPOINT_LOGGING 1
#if ENABLE_DIVERGENCE_CHECKPOINT_LOGGING
#include <iostream>
#include <optional>
#include <sstream>

namespace heck
{
	inline int gPlotGroupUpdateNesting = 0;

	void diverganceCheckpoint(const char* funcName, std::string str);
}

#define FORTSNEK_DIVERGENCE_CHECKPOINT(args) heck::diverganceCheckpoint(__FUNCTION__, (std::ostringstream() << args).str())

#else

#define FORTSNEK_DIVERGENCE_CHECKPOINT(args) do {} while(0)

#endif
