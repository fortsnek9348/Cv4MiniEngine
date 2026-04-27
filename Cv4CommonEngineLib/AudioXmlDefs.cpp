#include "inc/Cv4CommonEngineLib/AudioXmlDefs.h"
#include "MyCvDLLXml.h"

#include <CvGameCoreDLL/CvGlobals.h>

#include <ranges>

using namespace cvengine;

namespace
{
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
}

AudioXmlDefs& AudioXmlDefs::getInstance()
{
	static AudioXmlDefs g;
	return g;
}

void AudioXmlDefs::loadXmlDefs()
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

//int AudioXmlDefs::getAudioTagIndex(std::string_view name, AudioTag tagType) const
//{
//	if (name.empty())
//		return -1;
//
//	int soundType = -1;
//
//	switch (tagType)
//	{
//	case AUDIOTAG_NONE:
//		break;
//	case AUDIOTAG_2DSCRIPT:
//		soundType = scriptType2D;
//		break;
//	case AUDIOTAG_3DSCRIPT:
//		soundType = scriptType3D;
//		break;
//	case AUDIOTAG_SOUNDSCAPE:
//		soundType = scriptTypeSoundscape;
//		break;
//	default:
//		std::abort();
//	}
//
//	const auto it = tagIndexLookup.find(name);
//	if (it != tagIndexLookup.end() && (soundType < 0 || it->second.type == soundType))
//		return it->second.index;
//	else
//		std::abort();
//}
