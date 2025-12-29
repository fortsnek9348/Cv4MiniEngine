#include "AudioSystem.h"
#include "CvVFS.h"
#include "Common.h"
#include "DLLInterface/MyCvDLLXml.h"
#include "MyFFile.h"
#include "CivIni.h"

#include "CvInterface.h"

#include <CvGlobals.h>
#include <CvXMLLoadUtility.h>
#include <CvRandom.h>
#include <CvMap.h>
#include <CvCity.h>
#include <CvGameAI.h>

#include <SFML/Audio.hpp>

#include <CommonStuff/range.h>
#include <CommonStuff/NoCopy.h>

#include <atomic>
#include <iostream>
#include <random>

using heck::range;
using namespace cvengine;

static constexpr bool kEnable = true;
//static constexpr float kWorldSoundscapeVolume = 70.0f;
//static constexpr float kCityScreenSoundscapeVolume = 90.0f;
static constexpr float kWorldSoundscapeVolume = 100.0f;
static constexpr float kCityScreenSoundscapeVolume = 100.0f;
static constexpr int kDefaultSimultaneouslyStartedSoundLimit = 1;

namespace
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

	// Basically CvXMLLoadUtility::SetGlobalStringArray
	std::vector<std::string> loadAudioDefinesEnum(MyCvDLLXml& xmlSys, FXml* xml, const char* path)
	{
		std::vector<std::string> list;

		if (xmlSys.LocateNode(xml, path))
		{
			const int numValues = xmlSys.NumOfElementsByTagName(xml, path);

			list.reserve(numValues);

			int index = 0;
			std::string temp;
			do
			{
				if (!xmlSys.GetLastNodeValue(xml, temp))
					std::abort();
				if (temp != "NONE") // HACK: Special case this
				{
					// Let's just hijack globals for this. It will check for duplicates too.
					gGlobals.setTypesEnum(temp.c_str(), index);
				}
				else if (index != 0)
					std::abort();
				list.push_back(std::move(temp));
				++index;
			} while (xmlSys.NextSibling(xml));
		}

		return list;
	}

	template<class T>
	T getChildXmlValByName(MyCvDLLXml& xmlSys, FXml* xml, const char* szName, T def = {})
	{
		if (xmlSys.SetToChildByTagName(xml, szName))
		{
			T value = def;
			if constexpr (requires { xmlSys.GetLastNodeValue(xml, &value); })
				xmlSys.GetLastNodeValue(xml, &value);
			else
				xmlSys.GetLastNodeValue(xml, value);
			xmlSys.SetToParent(xml);
			return value;
		}
		else
			std::abort();
	}

	int getChildXmlEnumByName(MyCvDLLXml& xmlSys, FXml* xml, const char* tag)
	{
		const std::string name = getChildXmlValByName<std::string>(xmlSys, xml, tag);
		return name.empty() ? -1 : name == "NONE" ? 0 : gGlobals.getTypesEnum(name.c_str());
	}


	std::unordered_map<std::string_view, SoundData> loadAudioDefinesSounds(MyCvDLLXml& xmlSys, FXml* xml)
	{
		std::unordered_map<std::string_view, SoundData> map;

		if (xmlSys.LocateNode(xml, "AudioDefines/SoundDatas/SoundData"))
		{
			do
			{
				SoundData data{
					.soundId = getChildXmlValByName<std::string>(xmlSys, xml, "SoundID"),
					.filename = getChildXmlValByName<std::string>(xmlSys, xml, "Filename", ""),
					.loadType = getChildXmlEnumByName(xmlSys, xml, "LoadType"),
					.isCompressed = getChildXmlValByName(xmlSys, xml, "bIsCompressed", false),
					.inGeneric = getChildXmlValByName(xmlSys, xml, "bInGeneric", false),
				};

				auto node = map.extract(map.emplace(data.soundId, std::move(data)).first);
				node.key() = node.mapped().soundId;
				map.insert(std::move(node));
			} while (xmlSys.NextSibling(xml));
		}

		return map;
	}

	AudioDefines loadAudioDefines(MyCvDLLXml& xmlSys)
	{
		FXml* const xml = xmlSys.CreateFXml(nullptr);
		if (!xmlSys.LoadXml(xml, "XML/Audio/AudioDefines.xml"))
			std::abort();

		return {
			.contextTypes = loadAudioDefinesEnum(xmlSys, xml, "AudioDefines/ContextTypes/ContextType"),
			.soundTypes = loadAudioDefinesEnum(xmlSys, xml, "AudioDefines/SoundTypes/SoundType"),
			.positionTypes = loadAudioDefinesEnum(xmlSys, xml, "AudioDefines/PositionTypes/PositionType"),
			.scriptTypes = loadAudioDefinesEnum(xmlSys, xml, "AudioDefines/ScriptTypes/ScriptType"),
			.loadTypes = loadAudioDefinesEnum(xmlSys, xml, "AudioDefines/LoadTypes/LoadType"),
			.effectTypes = loadAudioDefinesEnum(xmlSys, xml, "AudioDefines/EffectTypes/EffectType"),
			.gameEnvironmentTypes = loadAudioDefinesEnum(xmlSys, xml, "AudioDefines/GameEnvironmentTypes/GameEnvironmentType"),
			.speakerConfigs = loadAudioDefinesEnum(xmlSys, xml, "AudioDefines/SpeakerConfigs/SpeakerConfig"),
			.sounds = loadAudioDefinesSounds(xmlSys, xml),
		};
	}

	void loadAudioCommonScript(MyCvDLLXml& xmlSys, FXml* xml, const AudioDefines& audioDefs, ScriptCommonSound& data)
	{
		data.scriptId = getChildXmlValByName<std::string>(xmlSys, xml, "ScriptID");
		data.soundData = &audioDefs.sounds.at(getChildXmlValByName<std::string>(xmlSys, xml, "SoundID"));
		data.soundType = getChildXmlEnumByName(xmlSys, xml, "SoundType");
		data.minVolume = getChildXmlValByName(xmlSys, xml, "iMinVolume", 0);
		data.maxVolume = getChildXmlValByName(xmlSys, xml, "iMaxVolume", 0);
		data.pitchChangeDown = getChildXmlValByName(xmlSys, xml, "iPitchChangeDown", 0);
		data.pitchChangeUp = getChildXmlValByName(xmlSys, xml, "iPitchChangeUp", 0);
		data.looping = getChildXmlValByName(xmlSys, xml, "bLooping", false);
		data.minTimeDelay = getChildXmlValByName(xmlSys, xml, "iMinTimeDelay", 0);
		data.maxTimeDelay = getChildXmlValByName(xmlSys, xml, "iMaxTimeDelay", 0);
		data.taperForSoundtracks = getChildXmlValByName(xmlSys, xml, "bTaperForSoundtracks", false);
		data.lengthOfSound = getChildXmlValByName(xmlSys, xml, "iLengthOfSound", 0);
		data.minDryLevel = getChildXmlValByName(xmlSys, xml, "fMinDryLevel", 1.0f);
		data.maxDryLevel = getChildXmlValByName(xmlSys, xml, "fMaxDryLevel", 1.0f);
		data.minWetLevel = getChildXmlValByName(xmlSys, xml, "fMinWetLevel", 0.0f);
		data.maxWetLevel = getChildXmlValByName(xmlSys, xml, "fMaxWetLevel", 0.0f);
	}

	std::vector<Script2DSound> loadAudio2DScripts(MyCvDLLXml& xmlSys, const AudioDefines& audioDefs)
	{
		FXml* const xml = xmlSys.CreateFXml(nullptr);
		if (!xmlSys.LoadXml(xml, "XML/Audio/Audio2DScripts.xml"))
			std::abort();

		std::vector<Script2DSound> map;

		if (xmlSys.LocateNode(xml, "Script2DSounds/Script2DSound"))
		{
			do
			{
				Script2DSound data{};
				loadAudioCommonScript(xmlSys, xml, audioDefs, data);
				data.minLeftPan = getChildXmlValByName(xmlSys, xml, "iMinLeftPan", 0);
				data.maxLeftPan = getChildXmlValByName(xmlSys, xml, "iMaxLeftPan", 0);
				data.minRightPan = getChildXmlValByName(xmlSys, xml, "iMinRightPan", 0);
				data.maxRightPan = getChildXmlValByName(xmlSys, xml, "iMaxRightPan", 0);
				map.push_back(std::move(data));
			} while (xmlSys.NextSibling(xml));
		}

		return map;
	}

	std::vector<Script3DSound> loadAudio3DScripts(MyCvDLLXml& xmlSys, const AudioDefines& audioDefs)
	{
		FXml* const xml = xmlSys.CreateFXml(nullptr);
		if (!xmlSys.LoadXml(xml, "XML/Audio/Audio3DScripts.xml"))
			std::abort();

		std::vector<Script3DSound> map;

		if (xmlSys.LocateNode(xml, "Script3DSounds/Script3DSound"))
		{
			do
			{
				Script3DSound data{};
				loadAudioCommonScript(xmlSys, xml, audioDefs, data);
				data.startPositionType = getChildXmlEnumByName(xmlSys, xml, "StartPosition");
				data.endPositionType = getChildXmlEnumByName(xmlSys, xml, "EndPosition");
				data.minVelocity = getChildXmlValByName(xmlSys, xml, "iMinVelocity", 0);
				data.maxVelocity = getChildXmlValByName(xmlSys, xml, "iMaxVelocity", 0);
				data.minDistanceFromListener = getChildXmlValByName(xmlSys, xml, "iMinDistanceFromListener", 0);
				data.maxDistanceFromListener = getChildXmlValByName(xmlSys, xml, "iMaxDistanceFromListener", 0);
				data.minDistanceForMaxVolume = getChildXmlValByName(xmlSys, xml, "iMinDistanceForMaxVolume", 100000);
				data.maxDistanceForMaxVolume = getChildXmlValByName(xmlSys, xml, "iMaxDistanceForMaxVolume", 100000);
				data.minCutoffDistance = getChildXmlValByName(xmlSys, xml, "iMinCutoffDistance", 100000);
				data.maxCutoffDistance = getChildXmlValByName(xmlSys, xml, "iMaxCutoffDistance", 100000);
				map.push_back(std::move(data));
			} while (xmlSys.NextSibling(xml));
		}

		return map;
	}

	std::vector<Soundscape> loadSoundscapes(MyCvDLLXml& xmlSys, [[maybe_unused]] const AudioDefines& audioDefs, const std::unordered_map<std::string_view, ScriptRef>& tagIndexLookup)
	{
		FXml* const xml = xmlSys.CreateFXml(nullptr);
		if (!xmlSys.LoadXml(xml, "XML/Audio/AudioSoundscapeScripts.xml"))
			std::abort();

		std::vector<Soundscape> map;

		if (xmlSys.LocateNode(xml, "ScriptSoundscapes/ScriptSoundscape"))
		{
			do
			{
				Soundscape data{};
				data.scriptId = getChildXmlValByName<std::string>(xmlSys, xml, "ScriptID");
				data.iMinVolume = getChildXmlValByName(xmlSys, xml, "iMinVolume", data.iMinVolume);
				data.iMaxVolume = getChildXmlValByName(xmlSys, xml, "iMaxVolume", data.iMaxVolume);
				// CvXMLLoadUtility::SetVariableListTagPairForAudioScripts and similar
				if (xmlSys.SetToChildByTagName(xml, "SoundscapeElement"))
				{
					do
					{
						// Ignoring ScriptType.
						std::string elementScriptName = getChildXmlValByName<std::string>(xmlSys, xml, "ScriptID");
						const ScriptRef ref = tagIndexLookup.at(elementScriptName);
						data.elements.push_back(ref);
					} while (xmlSys.NextSibling(xml));

					xmlSys.SetToParent(xml);
				}
				map.push_back(std::move(data));
			} while (xmlSys.NextSibling(xml));
		}

		return map;
	}

	struct ADPCMDecodeState
	{
		int sample = 0;
		int stepTableI = 0;

		int16_t decodeNibble(int value)
		{
			static constexpr std::array kStepTable{
				7, 8, 9, 10, 11, 12, 13, 14,
				16, 17, 19, 21, 23, 25, 28, 31,
				34, 37, 41, 45, 50, 55, 60, 66,
				73, 80, 88, 97, 107, 118, 130, 143,
				157, 173, 190, 209, 230, 253, 279, 307,
				337, 371, 408, 449, 494, 544, 598, 658,
				724, 796, 876, 963, 1060, 1166, 1282, 1411,
				1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024,
				3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484,
				7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
				15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
				32767
			};
			static constexpr std::array kIndexTable{
				-1, -1, -1, -1, 2, 4, 6, 8,
				-1, -1, -1, -1, 2, 4, 6, 8
			};

			int diff = kStepTable[stepTableI] >> 3;
			if (value & 4)
				diff += kStepTable[stepTableI];
			if (value & 2)
				diff += kStepTable[stepTableI] >> 1;
			if (value & 1)
				diff += kStepTable[stepTableI] >> 2;
			if (value & 8)
				diff = -diff;
			sample = std::clamp<int>(sample + diff, INT16_MIN, INT16_MAX);
			stepTableI = std::clamp<int>(stepTableI + kIndexTable[value], 0, std::size(kStepTable) - 1);
			return (int16_t)sample;
		}
	};

	std::optional<sf::SoundBuffer> tryLoadFileUsingSFML(const std::filesystem::path& physPath)
	{
		FFile<StdRawBinaryStream> file(physPath, std::ios::in);
		sf::SoundBuffer buf;
		if (!buf.loadFromFile(physPath))
			return std::nullopt;
		return buf;
	}

	sf::SoundBuffer loadWaveFile(const std::filesystem::path& physPath)
	{
		// Could be PCM or ADPCM. Try SFML first.
		if (std::optional<sf::SoundBuffer> buf = tryLoadFileUsingSFML(physPath))
			return std::move(*buf);

		FFile<StdRawBinaryStream> file(physPath, std::ios::in);

		if (!file.stream.stream)
		{
			std::clog << "Couldn't open audio file " << physPath << "." << std::endl;
			return sf::SoundBuffer();
		}

		// all chunks together for IMA ADPCM.
		struct FullHeader
		{
			char riffId[4]{};
			uint32_t riffSize = 0;
			char waveId[4]{};
			char fmtId[4]{};
			uint32_t fmtSize = 0; // 0x14
			uint16_t fmt = 0; // 0x11 - WAVE_FORMAT_DVI_ADPCM
			uint16_t channels = 0;
			uint32_t sampleRate = 0;
			uint32_t byteRate = 0;
			uint16_t blockAlign = 0;
			uint16_t bitsPerSample = 0; // 4
			uint16_t extraSize = 0; // 2
			uint16_t samplesPerBlock = 0;
			char factId[4]{};
			uint32_t factSize = 0; // 4
			uint32_t numSamples = 0;
			char dataId[4]{};
			uint32_t dataSize = 0;
			// Audio data begins.
		};

		FullHeader fullHeader{};
		file.stream.readTrivial(fullHeader);

		const bool isValid = std::string_view(fullHeader.riffId, 4) == "RIFF"
			&& std::string_view(fullHeader.waveId, 4) == "WAVE"
			&& std::string_view(fullHeader.fmtId, 4) == "fmt "
			&& fullHeader.fmtSize == 0x14
			&& fullHeader.fmt == 0x11
			&& (1 <= fullHeader.channels && fullHeader.channels <= 2)
			&& fullHeader.bitsPerSample == 4
			&& fullHeader.extraSize == 2
			&& std::string_view(fullHeader.factId, 4) == "fact"
			&& fullHeader.factSize == 4
			&& std::string_view(fullHeader.dataId, 4) == "data";

		if (!isValid)
		{
			std::clog << "Couldn't load " << physPath << ". Unknown format." << std::endl;
			return sf::SoundBuffer();
		}

		static constexpr int kSamplesPerWord = 8;

		// Each 4-byte word contains 8 samples.
		const int numWordsPerChannelPerBlock = fullHeader.blockAlign / (4 * fullHeader.channels) - 1;
		const int numBlocks = (fullHeader.dataSize + fullHeader.blockAlign - 1) / fullHeader.blockAlign; // rounding up

		const int numSamplesInCompleteBlocks = std::max(0, numBlocks - 1) * fullHeader.samplesPerBlock;
		const int numSamplesInLastBlock = fullHeader.numSamples - numSamplesInCompleteBlocks;

		const int numWordsPerChannelInLastBlock = (numSamplesInLastBlock - 1 + kSamplesPerWord - 1) / kSamplesPerWord; // Round up, file should probably pad the last word.

		const int numSamplesInLastBlockAligned = 1 + numWordsPerChannelInLastBlock * kSamplesPerWord;

		std::vector<int16_t> rawPCM((numSamplesInCompleteBlocks + numSamplesInLastBlockAligned) * fullHeader.channels);

		size_t pcmI = 0;
		
		for (const int blkI : range(numBlocks))
		{
			std::array<ADPCMDecodeState, 2> decoder{};
			for (const int channelI : range(fullHeader.channels))
			{
				struct BlockHeader
				{
					int16_t sample = 0;
					uint8_t stepTableI = 0;
					uint8_t unk = 0;
				};

				BlockHeader blkHeader{};
				file.stream.readTrivial(blkHeader);
				decoder[channelI] = { .sample = blkHeader.sample, .stepTableI = blkHeader.stepTableI };

				rawPCM[pcmI++] = blkHeader.sample;
			}

			for ([[maybe_unused]] const int wordI : range(blkI < numBlocks - 1 ? numWordsPerChannelPerBlock : numWordsPerChannelInLastBlock))
			{
				std::array<std::array<uint8_t, kSamplesPerWord / 2>, 2> bytes;

				file.stream.readTrivialArray(std::span(bytes).subspan(0, fullHeader.channels));

				for (const int i : range(kSamplesPerWord / 2))
				{
					for (const int channelI : range(fullHeader.channels))
						rawPCM[pcmI++] = decoder[channelI].decodeNibble(bytes[channelI][i] % 16);

					for (const int channelI : range(fullHeader.channels))
						rawPCM[pcmI++] = decoder[channelI].decodeNibble(bytes[channelI][i] / 16);
				}
				
			}
		}

		static const std::vector kMonoMapping{ sf::SoundChannel::Mono };
		static const std::vector kStereoMapping{ sf::SoundChannel::FrontLeft, sf::SoundChannel::FrontRight };
		
		sf::SoundBuffer buf;
		if (!buf.loadFromSamples(rawPCM.data(), fullHeader.numSamples * fullHeader.channels, fullHeader.channels, fullHeader.sampleRate,
			fullHeader.channels == 1 ? kMonoMapping : kStereoMapping))
		{
			std::clog << "Couldn't load " << physPath << ". SFML rejected sample data..." << std::endl;
			return sf::SoundBuffer();
		}

		//buf.saveToFile("test.wav");

		return buf;
	}

	

	struct ScriptSoundStream : sf::SoundStream
	{
		const ScriptCommonSound* script = nullptr;
		const sf::SoundBuffer* buf = nullptr;

		static constexpr double kFadeLatencySeconds = 0.2f; // The buffer size. Reaction to fade requests will be delayed by up to this amount, ideally.
		static constexpr double kFadeInDurationSeconds = 0.8;
		static constexpr double kFadeOutDurationSeconds = 1.7;

		const int fadeResolutionSamples = 0;
		const int fadeInDurationSamples = 0;
		const int fadeOutDurationSamples = 0;

		// Negative for delay silence
		int position = 0;

		//struct FadeState
		//{
		//	uint32_t fadeIn : 1 = 0; // 0 = fade out then stop, 1 = fade in then hold steady
		//	uint32_t samples : 31 = 0; // Numbers of samples into the fade.
		//};
		//
		//std::atomic<FadeState> fadeState{};

		struct FadeState
		{
			bool wantFadeIn{};
			int samples{};
		};
		std::atomic<FadeState> fadeState{ FadeState{ .wantFadeIn = true } };

		bool prevFadeInState = true;
		

		std::vector<int16_t> fadeSamplesBuffer;

		explicit ScriptSoundStream(const ScriptCommonSound* script, const sf::SoundBuffer* buf)
			: script(script), buf(buf)
			, fadeResolutionSamples(std::lround(kFadeLatencySeconds * buf->getSampleRate()))
			, fadeInDurationSamples(std::lround(kFadeInDurationSeconds * buf->getSampleRate()))
			, fadeOutDurationSamples(std::lround(kFadeOutDurationSeconds * buf->getSampleRate()))
		{
			//fadeSamplesBuffer.resize(std::max(fadeInDurationSamples, fadeOutDurationSamples) * buf->getChannelCount());
			fadeSamplesBuffer.resize(fadeResolutionSamples * buf->getChannelCount());
			initialize(buf->getChannelCount(), buf->getSampleRate(), buf->getChannelMap());
			setSpatializationEnabled(false);
			setLooping(true);
		}
		ScriptSoundStream(ScriptSoundStream&&) = delete;
		// SFML doesn't wait for audio thread currently. And we don't need this anyway.
		ScriptSoundStream& operator=(ScriptSoundStream&&) = delete;
		~ScriptSoundStream()
		{
			// SFML doesn't wait for the audio thread.
			stop();
		}

		void fadeInAndPlay()
		{
			assert(getStatus() == Status::Stopped);
			fadeState.store({ true, 0 }, std::memory_order_relaxed);
			SoundStream::play();
		}

		void playWithoutFadeIn()
		{
			assert(getStatus() == Status::Stopped);
			fadeState.store({ true, fadeInDurationSamples }, std::memory_order_relaxed);
			SoundStream::play();
		}

		bool isFadingIn() const
		{
			return fadeState.load(std::memory_order_relaxed).wantFadeIn;
		}

		bool hasFinishedFadingOut() const
		{
			const FadeState oldValue = fadeState.load(std::memory_order_relaxed);
			return !oldValue.wantFadeIn && oldValue.samples >= fadeOutDurationSamples;
			
		}

		void fadeIn()
		{
			FadeState oldValue = fadeState.load(std::memory_order_relaxed);
			FadeState newValue;
			do
			{
				newValue = oldValue;
				newValue.wantFadeIn = true;
			} while (!fadeState.compare_exchange_weak(oldValue, newValue));		}

		void fadeOut()
		{
			FadeState oldValue = fadeState.load(std::memory_order_relaxed);
			FadeState newValue;
			do
			{
				newValue = oldValue;
				newValue.wantFadeIn = false;
			} while (!fadeState.compare_exchange_weak(oldValue, newValue));
		}

		virtual bool onGetData(Chunk& data) override
		{
			// NOTE: getSampleCount == buffer size / 2, not actual sample count
			const int numChannels = buf->getChannelCount();

			// Sound is looped, so this should never happen.
			if (position >= int(buf->getSampleCount() / numChannels))
			{
				data = { .sampleCount = 0 };
				return false;
			}

			const std::span bufSamples{ buf->getSamples(), buf->getSampleCount() };
			const std::span bufSamplesRemaining = bufSamples.subspan(position * numChannels);

			//data = {
			//			.samples = bufSamplesRemaining.data(),
			//			.sampleCount = std::min<size_t>(bufSamplesRemaining.size(), fadeResolutionSamples * numChannels)
			//};

			FadeState localFadeSate = fadeState.load(std::memory_order_acquire);
			int fadeStateSamples = 0;
			for (;;)
			{
				FadeState newFadeState = localFadeSate;
				if (prevFadeInState != localFadeSate.wantFadeIn)
				{
					prevFadeInState = localFadeSate.wantFadeIn;

					if (localFadeSate.wantFadeIn)
					{
						// Fade-out -> fade-in
						newFadeState.samples = int((int64_t(newFadeState.samples) * fadeInDurationSamples + fadeOutDurationSamples / 2)
							/ fadeOutDurationSamples);
						newFadeState.samples = fadeInDurationSamples - newFadeState.samples;
					}
					else
					{
						// Fade-in -> fade-out
						// Transition from fade in to fade out. Convert sample count.
						newFadeState.samples = int((int64_t(fadeInDurationSamples - newFadeState.samples) * fadeOutDurationSamples + fadeInDurationSamples / 2)
							/ fadeInDurationSamples);
					}
				}

				fadeStateSamples = newFadeState.samples;

				if (localFadeSate.wantFadeIn)
				{
					if (newFadeState.samples < fadeInDurationSamples)
					{
						const size_t n = std::min<size_t>({ size_t(fadeInDurationSamples - newFadeState.samples), (size_t)fadeResolutionSamples, bufSamplesRemaining.size() / numChannels });
						newFadeState.samples += (int)n;
					}
					// else, finished fade in
				}
				else
				{
					if (newFadeState.samples < fadeOutDurationSamples)
					{
						const size_t n = std::min<size_t>({ size_t(fadeOutDurationSamples - newFadeState.samples), (size_t)fadeResolutionSamples, bufSamplesRemaining.size() / numChannels });
						newFadeState.samples += (int)n;
					}
					// else, finished fade out
				}

				if (fadeState.compare_exchange_weak(localFadeSate, newFadeState))
					break;
			}
			
			if (localFadeSate.wantFadeIn)
			{
				if (fadeStateSamples < fadeInDurationSamples)
				{
					const size_t n = std::min<size_t>({ size_t(fadeInDurationSamples - fadeStateSamples), (size_t)fadeResolutionSamples, bufSamplesRemaining.size() / numChannels });
					std::ranges::copy(bufSamplesRemaining.subspan(0, n * numChannels), fadeSamplesBuffer.begin());
					for (size_t i = 0; i < n; ++i)
					{
						for (int j = 0; j < numChannels; ++j)
						{
							auto& y = fadeSamplesBuffer[i * numChannels + j];
							y = int16_t(int64_t(int64_t(y) * (fadeStateSamples + i) + fadeInDurationSamples / 2) / int64_t(fadeInDurationSamples));
						}
					}
					data = {
						.samples = fadeSamplesBuffer.data(),
						.sampleCount = n * numChannels,
					};
				}
				else
				{
					data = {
						.samples = bufSamplesRemaining.data(),
						.sampleCount = std::min<size_t>(bufSamplesRemaining.size(), fadeResolutionSamples * numChannels)
					};
				}
			}
			else
			{
				if (fadeStateSamples < fadeOutDurationSamples)
				{
					const size_t n = std::min<size_t>({ size_t(fadeOutDurationSamples - fadeStateSamples), (size_t)fadeResolutionSamples, bufSamplesRemaining.size() / numChannels });
					std::ranges::copy(bufSamplesRemaining.subspan(0, n * numChannels), fadeSamplesBuffer.begin());
					for (size_t i = 0; i < n; ++i)
					{
						for (int j = 0; j < numChannels; ++j)
						{
							auto& y = fadeSamplesBuffer[i * numChannels + j];
							y = int16_t(int64_t(int64_t(y) * (fadeOutDurationSamples - (fadeStateSamples + i)) + fadeOutDurationSamples / 2)
								/ int64_t(fadeOutDurationSamples));
						}
					}
					data = {
						.samples = fadeSamplesBuffer.data(),
						.sampleCount = n * numChannels,
					};
				}
				else // Finished fading out.
				{
					std::ranges::fill(fadeSamplesBuffer, 0);
					data = {
						.samples = fadeSamplesBuffer.data(),
						.sampleCount = fadeSamplesBuffer.size(),
					};
					return true;
				}
			}

			position += int(data.sampleCount / numChannels);
			assert(data.sampleCount > 0);

			return true;
		}

		virtual void onSeek(sf::Time timeOffset) override
		{
			position = int(timeOffset.asMicroseconds() / 1'000'000.0 * buf->getSampleRate());
		}

		virtual std::optional<std::uint64_t> onLoop() override
		{
			if (script->looping && isFadingIn())
				return 0;
			else
				return std::nullopt;
		}

	private:
		static inline std::mt19937 smRNG{ 42 };
	};
}

