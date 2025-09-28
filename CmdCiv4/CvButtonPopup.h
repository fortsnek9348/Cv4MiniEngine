#pragma once

#include "CvPopup.h"

#include <CvPopupInfo.h>

class CvButtonPopup : public CvPopup
{
public:
	explicit CvButtonPopup(std::unique_ptr<CvPopupInfo> popupInfo);

	CvPopupInfo& getPopupInfo() noexcept;

	// static because this popup may or may not be deleted and ownership may or may not be transferred.
	static void launch(std::unique_ptr<CvButtonPopup> popup);

	virtual void onPopupDisplayed() override;
	virtual void submit(const PopupReturn& result) override;
	virtual void escCancel() override;

private:
	std::unique_ptr<CvPopupInfo> mPopupInfo;
};