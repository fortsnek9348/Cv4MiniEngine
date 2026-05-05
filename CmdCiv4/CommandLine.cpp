#include "CommandLine.h"

#include <CLI11.hpp>

using namespace cvengine;

// -bot_autorun
// When in-game, automatically run the bot until victory or bot error.

AppStartupConfig cvengine::parseCommandLine(int argc, char** argv)
{
	CLI::App app{ "Cv4MiniEngine - Civilization 4 in the console" };
	argv = app.ensure_utf8(argv);

	AppStartupConfig config;

	app.add_option("savefile", config.save, "Load this save on startup.");
	app.add_flag("--autostart", config.isAutostart, "Auto-start a single-player game using INI settings, if not loading a save.");
	app.add_option("--mod", config.modRelPath, "BTS-relative path to a mod to load.");
	app.add_option("--bot", config.botPath, "Use the specified bot DLL/so for the human player.");
	app.add_option("--generate-player-bot-game-binding", config.modRelPath, "Generate source files for the PlayerBotGameBinding lib in the specified directory.");

	try
	{
		app.parse(argc, argv);
	}
	catch (const CLI::ParseError& e)
	{
		std::exit(app.exit(e));
	}

	return config;
}
