#include "inc/CommonStuff/StringConversion.h"

#include <SFML/System.hpp>

#include <stdexcept>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>
#endif

std::string heck::toUtf8(std::wstring_view w)
{
	std::string out;
	out.reserve(w.size());
	sf::Utf8::fromWide(w.data(), w.data() + w.size(), std::back_inserter(out));
	return out;
//#ifdef _WIN32
//	if (w.size() > INT_MAX)
//		throw std::runtime_error("String too big.");
//
//	const int size_needed = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
//	std::string str;
//	str.resize_and_overwrite(size_needed, [w](char* buf, size_t size) {
//		(void)WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), buf, (int)size, nullptr, nullptr);
//		return size;
//		});
//	return str;
//#else
//	// http://en.cppreference.com/w/cpp/string/multibyte/wcsrtombs.html
//	const std::wstring nullTerminated(w);
//	const wchar_t* wstr = nullTerminated.data();
//	std::mbstate_t state{};
//	std::size_t len = 1 + std::wcsrtombs(nullptr, &wstr, 0, &state);
//	std::string mbstr;
//	mbstr.resize_and_overwrite(len, [&](char* buf, size_t size) { std::wcsrtombs(buf, &wstr, size, &state); return size; });
//	return mbstr;
//#endif
}

std::wstring heck::toWide(std::string_view s)
{
	std::wstring out;
	out.reserve(s.size());
	sf::Utf8::toWide(s.data(), s.data() + s.size(), std::back_inserter(out));
	return out;
//#ifdef _WIN32
//	if (s.size() > INT_MAX)
//		throw std::runtime_error("String too big.");
//
//	std::wstring wstr;
//
//	if (s.empty())
//		return wstr;
//
//	int convertResult = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
//	if (convertResult <= 0)
//		throw std::runtime_error("Failed to convert UTF8 to UTF16.");
//	wstr.resize(convertResult);
//	convertResult = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), wstr.data(), convertResult);
//	if (convertResult <= 0)
//		throw std::runtime_error("Failed to convert UTF8 to UTF16.");
//
//	return wstr;
//#else
//	const std::string nullTerminated(s);
//	const char* mbstr = nullTerminated.data();
//	std::mbstate_t state{};
//	std::size_t len = 1 + std::mbsrtowcs(nullptr, &mbstr, 0, &state);
//	std::wstring wstr;
//	wstr.resize_and_overwrite(len, [&](wchar_t* buf, size_t size) { std::mbsrtowcs(buf, &mbstr, size, &state); return size; });
//	return wstr;
//#endif
}

#ifndef _WIN32
std::u16string heck::toUtf16(std::wstring s)
{
	std::u16string out;
	out.reserve(s.size());
	sf::Utf16::fromWide(s.data(), s.data() + s.size(), std::back_inserter(out));
	return out;
}
std::wstring heck::toWide(std::u16string_view s)
{
	std::wstring out;
	out.reserve(s.size());
	sf::Utf16::toWide(s.data(), s.data() + s.size(), std::back_inserter(out));
	return out;
}
#endif