struct AudioSystem::XmlDefsInternals
{
	AudioDefines audioDefs;
	std::vector<Script2DSound> script2dList;
	std::vector<Script3DSound> script3dList;
	std::vector<Soundscape> soundscapes;

	std::unordered_map<std::string_view, ScriptRef> tagIndexLookup;

	int scriptType2D = -1;
	int scriptType3D = -1;
	int scriptTypeSoundscape = -1;

	void loadXmlDefs()
	{
		MyCvDLLXml xmlSys;
		audioDefs = loadAudioDefines(xmlSys);
		script2dList = loadAudio2DScripts(xmlSys, audioDefs);
		script3dList = loadAudio3DScripts(xmlSys, audioDefs);

		scriptType2D = gGlobals.getTypesEnum("2D");
		scriptType3D = gGlobals.getTypesEnum("3D");
		scriptTypeSoundscape = gGlobals.getTypesEnum("SOUNDSCAPE");
		for (const auto& [i, script] : std::views::enumerate(script2dList))
			tagIndexLookup[script.scriptId] = { scriptType2D, int(i) };
		for (const auto& [i, script] : std::views::enumerate(script3dList))
			tagIndexLookup[script.scriptId] = { scriptType3D, int(i) };
		soundscapes = loadSoundscapes(xmlSys, audioDefs, tagIndexLookup);
		for (const auto& [i, script] : std::views::enumerate(soundscapes))
			tagIndexLookup[script.scriptId] = { scriptTypeSoundscape, int(i) };
	}
};

