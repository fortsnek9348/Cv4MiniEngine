#include "CvPythonExtensions.h"
#include "CyTranslator.h"
#include "CyInterface.h"
#include "CyDiplomacy.h"
#include "CyUserProfile.h"
#include "CyFractal.h"
#include "CyCamera.h"
#include "CyEngine.h"
#include "CyPythonMgr.h"
#include "CyGInterfaceScreen.h"
#include "CyTuiDialog.h"
#include "CyGlobeLayerManager.h"
#include "CyPopupInfo.h"
#include "CyPopup.h"
#include "CyPopupReturn.h"
#include "CyStatistics.h"

#include "../CvVFS.h"
#include "../CvEngineEnums.h"
#include "../Common.h"

#include <CvRandom.h>
#include <CommonShared.h>

#include <pybind11/embed.h>
#include <pybind11/stl.h>

#include <CommonStuff/range.h>

#include <iostream>

using heck::range;

DllExport void DLLPublishToPython(pybind11::module m);

PYBIND11_EMBEDDED_MODULE(CvPythonExtensions, m)
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
		return cvengine::getUserConfigDir().native();
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
	CyPopupInfo::registerWithPython(m);
	CyPopup::registerWithPython(m);
	CyPopupReturn::registerWithPython(m);
	CyStatistics::registerWithPython(m);

	DLLPublishToPython(m);

	// Stick this here. Needs WidgetTypes.
	CyGInterfaceScreen::registerWithPython(m);

	CyTuiDialog::registerWithPython(m);
}

void cvengine::unimplementedPythonFunction(std::source_location loc)
{
	throw std::runtime_error(std::string("Unimplemented CvPythonExtensions function: ") + loc.function_name());
}