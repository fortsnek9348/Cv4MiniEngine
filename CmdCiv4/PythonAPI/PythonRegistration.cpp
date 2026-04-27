#include <Cv4CommonEngineLib/EngineSpecificsHeader.h>
#include "CyPopup.h"
#include "CyTuiDialog.h"
#include "CyGInterfaceScreen.h"

void cvengine::engine_specific::registerEngineSpecificPythonBindings(pybind11::module_ m)
{
	CyPopup::registerWithPython(m);
	CyGInterfaceScreen::registerWithPython(m);
	CyTuiDialog::registerWithPython(m);
}