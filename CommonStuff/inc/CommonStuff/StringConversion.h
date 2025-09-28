#pragma once

#include <string>
#include <string_view>

namespace heck
{
	std::string toUtf8(std::wstring_view s);
	std::wstring toWide(std::string_view s);
#ifndef _WIN32
	std::u16string toUtf16(std::wstring s);
	std::wstring toWide(std::u16string_view s);
#endif
}