struct AudioSystem::Internals
{
	int maxConcurrentSounds = kDefaultSimultaneouslyStartedSoundLimit;
	double maxConcurrentSoundTimeSeconds = 0.2;

	

	// Keep a dummy sound around so that SFML doesn't constantly recreate the audio device if we otherwise only have up to one sound playing at a time.
	// Stops debug output spam. Probably stops a memory leak too.
	sf::SoundBuffer dummySoundBuffer;
	sf::Sound dummySound{ dummySoundBuffer };

	// Multimap for sound variants.
	std::unordered_multimap<const SoundData*, sf::SoundBuffer> loadedSounds;

	struct PlayingSound
	{
		sf::Sound audioDeviceSound;
		const SoundData* soundData{};
	};

	std::list<PlayingSound> playingSounds;

	struct PlayingSoundscape : heck::NoCopy
	{
		int soundscapeId = -1;
		std::list<ScriptSoundStream> playingStreams{};

		void fadeIn()
		{
			for (ScriptSoundStream& s : playingStreams)
				s.fadeIn();
		}

		void fadeOut()
		{
			for (ScriptSoundStream& s : playingStreams)
				s.fadeOut();
		}

		void stop()
		{
			for (ScriptSoundStream& s : playingStreams)
				s.stop();
		}

