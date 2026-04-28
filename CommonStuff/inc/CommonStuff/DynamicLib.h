#pragma once

#include <string_view>
#include <filesystem>
#include <memory>

namespace heck
{
	class DynamicLibrary
	{
	public:
		DynamicLibrary() = default;

		// Throws if not found.
		explicit DynamicLibrary(const std::filesystem::path& path);

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