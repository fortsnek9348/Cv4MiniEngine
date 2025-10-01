#include "inc/CommonStuff/System.h"
#include "inc/CommonStuff/StringConversion.h"

#include <utility>

#ifdef _WIN32
#include <shlobj.h>
#include <objbase.h>
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#else
#include <cwchar>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

std::optional<std::wstring> heck::findEnvironmentVariable(const std::wstring& name)
{
#ifdef _WIN32
	std::wstring env;
	env.resize(16);
	size_t bufSize = 0;
	for (;;)
	{
		const errno_t status = _wgetenv_s(&bufSize, env.data(), env.size(), name.c_str());
		if (bufSize <= 0)
			return std::nullopt;
		env.resize(bufSize);
		if (status == 0)
		{
			// Nul-terminator.
			env.pop_back();
			return env;
		}
		else if (status != ERANGE)
			return std::nullopt; // error
		// else, try again.
	}
#else
	if (const char* const value = getenv(convertWideToUtf8(name).c_str()))
		return convertUtf8ToWide(value);
	else
		return std::nullopt;
#endif
}

#ifdef _WIN32
namespace
{
	struct Deleter
	{
		using pointer = PWSTR;

		void operator()(pointer p) const noexcept
		{
			CoTaskMemFree(p);
		}
	};

	std::unique_ptr<void, Deleter> tryGetWin32SpecialPath(const GUID& guid)
	{
		std::unique_ptr<void, Deleter> path;
		if (SUCCEEDED(SHGetKnownFolderPath(guid, KF_FLAG_DEFAULT, nullptr, std::out_ptr(path))))
			return path;
		else
			return nullptr;
	}
}
#endif

std::filesystem::path heck::getUserGamesSpecialDirectory(const std::filesystem::path& gameName, EUserGamesSpecialDirectory i)
{
#ifdef _WIN32
	

	switch (i)
	{
	case EUserGamesSpecialDirectory::Data:
	case EUserGamesSpecialDirectory::Config:
	default:
		if (const auto path = tryGetWin32SpecialPath(FOLDERID_Documents))
			return fs::path(path.get()) / L"My Games" / gameName;
		else
			throw std::runtime_error("Failed to get FOLDERID_Documents folder path.");
	case EUserGamesSpecialDirectory::Cache:
		if (const auto path = tryGetWin32SpecialPath(FOLDERID_LocalAppData))
			return fs::path(path.get()) / gameName / L"cache";
		else
			throw std::runtime_error("Failed to get FOLDERID_LocalAppData folder path.");
	}
#else
	// https://specifications.freedesktop.org/basedir-spec/latest/
	static constexpr auto kXDG = std::to_array<const char*>({
		"XDG_CONFIG_HOME",
		"XDG_DATA_HOME",
		"XDG_CACHE_HOME",
		});

	static constexpr auto kDefaults = std::to_array<const char*>({
		".config",
		".local/share",
		".cache",
		});

	if (const char* const dir = std::getenv(kXDG[std::to_underlying(i)]); dir && dir[0]) // set and non-empty.
		return fs::path(dir) / gameName;
	else if (const char* const dir2 = std::getenv("HOME"); dir2 && dir2[0])
		return fs::path(dir2) / kDefaults[std::to_underlying(i)] / gameName;
	else
		throw std::runtime_error("Could not identify a home directory. Check that " + std::string(kXDG[std::to_underlying(i)]) + " or HOME is set.");
#endif
}

std::wstring heck::getUserName()
{
#ifdef _WIN32
	std::wstring name;
	DWORD n = 1;
	while (GetUserNameW(name.data(), &n) == 0)
	{
		if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
			name.resize(std::max<DWORD>(1, n) - 1);
		else
			throw std::runtime_error("Failed to get user name using GetUserNameW.");
	}
	return name;
#else
	if (const char* const name1 = std::getenv("LOGNAME"); name1 && name1[0])
		return convertUtf8ToWide(name1);
	else if (const char* const name2 = getlogin(); name2 && name2[0])
		return convertUtf8ToWide(name2);
	else
		return L"Unknown User";
#endif
}

std::filesystem::path heck::getUserHomeDirectory()
{
#ifdef _WIN32
	if (const auto path = tryGetWin32SpecialPath(FOLDERID_Profile))
		return fs::path(path.get());
	else
		throw std::runtime_error("Failed to get FOLDERID_Profile folder path.");
#else
	if (const char* const dir = std::getenv("HOME"); dir && dir[0])
		return fs::path(dir);
	else
		throw std::runtime_error("Could not identify a home directory. Check that HOME is set.");
#endif
}