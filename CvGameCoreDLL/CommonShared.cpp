#include "CommonShared.h"
#include "CommonDLLOnly.h"
#include "PlayerBotEnablement.h"

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

std::span<const std::string_view> getCvGameCoreDLLConfigStrings()
{
	static constexpr std::array k = std::to_array<std::string_view>({
#if defined(_MSC_VER) && defined(__clang__) 
		"clang-cl " __VERSION__,
#elif defined(_MSC_VER)
		"MSVC " STR(_MSC_FULL_VER),
#elif defined(__clang__)
		"Clang " __VERSION__,
#elif defined(__GNUC__)
		"G++ " __VERSION__,
#else
		"Unknown compiler",
#endif
#if __AVX512F__
		"AVX-512",
#else
		"AVX2",
#endif
#if !defined(NDEBUG) || defined(_DEBUG)
		"Debug",
#else
		"Release",
#endif
#if ENABLE_GAMECOREDLL_ENHANCEMENTS
		"Perf-Enhanced DLL",
#else
		"Regular DLL",
#endif
		});
	return k;
}

bool hasCvGameCoreDLLPlayerBotSupport()
{
	return !!ENABLE_PLAYER_BOT;
}