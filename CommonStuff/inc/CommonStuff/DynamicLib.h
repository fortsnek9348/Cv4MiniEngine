#pragma once

#include <string_view>
#include <memory>

namespace heck
{
	class DynamicLibrary
	{
	public:
		DynamicLibrary() = default;

		// Throws if not found.
		explicit DynamicLibrary(const wchar_t* name);

		void* resolve(const char* name) const;

		explicit operator bool() const noexcept
		{
			return !!mPtr;
		}

	private:
		// Kept loaded.
		void* mPtr = nullptr;
	};
}