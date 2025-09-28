#include "CyUserProfile.h"
#include "../CvUserProfile.h"

void CyUserProfile::registerWithPython(const pybind11::module& m)
{
#define R(x) def(#x, &CyUserProfile::x)

	pybind11::class_<CyUserProfile>(m, "CyUserProfile")
		.def(pybind11::init())
		.R(deleteProfileFile)
		.R(getAmbienceVolume)
		.R(getAntiAliasing)
		.R(getAntiAliasingMaxMultiSamples)
		.R(getCaptureDeviceDesc)
		.R(getCaptureDeviceIndex)
		.R(getCaptureVolume)
		.R(getCurrentVersion)
		.R(getGlobeLayer)
		.R(getGlobeViewRenderLevel)
		.R(getGraphicOption)
		.R(getGraphicsLevel)
		.R(getGrid)
		.R(getInterfaceVolume)
		.R(getMainMenu)
		.R(getMap)
		.R(getMasterVolume)
		.R(getMaxCaptureVolume)
		.R(getMaxPlaybackVolume)
		.R(getMovieQualityLevel)
		.R(getMusicPath)
		.R(getMusicVolume)
		.R(getNumCaptureDevices)
		.R(getNumPlaybackDevices)
		.R(getNumProfileFiles)
		.R(getPlaybackDeviceDesc)
		.R(getPlaybackDeviceIndex)
		.R(getPlaybackVolume)
		.R(getPlayerOption)
		.R(getProfileFileName)
		.R(getProfileName)
		.R(getProfileVersion)
		.R(getRenderQualityLevel)
		.R(getResolution)
		.R(getResolutionMaxModes)
		.R(getResolutionString)
		.R(getScores)
		.R(getSoundEffectsVolume)
		.R(getSpeakerConfig)
		.R(getSpeakerConfigFromList)
		.R(getSpeechVolume)
		.R(getVolumeStops)
		.R(getYields)
		.R(is24Hours)
		.R(isAmbienceNoSound)
		.R(isClockOn)
		.R(isInterfaceNoSound)
		.R(isMasterNoSound)
		.R(isMusicNoSound)
		.R(isProfileFileExist)
		.R(isSoundEffectsNoSound)
		.R(isSpeechNoSound)
		.R(loadProfileFileNames)
		.R(musicPathDialogBox)
		.R(readFromFile)
		.R(recalculateAudioSettings)
		.R(resetOptions)
		.R(set24Hours)
		.R(setAmbienceNoSound)
		.R(setAmbienceVolume)
		.R(setAntiAliasing)
		.R(setCaptureDevice)
		.R(setCaptureVolume)
		.R(setClockJustTurnedOn)
		.R(setClockOn)
		.R(setGlobeViewRenderLevel)
		.R(setGraphicOption)
		.R(setGraphicsLevel)
		.R(setInterfaceNoSound)
		.R(setInterfaceVolume)
		.R(setMainMenu)
		.R(setMasterNoSound)
		.R(setMasterVolume)
		.R(setMovieQualityLevel)
		.R(setMusicNoSound)
		.R(setMusicPath)
		.R(setMusicVolume)
		.R(setPlaybackDevice)
		.R(setPlaybackVolume)
		.R(setProfileName)
		.R(setRenderQualityLevel)
		.R(setResolution)
		.R(setSoundEffectsNoSound)
		.R(setSoundEffectsVolume)
		.R(setSpeakerConfig)
		.R(setSpeechNoSound)
		.R(setSpeechVolume)
		.R(setUseVoice)
		.R(useVoice)
		.R(wasClockJustTurnedOn)
		.R(writeToFile)
		;
}

bool CyUserProfile::deleteProfileFile([[maybe_unused]] const std::string& szNewName)
{
	std::abort();
}

int CyUserProfile::getAmbienceVolume()
{
	std::abort();
}

int CyUserProfile::getAntiAliasing()
{
	std::abort();
}

int CyUserProfile::getAntiAliasingMaxMultiSamples()
{
	std::abort();
}

std::string CyUserProfile::getCaptureDeviceDesc([[maybe_unused]] int iIndex)
{
	std::abort();
}

