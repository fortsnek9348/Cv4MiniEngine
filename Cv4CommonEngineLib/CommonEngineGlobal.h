#pragma once

#include "inc/Cv4CommonEngineLib/CommonEngine.h"

#include <memory>

namespace cvengine
{
	class CvVFS;

	extern CommonEngineConfig gCommonEngineConfig;
	extern std::unique_ptr<CvVFS> gVFS;
}