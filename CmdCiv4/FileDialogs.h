#pragma once

#include <filesystem>
#include <optional>

namespace cvengine
{
	std::optional<std::filesystem::path> promptSaveFilePath(const std::filesystem::path& defPath);
	std::optional<std::filesystem::path> promptLoadFilePath(const std::filesystem::path& defDir);
	std::optional<std::filesystem::path> promptDirectoryPath();
}