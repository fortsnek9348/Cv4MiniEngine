#include "AppGameSetupMapScript.h"

#include <CvDLLPythonIFaceBase.h>
#include <CvDLLUtilityIFaceBase.h>
#include <CvGlobals.h>
#include <CyArgsList.h>

bool AppGameSetupMapScriptInfo::isClimateMap() const
{
	long value = 1;
	(void)gDLL->getPythonIFace()->callFunction(moduleName.c_str(), "isClimateMap", nullptr, &value);
	return !!value;
}

bool AppGameSetupMapScriptInfo::isSeaLevelMap() const
{
	long value = 1;
	(void)gDLL->getPythonIFace()->callFunction(moduleName.c_str(), "isSeaLevelMap", nullptr, &value);
	return !!value;
}

std::wstring AppGameSetupMapScriptInfo::getCustomMapOptionName(int i) const
{
	CvWString name;
	CyArgsList args;
	args.add(i);
	if (!gDLL->getPythonIFace()->callFunction(moduleName.c_str(), "getCustomMapOptionName", args.makeFunctionArgs(), &name))
		name = L"Custom Map Option " + std::to_wstring(i);
	return name;
}

int AppGameSetupMapScriptInfo::getNumCustomMapOptionValues(int i) const
{
	long n = 1;
	CyArgsList args;
	args.add(i);
	(void)gDLL->getPythonIFace()->callFunction(moduleName.c_str(), "getNumCustomMapOptionValues", args.makeFunctionArgs(), &n);
	return n;
}

std::wstring AppGameSetupMapScriptInfo::getCustomMapOptionChoiceName(int i, CustomMapOptionTypes choice) const
{
	CvWString name;
	CyArgsList args;
	args.add(i);
	args.add(std::to_underlying(choice));
	if (!gDLL->getPythonIFace()->callFunction(moduleName.c_str(), "getCustomMapOptionDescAt", args.makeFunctionArgs(), &name))
		name = L"Custom Map Option " + std::to_wstring(i) + L" choice " +  std::to_wstring(std::to_underlying(choice));
	return name;
}

bool AppGameSetupMapScriptInfo::isRandomisableCustomOption(int i) const
{
	long value = 0;
	CyArgsList args;
	args.add(i);
	(void)gDLL->getPythonIFace()->callFunction(moduleName.c_str(), "isRandomCustomMapOption", args.makeFunctionArgs(), &value);
	return !!value;
}