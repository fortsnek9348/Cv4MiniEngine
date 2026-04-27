#include "CyCommon.h"

#include <iostream>

void cvengine::abortOnUnimplementedPythonFunction(std::source_location loc)
{
	throw std::runtime_error(std::string("Unimplemented CvPythonExtensions function: ") + loc.function_name());
}

void cvengine::logUnimplementedPythonFunction(std::source_location loc)
{
	std::clog << std::string("Unimplemented CvPythonExtensions function: ") + loc.function_name() << '\n';
}