#pragma once

#include <string>
#include <string_view>
#include <cwctype>

namespace heck
{
	// Returns successfully or will throw on non-ascii.
	std::string toAsciiString(std::wstring_view s);

	std::string toAsciiLower(std::string s);

	constexpr char toAsciiLower(char c)
	{
		if ('A' <= c && c <= 'Z')
			return static_cast<char>(c - 'A' + 'a');
		else
			return c;
	}

	std::string toUtf8(std::wstring_view s);
	std::wstring toWide(std::string_view s);
#ifndef _WIN32
	std::u16string toUtf16(std::wstring s);
	std::wstring toWide(std::u16string_view s);
#endif

	struct ci_wchar_traits : std::char_traits<wchar_t>
	{
		static bool eq(wchar_t c1, wchar_t c2) { return std::towupper(c1) == std::towupper(c2); }
		static bool ne(wchar_t c1, wchar_t c2) { return std::towupper(c1) != std::towupper(c2); }
		static bool lt(wchar_t c1, wchar_t c2) { return std::towupper(c1) <  std::towupper(c2); }
		static int compare(const wchar_t* s1, const wchar_t* s2, size_t n)
		{
			return
#ifdef _WIN32
				_wcsnicmp
#else
				wcsncasecmp
#endif
				(s1, s2, n);
		}
		static const wchar_t* find(const wchar_t* s, size_t n, wchar_t a)
		{
			for (size_t i = 0; i < n; ++i)
				if (eq(s[i], a))
					return s + i;
			return s + n;
		}
	};

	using ci_wstring_view = std::basic_string_view<wchar_t, ci_wchar_traits>;
}