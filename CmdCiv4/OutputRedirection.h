#pragma once

namespace cvengine
{
	class IniData;

	void initDebugOutput(IniData& ini);

	// Thread-safe. Use CvDLLUtility::logMemState from the DLL.
	void outputDebugString(const char* str);
	void outputDebugString(const wchar_t* str);
}