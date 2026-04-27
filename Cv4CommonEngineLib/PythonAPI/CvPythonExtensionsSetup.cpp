#include "../inc/Cv4CommonEngineLib/CvPythonExtensionsSetup.h"
#include "../inc/Cv4CommonEngineLib/CvEngineEnums.h"
#include "../inc/Cv4CommonEngineLib/CvVFS.h"
#include "../CommonEngineGlobal.h"
#include "CyPopupReturn.h"
#include "CyCamera.h"
#include "CyCommon.h"
#include "CyDiplomacy.h"
#include "CyEngine.h"
#include "CyFractal.h"
#include "CyGlobeLayerManager.h"
#include "CyInterface.h"
#include "CyPopup.h"
#include "CyPopupInfo.h"
#include "CyPythonMgr.h"
#include "CyStatistics.h"
#include "CyTranslator.h"
#include "CyUserProfile.h"

#include <CvGameCoreDLL/CommonShared.h>
#include <CvGameCoreDLL/CvRandom.h>

#include <pybind11/stl.h>

#include <CommonStuff/range.h>

#include <iostream>

using heck::range;

// importing
DllExport void DLLPublishToPython(pybind11::module m);

//PYBIND11_EMBEDDED_MODULE(CvPythonExtensions, m)
void cvengine::setupCvPythonExtensionsModule(pybind11::module m)
{
	m.attr("true") = true;
	m.attr("false") = false;

	m.def("NiTextOut", [](std::string_view text) {
		std::clog << "NiTextOut: " << text << std::endl;
	});

	static std::filesystem::path gLastLoadedModuleVfsPath;

	m.def("loadImportModuleSourceCode", [](const std::string& fullName) {
		// Must return "bytes". Otherwise, I guess pybind11 passes the string along as unicode, which can't contain an encoding override.
		return pybind11::bytes(cvengine::gVFS->loadPythonCodeIfExists(fullName, gLastLoadedModuleVfsPath).value_or(std::string()));
	});

	m.def("getLastImportModuleVfsPath", [] {
		return gLastLoadedModuleVfsPath.native();
	});

	m.def("getUserProfileDirectory", [] {
		return cvengine::gCommonEngineConfig.userConfigDirPath.native();
		});

	cvengine::registerEnumsWithPython(m);

	pybind11::class_<CyTranslator>(m, "CyTranslator")
		.def(pybind11::init())
		.def("changeTextColor", &CyTranslator::changeTextColor)
		.def("getColorText", &CyTranslator::getColorText)
		.def("getObjectText", &CyTranslator::getObjectText)
		.def("getText", &CyTranslator::getText)
		.def("stripHTML", &CyTranslator::stripHTML)
		;

	m.def("shuffleList", [](int num, CvRandom& rng, pybind11::list list) {
		std::vector<int> v(std::from_range, range(num));
		// https://en.cppreference.com/w/cpp/algorithm/random_shuffle.html#Version_2
		for (int i = num - 1; i > 0; --i)
			std::swap(v[i], v[rng.get(static_cast<unsigned short>(i + 1), "shuffleList")]);
		for (const int i : range(num))
			list[i] = v[i];
		});

	CyInterface::registerWithPython(m);
	CyDiplomacy::registerWithPython(m);
	CyUserProfile::registerWithPython(m);
	CyFractal::registerWithPython(m);
	CyCamera::registerWithPython(m);
	CySign::registerWithPython(m);
	CyEngine::registerWithPython(m);
	CyPythonMgr::registerWithPython(m);
	
	CyGlobeLayerManager::registerWithPython(m);
	CyPopup::registerWithPython(m);
	CyPopupInfo::registerWithPython(m);
	CyPopupReturn::registerWithPython(m);
	CyStatistics::registerWithPython(m);

	DLLPublishToPython(m);
}

