#pragma once

#include <CommonStuff/CompilerMacros.h>

#include <format>
#include <source_location>

namespace cvengine
{
	enum class ELogLevel
	{
		Verbose,
		Debug,
		Info,
		Warning,
		Error,
	};

	inline constexpr ELogLevel kGlobalLogLevel = ELogLevel::Info;

	template<class... T>
	struct FormatStringWithSourceLocation : std::format_string<T...>
	{
		std::source_location loc;

		consteval FormatStringWithSourceLocation(const auto& fmt, std::source_location loc = std::source_location::current()) : std::format_string<T...>(fmt), loc(loc)
		{
		}
	};

	template<ELogLevel kLogLevel, class... T>
		requires (kLogLevel >= kGlobalLogLevel)
	inline void log(FormatStringWithSourceLocation<T...> fmt, const T&... args)
	{
		if constexpr (kLogLevel >= kGlobalLogLevel)
		{
			extern void _internalLog(ELogLevel level, const std::source_location& loc, const std::string_view& fmt, std::format_args&& formatArgs);
			_internalLog(kLogLevel, fmt.loc, fmt.get(), std::make_format_args(args...));
		}
	}

	template<ELogLevel kLogLevel, class... T>
		requires (kLogLevel < kGlobalLogLevel)
	constexpr void log(FormatStringWithSourceLocation<std::type_identity_t<T>...>, const T&...)
	{
	}

	//

	template<class... T>
		requires (kGlobalLogLevel <= ELogLevel::Verbose)
	constexpr void logVerbose(FormatStringWithSourceLocation<std::type_identity_t<T>...> fmt, const T&... args)
	{
		return log<ELogLevel::Verbose>(fmt, args...);
	}

	template<class... T>
		requires (kGlobalLogLevel <= ELogLevel::Debug)
	constexpr void logDebug(FormatStringWithSourceLocation<std::type_identity_t<T>...> fmt, const T&... args)
	{
		return log<ELogLevel::Debug>(fmt, args...);
	}

	template<class... T>
		requires (kGlobalLogLevel <= ELogLevel::Info)
	constexpr void logInfo(FormatStringWithSourceLocation<std::type_identity_t<T>...> fmt, const T&... args)
	{
		return log<ELogLevel::Info>(fmt, args...);
	}

	template<class... T>
		requires (kGlobalLogLevel <= ELogLevel::Warning)
	constexpr void logWarning(FormatStringWithSourceLocation<std::type_identity_t<T>...> fmt, const T&... args)
	{
		return log<ELogLevel::Warning>(fmt, args...);
	}

	template<class... T>
		requires (kGlobalLogLevel <= ELogLevel::Error)
	constexpr void logError(FormatStringWithSourceLocation<std::type_identity_t<T>...> fmt, const T&... args)
	{
		return log<ELogLevel::Error>(fmt, args...);
	}

	//

	template<class... T>
		requires (kGlobalLogLevel > ELogLevel::Verbose)
	HECK_FORCEINLINE constexpr void logVerbose(const T&...)
	{
	}

	template<class... T>
		requires (kGlobalLogLevel > ELogLevel::Debug)
	HECK_FORCEINLINE constexpr void logDebug(const T&...)
	{
	}

	template<class... T>
		requires (kGlobalLogLevel > ELogLevel::Info)
	HECK_FORCEINLINE constexpr void logInfo(const T&...)
	{
	}

	template<class... T>
		requires (kGlobalLogLevel > ELogLevel::Warning)
	HECK_FORCEINLINE constexpr void logWarning(const T&...)
	{
	}

	template<class... T>
		requires (kGlobalLogLevel > ELogLevel::Error)
	HECK_FORCEINLINE constexpr void logError(const T&...)
	{
	}
}