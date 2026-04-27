#pragma once

#include "inc/Cv4CommonEngineLib/MyFFile.h"

namespace cvengine::app
{
	void serialise(FFile<StdRawBinaryStream>& file);
	void deserialise(FFile<StdRawBinaryStream>& file);
}