#pragma once

//
// includes (pch) for gamecore dll files
// Author - Mustafa Thamer
//

//
// WINDOWS
//

/*
#pragma warning( disable: 4530 )	// C++ exception handler used, but unwind semantics are not enabled

#define WIN32_LEAN_AND_MEAN

// fortsnek: No min, no max
#define NOMINMAX

#include <windows.h>
#include <MMSystem.h>
#if defined _DEBUG && !defined USE_MEMMANAGER
//#define USE_MEMMANAGER
#include <crtdbg.h>
#endif

#include <vector>
#include <list>
#include <tchar.h>
#include <math.h>
#include <assert.h>
#include <map>
#include <unordered_map>
#include <string>

#include "CommonShared.h"

// fortsnek: new includes
#include <unordered_set>
#include <ranges>
#include <iostream>
#include <execution>


#include "CommonDLLOnly.h"

//
// Boost Python
//
//# include <boost/python/list.hpp>
//# include <boost/python/tuple.hpp>
//# include <boost/python/class.hpp>
//# include <boost/python/manage_new_object.hpp>
//# include <boost/python/return_value_policy.hpp>
//# include <boost/python/object.hpp>
//# include <boost/python/def.hpp>
//
//namespace python = boost::python;

#include <pybind11/pybind11.h>

#ifdef FINAL_RELEASE
// Undefine OutputDebugString in final release builds
#undef OutputDebugString
#define OutputDebugString(x)
#endif //FINAL_RELEASE
*/

#include "CommonShared.h"
#include "CommonDLLOnly.h"