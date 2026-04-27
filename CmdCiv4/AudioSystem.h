#pragma once

#include "WorldView.h"

#include <string_view>
#include <memory>

namespace cvengine
{
	class CvVFS;

	class AudioSystem
	{
	public:
		AudioSystem(const CvVFS& vfs);
		AudioSystem(AudioSystem&&) noexcept;
		AudioSystem& operator=(AudioSystem&&) noexcept;
		~AudioSystem() noexcept;

		void playSound(std::string_view name);
		void playScript2DSoundByIndex(int index);
		void playScript3DSoundByIndex(int index, heck::ivec2 plotCoord);
		//void setActiveSoundscape(int index, float volume);
		void updateListener();

		void clearSoundScape();

	private:
		struct Internals;
		std::unique_ptr<Internals> mInternals;
	};
}