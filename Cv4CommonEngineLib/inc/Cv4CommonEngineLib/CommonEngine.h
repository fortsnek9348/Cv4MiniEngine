#pragma once

#include <filesystem>
#include <optional>

class CvDLLEntityIFaceBase;
class CvDLLSymbolIFaceBase;
class CvDLLFeatureIFaceBase;
class CvDLLRouteIFaceBase;
class CvDLLPlotBuilderIFaceBase;
class CvDLLRiverIFaceBase;
class CvDLLFlagEntityIFaceBase;
class CvInterface;
class CvEngine;

namespace pybind11
{
	class module_;
}

namespace cvbot
{
	class IPlayerBotPlugin;
}

namespace cvengine
{
	class CvVFS;

	// Interface of whatever displays CvPopups.
	class IPopupUIWindow
	{
	public:
		virtual void setWantClose() = 0;
		virtual bool isWantClose() const = 0;
		virtual ~IPopupUIWindow() = default;
	};

	class ICommonEngineCallbackHandler
	{
	public:
		virtual bool isShiftDown() const = 0;
		virtual bool isAltDown() const = 0;
		virtual bool isCtrlDown() const = 0;

		virtual std::optional<std::filesystem::path> promptSaveGamePath(const std::filesystem::path& defPath) = 0;
		virtual std::optional<std::filesystem::path> promptLoadGamePath(const std::filesystem::path& defDir) = 0;

		virtual void deferLoadGame(const std::filesystem::path&) = 0;

		virtual bool isAutorun() const = 0;

		virtual void exitApp() = 0;
		virtual bool isAppExiting() const = 0;

		virtual ~ICommonEngineCallbackHandler() = default;
	};

	struct CommonEngineConfig
	{
		std::filesystem::path civ4InstallationRoot;
		std::optional<std::filesystem::path> optModRelPath;
		std::optional<std::filesystem::path> optEngineAssetsOverrideDir;
		// Cv4MiniEngine: heck::getUserGamesSpecialDirectory("Beyond the Sword", heck::EUserGamesSpecialDirectory::Data);
		// Used for saves and replays.
		std::filesystem::path userDataDirPath;
		// Cv4MiniEngine: heck::getUserGamesSpecialDirectory("Beyond the Sword", heck::EUserGamesSpecialDirectory::Config);
		// Used for INI, user options profile, custom assets, public maps
		std::filesystem::path userConfigDirPath;
		// Cv4MiniEngine: heck::getUserGamesSpecialDirectory("Cv4MiniEngine", heck::EUserGamesSpecialDirectory::Cache);
		// For XML cache.
		std::filesystem::path cacheDirPath;
		// Cv4MiniEngine: "Replays (Cv4MiniEngine)"
		// Relative to userDataDirPath.
		std::filesystem::path replaysDirName;
		// Cv4MiniEngine: "Saves (Cv4MiniEngine)"
		std::filesystem::path savesDirName;
		// Cv4MiniEngine: "Cv4MiniEngine.ini"
		// Relative to userConfigDirPath.
		std::filesystem::path iniFilename;
		// Cv4MiniEngine: "Cv4MiniEngine Profile.txt"
		// Relative to userConfigDirPath.
		std::filesystem::path profileFilename;
		// All these must remain valid until after shutdown.
		CvInterface* interface{};
		CvEngine* engine{};
		CvDLLEntityIFaceBase* entityIFace{};
		CvDLLSymbolIFaceBase* symbolIFace{};
		CvDLLFeatureIFaceBase* featureIFace{};
		CvDLLRouteIFaceBase* routeIFace{};
		CvDLLPlotBuilderIFaceBase* plotBuilderIFace{};
		CvDLLRiverIFaceBase* riverIFace{};
		CvDLLFlagEntityIFaceBase* flagEntityIFace{};
		const cvbot::IPlayerBotPlugin* optPlayerBotPlugin{};
		ICommonEngineCallbackHandler* callbackHandler{};
		// Set this to suppress python screens in a headless engine.
		bool isPitbossHost{};
		// Set this to false to stop BUG from loading in a headless engine.
		bool enableCustomAssets = true;
	};

	struct [[nodiscard]] CommonEngineInitResult
	{
		const CvVFS* vfs{};
	};

	CommonEngineInitResult initialiseCommonEngine(std::wostream& log, const CommonEngineConfig& config);
	void shutdownCommonEngine();
}