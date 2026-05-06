#pragma once

#include <filesystem>

struct CommandLine
{
	std::filesystem::path saveFile;
	std::filesystem::path botPath;
	std::filesystem::path modRelPath;
	int scenarioPreferredPlayer = -1;
	int maxTurns = INT_MAX;
	bool wantInitialSave = false;
	bool wantEndSave = false;
};

CommandLine parseCommandLine(int argc, char** argv);