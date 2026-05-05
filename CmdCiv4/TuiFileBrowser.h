#pragma once

#include <filesystem>

namespace cvengine
{
	struct TuiFileBrowserConfig
	{
		std::filesystem::path defPath;
		std::vector<std::wstring> fileExts{}; // First is used as the default
		bool toSave = false;
		bool wantDirectory = false;
		bool useText = true;
	};

	std::optional<std::filesystem::path> tryPromptTuiFileBrowserPath(const TuiFileBrowserConfig& config);
}