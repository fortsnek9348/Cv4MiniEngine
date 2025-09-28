#pragma once

#include "CvGInterfaceScreen.h"

class PythonScreen : public CvGInterfaceScreen
{
public:
	using CvGInterfaceScreen::CvGInterfaceScreen;

	virtual void rebuildPythonScreen() override;
	virtual void updateFromGameState(hecktui::Window&) override;

private:
};