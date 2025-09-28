#include "Logger.h"

#include <array>
#include <iostream>
#include <utility>

namespace cmdciv4
{
	void _internalLog(ELogLevel level, const std::source_location& loc, const std::string_view& fmt, std::format_args&& formatArgs)
	{
		static constexpr auto kLevelNames = std::to_array<std::string_view>({
			"Verbose",
			"Debug",
			"Info",
			"Warning",
			"Error",
		});
		//std::print(stdout, "CmdCiv4 log {}, {}: ", kLevelNames[std::to_underlying(level)], loc.function_name());
		//std::vprint_unicode(stdout, fmt, formatArgs);
		//std::putchar('\n');
		std::clog <<  "CmdCiv4 log " << kLevelNames[std::to_underlying(level)] << ' ' << loc.function_name() << ": " << std::vformat(fmt, formatArgs) << std::endl;
	}
}