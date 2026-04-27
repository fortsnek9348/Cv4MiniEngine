#include "PythonScreen.h"

#include <Cv4CommonEngineLib/MyCvDLLPython.h>

#include <CvGameCoreDLL/CyArgsList.h>

using namespace cvengine;

void PythonScreen::rebuildPythonScreen()
{
	clear();
	switch (getKind())
	{
	case cvengine::ECvScreen::MAIN_INTERFACE:
		(void)MyCvDLLPython().callFunction("CvScreensInterface", "showMainInterface", nullptr);
		break;
	default:
		break;
	}
}

void PythonScreen::updateFromGameState(hecktui::Window&)
{
	// CvScreensInterface has a `update`, `forceScreenUpdate`, and `forceScreenRedraw`.
	// Just call all of them.
	{
		CyArgsList args;
		args.add(int(getKind()));
		args.add(0.0f); // Dummy value for update timestep
		(void)MyCvDLLPython().callFunction("CvScreensInterface", "update", args.makeFunctionArgs());
	}
	{
		CyArgsList args;
		args.add(int(getKind()));
		(void)MyCvDLLPython().callFunction("CvScreensInterface", "forceScreenUpdate", args.makeFunctionArgs());
	}
	{
		CyArgsList args;
		args.add(int(getKind()));
		(void)MyCvDLLPython().callFunction("CvScreensInterface", "forceScreenRedraw", args.makeFunctionArgs());
	}
}
