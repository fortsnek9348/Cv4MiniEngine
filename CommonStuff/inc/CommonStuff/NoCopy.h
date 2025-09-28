#pragma once

namespace heck
{
	struct NoCopy
	{
		NoCopy() = default;
		NoCopy(const NoCopy&) = delete;
		NoCopy& operator=(const NoCopy&) = delete;
		NoCopy(NoCopy&&) = default;
		NoCopy& operator=(NoCopy&&) = default;
	};

	struct NoCopyNoMove
	{
		NoCopyNoMove() = default;
		NoCopyNoMove(const NoCopyNoMove&) = delete;
		NoCopyNoMove& operator=(const NoCopyNoMove&) = delete;
	};
}