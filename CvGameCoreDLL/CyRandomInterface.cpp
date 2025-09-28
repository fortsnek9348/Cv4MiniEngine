#include "CvGameCoreDLL.h"
#include "CvRandom.h"

#include <pybind11/pybind11.h>

//
// published python interface for CvRandom
//
void CyRandomPythonInterface(pybind11::module m)
{
	//OutputDebugString("Python Extension Module - CyRandomPythonInterface\n");

	pybind11::class_<CvRandom>(m, "CyRandom")
		.def("get", &CvRandom::get, "unsigned short (unsigned short usNum, const char* pszLog) - returns a random number")
		.def("init", &CvRandom::init, "void (unsigned long int ulSeed)")
		// fortsnek: access seed
		.def("getSeed", &CvRandom::getSeed, "unsigned long ()")
		;
}