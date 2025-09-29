#include "CvGInterfaceScreen.h"
#include "MainInterface.h"
#include "CvInterface.h"
#include "MainInterfaceControls.h"
#include "CvApp.h"

#include <CvGameTextMgr.h>

#include <HeckTextUI/BasicControls.h>
#include <HeckTextUI/Window.h>

#include <algorithm>

using namespace hecktui;

CvGInterfaceScreen::CvGInterfaceScreen(std::string name, CvEngineEnums::ECvScreen kind)
	: mScreenKind(kind)
	, mName(std::move(name))
	, mTitle(L"Civ4 Screen")
	, mRoot(std::make_shared<hecktui::Element>())
{
}

CvEngineEnums::ECvScreen CvGInterfaceScreen::getKind() const noexcept
{
	return mScreenKind;
}

bool CvGInterfaceScreen::isActive() const
{
	return CvInterface::getInstance().isActive(getKind());
}

bool CvGInterfaceScreen::isPersistent() const
{
	return mIsPersistent;
}
void CvGInterfaceScreen::setPersistent(bool x)
{
	mIsPersistent = x;
}

std::shared_ptr<hecktui::Element> CvGInterfaceScreen::getTuiRoot() const noexcept
{
	return mRoot;
}

void CvGInterfaceScreen::clear() noexcept
{
	mRoot->removeAllChildren();
	mElementDict.clear();
}

void CvGInterfaceScreen::set(std::string name, std::shared_ptr<hecktui::Element> ctrl)
{
	auto& existing = mElementDict[std::move(name)];
	if (existing)
		existing->orphan();
	existing = std::move(ctrl);
}

void CvGInterfaceScreen::delByPrefix(std::string_view prefix)
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

std::shared_ptr<hecktui::Element> CvGInterfaceScreen::at(const std::string& name) const
{
	if (name.empty())
		return mRoot;
	else
		return mElementDict.at(name);
}

void CvGInterfaceScreen::setPlotHelpTarget(std::string name)
{
	mPlotHelpTargetName = std::move(name);
}
void CvGInterfaceScreen::updatePlotHelp(CvPlot* plot)
{
	if (mPlotHelpTargetName.size())
	{
		CvWStringBuffer buf;
		//if (plot)
		{
			// Not sure if this is correct, but good enough.
			//if (!(CvInterface::getInstance().getInterfaceMode() != INTERFACEMODE_SELECTION && CvGameTextMgr::GetInstance().setCombatPlotHelp(buf, plot)))
			//	CvGameTextMgr::GetInstance().setPlotListHelp(buf, plot, false, false);
			//if (!buf.isEmpty())
			//	buf.append(L'\n');
			//CvGameTextMgr::GetInstance().setPlotHelp(buf, plot);

			CvGameTextMgr::GetInstance().getPlotHelp(plot, CvInterface::getInstance().getCurrentCityBillboardHoverCity(), nullptr, false, buf);
		}
		//static_cast<RichLabel&>(*at(mPlotHelpTargetName)).debug = std::wstring_view(buf.getCString()).starts_with(L"Defense");
		//if (std::wstring_view(buf.getCString()).starts_with(L"Defense"))
		//	_CrtDbgBreak();
		static_cast<RichLabel&>(*at(mPlotHelpTargetName)).setLabel(buf.getCString());
	}
}

void CvGInterfaceScreen::setSoundName(std::string name)
{
	mSoundName = std::move(name);
}

void CvGInterfaceScreen::setInitialTitle(std::wstring title)
{
	mTitle = std::move(title);
}

void CvGInterfaceScreen::setAutoSizeBehaviour(EAutoSizeBehaviour b)
{
	mAutoSizeBehaviour = b;
}

void CvGInterfaceScreen::setModal(bool b)
{
	mIsModal = b;
}

void CvGInterfaceScreen::setWantClose(bool x) noexcept
{
	mWantClose = x;
}

std::shared_ptr<hecktui::Window> CvGInterfaceScreen::createTuiWindow(bool passInput) const
{
	const hecktui::WindowConfig config{
		.isDefaultFocus = passInput,
		.isFullscreen = mScreenKind == CvEngineEnums::ECvScreen::MAIN_INTERFACE,
		.isModal = mIsModal,
		.canClose = mScreenKind != CvEngineEnums::ECvScreen::MAIN_INTERFACE,
		.borderStyle = mScreenKind != CvEngineEnums::ECvScreen::MAIN_INTERFACE ? hecktui::EBorderStyle::Rounded : hecktui::EBorderStyle::None,
	};

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
			if (e.type == EConsoleEventType::ActionKeyPressed && static_cast<const ActionKeyEvent&>(e).key == EActionKey::Esc && getWindowConfig().canClose)
			{
				setWantClose();
				return true;
			}
			return hecktui::Window::onEvent(e) || (isPassInput && handleMainInterfaceConsoleEvent(e, CvInterface::getInstance()));
		}
	};

	CvApp::getInstance().audioSystem->playSound(mSoundName);

	auto wnd = std::make_shared<ScreenWindow>(mTitle, config);
	wnd->isPassInput = passInput;
	wnd->autoSizeBehaviour = mAutoSizeBehaviour;
	wnd->setClientArea(getTuiRoot());
	return wnd;
}

bool CvGInterfaceScreen::wantClose() const
{
	return mWantClose;
}