		void garbageCollect()
		{
			for (auto it = playingStreams.begin(); it != playingStreams.end(); )
				if (it->getStatus() != sf::SoundSource::Status::Playing)
					it = playingStreams.erase(it);
				else
					++it;
		}

		[[nodiscard]] bool isGarbage() const
		{
			return playingStreams.empty() || std::ranges::any_of(playingStreams, &ScriptSoundStream::hasFinishedFadingOut);
		}
	};

	static constexpr heck::ivec2 kCitySelectionSoundscapeKey{ INVALID_PLOT_COORD };

	std::multimap<heck::ivec2, PlayingSoundscape, heck::VectorComparator> deactivatedSoundscapes;
	std::map<heck::ivec2, PlayingSoundscape, heck::VectorComparator> playingSoundscapes;


	Internals()
	{
		const int defaultMaxConcurrentSoundTimeMilliseconds = std::lround(maxConcurrentSoundTimeSeconds * 1000);
		maxConcurrentSounds = gCivilizationIVIni.grab(kCivilizationIVIniSection_CV4ENGINE, kCivilizationIVIniProp_MaxConcurrentSounds, maxConcurrentSounds);
		maxConcurrentSoundTimeSeconds = gCivilizationIVIni.grab(kCivilizationIVIniSection_CV4ENGINE, kCivilizationIVIniProp_MaxConcurrentSoundTimeMilliseconds, defaultMaxConcurrentSoundTimeMilliseconds) / 1000.0;
	}

	

