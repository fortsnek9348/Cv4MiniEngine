#pragma once

#include <pybind11/pybind11.h>

#include <exception>
#include <source_location>

namespace cvengine
{
	// This triggers a nice stacktrace on unimplemented functions.
	[[noreturn]] void unimplementedPythonFunction(std::source_location = std::source_location::current());

	void registerEnumsWithPython(pybind11::module& m);
}