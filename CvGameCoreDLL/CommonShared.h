#pragma once

#include "CommonConfig.h"
#include "FAssert.h"

#include <cassert>
#include <cmath>
#include <array>
#include <span>
#include <string_view>
#include <limits.h>

#ifdef _WIN32
#include <tchar.h>
#else
#include <stdexcept>
#include <stdint.h>
#include <time.h>
#include <cstdarg>
using TCHAR = char;
#define _T(s) s
#define _wcsicmp(a, b) wcscasecmp(a, b)
#define _strdup strdup
#define _vsnprintf vsnprintf
#define _tcscmp strcmp
// BS differences between MSVC and Linux. TODO: Stop using these.
#define strcpy_s(dst, n, src) strcpy(dst, src)
#define wcscpy_s(dst, n, src) wcscpy(dst, src)
inline uint32_t timeGetTime()
{
	struct timespec tp {};
	if (clock_gettime(CLOCK_MONOTONIC, &tp) != 0)
		throw std::runtime_error("clock_gettime(CLOCK_MONOTONIC) failed!");
	return tp.tv_sec * 1000 + tp.tv_nsec / 1'000'000;
}
template<size_t k>
inline int swprintf(wchar_t(&out)[k], const wchar_t* fmt, ...)
{
	std::va_list args;
	va_start(args, fmt);
	const int result = vswprintf(out, k - 1, fmt, args);
	va_end(args);
	return result;
}



#endif



#define SAFE_DELETE(p)       { if(p) { delete (p);     (p)=nullptr; } }
#define SAFE_DELETE_ARRAY(p) { if(p) { delete[] (p);   (p)=nullptr; } }
#define SAFE_RELEASE(p)      { if(p) { (p)->Release(); (p)=nullptr; } }



typedef unsigned short   word;

//
// GameBryo
//
class NiColor
{
public:
	float r, g, b;
};
class NiColorA
{
public:
	NiColorA(float fr, float fg, float fb, float fa) : r(fr), g(fg), b(fb), a(fa) {}
	NiColorA() {}
	float r, g, b, a;
};
class NiPoint2
{
public:
	NiPoint2() {}
	constexpr NiPoint2(float fx, float fy) : x(fx), y(fy) {}

	float x, y;
};
class NiPoint3
{
public:
	NiPoint3() {}
	constexpr NiPoint3(float fx, float fy, float fz) : x(fx), y(fy), z(fz) {}

	bool operator== (const NiPoint3& pt) const
	{
		return (x == pt.x && y == pt.y && z == pt.z);
	}

	inline NiPoint3 operator+ (const NiPoint3& pt) const
	{
		return NiPoint3(x+pt.x, y+pt.y, z+pt.z);
	}

	inline NiPoint3 operator- (const NiPoint3& pt) const
	{
		return NiPoint3(x-pt.x, y-pt.y, z-pt.z);
	}

	inline float operator* (const NiPoint3& pt) const
	{
		return x*pt.x+y*pt.y+z*pt.z;
	}

	inline NiPoint3 operator* (float fScalar) const
	{
		return NiPoint3(fScalar*x, fScalar*y, fScalar*z);
	}

	inline NiPoint3 operator/ (float fScalar) const
	{
		float fInvScalar = 1.0f/fScalar;
		return NiPoint3(fInvScalar*x, fInvScalar*y, fInvScalar*z);
	}

	inline NiPoint3 operator- () const
	{
		return NiPoint3(-x, -y, -z);
	}

	inline float Length() const
	{
		return std::sqrt(x * x + y * y + z * z);
	}

	inline float Unitize()
	{
		float length = Length();
		if (length != 0)
		{
			x /= length;
			y /= length;
			z /= length;
		}
		return length;
	}

	//	inline NiPoint3 operator* (float fScalar, const NiPoint3& pt)
	//	{	return NiPoint3(fScalar*pt.x,fScalar*pt.y,fScalar*pt.z);	}
	float x, y, z;
};

// fortsnek: New function for the engine to use to get the config string of this DLL.
DllExportForInterface std::span<const std::string_view> getCvGameCoreDLLConfigStrings();
DllExportForInterface bool hasCvGameCoreDLLPlayerBotSupport();