	const sf::SoundBuffer& grabSound(const SoundData* soundData)
	{
		const auto [first, last] = loadedSounds.equal_range(soundData);
		if (first != last)
		{
			const size_t n = std::distance(first, last);
			const int index = gGlobals.getASyncRand().get(uint16_t(n));
			return std::next(first, index)->second;
		}

		// Load.
		// First, check the number of variants.
		int n = 0;
		while (n < 100)
		{
			if (!gVFS->exists(std::format("{}-{:03d}.wav", soundData->filename, n)))
				break;
			++n;
		}

		decltype(loadedSounds)::iterator it;

		static const sf::SoundBuffer kEmpty{};

		if (n <= 0)
		{
			// No variants, try the default.
			std::string path = soundData->filename + ".wav";
			if (gVFS->exists(path))
				it = loadedSounds.emplace(soundData, loadWaveFile(gVFS->resolve(path)));
			else if (gVFS->exists(path = soundData->filename + ".mp3"))
			{
				if (auto optBuf = tryLoadFileUsingSFML(gVFS->resolve(path)))
					it = loadedSounds.emplace(soundData, std::move(*optBuf));
				else
				{
					std::clog << "Could not load MP3 " << soundData->filename << std::endl;
					return kEmpty;
				}
			}
			else
			{
				std::clog << "Could not find file for " << soundData->filename << std::endl;
				return kEmpty;
			}
		}
		else
		{
			// Load the variants.
			for (const int i : range(n))
				it = loadedSounds.emplace(soundData, loadWaveFile(gVFS->resolve(std::format("{}-{:03d}.wav", soundData->filename, i))));
		}

		return it->second;
	}

