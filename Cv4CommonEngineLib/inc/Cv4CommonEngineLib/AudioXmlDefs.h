#pragma once

#include <CvEnums.h>

#include <string>
#include <vector>
#include <optional>
#include <unordered_map>

namespace cvengine
{
	struct SoundData
	{
		std::string soundId;
		std::string filename;
		int loadType = -1;
		bool isCompressed = false; // always true?
		bool inGeneric = false; // always true?
	};

	struct ScriptCommonSound
	{
		std::string scriptId;
		const SoundData* soundData = nullptr;
		int soundType = -1;
		int minVolume = 0;
		int maxVolume = 0;
		int pitchChangeDown = 0;
		int pitchChangeUp = 0;
		int minTimeDelay = 0;
		int maxTimeDelay = 0;
		bool taperForSoundtracks = false;
		int lengthOfSound = 0;
		float minDryLevel = 1;
		float maxDryLevel = 1;
		float minWetLevel = 0;
		float maxWetLevel = 0;
		bool looping = false;
	};

	struct Script2DSound : ScriptCommonSound
	{
		int minLeftPan = 0;
		int maxLeftPan = 0;
		int minRightPan = 0;
		int maxRightPan = 0;
	};

	struct Script3DSound : ScriptCommonSound
	{
		int startPositionType = -1;
		int endPositionType = -1;
		int minVelocity = 0;
		int maxVelocity = 0;
		int minDistanceFromListener = 0;
		int maxDistanceFromListener = 0;
		int minDistanceForMaxVolume = 0;
		int maxDistanceForMaxVolume = 0;
		int minCutoffDistance = 0;
		int maxCutoffDistance = 0;
	};

	struct ScriptRef
	{
		int type = -1;
		int index = -1;
	};

	struct Soundscape
	{
		std::string scriptId;
		int iMinVolume = 100;
		int iMaxVolume = 100;
		std::vector<ScriptRef> elements;
	};

	struct AudioDefines
	{
		std::vector<std::string> contextTypes;
		std::vector<std::string> soundTypes;
		std::vector<std::string> positionTypes;
		std::vector<std::string> scriptTypes;
		std::vector<std::string> loadTypes;
		std::vector<std::string> effectTypes;
		std::vector<std::string> gameEnvironmentTypes;
		std::vector<std::string> speakerConfigs;
		std::unordered_map<std::string_view, SoundData> sounds;
	};

	struct AudioXmlDefs
	{
		static AudioXmlDefs& getInstance();

		AudioDefines audioDefs;
		std::vector<Script2DSound> script2dList;
		std::vector<Script3DSound> script3dList;
		std::vector<Soundscape> soundscapes;

		std::unordered_map<std::string_view, ScriptRef> tagIndexLookup;

		int scriptType2D = -1;
		int scriptType3D = -1;
		int scriptTypeSoundscape = -1;

		void loadXmlDefs();
		//int getAudioTagIndex(std::string_view name, AudioTag tagType) const;

	};
}