## Cv4MiniEngine

An x64 engine reimplementation of Civlization IV in the terminal, with optional DLL enhancements.

#### Usage

* BUG Mod is recommended. Expected install location is `My Games\Beyond the Sword\CustomAssets`.
* If a release is available, unzip it somewhere. But initial release is definitely experimental. See "Troubleshooting".
* If you need to build it yourself, see `BUILDING.md`.
* Read all usage information below.
* An "installed" version (a release or the output of the build install target) has multiple launchers.
	* These are launch scripts that setup the DLL path, so you can't run the executable directly.
	* The Regular version is for when you just want to play regular-sized maps. The Civ4 DLL is only minimally modified to get it to work.
	* The Enhanced version is more experimental as it adds a collection of non-trivial optimisations to the DLL, but is practically necessary for maps with hundreds of thousands of plots.
	* You may use the Enhanced AVX512 version only if your CPU supports AVX512.

#### Configuration

* Config is stored in your `My Games\Beyond the Sword\CivilizationIV.ini` if on Windows.
* On Linux, Cv4MiniEngine should conform to XDG
	* Saves and replays are stored in `~/.local/share/Beyond the Sword`, by default.
	* Config is stored in `~/.config/Beyond the Sword`, by default.
	* These can be overriden by XDG and `HOME` environment variables.
* Saves and replays are saved to new directories, to keep files separate.
* XML cache is stored in `AppData` or `~/.cache`.
* If `[CV4MINIENGINE]` config is missing, you will be prompted for Civ4 root path on startup.
* Other settings will be added to the INI. All enhancements are enabled by default, if running an enhanced DLL.
* Player options, the Civ4 user profile, is saved to `Cv4MiniEngine Profile.txt` within the config directory.

#### What works

* Single player custom game
* Saving, loading, autosaves
* Diplomacy, city screen, other screens
* Production queuing
* Victory
* Sounds
* Signs
* Minimap
* Event log
* Some player options
* Most Civ4 keyboard shortcuts (`G` for move, `F4` for foreign advisor, ...)

#### What doesn't work

* Multiplayer
* Advanced start
* Unit movement animations
* Great Wall visual
* Map display buttons such as grid lines, globe view, resource icons...
* WorldBuilder
* Music
* Unit order queuing (console input limitation)
* City waypoints
* The right-click menu
* BUG Options UI
* BUG's domestic advisor customisation
* Save/load production queue

#### Troubleshooting

* Crashes and bugs are LIKELY.
* Check log.
* If you are a dev, see `DEBUGGING.md`.
* If you are not a dev, then here be dragons.
* If a python error happens, it should be logged.
* Autosaves should work, so don't be too worried.
* DLL enhancements will be a source of in-game bugs, as they contain plenty of non-trivial code. If they cause problems, they can be disabled.
* End Turn key is quirky. You may need to press enter multiple times to end the turn if you have automated units. This is because the TUI is action-based whereas automated movements would happen on a timer in vanilla.

#### TUI

* IMPORTANT: Terminals hijack some inputs:
	* Shift+left-click. Instead, some buttons will translate right click to shift+left-click. This includes city production queuing and plot unit list buttons.
	* Middle click. Instead, use `C` instead to center on unit.
	* Some function keys.
	* On Windows, builds will use the Win32 console input API instead of ANSI, which should have better input capability.
	* Run HeckTUIInputTest to see if you're getting the inputs you want.
* Known to work in: Windows Console, Windows Terminal, Xfce Terminal.
	* When it comes to Unicode support, Windows Console is lacking, but the Unicode art and symbols so far are designed for Windows Console with the Consolas font.
* UI should mostly function like Civ4 UI.
* There is no keyboard navigation.
* Responds to console resize.
* Can sort tables by column.
* Use your terminal's fullscreen function.

#### TUI World View

* Use the mouse wheel to zoom in/out.
* Arrow keys to move the camera.
* Can place signs with Alt+S.
* Click and drag units.

#### Logging

The main log is printed to `std::[w]c[log|err]`, which is "log.txt" by default.

Configured through the INI.

If you output to stderr, then you must redirect to some location other than the terminal that the UI is on.

#### VFS

* Cv4MiniEngine has a few script overrides in `CustomAssets` that take precedence over all other files.
* Cv4MiniEngine also has its own `PublicMaps` folder for Pangaea that has improved performance for gigantic-sized maps.

#### Mechanics Compared to Vanilla BTS

* Most DLL enhancements will change AI decisions slightly. 
	* The pathfinder may return different "optimal" paths, and there may be places where AI decision tie-breaking is different.
	* Parallel loops can't use `CvRandom` directly.
	* Determinism remains paramount. Non-deterministic AI is a bug.
* Regular builds should have identical mechanics. Ie: Game state hashes should match.
	* With the provided script patches that fix non-determinism in the original scripts.
* Random choices in game setup should be the same.
* Map generation should be the same as vanilla with identical settings.
* Custom game UI has new settings:
	* Explicitly choose the number of players (instead of manually changing the slot status of players).
	* Explicit map and sync seed. Sync seed is used for random settings in game setup, and AI decisions.
	* Map size multiplier.
* `CvPlayer::startingPlotRange` has been changed to scale differently when using a world size multiplier. This is to avoid starting plot range from becoming bigger than the map.
* There is a DLL enhancement INI setting to limit the number of barbarians, which you will need if playing on a gigantic map at high difficulty.
* Diplo is fixed so that AI can offer peace or capitulation.

#### Python

* Python 2.7 is used instead of Python 2.4.
* pybind11 is used instead of Boost 1.32.0.
	* There is an implementation quirk: pybind11 only has strong enums for this version of python. So the bindings add operators and conversions to the enums to make scripts work. Watch out for script errors!
* Python 2.7 is more strict about character encoding, so `# coding=cp1252` is inserted at the top of all scripts.

#### Gigantic Map Sizes

* You may use the map size multiplier in custom game setup to generate truly monsterous maps that are totally impractical, for mere mortals.
* Generating gigantic maps may take a few minutes on even decent PCs.
* Use `PangaeaForGiganticMaps` instead of `Pangaea`.
* Without DLL enhancements, turn times will quickly spiral out of control.
* With DLL enhancements, turn times will still spiral out of control, just not as much. Expect 20s turn times after a few hundred turns on 5x. But this is still much faster than the Firaxis DLL.

#### DLL Enhancements

For gigantic map sizes, use an Enhanced DLL build for extra performance

Enhancements are configured through INI settings.

These include:

* Utilisation of AVX-512, if enabled.
* SIMD-accelerated A* for single-unit AI pathing.
* Multi-threaded SIMD-accelerated found value calculation.
* Parallel barbarian unit update.
* Trivial parallelisation of various loops in AI code. Mostly all-plots loops.
* Localised scouting AI (don't path to every plot on the map).

See your INI file, after it's been updated.

#### Modding

* Only limited forms of modding are possible.
* Of course, you can't use art assets or fonts in this TUI.
* The "world view" hardcodes all art. That includes things like bonuses and route types.
* DLL Enhancements may make assumptions about things.
* Map scripts that have UI, such as "Full_of_Resources", won't work.
* Really, it is intended that a graphical engine reimplementation would be what supports the bigger mods.
* Explicitly supported:
	* BUG Mod. Even works on Linux with bundled patch.
	* Next War

#### Compatibility

* Vanilla BTS save games are compatible with Cv4MiniEngine.
* Cv4MiniEngine save games are NOT compatible with Vanilla BTS, due to missing checksum.
* Saved replays are compatible in both directions.

