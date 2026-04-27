#include "CvApp.h"
#include "CommandLine.h"
#include "GeneratePlayerBotHeader.h"
#include "AppGameSetupWindow.h"

#include <Cv4CommonEngineLib/CvTranslator.h>
#include <Cv4CommonEngineLib/MyCvDLLPython.h>
#include <Cv4CommonEngineLib/EngineSpecificsHeader.h>

#include <CvGlobals.h>
#include <CvXMLLoadUtility.h>
#include <CvGameTextMgr.h>
#include <CvPlayerAI.h>
#include <CvRandom.h>
#include <CvInitCore.h>
#include <CvArtFileMgr.h>
#include <CvEventReporter.h>
#include <CvGameAI.h>
#include <CvTeamAI.h>
#include <CvMap.h>
#include <CvMapGenerator.h>
#include <GeneratePlayerBotHeader.h>

#include <HeckTextUI/BasicControls.h>

#include <CommonStuff/StringConversion.h>
#include <CommonStuff/range.h>

#include <iostream>
#include <sstream>
#include <chrono>

#ifndef _WIN32
#include <linux/prctl.h>
#include <sys/prctl.h>
#endif

using heck::range;
using namespace cvengine;

void engine_specific::deferLoadGame(const std::filesystem::path& path)
{
	CvApp::getInstance().deferLoadGame(path);
}

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

	// Start CvApp now.
	CvApp::getInstance().start(appStartConfig);

	cvengine::app::initCommon(); // python initialised here

	// -generate_player_bot_game_binding "..\PlayerBotGameBinding"
	if (!appStartConfig.generatePlayerBotGameDefsDir.empty())
	{
		cvbot::generatePlayerBotGameBindingHeaders(std::filesystem::path(appStartConfig.generatePlayerBotGameDefsDir));
	}

	//gGlobals.getLogging() = true;
	//gGlobals.getRandLogging() = true;
	//gGlobals.getSynchLogging() = true;
	
	const struct PythonDestroyer
	{
		~PythonDestroyer()
		{
			// Python needs to be shutdown before main ends, for some reason. Probably static destruction order issues.
			MyCvDLLPython::shutdownPython();
		}
	} dtor{};

	CvApp::getInstance().startUI();

	if (appStartConfig.save.empty())
	{
		if (appStartConfig.isAutostart)
		{
			// Bit of a hack. Invoke UI and immediately destroy the UI.
			AppGameSetupWindow{}.launch();
		}
		else
			CvApp::getInstance().deferMainMenu();
	}
	else
		CvApp::getInstance().deferLoadGame(appStartConfig.save);

	return CvApp::getInstance().run();
}
