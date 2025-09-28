#pragma once

#include <assert.h>

// Only compile in FAssert's if FASSERT_ENABLE is defined.  By default, however, let's key off of
// _DEBUG.  Sometimes, however, it's useful to enable asserts in release builds, and you can do that
// simply by changing the following lines to define FASSERT_ENABLE or using project settings to override
#ifdef _DEBUG
#define FASSERT_ENABLE
#endif 

#ifdef FASSERT_ENABLE

#ifdef _WIN32

bool FAssertDlg( const char*, const char*, const char*, unsigned int, bool& );

#define FAssert( expr )	\
{ \
	static bool bIgnoreAlways = false; \
	if( !bIgnoreAlways && !(expr) ) \
{ \
	if( FAssertDlg( #expr, 0, __FILE__, __LINE__, bIgnoreAlways ) ) \
__debugbreak(); \
} \
}

#define FAssertMsg( expr, msg ) \
{ \
	static bool bIgnoreAlways = false; \
	if( !bIgnoreAlways && !(expr) ) \
{ \
	if( FAssertDlg( #expr, msg, __FILE__, __LINE__, bIgnoreAlways ) ) \
__debugbreak(); \
} \
}

#else
// Non Win32 platforms--just use built-in FAssert
#define FAssert( expr )	assert( expr )
#define FAssertMsg( expr, msg )	assert( expr )

#endif

#else
// FASSERT_ENABLE not defined
#define FAssert( expr )
#define FAssertMsg( expr, msg )

#endif
