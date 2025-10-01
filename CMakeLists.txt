cmake_minimum_required(VERSION 3.28)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_EXECUTE_PROCESS_COMMAND_ECHO STDOUT)

#add_link_options("-fuse-ld=mold")
#add_link_options("-fuse-ld=lld-20")

# If you want to strip.
#if (NOT MSVC)
#	add_link_options($<$<CONFIG:RELEASE>:-s>)
#endif()

project(Cv4MiniEngine)

option(ENABLE_BUILD_ENGINE "Build the engine." ON)
option(ENABLE_GAMECOREDLL_ENHANCEMENTS "Compile in the DLL enhancements that can be enabled via INI at runtime" OFF)
option(ENABLE_AVX512 "Compile everything with AVX512/x86-64-v4 support." OFF)
option(ENABLE_NFD "Use native file dialogs via nativefiledialog-extended. Don't enable on pure console systems." OFF)

# Break glass in case of emergancy
#add_compile_options(-fsanitize=address -static-libasan)
#add_link_options(-fsanitize=address -static-libasan)
# Then, put "interceptor_via_lib:libpython2.7" in a text file and run with ASAN_OPTIONS=suppressions=supp.txt

# =============================================================================
find_package(SFML 3.0.2 COMPONENTS Audio System REQUIRED)
add_library(CommonStuff STATIC
	"CommonStuff/PPM.cpp"
	"CommonStuff/PQCoordSimdAStar.cpp"
	"CommonStuff/ProfilerMarkers.cpp"
	"CommonStuff/QuadTree.cpp"
	"CommonStuff/StatisticsRegistry.cpp"
	"CommonStuff/StringConversion.cpp"
	"CommonStuff/System.cpp"
	"CommonStuff/TradeNetworkCore.cpp"
	"CommonStuff/TradeNetworkSystem.cpp"
	"CommonStuff/WorkStealingParallelFor.cpp"
)
target_include_directories(CommonStuff INTERFACE "CommonStuff/inc")
#target_link_libraries(CommonStuff PRIVATE SFML::System)
target_link_libraries(CommonStuff PRIVATE $<COMPILE_ONLY:SFML::System>)

# Solution-wide flags
if (MSVC)
	target_compile_options(CommonStuff PUBLIC /arch:AVX2)
	target_compile_options(CommonStuff PUBLIC /W4)
	target_compile_options(CommonStuff PUBLIC /MP)
	target_compile_options(CommonStuff PUBLIC /Zc:preprocessor)
	
	if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
		target_compile_options(CommonStuff PUBLIC /showFilenames)
	else()
		target_compile_options(CommonStuff PUBLIC /wd4251) # ... needs to have dll-interface to be used by clients of ...
	endif()
else()
	target_compile_options(CommonStuff PUBLIC -march=x86-64-v3)

	if (UNIX)
		target_link_libraries(CommonStuff PUBLIC atomic)
	endif()
endif()

#set(IsActualMSVCCompiler (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC" AND CMAKE_CXX_COMPILER_ID STREQUAL "MSVC"))
#set(DoesCompilerTakeGccStyleWatnings (NOT ${IsActualMSVCCompiler}))
#set(IsClangCl (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC" AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang"))

