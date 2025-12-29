#include "CvApp.h"
#include "CivIni.h"

#include <HeckTextUI/HeckTextUI.h>
#include <HeckTextUI/UIScene.h>
#include <HeckTextUI/Layout.h>
#include <HeckTextUI/BasicControls.h>
#include <HeckTextUI/Window.h>

using namespace hecktui;

using namespace cvengine;

class CvAppUI : public ICvAppUI
{
public:
	ConsoleDevice consoleDevice;
	hecktui::Framebuffer fb{ consoleDevice.getDim() };
	UIScene scene;

	explicit CvAppUI()
	{
		resetUI();
	}

	virtual hecktui::ModifierKeyState exchangeLastModifierKeysState(hecktui::ModifierKeyState state) override
	{
		return scene.exchangeLastModifierKeysState(state);
	}

	virtual hecktui::ModifierKeyState getLastModifierKeysState() const noexcept override
	{
		return scene.getModifierKeyState();
	}

	virtual std::shared_ptr<hecktui::Element> getCurrentHoverControl() const override
	{
		return scene.getCurrentHoverControl();
	}

	virtual void pushWindow(std::shared_ptr<hecktui::Window> window) override
	{
		//if (!window->getWindowConfig().isFullscreen)
		{
			//window->layoutPhase1Measure();
			//const ivec2 sceneDim = fb.getDim();
			//const ivec2 dialogPos = (sceneDim - window->getLayoutSizeInfo().preferred) / 2;
			//// Initially, size within the scene, minus margins.
			//window->setRect(iaabb2{
			//	.min = dialogPos,
			//	.max = dialogPos + window->getLayoutSizeInfo().preferred,
			//	}.intersection(iaabb2{ .max = sceneDim }.shrunk({ 1, 1 })));
			//window->layoutPhase2Apply();
			window->layoutWindow(fb.getDim());
		}
		scene.pushWindow(std::move(window));
	}

	virtual void insertWindowAfter(const hecktui::Window* prev, std::shared_ptr<hecktui::Window> window) override
	{
		window->layoutWindow(fb.getDim());
		scene.insertWindowAfter(prev, std::move(window));
	}

	virtual void modalLoopPythonReloadPrompt(std::wstring_view message, std::wstring_view btnLabel) override
	{
		auto table = std::make_shared<Element>(std::make_unique<TableLayout>(TableLayoutConfig{
			.cols{ {.weight = 1 } },
			.rows{ {.weight = 1 }, {} },
			.cells{ {.coord{}}, {.coord{ 0, 1 }, .align{ EJustilign::Center, EJustilign::Stretch } } },
			}));

		bool confirmed = false;

		table->addChild(std::make_shared<Label>(std::wstring(message)));
		table->addChild(std::make_shared<Button>(std::wstring(btnLabel), [&] { confirmed = true; }));

		auto wnd = std::make_shared<Window>(L"Python Error", WindowConfig{
			.isModal = true,
			.canClose = false,
			.borderStyle = EBorderStyle::Double,
			});

		wnd->getClientArea()->addChild(table);

		wnd->setRect(iaabb2{ fb.getDim() / 2, fb.getDim() / 2 }.shrunk({ -30, -5 }));

		pushWindow(wnd);

		while (!confirmed)
			updateUI();

		removeWindow(wnd.get());
	}

	virtual void removeWindow(hecktui::Window* window) override
	{
		scene.removeWindow(window);
	}

	virtual void resetUI() override
	{
		scene = {};
		scene.setDim(fb.getDim());
	}

	virtual void updateUI() override
	{
		drawUI();

		consoleDevice.wait();

		while (const EventStorage e = consoleDevice.poll())
			(void)scene.injectEvent(*e);
	}

	virtual void drawUI() override
	{
		if (consoleDevice.getDim() != fb.getDim())
		{
			fb = hecktui::Framebuffer(consoleDevice.getDim());
			scene.setDim(fb.getDim());
		}

		scene.update();
		fb.clear();
		scene.draw(fb);
		fb.mergeBorders();
		consoleDevice.print(fb);
	}
};

void CvApp::startUI()
{
	mAppUI = std::make_unique<CvAppUI>();
	mIsShiftClickHackEnabled = gCivilizationIVIni.get(kCivilizationIVIniSection_CV4ENGINE, kCivilizationIVIniProp_EnableRightClickToShiftClickRemap, 1);
}