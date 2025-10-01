#include "CommandLine.h"

#include <CommonStuff/StringConversion.h>

#include <optional>
#include <stdexcept>
#include <vector>
#include <span>
#include <iostream>
#include <ranges>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN 1
#define NOMINMAX 1
#include <Windows.h>
#include <shellapi.h>
#endif

static constexpr const char* kHelp = R"(Usage: Cv4MiniEngine [options] [save filename]
Options:
    -mod <name of mod>     Load the specified mod. Ignored when loading a save.
)";

namespace
{
	struct CmdLineException : std::runtime_error
	{
		using std::runtime_error::runtime_error;
	};

#ifdef _WIN32
	std::vector<std::wstring> getCommandLine(int, const char * const *)
	{
		// Ignore given command line and use Unicode API.
		int argc{};
		wchar_t** const szArglist = CommandLineToArgvW(GetCommandLineW(), &argc);
		
		if (!szArglist)
			throw std::runtime_error("CommandLineToArgvW failed.");

		struct Dtor
		{
			void* ptr{};

			~Dtor()
			{
				LocalFree(ptr);
			}
		};

		const Dtor dtor(szArglist);
		
		std::vector<std::wstring> args(std::from_range, std::span(szArglist, argc));

		return args;
	}
#else
	std::vector<std::wstring> getCommandLine(int argc, const char * const * argv)
	{
		// Assume UTF-8.
		// There is /proc/self/cmdline, but it's limited apparently.
		return { std::from_range, std::span(argv, argc) | std::views::transform([](const char* arg) {
			return heck::convertUtf8ToWide(arg);
			}) };
	}
#endif
}

AppStartupConfig(::parseCommandLine)(int argc, const char * const * argv) try
{
	const std::vector<std::wstring> cmdLine = getCommandLine(argc, argv);

	AppStartupConfig config;

	size_t argI = 1;

	const auto tryConsumeOpt = [&](std::wstring_view opt) {
		if (argI < cmdLine.size() && std::wstring_view(cmdLine[argI]) == opt)
			return ++argI, true;
		else
			return false;
		};

	const auto tryConsumeString = [&]() -> std::optional<std::wstring_view> {
		if (argI < cmdLine.size())
			return cmdLine[argI++];
		else
			return std::nullopt;
		};

	if (tryConsumeOpt(L"-mod"))
	{
		if (const std::optional<std::wstring_view> mod = tryConsumeString())
			config.modRelPath = std::wstring(L"Mods/") + std::wstring(*mod);
		else
			throw CmdLineException("Missing mod name.");
	}

	if (const std::optional<std::wstring_view> save = tryConsumeString())
		config.save = *save;

	if (argI < cmdLine.size())
		throw CmdLineException("Too many args.");

	return config;
}
catch (const CmdLineException& ex)
{
	std::cout << ex.what() << std::endl;
	std::cout << kHelp << std::endl;
	std::exit(EXIT_FAILURE);
}
