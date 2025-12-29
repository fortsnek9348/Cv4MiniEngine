#pragma once

#include <string>

namespace cvengine
{
	struct [[nodiscard]] AppStartupConfig
	{
		std::wstring modRelPath;
		std::wstring save;
	};

	AppStartupConfig parseCommandLine(int argc, const char * const * argv);
}