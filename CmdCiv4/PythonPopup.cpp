#include "PythonPopup.h"

#include <Cv4CommonEngineLib/CyPopupReturn.h>
#include <Cv4CommonEngineLib/MyCvDLLPython.h>

#include <CvPopupReturn.h>

#include <CvGameAI.h>
#include <CvGlobals.h>
#include <CvPlayerAI.h>
#include <CyArgsList.h>

#include <pybind11/cast.h>

using namespace cvengine;

PythonPopup::PythonPopup(int eventId, EventContextTypes eventCtxType) : eventId(eventId), eventCtxType(eventCtxType)
{
}

void PythonPopup::onPopupDisplayed()
{
	// Is there a python focus callback? There is for BUTTONPOPUP_PYTHON.
}

void PythonPopup::submit(const PopupReturn& result)
{
	// CvEventInterface.py, CvEventManager.py
	// def applyEvent(argsList):
	// 'Apply the effects of an event '
	// context, playerID, netUserData, popupReturn = argsList
	// netUserData is the popup user data

	if (eventId < 0)
	{
		// Options screen language change popup.
		// `applyEvent` is probably not called.
		return;
	}

	const PlayerTypes playerId = gGlobals.getGame().getActivePlayer();

	CyArgsList args;
	args.add(eventId);
	args.add(playerId);
	args.push_back(userData.ptr());
	userData.inc_ref();
	const pybind11::object resultObject = pybind11::cast(CyPopupReturn(&result));
	if (!resultObject)
		std::abort();
	resultObject.inc_ref();
	args.push_back(resultObject.ptr());

	// makeFunctionArgs will dec ref

	(void)MyCvDLLPython().callFunction("CvEventInterface", "applyEvent", args.makeFunctionArgs());
}

void PythonPopup::escCancel()
{
}