	void garbageCollect()
	{
		std::erase_if(playingSounds, [](const PlayingSound& sound) {
			return sound.audioDeviceSound.getStatus() != sf::SoundSource::Status::Playing;
		});

		for (PlayingSoundscape& ss : deactivatedSoundscapes | std::views::values)
			ss.garbageCollect();

		std::erase_if(deactivatedSoundscapes, [](const auto& kv) { return kv.second.isGarbage(); });

		while (playingSounds.size() > 10)
		{
			playingSounds.front().audioDeviceSound.stop();
			playingSounds.pop_front();
		}

		for (auto it = deactivatedSoundscapes.begin(); it != deactivatedSoundscapes.end(); )
			if (it->second.isGarbage())
				it = deactivatedSoundscapes.erase(it);
			else
				++it;
		

		//while (deactivatedSoundscapes.size() > 10)
		//{
		//	deactivatedSoundscapes.front().stop();
		//	deactivatedSoundscapes.erase(deactivatedSoundscapes.begin());
		//}
	}

	size_t countConcurrentSounds(const SoundData* soundData) const
	{
		return std::ranges::count_if(playingSounds, [soundData, this](const PlayingSound& sound) {
			//return sound.soundData == soundData && sound.audioDeviceSound.getPlayingOffset().asSeconds() <= maxConcurrentSoundTimeSeconds;
			// Compare filename instead. This is why peace sounds so loud: SND_MAKEPEACE and SND_THEIRMAKEPEACE happen at the same time.
			return sound.soundData->filename == soundData->filename && sound.audioDeviceSound.getPlayingOffset().asSeconds() <= maxConcurrentSoundTimeSeconds;
			});
	}