int CyUserProfile::getCaptureDeviceIndex()
{
	std::abort();
}

int CyUserProfile::getCaptureVolume()
{
	std::abort();
}

int CyUserProfile::getCurrentVersion()
{
	std::abort();
}

int CyUserProfile::getGlobeLayer()
{
	std::abort();
}

int CyUserProfile::getGlobeViewRenderLevel()
{
	std::abort();
}

bool CyUserProfile::getGraphicOption(int i)
{
	// For now I guess.
	if (i == GRAPHICOPTION_NO_MOVIES)
		return true;
	std::abort();
}

int CyUserProfile::getGraphicsLevel()
{
	std::abort();
}

bool CyUserProfile::getGrid()
{
	std::abort();
}

int CyUserProfile::getInterfaceVolume()
{
	std::abort();
}

int CyUserProfile::getMainMenu()
{
	std::abort();
}

bool CyUserProfile::getMap()
{
	std::abort();
}

int CyUserProfile::getMasterVolume()
{
	std::abort();
}

int CyUserProfile::getMaxCaptureVolume()
{
	std::abort();
}

int CyUserProfile::getMaxPlaybackVolume()
{
	std::abort();
}

int CyUserProfile::getMovieQualityLevel()
{
	std::abort();
}

std::string CyUserProfile::getMusicPath()
{
	std::abort();
}

int CyUserProfile::getMusicVolume()
{
	std::abort();
}

int CyUserProfile::getNumCaptureDevices()
{
	std::abort();
}

int CyUserProfile::getNumPlaybackDevices()
{
	std::abort();
}

int CyUserProfile::getNumProfileFiles()
{
	std::abort();
}

std::string CyUserProfile::getPlaybackDeviceDesc([[maybe_unused]] int iIndex)
{
	std::abort();
}

int CyUserProfile::getPlaybackDeviceIndex()
{
	std::abort();
}

int CyUserProfile::getPlaybackVolume()
{
	std::abort();
}

bool CyUserProfile::getPlayerOption(int i)
{
	return CvUserProfile::getInstance().getPlayerOption(static_cast<PlayerOptionTypes>(i));
}

std::string CyUserProfile::getProfileFileName([[maybe_unused]] int iFileID)
{
	std::abort();
}

std::string CyUserProfile::getProfileName()
{
	return "Default Profile";
}

int CyUserProfile::getProfileVersion()
{
	std::abort();
}

int CyUserProfile::getRenderQualityLevel()
{
	std::abort();
}

int CyUserProfile::getResolution()
{
	std::abort();
}

int CyUserProfile::getResolutionMaxModes()
{
	std::abort();
}

std::string CyUserProfile::getResolutionString([[maybe_unused]] int iResolution)
{
	std::abort();
}

bool CyUserProfile::getScores()
{
	std::abort();
}

int CyUserProfile::getSoundEffectsVolume()
{
	std::abort();
}

std::string CyUserProfile::getSpeakerConfig()
{
	std::abort();
}

std::string CyUserProfile::getSpeakerConfigFromList([[maybe_unused]] int iIndex)
{
	std::abort();
}

int CyUserProfile::getSpeechVolume()
{
	std::abort();
}

int CyUserProfile::getVolumeStops()
{
	std::abort();
}

bool CyUserProfile::getYields()
{
	std::abort();
}

bool CyUserProfile::is24Hours()
{
	std::abort();
}

bool CyUserProfile::isAmbienceNoSound()
{
	std::abort();
}

bool CyUserProfile::isClockOn()
{
	// No clock for now.
	return false;
}

bool CyUserProfile::isInterfaceNoSound()
{
	std::abort();
}

bool CyUserProfile::isMasterNoSound()
{
	std::abort();
}

bool CyUserProfile::isMusicNoSound()
{
	std::abort();
}

bool CyUserProfile::isProfileFileExist([[maybe_unused]] const std::string& szNewName)
{
	std::abort();
}

bool CyUserProfile::isSoundEffectsNoSound()
{
	std::abort();
}

bool CyUserProfile::isSpeechNoSound()
{
	std::abort();
}

void CyUserProfile::loadProfileFileNames()
{
	std::abort();
}

