### Dev/Debug Guidelines

* Run Cv4MiniEngine in a debugger. Use a debug build to enable all the asserts.
	* On Linux, releases have a GDB launcher.
* There are a lot of unimplemented functions in the various DLL interface classes. Crashes in them are expected.
* If you get the fancy Civ4 assert dialog, then this may be a false positive caused by, say, a BUG Mod script. Or it may be a real bug.
	* If you switch between debug/release, the DLL will complain that the XML cache doesn't match up.
* GiganticMapsOptimisationsLib contains a lot of built-in verification functionality, typically enabled by constants at the top of files.
	* Verification can be very very slow. Especially for found values and pathing.
	* Path verification can not check for exact path cost match even with the FAStar fix because the Civ4 path cost and step function possibly produce inconsistant A* evaluation and depend on visit order.
	* Ctrl+Shift+A will enable AI autoplay for 10,000 turns. It will stop at victory. AI autoplay is very useful for checking the various caching subsystems.
* If you get a crash in A* path reconstruction, then that is not entirely surprising. Civ4 pathing logic may create problematic A* results that confuse path reconstruction in exceptionally rare cases. This can be fixed if it becomes a problem, at a cost to performance.
* If you get a crash with python in the call stack, then call `dumpPythonStackTrace()` from the VS debugger to get a python call stack dump.
* PPM images are used to debug systems that work on the map. You can open them with GIMP.
* Do not mix MSVC and Clang-CL `__vectorcall`. They should be ABI-compatible, but they're not.
* Sync checksum comparisons (`CvGame::calculateSyncChecksum`, logging enabled by `kEnableSyncChecksumLogging`)
	* Useful for determinism checking.
	* Use AI autoplay and `kEnableSyncChecksumLogging` to find when deviation first occurs.
	* To check for data races, change `parallelForEachN` to use a serial loop, then compare checksums.
	* Some DLL enhancements will change game state hashes.
* Plot danger verification
	* If unit plot danger cache thinks there is danger at a plot but there isn't, then log the attacker and check plot danger conditions in verification to see what event was missed.

#### Game State Hash Verification

* Ensure script fixes are active for both the original Firaxis engine and Cv4MiniEngine.
* Start a new game in the Firaxis engine and let it run with AI autoplay until victory. Note down the final game state hash.
* Do the same with Cv4MiniEngine (Regular). Verify the hashes match.
* If you got a hash mismatch, history says it's likely caused by undefined behaviour because CvGameCoreDLL code should be largely unchanged in the regular build.
	* Have had unsequenced RNG use, a non-deterministic random event function, and an uninitialised script return variable.
* If it matches, move onto an enhanced build with all enhancements disabled. Verify again.
* Then, enable all game hash-preserving enhancements. Verify again.
* Further verification would be possible with changes. Some DLL enhancements can only preserve hash when `GeneralFixes` is enabled and some RNG uses are changed to be compatible with a parallel-loop.

#### Testing Tips

* Change INI to keep lots and lots of autosaves around.
* Find the latest autosave that exhibits the problem.
* In case of divergent game state where you have no idea what causes the problem, set `ENABLE_DIVERGENCE_CHECKPOINT_LOGGING` and tweak logging code.
* If you suspect bugs in DLL enhancements, enable verification.
