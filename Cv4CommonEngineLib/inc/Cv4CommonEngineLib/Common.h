#pragma once

#include <filesystem>

namespace cvengine
{
	void initDebugOutput();

	// Thread-safe. Use CvDLLUtility::logMemState from the DLL.
	void outputDebugString(const char* str);
	void outputDebugString(const wchar_t* str);

	const std::filesystem::path& getUserConfigDir();
	const std::filesystem::path& getUserDataDir();
	const std::filesystem::path& getUserCacheDir();

	std::wstring trim(std::wstring s);

	struct StringHasher
	{
		using is_transparent = void;

		size_t operator()(std::string_view) const noexcept;
	};

	extern const std::filesystem::path kSavesDirName;
	extern const std::filesystem::path kSavesAutoDirName;
	extern const std::filesystem::path kSavesSingleDirName;
	extern const std::filesystem::path kReplaysDirName;
	extern const std::filesystem::path kCustomAssetsDirName;
	extern const std::filesystem::path kPublicMapsDirName;
}