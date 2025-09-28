#pragma once

#include <pybind11/pybind11.h>

#include <exception>
#include <source_location>

// This triggers a nice stacktrace on unimplemented functions.
[[noreturn]] void unimplementedPythonFunction(std::source_location = std::source_location::current());

namespace CvEngineEnums
{
	void registerWithPython(pybind11::module& m);
}