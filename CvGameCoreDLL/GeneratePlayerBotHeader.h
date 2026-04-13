#pragma once

#include "CommonConfig.h"

#include <filesystem>

namespace cvbot
{
	// Putting this in the DLL avoids needing to export a bunch of extra stuff from CvGlobals.
	DllExportForInterface void generatePlayerBotGameBindingHeaders(const std::filesystem::path& dir);
}