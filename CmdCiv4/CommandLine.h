#pragma once

#include <filesystem>

namespace cvengine
{
	struct [[nodiscard]] AppStartupConfig
	{
		std::filesystem::path generatePlayerBotGameDefsDir{};
		std::filesystem::path modRelPath;
		std::filesystem::path save;
		std::filesystem::path botPath;
		bool isAutostart = false;
		//bool isBotAutorun = false;
	};

	AppStartupConfig parseCommandLine(int argc, char** argv);
}