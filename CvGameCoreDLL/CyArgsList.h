#pragma once

#include "CvString.h"

// fortsnek: Include just the common header that will include Python.h in pybind11's particular way (avoid linking python27_d.lib).
#include <pybind11/detail/common.h>

//
// type for input args to python functions
//
class CyArgsList
{
public:
	enum
	{
		MAX_CY_ARGS=20
	};
	DllExport CyArgsList();
	DllExport void add(int i);
	DllExport void add(unsigned int ui);
	DllExport void add(float f);
	DllExport void add(const char* s);					// null-terminated string
	DllExport void add(const wchar_t* s);					// null-terminated widestring
	void add(const CvString& s);
	void add(const CvWString& s);
	DllExport void add(const char* s, size_t iLength);		// makes a data string
	DllExport void add(const uint8_t* s, size_t iLength);		// makes a list
	DllExport void add(const int* s, size_t iLength);		// makes a list
	DllExport void add(const float* s, size_t iLength);		// makes a list
	DllExport void add(void* p);
	DllExport PyObject* makeFunctionArgs();
	DllExport int size() const;
	DllExport void push_back(PyObject* p);
	DllExport void clear();
protected:
	PyObject* m_aList[MAX_CY_ARGS];
	int m_iCnt;
};
