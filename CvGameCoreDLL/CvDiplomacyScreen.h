#pragma once

#include "CvDiplomacyController.h"

// Minimal base class to support both TUI and player bot diplo.
class DllExportForInterface CvDiplomacyScreen
{
public:
	explicit CvDiplomacyScreen(std::unique_ptr<CvDiploParameters> diploParams);

	CvDiplomacyController& getController();

	// Called by NetKillDeal popup.
	virtual void regreet();
	// Called by CvNetChangeWar
	virtual void endDiplomacy();

	virtual ~CvDiplomacyScreen() noexcept;

private:
	CvDiplomacyController mController;
};