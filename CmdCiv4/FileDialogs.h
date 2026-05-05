#pragma once

#include <filesystem>
#include <optional>

namespace cvengine
{
	enum class EFileType
	{
		Saves,
		WorldBuilder,
	};

	std::optional<std::filesystem::path> promptSaveFilePath(const std::filesystem::path& defPath);
	std::optional<std::filesystem::path> promptLoadFilePath(const std::filesystem::path& defDir, EFileType fileType = EFileType::Saves);
	std::optional<std::filesystem::path> promptDirectoryPath();
}