#pragma once

#include <string>

struct [[nodiscard]] AppStartupConfig
{
	std::wstring modRelPath;
	std::wstring save;
};

AppStartupConfig parseCommandLine(int argc, const char * const * argv);