	sf::Sound* genericPlaySound(const SoundData* soundData)
	{
		const sf::SoundBuffer& buf = grabSound(soundData);

		if (buf.getSampleCount() > 0)
		{
			garbageCollect();

			if (std::cmp_less(playingSounds.size(), maxConcurrentSounds) || std::cmp_less(countConcurrentSounds(soundData), maxConcurrentSounds))
			{
				//std::clog << "Playing sound " << soundData->soundId << std::endl;
				PlayingSound& sound = playingSounds.emplace_back(sf::Sound(buf), soundData);
				sound.audioDeviceSound.play();
				return &sound.audioDeviceSound;
			}
		}

		return nullptr;
	}

	void play2DSoundScript(const XmlDefsInternals& xmlDefs, std::string_view name)
	{
		if (name.empty())
			return;

		const auto it = xmlDefs.tagIndexLookup.find(name);
		if (it == xmlDefs.tagIndexLookup.end() || it->second.type != xmlDefs.scriptType2D)
		{
			std::clog << "Sound script " << name << " doesn't exist." << std::endl;
			return;
		}

		sf::Sound* sound = nullptr;

		if (it->second.type == xmlDefs.scriptType2D)
		{
			const Script2DSound& script = xmlDefs.script2dList[it->second.index];
			// Why is spatialisation enabled by default??
			sound = genericPlaySound(script.soundData);
		}
		else if (it->second.type == xmlDefs.scriptType3D)
		{
			const Script3DSound& script = xmlDefs.script3dList[it->second.index];
			sound = genericPlaySound(script.soundData);
		}

		if (sound)
			sound->setSpatializationEnabled(false);
	}

	void playScript3DSoundByIndex(const XmlDefsInternals& xmlDefs, int index, heck::ivec2 plotCoord)
	{
		if (index < 0)
			return;

		const Script3DSound& script = xmlDefs.script3dList[index];

		if (sf::Sound* const sound = genericPlaySound(script.soundData))
		{
			//const IVector2D center = CvInterface::getInstance().getWorldView().getLookAtPlotCoord();
			sound->setSpatializationEnabled(true);
			sound->setPosition({ float(plotCoord.x), float(plotCoord.y), 0.0f });
		}
	}

	void deactivateAllSoundscapes()
	{
		for (auto&& [key, playingSoundscape] : playingSoundscapes)
		{
			playingSoundscape.fadeOut();
			deactivatedSoundscapes.emplace(key, std::move(playingSoundscape));
		}
		playingSoundscapes.clear();
	}

	PlayingSoundscape createPlayingSoundscape(const XmlDefsInternals& xmlDefs, int index, float volume)
	{
		const Soundscape& soundscape = xmlDefs.soundscapes[index];

		PlayingSoundscape playingSoundscape{};
		playingSoundscape.soundscapeId = index;

		for (const auto& element : soundscape.elements)
		{
			if (element.type == xmlDefs.scriptType2D)
			{
				const Script2DSound& script = xmlDefs.script2dList[element.index];
				const sf::SoundBuffer* const buf = &grabSound(script.soundData);

				if (buf->getSampleCount() > 0)
				{
					//const auto it = std::ranges::find_if(playingStreams, [&script](const ScriptSoundStream& stream) {
					//	return stream.script == &script;
					//});
					//
					//if (it == playingStreams.end())
					{
						//std::clog << "Activating new soundscape " << index << std::endl;
						//for (auto& stream : playingStreams)
						//	stream.fadeOut();
						playingSoundscape.playingStreams.emplace_back(&script, buf).fadeInAndPlay();
					}
					//else
					//{
					//	if (!it->isFadingIn()) // Don't spam this message.
					//		std::clog << "Fading-in existing soundscape " << index << std::endl;
					//	// Fade-out all the others.
					//	for (auto& stream : playingStreams)
					//		if (&stream != &*it)
					//			stream.fadeOut();
					//	it->fadeIn();
					//	playingStreams.splice(playingStreams.end(), playingStreams, it);
					//}

					playingSoundscape.playingStreams.back().setVolume(volume);
				}
				break;
			}
		}

		return playingSoundscape;
	}
};

