#include "inc/Cv4CommonEngineLib/CvUserProfile.h"
#include "CommonEngineGlobal.h"

#include <CvGameCoreDLL/CvGlobals.h>
#include <CvGameCoreDLL/CvInfos.h>

#include <CommonStuff/range.h>

#include <fstream>
#include <iostream>

CvUserProfile& CvUserProfile::getInstance()
{
	static CvUserProfile g;
	return g;
}

CvUserProfile::CvUserProfile() : mPath(cvengine::gCommonEngineConfig.userConfigDirPath / cvengine::gCommonEngineConfig.profileFilename), mPlayerOptions(GC.getNumPlayerOptionInfos())
{
}

void CvUserProfile::resetOptions(TabGroupTypes resetTab)
{
	switch (resetTab)
	{
	case TABGROUP_GAME:
		for (const size_t i : heck::range(mPlayerOptions.size()))
		{
			mPlayerOptions[i] = GC.getPlayerOptionInfo(static_cast<PlayerOptionTypes>(i)).getDefault();
			// Apply overrides.
			mPlayerOptions[i] = getPlayerOption(static_cast<PlayerOptionTypes>(i));
		}
		break;

	default:
		break;
	}
}

bool CvUserProfile::getPlayerOption(PlayerOptionTypes i) const
{
	switch (i)
	{
		// Mandatory options.
	case PLAYEROPTION_QUICK_MOVES:
	case PLAYEROPTION_QUICK_ATTACK:
	case PLAYEROPTION_QUICK_DEFENSE:
	case PLAYEROPTION_RIGHT_CLICK_MENU:
		return true;
	case PLAYEROPTION_MINIMIZE_POP_UPS:
	case PLAYEROPTION_SHOW_FRIENDLY_MOVES:
	case PLAYEROPTION_SHOW_ENEMY_MOVES:
	case PLAYEROPTION_NUMPAD_HELP:
		return false;
	default:
		return mPlayerOptions.at(i);
	}
}
void CvUserProfile::setPlayerOption(PlayerOptionTypes i, bool x)
{
	mPlayerOptions.at(i) = x;
}

unsigned int CvUserProfile::getLanguage() const
{
	return mLanguage;
}

void CvUserProfile::setLanguage(unsigned int i)
{
	mLanguage = i < static_cast<unsigned int>(CvGameText::NUM_LANGUAGES) ? i : 0;
}

void CvUserProfile::readFromFile()
{
	std::ifstream file(mPath);

	if (!file)
	{
		std::clog << "Could not open profile file. Using default options." << std::endl;
		resetOptions(TabGroupTypes::TABGROUP_GAME);
		resetOptions(TabGroupTypes::TABGROUP_INPUT);
		resetOptions(TabGroupTypes::TABGROUP_GRAPHICS);
		resetOptions(TabGroupTypes::TABGROUP_AUDIO);
		resetOptions(TabGroupTypes::TABGROUP_CLOCK);
		return;
	}

	for (const size_t i : heck::range(mPlayerOptions.size()))
	{
		bool x{};
		file >> x;
		mPlayerOptions[i] = x;
		// Apply overrides.
		mPlayerOptions[i] = getPlayerOption(static_cast<PlayerOptionTypes>(i));
	}

	file >> mLanguage;

	if (!file)
		std::clog << "Failed to read from profile. Some options will be defaulted." << std::endl;

	std::clog << "Profile loaded." << std::endl;
}

// This is called from python.
void CvUserProfile::writeToFile()
{
	std::filesystem::path tempPath = mPath;
	tempPath.replace_extension(".tmp");
	{
		std::ofstream file(tempPath);
		for (const size_t i : heck::range(mPlayerOptions.size()))
			file << mPlayerOptions[i] << '\n';
		file << mLanguage << '\n';
		if (!file)
			throw std::runtime_error("Failed to write user profile.");
	}

	// Atomically replace.
	std::filesystem::rename(tempPath, mPath);

	std::clog << "Profile saved." << std::endl;
}