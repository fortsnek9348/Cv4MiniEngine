#include "inc/CommonStuff/DynamicLib.h"
#include "inc/CommonStuff/StringConversion.h"

#include <stdexcept>

#ifdef _WIN32
#define _WIN32_LEAN_AND_MEAN 1
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

using namespace heck;

DynamicLibrary::DynamicLibrary(const wchar_t* name)
{
#ifdef _WIN32
	mPtr = LoadLibraryW(name);
#else
	mPtr = dlopen(convertWideToUtf8(std::wstring(name)), RTLD_NOW | RTLD_LOCAL);
#endif

	if (!mPtr)
		throw std::runtime_error("Could not load dynamic library '" + convertWideToUtf8(std::wstring(name)) + "'.");
}

void* DynamicLibrary::resolve(const char* name) const
{
	void* sym{};
#ifdef _WIN32
	sym = reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(mPtr), name));
#else
	sym = dlsym(mPtr, name);
#endif
	if (!sym)
		throw std::runtime_error(std::string("Symbol '") + name + "' not found in dynamic library.");
	return sym;
}