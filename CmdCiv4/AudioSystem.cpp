#include "AudioSystem.h"
#include "CvTuiInterface.h"

#include <Cv4CommonEngineLib/AudioXmlDefs.h>
#include <Cv4CommonEngineLib/CvVFS.h>
#include <Cv4CommonEngineLib/Common.h>
#include <Cv4CommonEngineLib/MyFFile.h>
#include <Cv4CommonEngineLib/CivIni.h>

#include <CvGlobals.h>
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
static constexpr float kWorldSoundscapeVolume = 70.0f;
static constexpr float kCityScreenSoundscapeVolume = 80.0f;
//static constexpr float kWorldSoundscapeVolume = 100.0f;
//static constexpr float kCityScreenSoundscapeVolume = 100.0f;
static constexpr int kDefaultSimultaneouslyStartedSoundLimit = 1;

namespace
{
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
		ScriptSoundStream& operator=(ScriptSoundStream&&) = delete;
		~ScriptSoundStream()
		{
			// SFML should wait for the audio thread here (https://github.com/SFML/SFML/blame/master/src/SFML/Audio/SoundStream.cpp#L316)
			// Do this before `fadeSamplesBuffer` gets destroyed.
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

			//sf::sleep(sf::milliseconds(1));

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
					// Okay, good, this makes the race condition easy to reproduce.
					// The race condition happens with an old build of SFML (aef34af5568c410f971be0fc53175f131127afde), even when calling `stop` in the dtor.
					//sf::sleep(sf::milliseconds(5));

					const size_t n = std::min<size_t>({ size_t(fadeOutDurationSamples - fadeStateSamples), (size_t)fadeResolutionSamples, bufSamplesRemaining.size() / numChannels });
					std::ranges::copy(bufSamplesRemaining.subspan(0, n * numChannels), fadeSamplesBuffer.begin());
					for (size_t i = 0; i < n; ++i)
					{
						for (int j = 0; j < numChannels; ++j)
						{
							// FIXED: A race condition happened that crashes here.
							//        A soundstream was destroyed just as we're finishing a fade out, but SFML/miniaudio should wait until the audio thread is finished?
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

struct AudioSystem::Internals
{
	cvengine::AudioXmlDefs& xmlDefs = cvengine::AudioXmlDefs::getInstance();

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
			{
				if (it->getStatus() != sf::SoundSource::Status::Playing)
					it = playingStreams.erase(it);
				else
					++it;
			}
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

		std::erase_if(deactivatedSoundscapes, [](auto& kv) {  return kv.second.isGarbage(); });

		while (playingSounds.size() > 10)
		{
			playingSounds.front().audioDeviceSound.stop();
			playingSounds.pop_front();
		}

		// Why did I leave this in...
		//for (auto it = deactivatedSoundscapes.begin(); it != deactivatedSoundscapes.end(); )
		//	if (it->second.isGarbage())
		//		it = deactivatedSoundscapes.erase(it);
		//	else
		//		++it;
		

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

	void play2DSoundScript(std::string_view name)
	{
		if (name.empty())
			return;

		const auto it = xmlDefs.tagIndexLookup.find(name);
		if (it == xmlDefs.tagIndexLookup.end() || it->second.type != xmlDefs.scriptType2D)
		{
			std::clog << "Sound script " << name << " doesn't exist." << std::endl;
			return;
		}

		if (it->second.type == xmlDefs.scriptType2D)
			play2DSoundScriptByIndex(it->second.index);
		else if (it->second.type == xmlDefs.scriptType3D)
			play3DSoundScriptByIndexNoSpatialisation(it->second.index);
	}

	void play2DSoundScriptByIndex(int index)
	{
		if (index < 0)
			return;

		const Script2DSound& script = xmlDefs.script2dList[index];
		// Why is spatialisation enabled by default??
		if (sf::Sound* const sound = genericPlaySound(script.soundData))
			sound->setSpatializationEnabled(false);
	}

	void play3DSoundScriptByIndexNoSpatialisation(int index)
	{
		if (index < 0)
			return;

		const Script3DSound& script = xmlDefs.script3dList[index];
		// Why is spatialisation enabled by default??
		if (sf::Sound* const sound = genericPlaySound(script.soundData))
			sound->setSpatializationEnabled(false);
	}

	void playScript3DSoundByIndex(int index, heck::ivec2 plotCoord)
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

	PlayingSoundscape createPlayingSoundscape(int index, float volume)
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

AudioSystem::AudioSystem()
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
	mInternals->xmlDefs.loadXmlDefs();
}

void AudioSystem::playSound(std::string_view name)
{
	if (mInternals)
		mInternals->play2DSoundScript(name);
}

void AudioSystem::playScript2DSoundByIndex(int index)
{
	if (mInternals)
		mInternals->play2DSoundScriptByIndex(index);
}

void AudioSystem::playScript3DSoundByIndex(int index, heck::ivec2 plotCoord)
{
	if (mInternals)
		mInternals->playScript3DSoundByIndex(index, plotCoord);
}

void AudioSystem::updateListener()
{
	if (mInternals)
	{
		const WorldView& worldView = CvTuiInterface::getInstance().getWorldView();
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
				
			mInternals->playingSoundscapes.emplace(key, mInternals->createPlayingSoundscape(index, volume));
			};

		// Update soundscape.
		if (const CvCity* const city = CvTuiInterface::getInstance().getHeadSelectedCity())
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
