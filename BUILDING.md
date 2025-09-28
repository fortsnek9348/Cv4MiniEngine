### Building

Cv4MiniEngine depends on:

* SFML 3.0.2 (for audio)
	* At least this specific version. Fixes use-after-free bug in the audio engine.
* Python 2.7.18 (final Python 2)
* tinyxml2
* zlib
* https://github.com/Benjamin-Dobell/s3tc-dxt-decompression
* If you have a GUI: nativefiledialog-extended
* Header-only: stb (for DXT compression)
* Header-only: https://github.com/pybind/pybind11/releases/tag/v2.9.2 (version must support Python 2.7)

Cv4MiniEngine also depends on the FAStar repository, as a submodule.

On Linux, you will likely need to build SFML 3, Python 2.7, and NFD. Remember to build AND install (install to anywhere).

Tested with VS 2022 MSVC and Clang-CL, and Ubuntu Clang 20.

#### Windows VS

* Clone this repository, with submodules.
* Use the provided VS solution.
* Configuration is done through the `ExternalDependencies.props` file. Change the "user macros" to point to the required directories. This can be done within the IDE. Further tweaks may be required if your lib installs have a different layout.
	* Note that the S3TC and tinyxml2 source files are referenced directly using paths from `ExternalDependencies.props`.
* If you get an ambiguous function error in pybind11 pointing to `foward_like`, just patch the headers to rename the function.

#### Windows CMake

* Clone this repository, with submodules.
* Grab the dependencies. Build them if necessary.
* Use `cmake-gui`. Fill in the variables as requested, including `CMAKE_INSTALL_PREFIX`. But you can ignore `SFML_DOC_DIR`.
* You can run from the build tree, but you'll need to set `CV4MINIENGINE_DATADIR`.

#### Linux CMake

* Clone this repository, with submodules.
* Ensure you have a very up-to-date gcc or clang, and std lib. I used `clang++-20` and `libstdc++-15`.
* Preferably, use a fast linker like `mold` or `lld`.
* `sudo apt install clang-20 mold libstdc++-15`
* `sudo apt install cmake pkg-config patch`
* `sudo apt install libtinyxml2-dev libstb-dev zlib1g-dev`
* SFML dependencies: `sudo apt install libvorbis-dev libflac-dev libogg-dev`
* `export CC=clang-20 CXX=clang++-20`
* Grab SFML 3 sources and build them:
	```
	tar -zxvf 3.0.1.tar.gz
	cd SFML-3.0.1
	mkdir BuildDebug BuildRelease BuildInstall
	cmake -DCMAKE_BUILD_TYPE=Debug   -DCMAKE_DEBUG_POSTFIX=d -DSFML_BUILD_WINDOW=OFF -DSFML_BUILD_GRAPHICS=OFF -DSFML_BUILD_NETWORK=OFF -DBUILD_SHARED_LIBS=ON -B BuildDebug   -S . && cmake --build BuildDebug   && cmake --install BuildDebug   --prefix BuildInstall
	cmake -DCMAKE_BUILD_TYPE=Release                         -DSFML_BUILD_WINDOW=OFF -DSFML_BUILD_GRAPHICS=OFF -DSFML_BUILD_NETWORK=OFF -DBUILD_SHARED_LIBS=ON -B BuildRelease -S . && cmake --build BuildRelease && cmake --install BuildRelease --prefix BuildInstall
	```
	This creates a cmake package.
* Grab S3TC DXT decompression and pybind11, and put them somewhere.
	* `git clone https://github.com/Benjamin-Dobell/s3tc-dxt-decompression.git`
	* `git clone --depth 1 --branch v2.9.2 https://github.com/pybind/pybind11.git`
* Install Python 2.7.18.
	* You can try https://askubuntu.com/questions/1527867/python-2-7-12-install-on-ubuntu-22-04 to install old packages.
	* Or, build from source, as in https://github.com/ctch3ng/Installing-Python-2.7-and-pip-on-Ubuntu-24.04-Noble-LTS.
		* I'd use `./configure --with-lto --prefix=/home/username/python27-install --enable-shared`, which worked for me.
* If you want to use native file dialogs:
	* Grab https://github.com/btzy/nativefiledialog-extended
	* `sudo apt install libgtk-3-dev`
	* Build with cmake, add to package prefix.
* Cobble together your cmake command line:
	* Add `-DENABLE_GAMECOREDLL_ENHANCEMENTS=ON` to enable DLL enhancements.
	* Add `-DENABLE_AVX512=ON` to build for AVX-512.
	* Add `-DENABLE_NFD=ON` to enable native file dialogs.
	* Add `-DENABLE_BUILD_ENGINE=OFF` to build an extra DLL without rebuilding the engine.
	```
	cmake -DCMAKE_BUILD_TYPE=Release            \
		-DPYBIND11_INC_DIR=~/pybind11/include                              \
		-DCMAKE_PREFIX_PATH="~/SFML-3.0.1/BuildInstall;~/python27-install;~/nfd-build/install" \
		-DS3TC_DIR=~/s3tc-dxt-decompression                                \
		-DCMAKE_LINKER_TYPE=MOLD                                           \
		-DENABLE_NFD=ON                                                    \
		../CmdCiv4
	```
* Run `cmake --build .`
* Install to a directory: `mkdir install && cmake --install . --prefix install && cd install`
	* The install script will copy over SFML and the python lib, as those are likely to be built from source right now.
* The install will create launch scripts. The "desktop" launchers can be double clicked, and will start a terminal.
* Run one of the launchers.
	* If you launch the engine from a console, it may be messed up on engine exit. Try `reset`, or even: `printf "\033[?1006l\033[?1003l\033[?1002l\033[?1000l\033[?9l\x1b["'!'"p\n"; reset;`.
* Run `ShowLog`. This will touch the log file before running `tail`.
* Run the "Attach GDB" launcher, if you want to debug.

