#pragma once

#include <CvEnums.h>

#include <string>

namespace cvengine
{
	struct AppGameSetupMapScriptInfo
	{
		std::string moduleName;
		//std::wstring descName;

		bool isClimateMap() const;
		bool isSeaLevelMap() const;
		std::wstring getCustomMapOptionName(int i) const;
		int getNumCustomMapOptionValues(int i) const;
		std::wstring getCustomMapOptionChoiceName(int i, CustomMapOptionTypes choice) const;
		bool isRandomisableCustomOption(int i) const;
	};
}