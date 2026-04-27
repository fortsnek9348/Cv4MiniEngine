#include "CvApp.h"
#include "CvTuiInterface.h"
#include "CvTuiMainInterface.h"

#include <HeckTextUI/BasicControls.h>
#include <HeckTextUI/Window.h>

using namespace cvengine;

namespace
{
	class LoadingScreen
	{
	public:
		LoadingScreen()
		{
			mTextLabel = std::make_shared<hecktui::Label>();
			mTextLabel->setLabelAlignment(hecktui::EAlign::Center);
			mWnd->getClientArea()->addChild(mTextLabel);
			mWnd->getClientArea()->setLayout(std::make_unique<hecktui::FillLayout>());

			CvApp::getInstance().getUI().pushWindow(mWnd);

			update(L"TXT_KEY_INIT_INITIALIZING");
		}

		void update(std::wstring_view text)
		{
			mTextLabel->setLabel(std::wstring(text));
			CvApp::getInstance().getUI().drawUI();
		}

		~LoadingScreen()
		{
			CvApp::getInstance().getUI().removeWindow(mWnd.get());
		}

	private:
		const std::shared_ptr<hecktui::Window> mWnd = std::make_shared<hecktui::Window>(std::wstring(), hecktui::WindowConfig{
			.isDefaultFocus = true,
			.isFullscreen = true,
			.isModal = true,
			.canClose = false,
			.borderStyle = hecktui::EBorderStyle::None,
			});
		std::shared_ptr<hecktui::Label> mTextLabel;
	};
}

void InGameCvAppState::onEnter(CvApp&)
{
	mImpl.onEnter([](std::wstring_view) {});
}
void InGameCvAppState::onUpdate(CvApp&)
{
	CvTuiInterface::getInstance().getTuiMainInterface()->updateMainInterface();
}
void InGameCvAppState::onLeave(CvApp&)
{
	mImpl.onLeave();
}

NewGameStartupCvAppState::NewGameStartupCvAppState(cvengine::app::SimplifiedInitCore config) : mImpl(std::move(config))
{
}
void NewGameStartupCvAppState::onEnter(CvApp& app)
{
	LoadingScreen loadingScreen;
	mImpl.onEnter([&](std::wstring_view text) { loadingScreen.update(text); });
	app.deferInGameUI();
}
void NewGameStartupCvAppState::onUpdate(CvApp&)
{
}
void NewGameStartupCvAppState::onLeave(CvApp&)
{
	mImpl.onLeave();
}

LoadGameCvAppState::LoadGameCvAppState(const std::filesystem::path& path) : mImpl(std::move(path))
{
}
void LoadGameCvAppState::onEnter(CvApp& app)
{
	LoadingScreen loadingScreen;
	mImpl.onEnter([&](std::wstring_view text) { loadingScreen.update(text); });
	app.deferInGameUI();
}
void LoadGameCvAppState::onUpdate(CvApp&)
{
}
void LoadGameCvAppState::onLeave(CvApp&)
{
	mImpl.onLeave();
}