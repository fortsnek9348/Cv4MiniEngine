#pragma once

#include "WorldView.h"

#include <CvEnums.h>

#include <string_view>
#include <memory>

class AudioSystem
{
public:
	AudioSystem();
	AudioSystem(AudioSystem&&) noexcept;
	AudioSystem& operator=(AudioSystem&&) noexcept;
	~AudioSystem() noexcept;

	void loadXmlDefs();

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