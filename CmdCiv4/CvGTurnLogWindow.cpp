#include "CvGTurnLogWindow.h"
#include "MainInterfaceControls.h"
#include "DLLInterface/MyCvDLLUtility.h"
#include "TuiTextCode.h"
#include "UITheme.h"

#include <HeckTextUI/BasicControls.h>

#include <CvGlobals.h>
#include <CvGameAI.h>
#include <CvPlayerAI.h>
#include <CvGameTextMgr.h>

#include <ranges>

namespace tui = hecktui;

static constexpr size_t kNumEventsPerLoad = 100;

namespace
{
	using FilterFunc = bool(*)(const CvTalkingHeadMessage&);

	static constexpr FilterFunc kFilters[]{
		[](const CvTalkingHeadMessage& msg) { const auto t = msg.getMessageType(); return t == MESSAGE_TYPE_INFO || t == MESSAGE_TYPE_MAJOR_EVENT || t == MESSAGE_TYPE_MINOR_EVENT; },
		[](const CvTalkingHeadMessage& msg) { const auto t = msg.getMessageType(); return t == MESSAGE_TYPE_CHAT; },
		[](const CvTalkingHeadMessage& msg) { const auto t = msg.getMessageType(); return t == MESSAGE_TYPE_COMBAT_MESSAGE; },
		[](const CvTalkingHeadMessage& msg) { const auto t = msg.getMessageType(); return t == MESSAGE_TYPE_QUEST; },
	};

	std::wstring buildMessageText(const CvTalkingHeadMessage& msg)
	{
		CvWString date;
		CvGameTextMgr::GetInstance().setTimeStr(date, msg.getTurn(), false);
		return L"T" + std::to_wstring(msg.getTurn()) + L", " + std::wstring(std::move(date)) + L": " + msg.getDescription();
	}
}

struct CvGTurnLogWindow::EventLogScrollPanel : tui::ScrollBarPanel
{
	EventLogScrollPanel() : tui::ScrollBarPanel(true, true)
	{
		itemsPanel = std::make_shared<tui::Element>();
		btnLoadMore = std::make_shared<tui::Button>(L"[...]", [this] { loadMore(); });
		btnLoadMore->setBorderStyle(tui::EBorderStyle::None);
		getPanel()->addChild(itemsPanel);
		getPanel()->addChild(btnLoadMore);
		getPanel()->setLayout(std::make_unique<tui::FlowLayout>(tui::FlowConfig{ .axis = tui::EAxis::Vertical
			, .itemsCrosswiseJustilign = tui::EJustilign::Start
			}));
		itemsPanel->setLayout(std::make_unique<tui::FlowLayout>(tui::FlowConfig{ .axis = tui::EAxis::Vertical }));
		loadMore();
	}

	size_t numEntriesDisplayed = 0;
	std::shared_ptr<tui::Element> itemsPanel;
	std::shared_ptr<tui::Button> btnLoadMore;
	ETab currentTab = kEvents;

	struct MessageLabel : cvengine::RichLabel
	{
		heck::ivec2 plotCoord{};

		explicit MessageLabel(const CvTalkingHeadMessage& msg)
			: RichLabel(L"")
			, plotCoord(msg.getX(), msg.getY())
		{
			// Same code as TurnMessageDisplay.
			colouring = { .text = msg.getFlashColor() != NO_COLOR ? cvengine::toTuiColour(msg.getFlashColor()) : tui::EColour::Silver, .back = tui::kTransparent };
			setLabel(buildMessageText(msg));
		}

		virtual bool onEvent(const tui::ConsoleEvent& e) override
		{
			if (RichLabel::onEvent(e))
				return true;
			
			if (e.type == tui::EConsoleEventType::MouseButtonDown && static_cast<const tui::MouseButtonEvent&>(e).button == tui::EMouseButton::Left)
			{
				if (plotCoord.x >= 0)
					CvInterface::getInstance().lookAt(plotCoord);
				return true;
			}
			
			return false;
		}
	};

