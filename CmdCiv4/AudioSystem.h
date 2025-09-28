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

	int getAudioTagIndex(std::string_view name, AudioTag tagType) const;
	void playSound(std::string_view name);
	void playScript3DSoundByIndex(int index, heck::ivec2 plotCoord);
	//void setActiveSoundscape(int index, float volume);
	void updateListener();

	void clearSoundScape();

private:
	struct XmlDefsInternals;
	struct Internals;
	std::unique_ptr<XmlDefsInternals> mXmlDefsInternals;
	std::unique_ptr<Internals> mInternals;
};