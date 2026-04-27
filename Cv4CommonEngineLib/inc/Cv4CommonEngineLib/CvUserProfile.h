#pragma once

#include <CvGameCoreDLL/CvEnums.h>

#include <vector>
#include <filesystem>

class CvUserProfile
{
public:
	// Let's just have a global like everything else...
	static CvUserProfile& getInstance();

	CvUserProfile();

	void resetOptions(TabGroupTypes resetTab);

	bool getPlayerOption(PlayerOptionTypes i) const;
	void setPlayerOption(PlayerOptionTypes i, bool);

	unsigned int getLanguage() const;
	void setLanguage(unsigned int i);

	void readFromFile();

	// This is called from python.
	void writeToFile();

private:
	std::filesystem::path mPath;
	std::vector<bool> mPlayerOptions;
	unsigned int mLanguage = 0;
};