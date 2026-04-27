#pragma once

#include "inc/Cv4CommonEngineLib/CvPopup.h"

#include <CvGameCoreDLL/CvEnums.h>

#include <pybind11/pytypes.h>

namespace cvengine
{
	class PythonPopup : public CvPopup
	{
	public:
		explicit PythonPopup(int eventId, EventContextTypes eventCtxType);

		virtual void onPopupDisplayed() override;
		virtual void submit(const PopupReturn& result) override;
		virtual void escCancel() override;

		int eventId = 0;
		EventContextTypes eventCtxType{};

		pybind11::tuple userData;
	};
}