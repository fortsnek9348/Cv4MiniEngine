#include "Common.h"
#include "CivIni.h"

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
const std::filesystem::path cvengine::kSavesDirName = "Saves (Cv4MiniEngine)";
const std::filesystem::path cvengine::kSavesAutoDirName = "auto";
const std::filesystem::path cvengine::kSavesSingleDirName = "single";
const std::filesystem::path cvengine::kReplaysDirName = "Replays (Cv4MiniEngine)";
const std::filesystem::path cvengine::kCustomAssetsDirName = "CustomAssets";
const std::filesystem::path cvengine::kPublicMapsDirName = "PublicMaps";

static constexpr const wchar_t* kSaveFileExtension = L"CivBeyondSwordSave";

#ifdef _WIN32
void cvengine::outputDebugString(const char* s)
{
	OutputDebugStringA(s);
}

void cvengine::outputDebugString(const wchar_t* s)
{
	OutputDebugStringW(s);
}
#else
void cvengine::outputDebugString(const char*)
{
}

void cvengine::outputDebugString(const wchar_t*)
{
}
#endif

namespace
{
	// This will flush whenever there's a line ending, including after the line ending.
	// This could create mangled output if you mix char and wchar_t output without a line ending.
	template <class CharT>
	class basic_debugbuf : public std::basic_streambuf<CharT, std::char_traits<CharT>>
	{
	public:
		virtual ~basic_debugbuf() override
		{
			sync();
		}

	protected:
		using typename std::basic_streambuf<CharT, std::char_traits<CharT>>::char_type;
		using typename std::basic_streambuf<CharT, std::char_traits<CharT>>::traits_type;
		using typename std::basic_streambuf<CharT, std::char_traits<CharT>>::int_type;

		virtual std::streamsize xsputn(const char_type* s, std::streamsize count) override
		{
			mLineBuf += std::basic_string_view<CharT>(s, count);
			if (std::basic_string_view<CharT>(s, count).contains(static_cast<CharT>('\n')))
				flushInternalBuffer();
			return count;
		}

		virtual int_type overflow(int_type ch = traits_type::eof()) override
		{
			if (!traits_type::eq_int_type(ch, traits_type::eof()))
			{
				if (ch == '\n')
				{
					mLineBuf += static_cast<CharT>('\n');
					flushInternalBuffer();
				}
				else
				{
					mLineBuf += static_cast<char_type>(ch);
					//updateBufView();
				}
			}
			return traits_type::not_eof(ch);
		}

		virtual int sync() override
		{
			//const auto& s = this->str();
			//outputDebugString(s.c_str());
			//this->str({}); // Clear the string buffer
			flushInternalBuffer();
			return 0;
		}

	private:
		std::basic_string<CharT> mLineBuf;

		void flushInternalBuffer()
		{
			outputDebugString(mLineBuf.c_str());
			mLineBuf.clear();
		}
	};

	// This also writes on line ending, but does not flush the output stream! The output can flush on line endings if it wants.
	class Utf8TranslationStreamOutput : public std::basic_streambuf<wchar_t, std::char_traits<wchar_t>>
	{
	public:
		explicit Utf8TranslationStreamOutput(std::ostream& target) : mTarget(target), mBuf(kMaxBufSize)
		{
			//updateBufView();
		}

		virtual ~Utf8TranslationStreamOutput() override
		{
			sync();
		}

	protected:

		virtual std::streamsize xsputn(const char_type* s, std::streamsize count) override
		{
			if (!mTarget || count < 0)
				return 0;
			else if (static_cast<size_t>(count) <= kMaxBufSize - mBufPosition)
			{
				std::ranges::copy_n(s, count, std::span(mBuf).subspan(mBufPosition).begin());
				mBufPosition += count;
				//if (std::ranges::contains(std::span(mBuf).subspan(0, mBufPosition), '\n'))
				flushInternalBuffer();
				//else
				//	updateBufView();
				return count;
			}
			else
			{
				flushInternalBuffer();
				return Utf8TranslationStreamOutput::xsputn(s, count);
			}
		}

		virtual int_type overflow(int_type ch = traits_type::eof()) override
		{
			flushInternalBuffer();
			if (!traits_type::eq_int_type(ch, traits_type::eof()))
			{
				if (ch == '\n')
					mTarget << '\n';
				else
				{
					mBuf[mBufPosition++] = static_cast<char_type>(ch);
					//updateBufView();
				}

				flushInternalBuffer();
			}
			return mTarget ? traits_type::not_eof(ch) : traits_type::eof();
		}

		virtual int sync() override
		{
			flushInternalBuffer();
			mTarget << std::flush;

			//updateBufView();

			return mTarget ? 0 : -1;
		}

