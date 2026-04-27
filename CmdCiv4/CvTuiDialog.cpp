#include "CvTuiDialog.h"
#include "MainInterface.h"
#include "MainInterfaceControls.h"
#include "CvApp.h"

#include <CvGameCoreDLL/CvGameTextMgr.h>

#include <HeckTextUI/BasicControls.h>
#include <HeckTextUI/Window.h>

#include <algorithm>

using namespace hecktui;
using namespace cvengine;

std::shared_ptr<hecktui::Element> CvTuiDialog::getTuiRoot() const noexcept
{
	return mRoot;
}

void CvTuiDialog::clear() noexcept
{
	mRoot->removeAllChildren();
	mElementDict.clear();
}

void CvTuiDialog::set(std::string name, std::shared_ptr<hecktui::Element> ctrl)
{
	auto& existing = mElementDict[std::move(name)];
	if (existing)
		existing->orphan();
	existing = std::move(ctrl);
}

void CvTuiDialog::delByPrefix(std::string_view prefix)
{
	for (auto it = mElementDict.begin(); it != mElementDict.end(); )
	{
		if (it->first.starts_with(prefix))
		{
			it->second->orphan();
			it = mElementDict.erase(it);
		}
		else
			++it;
	}
}

std::shared_ptr<hecktui::Element> CvTuiDialog::at(const std::string& name) const
{
	if (name.empty())
		return mRoot;
	else
		return mElementDict.at(name);
}

void CvTuiDialog::setSoundName(std::string name)
{
	mSoundName = std::move(name);
}

void CvTuiDialog::setInitialTitle(std::wstring title)
{
	mTitle = std::move(title);
}

void CvTuiDialog::setAutoSizeBehaviour(EAutoSizeBehaviour b)
{
	mAutoSizeBehaviour = b;
}

//void CvTuiDialog::setModal(bool b)
//{
//	mIsModal = b;
//}

void CvTuiDialog::setWantClose(bool x) noexcept
{
	mWantClose = x;
}

//void CvTuiDialog::setWantFullscreen(bool x)
//{
//	mWantFullscreen = x;
//}

std::shared_ptr<hecktui::Window> CvTuiDialog::createTuiWindow(bool passInputToMainInterface, hecktui::WindowConfig windowConfig) const
{
	//const hecktui::WindowConfig config{
	//	.isDefaultFocus = passInput,
	//	.isFullscreen = mWantFullscreen,
	//	.isModal = mIsModal,
	//	.canClose = mScreenKind != cvengine::ECvScreen::MAIN_INTERFACE,
	//	.borderStyle = mScreenKind != cvengine::ECvScreen::MAIN_INTERFACE ? hecktui::EBorderStyle::Rounded : hecktui::EBorderStyle::None,
	//};

	struct ScreenWindow : hecktui::Window
	{
		bool isPassInput = false;
		EAutoSizeBehaviour autoSizeBehaviour{};

		using hecktui::Window::Window;

		virtual void positionWindowInScene(ivec2 sceneDim) override
		{
			if (getWindowConfig().isFullscreen)
				return Window::positionWindowInScene(sceneDim);

			// TODO: CvAppUI and CvDiplomacyScreena dnt his all have similar code. Deduplicate.
			if (!wasMoved() && !wasResized())
			{
				const ivec2 measurement = getLayoutSizeInfo().preferred;
				//const ivec2 size{ 50, measurement.y };
				ivec2 size{ std::clamp(measurement.x, 0, sceneDim.x), std::clamp(measurement.y, 0, sceneDim.y) };

				switch (autoSizeBehaviour)
				{
				case EAutoSizeBehaviour::Minimum:
					size = measurement;
					break;
				case EAutoSizeBehaviour::GrowOnly:
				default:
					size = vmax(measurement, getRect().size());
					break;
				case EAutoSizeBehaviour::Maximise:
					size = sceneDim - ivec2(2, 2);
					break;
				}

				const ivec2 position = (sceneDim - size) / 2;

				setRect(iaabb2{
					.min = position,
					.max = position + size,
					}.intersection(iaabb2{ .max = sceneDim }.shrunk({ 1, 1 })));
			}
		}

		virtual bool onEvent(const hecktui::ConsoleEvent& e) override
		{
			return hecktui::Window::onEvent(e) || (isPassInput && cvengine::handleMainInterfaceConsoleEvent(e));
		}
	};

	CvApp::getInstance().audioSystem->playSound(mSoundName);

	auto wnd = std::make_shared<ScreenWindow>(mTitle, windowConfig);
	wnd->isPassInput = passInputToMainInterface;
	wnd->autoSizeBehaviour = mAutoSizeBehaviour;
	wnd->setClientArea(getTuiRoot());
	return wnd;
}

bool CvTuiDialog::wantClose() const
{
	return mWantClose;
}