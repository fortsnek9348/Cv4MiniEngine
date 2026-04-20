#include "CvDiplomacyScreen.h"

#include "CvDiploParameters.h"
#include "CvGlobals.h"
#include "CvPopupInfo.h"

CvDiplomacyScreen::CvDiplomacyScreen(std::unique_ptr<CvDiploParameters> diploParams)
	: mController(std::move(diploParams))
{
	gGlobals.setDiplomacyScreen(this);

	mController.startDiplo(false);
}

CvDiplomacyController& CvDiplomacyScreen::getController()
{
	return mController;
}

void CvDiplomacyScreen::regreet()
{
	mController.regreet();
}

void CvDiplomacyScreen::endDiplomacy()
{
	mController.endDiplomacy();
}

CvDiplomacyScreen::~CvDiplomacyScreen() noexcept
{
	assert(gGlobals.getDiplomacyScreen() == this);
	gGlobals.setDiplomacyScreen(nullptr);
}