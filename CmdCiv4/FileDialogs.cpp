#include "FileDialogs.h"

#include <Cv4CommonEngineLib/Common.h>
#include <Cv4CommonEngineLib/EngineSpecificsHeader.h>

#include <CvGlobals.h>

#include <CommonStuff/StringConversion.h>
#include <CommonStuff/System.h>

#include <iostream>
#include <fstream>
#include <cwctype>
#include <ranges>

namespace fs = std::filesystem;
using namespace cvengine;

// For testing
//#ifndef CV4MINIENGINE_USE_CONSOLE_FILE_BROWSER
//#define CV4MINIENGINE_USE_CONSOLE_FILE_BROWSER
//#endif

#if !CV4MINIENGINE_USE_CONSOLE_FILE_BROWSER
#include <nfd.hpp>
#else
#include "TuiFileBrowser.h"
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN 1
#define NOMINMAX 1
#include <Windows.h>
#endif

static const std::filesystem::path kGameName = "Beyond the Sword";

static constexpr const wchar_t* kSaveFileExtension = L"CivBeyondSwordSave";

std::optional<std::filesystem::path> engine_specific::promptSaveGamePath(const std::filesystem::path& defPath)
{
	return cvengine::promptSaveFilePath(defPath);
}
std::optional<std::filesystem::path> engine_specific::promptLoadGamePath(const std::filesystem::path& defDir)
{
	return cvengine::promptLoadFilePath(defDir);
}

#ifndef CV4MINIENGINE_USE_CONSOLE_FILE_BROWSER
static bool initNFD()
{
	static const bool x = [] {
		NFD_Init();
		return true;
	}();
	return x;
}

#ifdef _WIN32
#define HECK_NFD_STRING_LITERAL(s) L##s
static std::wstring convertToNFDString(std::wstring_view s)
{
	return std::wstring(s);
}
#else
#define HECK_NFD_STRING_LITERAL(s) s
static std::string convertToNFDString(std::wstring_view s)
{
	return heck::convertWideToUtf8(s);
}
#endif

static const auto kNfdSaveFileExtension = convertToNFDString(kSaveFileExtension);

std::optional<std::filesystem::path> cvengine::promptSaveFilePath(const std::filesystem::path& defPath)
{
	(void)initNFD();

	nfdnchar_t* outPath{};
	nfdnfilteritem_t filters[] = { { HECK_NFD_STRING_LITERAL("Civ4 BTS saves"), kNfdSaveFileExtension.c_str() } };
	const std::filesystem::path& defDir = std::filesystem::absolute(defPath.parent_path());
	const std::filesystem::path& defName = defPath.filename();
	const nfdsavedialognargs_t args{
		.filterList = filters,
		.filterCount = std::size(filters),
		.defaultPath = defDir.c_str(),
		.defaultName = defName.c_str(),
		.parentWindow{},
	};
	const nfdresult_t result = NFD_SaveDialogN_With(&outPath, &args);
	if (result == NFD_OKAY)
	{
		std::filesystem::path path = outPath;
		NFD_FreePathN(outPath);
		return path;
	}
	else if (result == NFD_CANCEL)
	{
		return std::nullopt;
	}
	else
	{
		std::cerr << "Native File Dialog error: " << NFD_GetError() << std::endl;
		return std::nullopt;
	}
}

std::optional<std::filesystem::path> cvengine::promptLoadFilePath(const std::filesystem::path& defDir)
{
	(void)initNFD();

	nfdnchar_t* outPath{};
	nfdnfilteritem_t filters[] = { { HECK_NFD_STRING_LITERAL("Civ4 BTS saves"), kNfdSaveFileExtension.c_str() } };
	const nfdopendialognargs_t args{
		.filterList = filters,
		.filterCount = std::size(filters),
		.defaultPath = defDir.c_str(),
		.parentWindow{},
	};
	const nfdresult_t result = NFD_OpenDialogN_With(&outPath, &args);
	if (result == NFD_OKAY)
	{
		std::filesystem::path path = outPath;
		NFD_FreePathN(outPath);
		return path;
	}
	else if (result == NFD_CANCEL)
	{
		return std::nullopt;
	}
	else
	{
		std::cerr << "Native File Dialog error: " << NFD_GetError() << std::endl;
		return std::nullopt;
	}

	
}

std::optional<std::filesystem::path> cvengine::promptDirectoryPath()
{
	(void)initNFD();

	nfdnchar_t* outPath{};

	const nfdresult_t result = NFD_PickFolderN(&outPath, nullptr);
	if (result == NFD_OKAY)
	{
		std::filesystem::path path = outPath;
		NFD_FreePathN(outPath);
		return path;
	}
	else if (result == NFD_CANCEL)
	{
		return std::nullopt;
	}
	else
	{
		std::cerr << "Native File Dialog error: " << NFD_GetError() << std::endl;
		return std::nullopt;
	}
}
#else
//static std::filesystem::path getDefaultDirectory()
//{
//	return getUserDataDir() / kSavesDirName / kSavesSingleDirName;
//}
std::optional<std::filesystem::path> cvengine::promptSaveFilePath(const std::filesystem::path& defPath)
{
	return cvengine::tryPromptTuiFileBrowserPath({
			.defPath = defPath,
			.fileExt = kSaveFileExtension,
			.toSave = true,
		});
}

std::optional<std::filesystem::path> cvengine::promptLoadFilePath(const std::filesystem::path& defDir)
{
	return cvengine::tryPromptTuiFileBrowserPath({
			.defPath = defDir,
			.fileExt = kSaveFileExtension,
		});
}

std::optional<std::filesystem::path> cvengine::promptDirectoryPath()
{
	return cvengine::tryPromptTuiFileBrowserPath({
		.defPath = heck::getUserHomeDirectory(),
		.wantDirectory = true,
		.useText = false, // This is used before text has been loaded.
		});
}
#endif
