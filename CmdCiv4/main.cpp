#include "CvApp.h"
#include "CommandLine.h"
#include "AppGameSetupWindow.h"

#include <CommonStuff/StringConversion.h>

#include <iostream>

#ifndef _WIN32
#include <linux/prctl.h>
#include <sys/prctl.h>
#endif

using namespace cvengine;

int main(int argc, char* argv[])
{
	// Call this first!
	std::ios::sync_with_stdio(false);

#ifndef _WIN32
	// Allow GDB to attach - https://man7.org/linux/man-pages/man2/pr_set_ptracer.2const.html
	prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY);
#endif

	if (heck::convertUtf8ToWide(heck::convertWideToUtf8(L"\x1b[↑")) != L"\x1b[↑")
	{
		for (const uint8_t c : heck::convertWideToUtf8(L"\x1b[↑"))
			printf("%#02x\n", c);
		printf("-\n");
		for (const wint_t c : heck::convertUtf8ToWide(heck::convertWideToUtf8(L"\x1b[↑")))
			printf("%#x\n", c);
		throw std::runtime_error("String conversion isn't working.");
	}

	AppStartupConfig appStartConfig = parseCommandLine(argc, argv);

	if (!appStartConfig.save.empty())
	{
		appStartConfig.modRelPath = cvengine::app::extractModRelPathFromSave(appStartConfig.save);
		std::wclog << L"Overriding mod with " << std::quoted(appStartConfig.modRelPath) << L" from save file." << std::endl;
	}

	CvApp& app = CvApp::getInstance();

	// Start CvApp now.
	app.start(appStartConfig);


	//gGlobals.getLogging() = true;
	//gGlobals.getRandLogging() = true;
	//gGlobals.getSynchLogging() = true;
	
	const struct CommonEngineDestroyer
	{
		~CommonEngineDestroyer()
		{
			shutdownCommonEngine();
		}
	} dtor{};

	app.startUI();

	if (appStartConfig.save.empty())
	{
		if (appStartConfig.isAutostart)
		{
			// Bit of a hack. Invoke UI and immediately destroy the UI.
			AppGameSetupWindow{app}.launch();
		}
		else
			app.deferMainMenu();
	}
	else
		app.deferLoadGame(appStartConfig.save);

	return app.run();
}
