#pragma once

#include <CvGameCoreDLL/CvEnums.h>

#include <pybind11/pybind11.h>

class CyUserProfile
{
public:
	static void registerWithPython(const pybind11::module& m);

	bool deleteProfileFile(const std::string& szNewName);
	int getAmbienceVolume();
	int getAntiAliasing();
	int getAntiAliasingMaxMultiSamples();
	std::string getCaptureDeviceDesc(int iIndex);
	int getCaptureDeviceIndex();
	int getCaptureVolume();
	int getCurrentVersion();
	int getGlobeLayer();
	int getGlobeViewRenderLevel();
	bool getGraphicOption(int i);
	int getGraphicsLevel();
	bool getGrid();
	int getInterfaceVolume();
	int getMainMenu();
	bool getMap();
	int getMasterVolume();
	int getMaxCaptureVolume();
	int getMaxPlaybackVolume();
	int getMovieQualityLevel();
	std::string getMusicPath();
	int getMusicVolume();
	int getNumCaptureDevices();
	int getNumPlaybackDevices();
	int getNumProfileFiles();
	std::string getPlaybackDeviceDesc(int iIndex);
	int getPlaybackDeviceIndex();
	int getPlaybackVolume();
	bool getPlayerOption(int i);
	std::string getProfileFileName(int iFileID);
	std::string getProfileName();
	int getProfileVersion();
	int getRenderQualityLevel();
	int getResolution();
	int getResolutionMaxModes();
	std::string getResolutionString(int iResolution);
	bool getScores();
	int getSoundEffectsVolume();
	std::string getSpeakerConfig();
	std::string getSpeakerConfigFromList(int iIndex);
	int getSpeechVolume();
	int getVolumeStops();
	bool getYields();
	bool is24Hours();
	bool isAmbienceNoSound();
	bool isClockOn();
	bool isInterfaceNoSound();
	bool isMasterNoSound();
	bool isMusicNoSound();
	bool isProfileFileExist(const std::string& szNewName);
	bool isSoundEffectsNoSound();
	bool isSpeechNoSound();
	void loadProfileFileNames();
	void musicPathDialogBox();
	bool readFromFile(const std::string& szFileName);
	void recalculateAudioSettings();
	void resetOptions(TabGroupTypes resetTab);
	void set24Hours(bool bValue);
	void setAmbienceNoSound(bool b);
	void setAmbienceVolume(int i);
	void setAntiAliasing(int i);
	void setCaptureDevice(int device);
	void setCaptureVolume(int volume);
	void setClockJustTurnedOn(bool bValue);
	void setClockOn(bool bValue);
	void setGlobeViewRenderLevel(int level);
	void setGraphicOption(int i, bool b);
	void setGraphicsLevel(int i);
	void setInterfaceNoSound(bool b);
	void setInterfaceVolume(int i);
	void setMainMenu(int i);
	void setMasterNoSound(bool b);
	void setMasterVolume(int i);
	void setMovieQualityLevel(int level);
	void setMusicNoSound(bool b);
	void setMusicPath(const std::string& szMusicPath);
	void setMusicVolume(int i);
	void setPlaybackDevice(int device);
	void setPlaybackVolume(int volume);
	void setProfileName(const std::string& szNewName);
	void setRenderQualityLevel(int level);
	void setResolution(int i);
	void setSoundEffectsNoSound(bool b);
	void setSoundEffectsVolume(int i);
	void setSpeakerConfig(const std::string& szConfigName);
	void setSpeechNoSound(bool b);
	void setSpeechVolume(int i);
	void setUseVoice(bool b);
	bool useVoice();
	bool wasClockJustTurnedOn();
	void writeToFile(const std::string& szFileName);
};