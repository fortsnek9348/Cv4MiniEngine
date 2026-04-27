#include "inc/Cv4CommonEngineLib/CvButtonPopup.h"

#include <CvGameCoreDLL/CvDLLButtonPopup.h>

CvButtonPopup::CvButtonPopup(std::unique_ptr<CvPopupInfo> popupInfo) : mPopupInfo(std::move(popupInfo))
{
}

CvPopupInfo& CvButtonPopup::getPopupInfo() noexcept
{
	return *mPopupInfo;
}

void CvButtonPopup::launch(std::unique_ptr<CvButtonPopup> popup)
{
	switch (popup->getPopupInfo().getButtonPopupType())
	{
	case BUTTONPOPUP_MAIN_MENU:
		popup->enableEscCancel = true;
		break;
	default:
		popup->enableEscCancel = false;
		break;
	}
	
	CvButtonPopup* const rawPtr = popup.release();
	if (!CvDLLButtonPopup::getInstance().launchButtonPopup(rawPtr, rawPtr->getPopupInfo()))
		delete rawPtr;
}

void CvButtonPopup::onPopupDisplayed()
{
	CvDLLButtonPopup::getInstance().OnFocus(this, *mPopupInfo);
}
void CvButtonPopup::submit(const PopupReturn& result)
{
	// OnOkClicked doesn't seem to modify this.
	PopupReturn mutableResult = result;

	CvDLLButtonPopup::getInstance().OnOkClicked(this, &mutableResult, *mPopupInfo);
}
void CvButtonPopup::escCancel()
{
	// Do nothing.
}
