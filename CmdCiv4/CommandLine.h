#pragma once

#include <string>

namespace cvengine
{
	struct [[nodiscard]] AppStartupConfig
	{
		std::wstring generatePlayerBotGameDefsDir{};
		std::wstring modRelPath;
		std::wstring save;
		std::wstring botPath;
		bool isAutostart = false;
		//bool isBotAutorun = false;
	};

	AppStartupConfig parseCommandLine(int argc, const char * const * argv);
}