	void loadMore(size_t amount = kNumEventsPerLoad)
	{
		const CvPlayer& player = GET_PLAYER(GC.getGame().getActivePlayer());
		const CvMessageQueue& messageQueue = player.getGameMessages();

		// Note that is should be the case that drop and take and safe against out of bounds.
		auto messagesView = messageQueue | std::views::reverse | std::views::filter(kFilters[currentTab])
			| std::views::drop(numEntriesDisplayed) | std::views::take(amount + 1); // Take one extra so we can detect if we reached the end without iterating over the list again.

		size_t numLoaded = 0;
		bool gotMore = false;
		for (const CvTalkingHeadMessage& msg : messagesView)
		{
			if (++numLoaded > amount)
			{
				gotMore = true;
				break;
			}
			itemsPanel->addChild(std::make_shared<MessageLabel>(msg));
			++numEntriesDisplayed;
		}

		btnLoadMore->setEnabled(gotMore);
	}

	void switchTab(ETab tab)
	{
		if (currentTab != tab)
		{
			currentTab = tab;
			numEntriesDisplayed = 0;
			itemsPanel->removeAllChildren();
			loadMore();
		}
	}

	void dirtyTurnLog()
	{
		const size_t numToLoad = std::max(kNumEventsPerLoad, numEntriesDisplayed);
		numEntriesDisplayed = 0;
		itemsPanel->removeAllChildren();
		loadMore(numToLoad);
	}
};

CvGTurnLogWindow::CvGTurnLogWindow() : tui::Window(L"", tui::WindowConfig{
	.isDefaultFocus = false,
	.isFullscreen = false,
	.isModal = false,
	.canClose = true,
	.isFocusContext = false,
	.borderStyle = tui::EBorderStyle::Rounded,
	})
{
	auto tabBar = std::make_shared<tui::Element>();
	tabBar->setLayout(std::make_unique<tui::FlowLayout>(tui::FlowConfig{ .axis = tui::EAxis::Horizontal }));

	const auto activateTab = [this](ETab tab) {
		for (const auto& btn : mTabBtns)
			btn->value = false;
		mTabBtns[tab]->value = true;
		mEventLogScrollPanel->switchTab(tab);
		};

	tabBar->addChild(mTabBtns[kEvents] = std::make_shared<tui::RadioButton>(tui::ECheckStyle::Border, MyCvDLLUtility::getInstance().getTextGeneric(L"TXT_KEY_EVENT_LOG", {}), [=](bool) { activateTab(kEvents); }));
	tabBar->addChild(mTabBtns[kChat] = std::make_shared<tui::RadioButton>(tui::ECheckStyle::Border, MyCvDLLUtility::getInstance().getTextGeneric(L"TXT_KEY_CHAT_LOG", {}), [=](bool) { activateTab(kChat); }));
	tabBar->addChild(mTabBtns[kCombat] = std::make_shared<tui::RadioButton>(tui::ECheckStyle::Border, MyCvDLLUtility::getInstance().getTextGeneric(L"TXT_KEY_COMBAT_MESSAGE_LOG", {}), [=](bool) { activateTab(kCombat); }));
	tabBar->addChild(mTabBtns[kQuests] = std::make_shared<tui::RadioButton>(tui::ECheckStyle::Border, MyCvDLLUtility::getInstance().getTextGeneric(L"TXT_KEY_QUEST_MESSAGE_LOG", {}), [=](bool) { activateTab(kQuests); }));
	getClientArea()->addChild(std::move(tabBar));

	for (auto& tabBtn : mTabBtns)
		tabBtn->setColouring({ .text = theme::kButtonDefaultLabelColour, .back = tui::EColour::Black });

	mEventLogScrollPanel = std::make_shared<EventLogScrollPanel>();
	getClientArea()->addChild(mEventLogScrollPanel);

	getClientArea()->setLayout(std::make_unique<tui::FlowLayout>(tui::FlowConfig{
		.axis = tui::EAxis::Vertical,
		.linesCrosswiseJustilign = tui::EJustilign::Stretch,
		}));

	dirtyTurnLog(NO_PLAYER);
	activateTab(kEvents);
}

void CvGTurnLogWindow::positionWindowInScene([[maybe_unused]] heck::ivec2 sceneDim)
{
	if (!wasMoved() && !wasResized())
		setRect(heck::iaabb2::sized(1, { 50, 20 }));
}

void CvGTurnLogWindow::dirtyTurnLog([[maybe_unused]] PlayerTypes ePlayer)
{
	mEventLogScrollPanel->dirtyTurnLog();
}