function (AddGccStyleOption target scope flag)
	if (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
		if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
			target_compile_options(${target} ${scope} /clang:${flag})
		endif()
	else()
		target_compile_options(${target} ${scope} ${flag})
	endif()
endfunction()

foreach(FLAG -Wall -Wextra -Wpedantic -Wno-range-loop-construct -Wno-extra-qualification -Wno-extra-semi
	-Wno-deprecated-literal-operator -Wno-undefined-inline) # pybind11!
	
	AddGccStyleOption(CommonStuff PUBLIC ${FLAG})
endforeach()

# Need this when linking into a Linux shared object, or you'll get a link error.
set_target_properties(CommonStuff PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ============================================================================= Python
find_path(PYBIND11_INC_DIR pybind11 REQUIRED)
#find_path(PYTHON_LIB_DIR python27 REQUIRED)
#find_path(PYTHON_INC_DIR python27 REQUIRED)
find_package(Python2 COMPONENTS Development)

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/generated/pybind11)
#execute_process(COMMAND ${Patch_EXECUTABLE} -p1 -o ${CMAKE_BINARY_DIR}/generated/pybind11/stl.h ${PYBIND11_INC_DIR}/pybind11/stl.h -i ${CMAKE_CURRENT_SOURCE_DIR}/pybind11_stl.patch
#	COMMAND_ERROR_IS_FATAL ANY)


if (WIN32)
	# Patch manually
	file(READ "${PYBIND11_INC_DIR}/pybind11/stl.h" temp)
	string(REPLACE "forward_like" "forward_like_pybind11" temp "${temp}")
	string(REPLACE [[#include "pybind11.h"]] [[#include <pybind11/pybind11.h>]] temp "${temp}")
	string(REPLACE [[#include "detail/common.h"]] [[#include <pybind11/detail/common.h>]] temp "${temp}")
	file(WRITE "${CMAKE_BINARY_DIR}/generated/pybind11/stl.h" "${temp}")
else()
	find_package(Patch REQUIRED)
	add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/generated/pybind11/stl.h"
		COMMAND "${Patch_EXECUTABLE}" -p1 -o "${CMAKE_BINARY_DIR}/generated/pybind11/stl.h" "${PYBIND11_INC_DIR}/pybind11/stl.h" -i "${CMAKE_CURRENT_SOURCE_DIR}/cmake/pybind11_stl.patch"
		# The build system actually tried to build these files. So how do you specify simple file dependencies?
		#DEPENDS "${PYBIND11_INC_DIR}/pybind11/stl.h" "${CMAKE_CURRENT_SOURCE_DIR}/cmake data/pybind11_stl.patch"
		VERBATIM
	)
endif()
add_custom_target(pybind11_codegen DEPENDS "${CMAKE_BINARY_DIR}/generated/pybind11/stl.h")

if(MSVC)
	link_directories(${Python2_LIBRARY_DIRS})
endif()

# ============================================================================= CvGameCoreDLL
file(GLOB_RECURSE SRCS "CvGameCoreDLL/*.cpp")
list(FILTER SRCS EXCLUDE REGEX "_precompile")
add_library(CvGameCoreDLL SHARED ${SRCS})
target_include_directories(CvGameCoreDLL INTERFACE "CvGameCoreDLL")
add_dependencies(CvGameCoreDLL pybind11_codegen)
target_include_directories(CvGameCoreDLL PUBLIC ${CMAKE_BINARY_DIR}/generated ${PYBIND11_INC_DIR})
target_include_directories(CvGameCoreDLL SYSTEM PUBLIC "${Python2_INCLUDE_DIRS}") # Macro redef error without SYSTEM.
# Workaround old python headers
if (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
	if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
		target_compile_options(CvGameCoreDLL PUBLIC -Wno-register) # error : ISO C++17 does not allow 'register' storage class specifier [-Wregister]
	else()
		target_compile_options(CvGameCoreDLL PUBLIC /wd5033) # warning C5033: 'register' is no longer a supported storage class
	endif()
	target_compile_options(CvGameCoreDLL PRIVATE /bigobj)
else()
	target_compile_definitions(CvGameCoreDLL PUBLIC register=)
endif()
target_compile_definitions(CvGameCoreDLL PRIVATE _USRDLL=1)
target_compile_definitions(CvGameCoreDLL PUBLIC PYBIND11_EXPORT=)
target_compile_definitions(CvGameCoreDLL PUBLIC FINAL_RELEASE=1)

if (MSVC)
	target_compile_definitions(CvGameCoreDLL PRIVATE HAVE_SNPRINTF=1)
	target_compile_definitions(CvGameCoreDLL PRIVATE _CRT_NON_CONFORMING_SWPRINTFS=1)
	target_compile_definitions(CvGameCoreDLL PRIVATE _CRT_SECURE_NO_WARNINGS=1)
endif()

target_link_libraries(CvGameCoreDLL PUBLIC CommonStuff)

if (WIN32)
	target_link_libraries(CvGameCoreDLL PRIVATE winmm.lib)
endif()

if (MSVC)
	set(AVX512Flag /arch:AVX512)
else()
	set(AVX512Flag -march=x86-64-v4)
endif()

if (ENABLE_GAMECOREDLL_ENHANCEMENTS)
	target_compile_definitions(CvGameCoreDLL PRIVATE ENABLE_GAMECOREDLL_ENHANCEMENTS=1)
	if (ENABLE_AVX512)
		target_compile_options(CommonStuff PUBLIC ${AVX512Flag})
		set(CvGameCoreDLL_NameSuffix "Enhanced AVX512")
	else()
		set(CvGameCoreDLL_NameSuffix "Enhanced AVX2")
	endif()
else()
	set(CvGameCoreDLL_NameSuffix "Regular")
endif()

# ============================================================================= Engine stuff
if (ENABLE_BUILD_ENGINE)
	file(GLOB_RECURSE SRCS "FAStar/*.cpp")
	add_library(FAStar STATIC ${SRCS})
	target_link_libraries(FAStar PRIVATE CommonStuff)
	target_link_libraries(FAStar PRIVATE $<COMPILE_ONLY:CvGameCoreDLL>)
	target_include_directories(FAStar INTERFACE "FAStar")
	# =============================================================================
	file(GLOB SRCS "HeckConsoleUI/*.cpp")
	add_library(HeckTUI SHARED ${SRCS})

	target_include_directories(HeckTUI INTERFACE "HeckConsoleUI/inc")
	target_link_libraries(HeckTUI PUBLIC CommonStuff)
	target_compile_definitions(HeckTUI PRIVATE HECKCONSOLEUI_EXPORTS=1)
	
	file(GLOB SRCS "HeckTUIInputTest/*.cpp")
	add_executable(HeckTUIInputTest ${SRCS})
	target_link_libraries(HeckTUIInputTest PRIVATE CommonStuff)
	target_link_libraries(HeckTUIInputTest PRIVATE HeckTUI)
	# =============================================================================
	file(GLOB_RECURSE SRCS "CmdCiv4/*.cpp")
	find_path(S3TC_DIR "s3tc.cpp" REQUIRED)
	add_executable(Cv4MiniEngine ${SRCS} "${S3TC_DIR}/s3tc.cpp")
	target_link_libraries(Cv4MiniEngine PRIVATE HeckTUI)
	target_link_libraries(Cv4MiniEngine PRIVATE CvGameCoreDLL)
	target_link_libraries(Cv4MiniEngine PRIVATE FAStar)

	target_link_libraries(Cv4MiniEngine PRIVATE SFML::Audio SFML::System)

	if (WIN32)
		# This gets the GUI to prompt for zlib.
		find_path(ZLIB_ROOT "include/zlib.h" REQUIRED)
		set(ZLIB_USE_STATIC_LIBS "ON")
	endif()
	#set(CMAKE_FIND_DEBUG_MODE TRUE)
	find_package(ZLIB REQUIRED)
	#set(CMAKE_FIND_DEBUG_MODE FALSE)
	target_link_libraries(Cv4MiniEngine PRIVATE ZLIB::ZLIB)
	target_compile_definitions(Cv4MiniEngine PRIVATE ZLIB_CONST=1)

	find_package(tinyxml2 REQUIRED)
	target_link_libraries(Cv4MiniEngine PRIVATE tinyxml2::tinyxml2)

	if (WIN32)
		find_path(STB_INC_DIR stb REQUIRED)
		target_include_directories(Cv4MiniEngine PRIVATE ${STB_INC_DIR})
	else()
		find_package(PkgConfig REQUIRED)
		pkg_check_modules(STB REQUIRED stb)
		target_link_libraries(Cv4MiniEngine PRIVATE ${STB_LIBRARIES})
		target_include_directories(Cv4MiniEngine PRIVATE ${STB_INCLUDE_DIRS})
		target_compile_definitions(Cv4MiniEngine PRIVATE ${STB_CFLAGS_OTHER})
	endif()


	target_include_directories(Cv4MiniEngine PRIVATE ${S3TC_DIR})
	
	if (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
		if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
			target_compile_options(Cv4MiniEngine PRIVATE "/clang:-Wno-error=unused-variable")
		endif()
	else()
		target_compile_options(Cv4MiniEngine PRIVATE "-Wno-error=unused-variable")
	endif()

	if (NOT MSVC)
		# If not MSVC, pybind11 won't link the lib automatically.
		target_link_libraries(Cv4MiniEngine PRIVATE Python2::Python)
	endif()

	if (ENABLE_NFD)
		find_package(nfd REQUIRED)
		target_link_libraries(Cv4MiniEngine PRIVATE nfd::nfd)
	else()
		target_compile_definitions(Cv4MiniEngine PRIVATE CV4MINIENGINE_USE_CONSOLE_FILE_BROWSER=1)
	endif()

	# What is this. "RPATH"? Why can't you just search for libs in the current directory?
	set_target_properties(Cv4MiniEngine PROPERTIES INSTALL_RPATH .)
	set_target_properties(HeckTUIInputTest PROPERTIES INSTALL_RPATH .)

	install(TARGETS Cv4MiniEngine HeckTUIInputTest DESTINATION .)
	
	# SFML too as we're building from source.
	install(IMPORTED_RUNTIME_ARTIFACTS SFML::Audio SFML::System DESTINATION .)

	# > For shared libraries on DLL platforms, if neither RUNTIME nor ARCHIVE destinations are specified, both the RUNTIME and ARCHIVE components
	# > are installed to their default destinations. If either a RUNTIME or ARCHIVE destination is specified, the component is installed to that
	# > destination, and the other component is not installed. If both RUNTIME and ARCHIVE destinations are specified, then both components are
	# > installed to their respective destinations.
	# Why, Cmake. Why.
	# It would be great if I could specify both RUNTIME and LIBRARY and have the archive lib portably not installed.
	if (WIN32)
		install(TARGETS HeckTUI RUNTIME DESTINATION .)
	else()
		install(TARGETS HeckTUI LIBRARY DESTINATION .)
	endif()

	#install(IMPORTED_RUNTIME_ARTIFACTS Python2::Python DESTINATION .) # This doesn't work.
	# Python lib is static on Windows.
	if (NOT WIN32)
		install(CODE "set(Python2_LIBRARIES \"${Python2_LIBRARIES}\")")
		install(CODE [[
			file(INSTALL
				DESTINATION "${CMAKE_INSTALL_PREFIX}"
				TYPE SHARED_LIBRARY
				FOLLOW_SYMLINK_CHAIN # that's right, cmake, I don't want you to just copy the symlinks!
				FILES "${Python2_LIBRARIES}"
			)
		]])
	endif()

	install(DIRECTORY "Data/CustomAssets" DESTINATION . PATTERN "*.bak" EXCLUDE)
	install(DIRECTORY "Data/PublicMaps" DESTINATION . PATTERN "*.bak" EXCLUDE)
endif()

if (WIN32)
	install(TARGETS CvGameCoreDLL RUNTIME DESTINATION "CvGameCoreDLL_${CvGameCoreDLL_NameSuffix}")
	# Note that when dragging a file onto the batch file, Windows will change the working directory.
	FILE(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/Cv4MiniEngine_${CvGameCoreDLL_NameSuffix}.bat CONTENT 
		"
		SET PATH=%PATH%;%~dp0CvGameCoreDLL_${CvGameCoreDLL_NameSuffix}
		cd /d %~dp0
		Cv4MiniEngine %*
		"
		FILE_PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ
	)
	install(PROGRAMS ${CMAKE_BINARY_DIR}/Cv4MiniEngine_${CvGameCoreDLL_NameSuffix}.bat DESTINATION .)

	install(FILES "cmake/ShowLog.ps1" DESTINATION .)
else()
	install(TARGETS CvGameCoreDLL LIBRARY DESTINATION "CvGameCoreDLL_${CvGameCoreDLL_NameSuffix}")
	FILE(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/Cv4MiniEngine_${CvGameCoreDLL_NameSuffix}.sh CONTENT 
		"#!/bin/sh
		LD_LIBRARY_PATH=$LD_LIBRARY_PATH:\"CvGameCoreDLL_${CvGameCoreDLL_NameSuffix}\" ./Cv4MiniEngine \"$@\"
		"
		FILE_PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ
	)
	configure_file("cmake/Launcher.desktop.in" "Cv4MiniEngine_${CvGameCoreDLL_NameSuffix}.desktop")
	install(PROGRAMS "${CMAKE_BINARY_DIR}/Cv4MiniEngine_${CvGameCoreDLL_NameSuffix}.sh" DESTINATION . PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
	install(FILES "${CMAKE_BINARY_DIR}/Cv4MiniEngine_${CvGameCoreDLL_NameSuffix}.desktop" DESTINATION . PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)

	install(FILES "cmake/ShowLog.desktop" DESTINATION . PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
	install(FILES "cmake/Cv4MiniEngine Attach GDB.desktop" DESTINATION . PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
	
endif()

install(FILES "LICENSE.txt" DESTINATION .)