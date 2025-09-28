#include "CvGameCoreDLL.h"
#include "CyArgsList.h"
#include "CyGlobalContext.h"
#include "CvDLLPythonIFaceBase.h"
#include "CvDLLUtilityIFaceBase.h"


//////////////////////////////////////////////////////
// CyArgsList
//////////////////////////////////////////////////////

CyArgsList::CyArgsList() : m_iCnt(0)
{
}

void CyArgsList::add(int i) 
{ 
	//HECK_LOG_DLL_CALL(Python);
	push_back(PyInt_FromLong(i)); 
}

void CyArgsList::add(unsigned int ui)
{
	//HECK_LOG_DLL_CALL(Python);
	add((int)ui);
}

void CyArgsList::add(float f) 
{ 
	//HECK_LOG_DLL_CALL(Python);
	push_back(PyFloat_FromDouble(f)); 
}

// add PyObject
void CyArgsList::add(void* p) 
{ 
	//HECK_LOG_DLL_CALL(Python);

	//push_back((PyObject*)p);	

	// fortsnek: I'm not sure what the heck if going on here, but gameUpdate's arg is doubly nested.
	//push_back(pybind11::make_tuple(pybind11::reinterpret_steal<pybind11::object>(static_cast<PyObject*>(p))).release().ptr());
	push_back(static_cast<PyObject*>(p));
}

// add null-terminated string
void CyArgsList::add(const char* s)
{
	//HECK_LOG_DLL_CALL(Python);
	push_back(PyString_FromString(s));
}

void CyArgsList::add(const CvString& s)
{
	add(s.c_str());
}

void CyArgsList::add(const CvWString& s)
{
	add(s.c_str());
}

// add null-terminated string
void CyArgsList::add(const wchar_t* s)
{
	//HECK_LOG_DLL_CALL(Python);
	if (s)
		push_back(PyUnicode_FromWideChar(s, (int)wcslen(s)));
	else
		push_back(PyUnicode_FromWideChar(L"", 0));
}

// add data string
void CyArgsList::add(const char* buf, size_t iLength)
{
	//HECK_LOG_DLL_CALL(Python);
	push_back(PyString_FromStringAndSize(buf, (int)iLength));
}

// add float list
void CyArgsList::add(const float* buf, size_t iLength)
{
	//HECK_LOG_DLL_CALL(Python);
	PyObject* pList = PyList_New((int)iLength);	// new ref
	FAssertMsg(pList, "failed creating PyList");
	size_t i;
	for(i=0;i<iLength;i++)
	{
		PyObject* pItem=PyFloat_FromDouble(buf[i]);		// new ref
		FAssertMsg(pItem, "failed creating PyFloat");
		PyList_SetItem(pList, i, pItem);				// steals the ref, no unref necesary
	}
	push_back(pList);
}

// add byte list
void CyArgsList::add(const uint8_t* buf, size_t iLength)
{
	//HECK_LOG_DLL_CALL(Python);
	PyObject* pList = PyList_New((int)iLength);	// new ref
	FAssertMsg(pList, "failed creating PyList");
	size_t i;
	for(i=0;i<iLength;i++)
	{
		PyObject* pItem=PyInt_FromLong(buf[i]);		// new ref
		FAssertMsg(pItem, "failed creating PyInt");
		PyList_SetItem(pList, i, pItem);				// steals the ref, no unref necesary
	}
	push_back(pList);
}

// add int list
void CyArgsList::add(const int* buf, size_t iLength)
{
	//HECK_LOG_DLL_CALL(Python);
	PyObject* pList = PyList_New((int)iLength);	// new ref
	FAssertMsg(pList, "failed creating PyList");
	size_t i;
	for(i=0;i<iLength;i++)
	{
		PyObject* pItem=PyInt_FromLong(buf[i]);		// new ref
		FAssertMsg(pItem, "failed creating PyInt");
		PyList_SetItem(pList, i, pItem);				// steals the ref, no unref necesary
	}
	push_back(pList);
}

PyObject* CyArgsList::makeFunctionArgs() 
{ 
	return gDLL->getPythonIFace()->MakeFunctionArgs(m_aList, m_iCnt);
}

int CyArgsList::size() const { return m_iCnt; }
void CyArgsList::push_back(PyObject* p) { FAssertMsg(m_iCnt<MAX_CY_ARGS, "increase cyArgsList::MAX_CY_ARGS"); m_aList[m_iCnt++] = p; }
void CyArgsList::clear() { m_iCnt = 0; }