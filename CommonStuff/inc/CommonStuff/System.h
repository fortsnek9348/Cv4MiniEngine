#pragma once

#include <string>
#include <optional>
#include <filesystem>

namespace heck
{
	std::optional<std::wstring> findEnvironmentVariable(const std::wstring& name);

	enum class EUserGamesSpecialDirectory
	{
		Config,
		Data,
		Cache,
	};

	// On Windows, Config and Data will be the same.
	std::filesystem::path getUserGamesSpecialDirectory(const std::filesystem::path& gameName, EUserGamesSpecialDirectory);
	std::wstring getUserName();

	std::filesystem::path getUserHomeDirectory();
}