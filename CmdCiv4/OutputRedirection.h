#pragma once

namespace cvengine
{
	void initDebugOutput();

	// Thread-safe. Use CvDLLUtility::logMemState from the DLL.
	void outputDebugString(const char* str);
	void outputDebugString(const wchar_t* str);
}