void CyUserProfile::musicPathDialogBox()
{
	std::abort();
}

bool CyUserProfile::readFromFile([[maybe_unused]] const std::string& szFileName)
{
	std::abort();
}

void CyUserProfile::recalculateAudioSettings()
{
	std::abort();
}

void CyUserProfile::resetOptions(TabGroupTypes resetTab)
{
	CvUserProfile::getInstance().resetOptions(resetTab);
}

void CyUserProfile::set24Hours([[maybe_unused]] bool bValue)
{
	std::abort();
}

void CyUserProfile::setAmbienceNoSound([[maybe_unused]] bool b)
{
	std::abort();
}

void CyUserProfile::setAmbienceVolume([[maybe_unused]] int i)
{
	std::abort();
}

void CyUserProfile::setAntiAliasing([[maybe_unused]] int i)
{
	std::abort();
}

void CyUserProfile::setCaptureDevice([[maybe_unused]] int device)
{
	std::abort();
}

void CyUserProfile::setCaptureVolume([[maybe_unused]] int volume)
{
	std::abort();
}

void CyUserProfile::setClockJustTurnedOn([[maybe_unused]] bool bValue)
{
	std::abort();
}

void CyUserProfile::setClockOn([[maybe_unused]] bool bValue)
{
	std::abort();
}

void CyUserProfile::setGlobeViewRenderLevel([[maybe_unused]] int level)
{
	std::abort();
}

void CyUserProfile::setGraphicOption([[maybe_unused]] int i, [[maybe_unused]] bool b)
{
	std::abort();
}

void CyUserProfile::setGraphicsLevel([[maybe_unused]] int i)
{
	std::abort();
}

void CyUserProfile::setInterfaceNoSound([[maybe_unused]] bool b)
{
	std::abort();
}

void CyUserProfile::setInterfaceVolume([[maybe_unused]] int i)
{
	std::abort();
}

void CyUserProfile::setMainMenu([[maybe_unused]] int i)
{
	std::abort();
}

void CyUserProfile::setMasterNoSound([[maybe_unused]] bool b)
{
	std::abort();
}

void CyUserProfile::setMasterVolume([[maybe_unused]] int i)
{
	std::abort();
}

void CyUserProfile::setMovieQualityLevel([[maybe_unused]] int level)
{
	std::abort();
}

void CyUserProfile::setMusicNoSound([[maybe_unused]] bool b)
{
	std::abort();
}

void CyUserProfile::setMusicPath([[maybe_unused]] const std::string& szMusicPath)
{
	std::abort();
}

void CyUserProfile::setMusicVolume([[maybe_unused]] int i)
{
	std::abort();
}

void CyUserProfile::setPlaybackDevice([[maybe_unused]] int device)
{
	std::abort();
}

void CyUserProfile::setPlaybackVolume([[maybe_unused]] int volume)
{
	std::abort();
}

void CyUserProfile::setProfileName([[maybe_unused]] const std::string& szNewName)
{
	std::abort();
}

void CyUserProfile::setRenderQualityLevel([[maybe_unused]] int level)
{
	std::abort();
}

void CyUserProfile::setResolution([[maybe_unused]] int i)
{
	std::abort();
}

void CyUserProfile::setSoundEffectsNoSound([[maybe_unused]] bool b)
{
	std::abort();
}

void CyUserProfile::setSoundEffectsVolume([[maybe_unused]] int i)
{
	std::abort();
}

void CyUserProfile::setSpeakerConfig([[maybe_unused]] const std::string& szConfigName)
{
	std::abort();
}

void CyUserProfile::setSpeechNoSound([[maybe_unused]] bool b)
{
	std::abort();
}

void CyUserProfile::setSpeechVolume([[maybe_unused]] int i)
{
	std::abort();
}

void CyUserProfile::setUseVoice([[maybe_unused]] bool b)
{
	std::abort();
}

bool CyUserProfile::useVoice()
{
	std::abort();
}

bool CyUserProfile::wasClockJustTurnedOn()
{
	std::abort();
}

void CyUserProfile::writeToFile([[maybe_unused]] const std::string& szFileName)
{
	CvUserProfile::getInstance().writeToFile();
}
