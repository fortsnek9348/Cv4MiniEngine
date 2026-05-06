#include "CommandLine.h"

#include <CLI11.hpp>

CommandLine parseCommandLine(int argc, char** argv)
{
	CLI::App app{ "Cv4HeadlessMiniEngine" };
	argv = app.ensure_utf8(argv);

	CommandLine config;

	app.add_option("savefile", config.saveFile, "Load this save or scenario on startup (else, autostart from INI settings).");
	app.add_option("--mod", config.modRelPath, "BTS-relative path to a mod to load.");
	app.add_option("--bot", config.botPath, "Use the specified bot DLL/so for the human player.");
	app.add_option("--scenario-preferred-player", config.scenarioPreferredPlayer, "The desired active player ID for a scenario.");
	app.add_option("--max-turns", config.maxTurns, "The maximum number of turns to run for after the start turn.");
	app.add_flag("--initial-save", config.wantInitialSave, "Make a 'manual' save after starting a new game or loading a scenario.");
	app.add_flag("--end-save", config.wantEndSave, "Make a 'manual' save just before exiting. If the active player is no longer alive, a new active player will be picked.");

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