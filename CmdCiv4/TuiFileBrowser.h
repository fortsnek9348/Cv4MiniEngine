#pragma once

#include <filesystem>

namespace cvengine
{
	struct TuiFileBrowserConfig
	{
		std::filesystem::path defPath;
		std::wstring fileExt{};
		bool toSave = false;
		bool wantDirectory = false;
		bool useText = true;
	};

	std::optional<std::filesystem::path> tryPromptTuiFileBrowserPath(const TuiFileBrowserConfig& config);
}