#pragma once

#ifdef _MSC_VER
#ifdef _USRDLL
#define DllExport __declspec(dllexport)
#else
#define DllExport __declspec(dllimport)
#endif
#else
#ifdef _USRDLL
#define DllExport __attribute__((visibility("default")))
#else
#define DllExport
#endif
#endif

// fortsnek: Extra exports are needed for interface code that's implemented in C++ instead of python.
#define DllExportForInterface DllExport