	private:
		static constexpr size_t kMaxBufSize = 1024;
		std::ostream& mTarget;
		std::vector<wchar_t> mBuf;
		size_t mBufPosition = 0;

		//void updateBufView()
		//{
		//	setp(mBuf.data() + mBufPosition, mBuf.data() + (mBuf.size() - mBufPosition));
		//}

		void flushInternalBuffer()
		{
			const size_t n = mBufPosition;
			if (n > 0 && 0xD800 <= mBuf[n - 1] && mBuf[n - 1] < 0xDC00) [[unlikely]]
			{
				// Last element is the start of a UTF16 surrogates pair. Leave it in the buffer.
				mTarget << heck::convertWideToUtf8(std::wstring_view(mBuf).substr(0, n - 1));
				mBufPosition = 1;
				mBuf[0] = mBuf[n - 1];
			}
			else
			{
				mTarget << heck::convertWideToUtf8(std::wstring_view(mBuf).substr(0, n));
				mBufPosition = 0;
			}
		}
	};
}

void cvengine::initDebugOutput()
{
	const std::string defaultTarget = "file log.txt";

	std::istringstream targetSS(gCivilizationIVIni.grab(kCivilizationIVIniSection_CV4ENGINE, kCivilizationIVIniProp_LogOutput, defaultTarget));
	std::string targetToken;
	bool failedToParseTarget = false;
	if (!(targetSS >> targetToken))
		failedToParseTarget = true;
	else
	{
		if (targetToken == "stderr")
			;
		else if (targetToken == "OutputDebugString")
		{
			static basic_debugbuf<char> gCharOutput{};
			static basic_debugbuf<wchar_t> gWCharOutput{};
			std::cerr.rdbuf(&gCharOutput);
			std::clog.rdbuf(&gCharOutput);
			std::wcerr.rdbuf(&gWCharOutput);
			std::wclog.rdbuf(&gWCharOutput);
		}
		else if (targetToken == "file")
		{
			targetSS >> std::quoted(targetToken);
			std::clog << "Redirecting log output to file '" << targetToken << "'." << std::endl;
			
			static std::ofstream gFileOutput{};
			gFileOutput.open(targetToken);
			if (!gFileOutput)
				throw std::runtime_error("Failed to open main log output file.");

			std::clog.rdbuf(gFileOutput.rdbuf());
			std::cerr.rdbuf(gFileOutput.rdbuf());

			// Universally translate wide to UTF8 to maintain sync between streams.
			static Utf8TranslationStreamOutput gCLogTranslationOutput(gFileOutput);
			static Utf8TranslationStreamOutput gCErrTranslationOutput(gFileOutput);
			std::wcerr.rdbuf(&gCLogTranslationOutput);
			std::wclog.rdbuf(&gCErrTranslationOutput);
		}
		else
		{
			failedToParseTarget = true;
		}
	}

	targetSS >> std::ws;
	failedToParseTarget |= !targetSS.eof();

	if (failedToParseTarget)
	{
		std::cerr << "Failed to parse logging output target. Aborting." << std::endl;
		std::abort();
	}

	// It should all be one line with the current implementation. Although, maybe cerr should be char-buffered.
	std::clog << "[TEST CLOG OUTPUT]" << std::flush;
	std::wclog << L" and [TEST WCLOG OUTPUT]" << std::flush;
	std::cerr << " and [TEST CERR OUTPUT]" << std::flush;
	std::wcerr << L" and [TEST WCERR OUTPUT]" << std::endl;
}

const fs::path& cvengine::getUserConfigDir()
{
	static const fs::path kPath = heck::getUserGamesSpecialDirectory(kGameName, heck::EUserGamesSpecialDirectory::Config);
	return kPath;
}

const fs::path& cvengine::getUserDataDir()
{
	static const fs::path kPath = heck::getUserGamesSpecialDirectory(kGameName, heck::EUserGamesSpecialDirectory::Data);
	return kPath;
}

const fs::path& cvengine::getUserCacheDir()
{
	static const fs::path kPath = heck::getUserGamesSpecialDirectory("Cv4MiniEngine", heck::EUserGamesSpecialDirectory::Cache);
	return kPath;
}

std::wstring cvengine::trim(std::wstring s)
{
	const auto startIt = std::ranges::find_if_not(s, std::iswspace);
	if (startIt != s.end())
	{
		const auto endIt = std::ranges::find_if_not(s | std::views::reverse, std::iswspace).base();
		s.erase(endIt, s.end());
		s.erase(s.begin(), startIt);
	}
	else
		s.clear();
	return s;
}

size_t StringHasher::operator()(std::string_view s) const noexcept
{
	return std::hash<std::string_view>()(s);
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
