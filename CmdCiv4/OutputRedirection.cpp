#include "OutputRedirection.h"
#include "Cv4MiniEngineIni.h"

#include <Cv4CommonEngineLib/CivIni.h>

#include <CommonStuff/StringConversion.h>

#include <algorithm>
#include <iostream>
#include <sstream>
#include <span>
#include <vector>
#include <fstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN 1
#define NOMINMAX 1
#include <Windows.h>
#endif

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
			cvengine::outputDebugString(mLineBuf.c_str());
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

void cvengine::initDebugOutput(IniData& ini)
{
	const std::string defaultTarget = "file log.txt";

	std::istringstream targetSS(ini.grab(kCivilizationIVIniSection_CV4ENGINE, kCivilizationIVIniProp_LogOutput, defaultTarget));
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
