#pragma once

#include <filesystem>

struct CommandLine
{
	std::filesystem::path saveFile;
	std::filesystem::path botPath;
	std::filesystem::path modRelPath;
	int scenarioPreferredPlayer = -1;
};

CommandLine parseCommandLine(int argc, char** argv);