#pragma once

//
// abstract interface for Python functions used by DLL
// Creator - Mustafa Thamer
// Copyright 2005 Firaxis Games
//

//#include "CvEnums.h"

#include "FAssert.h"

#include "CvString.h"
#include <pybind11/pybind11.h>

class CvUnit;
class CvPlot;
class CvDLLPythonIFaceBase
{
public:

	virtual bool isInitialized() = 0;

	virtual const char* getMapScriptModule() = 0;

	template <typename T>	void setSeqFromArray(const T* /*src*/, int size, PyObject* /*dst*/);
	template <typename T>	int putSeqInArray(PyObject* /*src*/, T** /*dst*/);
	template <typename T>	int putStringSeqInArray(PyObject* /*src*/, T** /*dst*/);
	template <typename T>	int putFloatSeqInArray(PyObject* /*src*/, T** /*dst*/);
	template <typename T>	PyObject* makePythonObject(T*);

	virtual PyObject* MakeFunctionArgs(PyObject** args, int argc) = 0;

	virtual bool moduleExists(const char* moduleName, bool bLoadIfNecessary) = 0;
	virtual bool callFunction(const char* moduleName, const char* fxnName, PyObject* fxnArg=nullptr) = 0;
	virtual bool callFunction(const char* moduleName, const char* fxnName, PyObject* fxnArg, long* result) = 0;
	virtual bool callFunction(const char* moduleName, const char* fxnName, PyObject* fxnArg, CvString* result) = 0;
	virtual bool callFunction(const char* moduleName, const char* fxnName, PyObject* fxnArg, CvWString* result) = 0;
	virtual bool callFunction(const char* moduleName, const char* fxnName, PyObject* fxnArg, std::vector<uint8_t>* pList) = 0;
	virtual bool callFunction(const char* moduleName, const char* fxnName, PyObject* fxnArg, std::vector<int> *pIntList) = 0;
	virtual bool callFunction(const char* moduleName, const char* fxnName, PyObject* fxnArg, int* pIntList, int* iListSize) = 0;
	virtual bool callFunction(const char* moduleName, const char* fxnName, PyObject* fxnArg, std::vector<float> *pFloatList) = 0;
	virtual bool callPythonFunction(const char* szModName, const char* szFxnName, int iArg, long* result) = 0; // HELPER version that handles 1 arg for you

	virtual bool pythonUsingDefaultImpl() = 0;
};

template <typename T>
PyObject* CvDLLPythonIFaceBase::makePythonObject(T* pObj)
{
	if (!pObj)
		return Py_None;

	return pybind11::cast(pObj).release().ptr();
}

#ifdef _USRDLL
//
// static
// convert array to python list
//
template <typename T>
void CvDLLPythonIFaceBase::setSeqFromArray(const T* aSrc, int size, PyObject* dst)
{
	if (size<1)
		return;

	int iSeqSize=PySequence_Length(dst);
	FAssertMsg(iSeqSize>=size, "sequence length too small");

	int i;
	[[maybe_unused]] int ret=0;
	for (i=0;i<size;i++)
	{
		PyObject* x = PyInt_FromLong(aSrc[i]);
		ret=PySequence_SetItem(dst, i, x); 
		FAssertMsg(ret!=-1, "PySequence_SetItem failed");
		Py_DECREF(x);
	}

	// trim extra space
	if (iSeqSize>size)
	{
		ret=PySequence_DelSlice(dst, size, iSeqSize);
		FAssertMsg(ret!=-1, "PySequence_DelSlice failed");
	}
}

//
// static 
// convert python list to array
// allocates array
//
template <typename T>
int CvDLLPythonIFaceBase::putSeqInArray(PyObject* src, T** aDst)
{
	*aDst = nullptr;
	int size = static_cast<int>(PySequence_Length(src));
	if (size<1)
		return 0;

	*aDst = new T[size];
	int i;
	for (i=0;i<size;i++)
	{
		PyObject* item = PySequence_GetItem(src, i); /* Can't fail */
		//FAssertMsg(PyInt_Check(item), "sequence item is not an int");
		//(*aDst)[i] = (T)PyInt_AsLong(item);
		//Py_DECREF(item);

		// fortsnek: Because enums are strongly typed with pybind11, we'll instead do a conversion
		//           This is for at least arrays of PlotTypes in Pangaea. FractalWorld.generatePlotTypes in CvMapGeneratorUtil.py. Note that this map script doesn't always do this, it's random.
		(*aDst)[i] = pybind11::cast<T>(pybind11::reinterpret_steal<pybind11::object>(pybind11::handle(item)));
	}
	return size;
}

//
// static 
// convert python list to array
// allocates array
//
template <typename T>
int CvDLLPythonIFaceBase::putFloatSeqInArray(PyObject* src, T** aDst)
{
	*aDst = nullptr;
	int size = PySequence_Length(src);
	if (size<1)
		return 0;

	*aDst = new T[size];
	int i;
	for (i=0;i<size;i++)
	{
		PyObject* item = PySequence_GetItem(src, i); /* Can't fail */
		FAssertMsg(PyFloat_Check(item), "sequence item is not an float");
		(*aDst)[i] = (T)PyFloat_AsDouble(item);
		Py_DECREF(item);
	}
	return size;
}

//
// static 
// convert python list to array
// allocates array
//
template <typename T>
int CvDLLPythonIFaceBase::putStringSeqInArray(PyObject* src, T** aDst)
{
	*aDst = nullptr;
	int size = PySequence_Length(src);
	if (size<1)
		return 0;

	*aDst = new T[size];
	int i;
	for (i=0;i<size;i++)
	{
		PyObject* item = PySequence_GetItem(src, i); /* Can't fail */
		FAssertMsg(PyString_Check(item), "sequence item is not a string");
		(*aDst)[i] = (T)PyString_AsString(item);
		Py_DECREF(item);
	}
	return size;
}

#endif
