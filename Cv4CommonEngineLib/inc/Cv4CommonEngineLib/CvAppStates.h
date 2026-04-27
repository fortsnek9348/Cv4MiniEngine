#pragma once

#include <CvGameCoreDLL/CvEnums.h>

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace cvengine::app
{
	struct SimplifiedInitCore
	{
		struct Player
		{
			TeamTypes team{};
			LeaderHeadTypes leader{}; // can be random
			CivilizationTypes civ{}; // can be random
		};

		std::wstring gameName;
		std::wstring playerName;
		HandicapTypes difficulty{};

		std::wstring mapScriptName;

		WorldSizeTypes worldSizeType{}; // can be random
		ClimateTypes climateType{}; // can be random
		SeaLevelTypes seaLevelType{}; // can be random
		EraTypes eraType{}; // can be random
		GameSpeedTypes speedType{};
		std::vector<CustomMapOptionTypes> customMapOptions; // can be random, depending on map script
		std::vector<bool> gameOptions;
		std::vector<bool> victoryOptions;

		std::vector<Player> players;

		int mapSizeOverrideMultiplier = 1;

		unsigned int mapSeed = 0;
		unsigned int syncSeed = 0;
	};

	using ProgressCallback = std::function<void(const std::wstring&)>;

	class IState
	{
	public:
		virtual void onEnter(const ProgressCallback& progressCallback) = 0;
		virtual void onLeave() = 0;
		virtual ~IState() = default;
	};

	class NewGameStartupState : public IState
	{
	public:
		explicit NewGameStartupState(SimplifiedInitCore);
		virtual void onEnter(const ProgressCallback& progressCallback) override;
		virtual void onLeave() override;
	protected:
		SimplifiedInitCore mSimplifiedInitCore;
	};

	class LoadGameState : public IState
	{
	public:
		explicit LoadGameState(const std::filesystem::path& path);
		virtual void onEnter(const ProgressCallback& progressCallback) override;
		virtual void onLeave() override;
	private:
		std::filesystem::path mPath;
	};

	class InGameState : public IState
	{
	public:
		virtual void onEnter(const ProgressCallback& progressCallback) override;
		virtual void onLeave() override;
	};

	std::wstring extractModRelPathFromSave(const std::filesystem::path& path);
}