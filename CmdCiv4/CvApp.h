#pragma once

#include "AudioSystem.h"
#include "CommandLine.h"

#include <Cv4CommonEngineLib/CvAppStates.h>
#include <Cv4CommonEngineLib/CommonEngine.h>

#include <vector>
#include <filesystem>

namespace hecktui
{
	class Window;
	struct ModifierKeyState;
	class Element;
}

namespace cvbot
{
	class IPlayerBotPlugin;
}

namespace cvengine
{
	class CvVFS;

	class ICvAppUI
	{
	public:
		// This is used to fake the shift key in ActionButton.
		[[nodiscard]] virtual hecktui::ModifierKeyState exchangeLastModifierKeysState(hecktui::ModifierKeyState) = 0;

		virtual hecktui::ModifierKeyState getLastModifierKeysState() const noexcept = 0;

		virtual std::shared_ptr<hecktui::Element> getCurrentHoverControl() const = 0;

		virtual void pushWindow(std::shared_ptr<hecktui::Window> window) = 0;
		virtual void insertWindowAfter(const hecktui::Window* prev, std::shared_ptr<hecktui::Window> window) = 0;
		virtual void removeWindow(hecktui::Window* window) = 0;

		virtual void resetUI() = 0;

		virtual void modalLoopPythonReloadPrompt(std::wstring_view message, std::wstring_view btnLabel) = 0;

		// Perform one loop of UI: Layout, draw, and process console events.
		// Note that this is REENTRANT. Python errors, inside UI event handlers, will call into this to show errors and wait for action.
		virtual void updateUI() = 0;

		virtual void drawUI() = 0;

		virtual ~ICvAppUI() = default;
	};

	class CvApp;

	// Main Menu, New Game Setup, New Game Init, Load Game, InGame, etc.
	class ICvAppState
	{
	public:
		// Setup UI
		virtual void onEnter(CvApp&) = 0;
		// Update UI
		virtual void onUpdate(CvApp&) = 0;
		// Teardown UI
		virtual void onLeave(CvApp&) = 0;
		virtual ~ICvAppState() = default;
	};

	class InGameCvAppState : public ICvAppState
	{
	public:
		virtual void onEnter(CvApp&) override;
		virtual void onUpdate(CvApp&) override;
		virtual void onLeave(CvApp&) override;
	private:
		cvengine::app::InGameState mImpl;
	};



	class NewGameStartupCvAppState : public ICvAppState
	{
	public:
		explicit NewGameStartupCvAppState(cvengine::app::SimplifiedInitCore);
		virtual void onEnter(CvApp&) override;
		virtual void onUpdate(CvApp&) override;
		virtual void onLeave(CvApp&) override;

	private:
		cvengine::app::NewGameStartupState mImpl;
	};

	class LoadGameCvAppState : public ICvAppState
	{
	public:
		explicit LoadGameCvAppState(const std::filesystem::path& path);
		virtual void onEnter(CvApp&) override;
		virtual void onUpdate(CvApp&) override;
		virtual void onLeave(CvApp&) override;

	private:
		cvengine::app::LoadGameState mImpl;
	};

	class CvApp : public ICommonEngineCallbackHandler
	{
	public:
		static CvApp& getInstance();

		void start(const cvengine::AppStartupConfig& config);

		static std::filesystem::path getUserDataDir();

		const cvengine::CvVFS& getVFS() const;

		const cvengine::AppStartupConfig& getCommandLineConfig() const;

		const cvbot::IPlayerBotPlugin* getPlayerBotPlugin() const;

		void startUI();
		ICvAppUI& getUI() noexcept;

		bool isShiftClickHackEnabled() const;

		// This is called before UI is started, so in this case, all modifier keys are not pressed.
		hecktui::ModifierKeyState getLastModifierKeysState() const noexcept;

		void deferMainMenu();

		// With current setup
		void deferNewGameStartup(cvengine::app::SimplifiedInitCore config);

		void deferInGameUI();

		// Main loop. Returns exit code.
		int run();

		virtual void registerPythonExtensions(const pybind11::module_& m) override;

		virtual bool isShiftDown() const override;
		virtual bool isAltDown() const override;
		virtual bool isCtrlDown() const override;

		virtual std::optional<std::filesystem::path> promptSaveGamePath(const std::filesystem::path& defPath) override;
		virtual std::optional<std::filesystem::path> promptLoadGamePath(const std::filesystem::path& defDir) override;

		// display load screen, do the loading, push InGame, reset UI
		// Deferred to next update.
		virtual void deferLoadGame(const std::filesystem::path&) override;

		virtual bool isAutorun() const override;

		virtual void exitApp() override;
		virtual bool isAppExiting() const override;

		std::unique_ptr<AudioSystem> audioSystem;

	private:
		void deferAppState(std::unique_ptr<ICvAppState>);

		cvengine::AppStartupConfig mCmdLineConfig;

		const cvengine::CvVFS* mVFS{};
		std::unique_ptr<ICvAppUI> mAppUI;
		std::unique_ptr<ICvAppState> mCurrentState;
		std::unique_ptr<ICvAppState> mNextState;

		bool mWantExit = false;

		bool mIsShiftClickHackEnabled = false;

		const cvbot::IPlayerBotPlugin* mPlayerBotPlugin = nullptr;
	};
}