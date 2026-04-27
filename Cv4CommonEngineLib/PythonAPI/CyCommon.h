#pragma once

#include <pybind11/pybind11.h>

#include <exception>
#include <source_location>

namespace cvengine
{
	[[noreturn]] void abortOnUnimplementedPythonFunction(std::source_location = std::source_location::current());
	void logUnimplementedPythonFunction(std::source_location = std::source_location::current());

	void registerEnumsWithPython(pybind11::module& m);
}