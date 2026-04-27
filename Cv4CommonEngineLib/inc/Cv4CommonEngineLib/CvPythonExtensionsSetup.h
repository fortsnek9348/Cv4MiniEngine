#pragma once

#include <pybind11/pybind11.h>

namespace cvengine
{
	void setupCvPythonExtensionsModule(pybind11::module m);
}