AudioSystem::AudioSystem() : mXmlDefsInternals(std::make_unique<XmlDefsInternals>())
{
	if constexpr (!kEnable)
		std::clog << "Not compiled with audio support." << std::endl;
	else
	{
		std::clog << "Initialising audio..." << std::endl;
		// Checking getAvailableDevices() is not reliable as WSL will give SFML a device even when SFML can't open it.
		const sf::SoundBuffer dummySoundBuffer;
		const sf::Sound dummySound{ dummySoundBuffer };
		if (const auto optDevice = sf::PlaybackDevice::getDevice())
		{
			std::clog << "SFML is using " << *optDevice << std::endl;
			mInternals = std::make_unique<Internals>();
		}
		else
			std::clog << "SFML failed to init an audio device. Continuing with no audio." << std::endl;
	}
}

AudioSystem::AudioSystem(AudioSystem&&) noexcept = default;
AudioSystem& AudioSystem::operator=(AudioSystem&&) noexcept = default;
AudioSystem::~AudioSystem() noexcept = default;

void AudioSystem::loadXmlDefs()
{
	mXmlDefsInternals->loadXmlDefs();
}

int AudioSystem::getAudioTagIndex(std::string_view name, AudioTag tagType) const
{
	if (name.empty())
		return -1;

	int soundType = -1;

	switch (tagType)
	{
	case AUDIOTAG_NONE:
		break;
	case AUDIOTAG_2DSCRIPT:
		soundType = mXmlDefsInternals->scriptType2D;
		break;
	case AUDIOTAG_3DSCRIPT:
		soundType = mXmlDefsInternals->scriptType3D;
		break;
	case AUDIOTAG_SOUNDSCAPE:
		soundType = mXmlDefsInternals->scriptTypeSoundscape;
		break;
	default:
		std::abort();
	}
	
	const auto it = mXmlDefsInternals->tagIndexLookup.find(name);
	if (it != mXmlDefsInternals->tagIndexLookup.end() && (soundType < 0 || it->second.type == soundType))
		return it->second.index;
	else
		std::abort();
}

void AudioSystem::playSound(std::string_view name)
{
	if (mInternals)
		mInternals->play2DSoundScript(*mXmlDefsInternals, name);
}

void AudioSystem::playScript3DSoundByIndex(int index, heck::ivec2 plotCoord)
{
	if (mInternals)
		mInternals->playScript3DSoundByIndex(*mXmlDefsInternals, index, plotCoord);
}

void AudioSystem::updateListener()
{
	if (mInternals)
	{
		const WorldView& worldView = CvInterface::getInstance().getWorldView();
		const heck::ivec2 center = worldView.getLookAtPlotCoord();
		const int zoom = worldView.getZoom();
		// TODO: Difficult to check whether Y and Z sign are correct.
		sf::Listener::setPosition({ float(center.x), float(center.y), 12.0f - zoom });

		mInternals->garbageCollect();

		auto oldSoundscapes = std::move(mInternals->playingSoundscapes);

		auto useSoundscape = [&](heck::ivec2 key, int index, float volume) {
			if (index < 0)
				return;


			if (const auto it = oldSoundscapes.find(key); it != oldSoundscapes.end() && it->second.soundscapeId == index)
			{
				mInternals->playingSoundscapes.insert(oldSoundscapes.extract(it));
				return;
			}


			if (const auto [first, last] = mInternals->deactivatedSoundscapes.equal_range(key); first != last)
			{
				const std::ranges::subrange r(first, last);
				if (const auto it = std::ranges::find_last(r, index, [](const auto& kv) {
					return kv.second.soundscapeId;
					}).begin(); it != r.end())
				{
					Internals::PlayingSoundscape ss = std::move(mInternals->deactivatedSoundscapes.extract(it).mapped());
					ss.fadeIn();
					mInternals->playingSoundscapes.emplace(key, std::move(ss));
					return;
				}
			}
				
			mInternals->playingSoundscapes.emplace(key, mInternals->createPlayingSoundscape(*mXmlDefsInternals, index, volume));
			};

		// Update soundscape.
		if (const CvCity* const city = CvInterface::getInstance().getHeadSelectedCity())
			useSoundscape(Internals::kCitySelectionSoundscapeKey, city->getSoundscapeScriptId(), kCityScreenSoundscapeVolume);
		else
		{
			// Could do more, but this isn't really how it's supposed to work, I think.
			constexpr int kRadius = 0;
			for (int dy = -kRadius; dy <= kRadius; ++dy)
			{
				for (int dx = -kRadius; dx <= kRadius; ++dx)
				{
					const heck::ivec2 relCoord(dx, dy);
					const heck::ivec2 coord = center + relCoord;
					if (const CvPlot* const plot = gGlobals.getMap().plot(coord.x, coord.y))
					{
						if (plot->isCity() && plot->getPlotCity()->isRevealed(gGlobals.getGame().getActiveTeam(), false))
							useSoundscape(relCoord, plot->getPlotCity()->getSoundscapeScriptId(), kWorldSoundscapeVolume);
						else
							useSoundscape(relCoord, plot->getSoundScriptId(), kWorldSoundscapeVolume);
					}
				}
			}
		}

		// Everything else gets deactivated.
		for (auto&& [key, ss] : oldSoundscapes)
		{
			ss.fadeOut();
			mInternals->deactivatedSoundscapes.emplace(key, std::move(ss));
		}
	}
}

void AudioSystem::clearSoundScape()
{
	if (mInternals)
		mInternals->deactivateAllSoundscapes();
}
