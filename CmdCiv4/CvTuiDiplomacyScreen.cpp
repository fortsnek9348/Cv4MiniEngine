#include "CvTuiDiplomacyScreen.h"
#include "MainInterfaceControls.h"
#include "UITheme.h"
#include "CvTuiInterface.h"
#include "CvTuiMainInterface.h"
#include "CvPopupTuiDialog.h"

#include <Cv4CommonEngineLib/CvButtonPopup.h>
#include <Cv4CommonEngineLib/CvTranslator.h>

#include <CvGameCoreDLL/CLinkListIterator.h>
#include <CvGameCoreDLL/CvDiploParameters.h>
#include <CvGameCoreDLL/CvGlobals.h>
#include <CvGameCoreDLL/CvPlayerAI.h>
#include <CvGameCoreDLL/CvDLLWidgetData.h>
#include <CvGameCoreDLL/CvGameTextMgr.h>

#include <HeckTextUI/BasicControls.h>

#include <CommonStuff/range.h>

using heck::range;
using enum CvDiplomacyController::ESide;
using namespace cvengine;

static constexpr int kOfferPanelHeight = 7;
static constexpr hecktui::EBorderStyle kUserCommentBorderStyle = hecktui::EBorderStyle::Rounded;

namespace
{
	std::wstring getTradeItemLabel(PlayerTypes ownerPlayerI, PlayerTypes otherPlayerI, TradeData item, bool isOffer, bool isShowCurrent)
	{
		if (isOffer)
			std::swap(ownerPlayerI, otherPlayerI); // WEIRD!

		CvString icon;
		CvWString label;
		if (!GET_PLAYER(ownerPlayerI).getItemTradeString(otherPlayerI, isOffer, isShowCurrent, item, label, icon))
			label = L"[UNKNOWN]";
		return label;
	}
}

struct CvTuiDiplomacyScreen::Internals
{
	struct UserCommentButton : hecktui::Button
	{
		CvTuiDiplomacyScreen& ui;
		CvDiplomacyController::UserComment userComment;

		explicit UserCommentButton(CvTuiDiplomacyScreen& ui, CvDiplomacyController::UserComment userComment)
			: Button(std::move(userComment.text))
			, ui(ui)
			, userComment(std::move(userComment))
		{
			setLabelColour(theme::kButtonDefaultLabelColour);
			setBorderStyle(kUserCommentBorderStyle);
		}

		virtual void onClick(hecktui::ModifierKeyState modifierKeyState) override
		{
			Button::onClick(modifierKeyState);
			ui.getController().onClickUserComment(userComment.type, userComment.data1, userComment.data2);
			ui.updateUI();
			CvTuiInterface::getInstance().getTuiMainInterface()->onGameStateChanged(CvTuiMainInterface::EGameStateChangeReason::DiploEvent);
		}
	};

	struct TradeItemButton : hecktui::EmptyButton
	{
		CvTuiDiplomacyScreen& ui;
		//PlayerTypes fromPlayerI{};
		//PlayerTypes toPlayerI{};
		TradeData tradeData{};
		std::wstring label;
		bool isOffer{};
		bool isAIPlayer = false;
		bool isDenial{};
		int dealId = -1;

		explicit TradeItemButton(CvTuiDiplomacyScreen& ui, TradeData tradeData, std::wstring label, bool isOffer, bool isAIPlayer, int dealId = -1)
			: ui(ui), tradeData(tradeData), label(std::move(label)), isOffer(isOffer), isAIPlayer(isAIPlayer), dealId(dealId)
		{
			setBorderStyle(hecktui::EBorderStyle::None);

			//const CvPlayerAI& player = GET_PLAYER(fromPlayerI);
			//CvWString temp;
			//CvString icon;
			//if (!player.getItemTradeString(toPlayerI, isOffer, false, tradeData, temp, icon))
			//	temp = L"[UNKNOWN]";
			//
			//label = std::move(temp);
		}

		virtual std::shared_ptr<Element> createTooltip() const override
		{
			//CvWStringBuffer buf;
			//CvDLLWidgetData::getInstance().parseHelp(buf, widgetData);
			//auto lblHelp = std::make_shared<RichLabel>(buf.getCString());
			//lblHelp->enableWrapping = true;
			//return lblHelp;
			return cvengine::createWidgetTooltip({
				.m_iData1 = tradeData.m_eItemType,
				.m_iData2 = tradeData.m_iData,
				.m_bOption = isAIPlayer,
				.m_eWidgetType = WIDGET_TRADE_ITEM,
				});
		}

		virtual void onClick(hecktui::ModifierKeyState) override
		{
			if (!isDenial)
			{
				TradeData offerItem = tradeData;

				if (!isOffer && CvDeal::isGold(offerItem.m_eItemType))
				{
					const auto sideI = static_cast<CvDiplomacyController::ESide>(!isAIPlayer);
					const auto otherSideI = static_cast<CvDiplomacyController::ESide>(isAIPlayer);
					auto& side = ui.getController().getSide(sideI);
					auto& otherSide = ui.getController().getSide(otherSideI);

					const bool isGpt = CvDeal::getGoldPerTurnItem() == offerItem.m_eItemType;
					const int max = isGpt
						? GET_PLAYER(side.playerI).AI_maxGoldPerTurnTrade(otherSide.playerI)
						: GET_PLAYER(side.playerI).AI_maxGoldTrade(otherSide.playerI);

					auto popup = std::make_unique<InternalPopup>();
					popup->headerString = CvTranslator::getInstance().getText(L"TXT_KEY_TRADE_TITLE_GOLD");
					popup->bodyString = CvTranslator::getInstance().getText(
						isAIPlayer
						? isGpt ? L"TXT_KEY_TRADE_GPT_FROM_THEM" : L"TXT_KEY_TRADE_GOLD_FROM_THEM"
						: isGpt ? L"TXT_KEY_TRADE_GPT_TO_OFFER" : L"TXT_KEY_TRADE_GOLD_TO_OFFER"
					);
					popup->controls.push_back(CvPopup::Control{
						.type = CvPopup::EControlType::SpinBox,
						.text = std::to_wstring(max),
						.spinBoxMax = max,
						});
					popup->controls.push_back(CvPopup::Control{
						.type = CvPopup::EControlType::Button,
						.text = CvTranslator::getInstance().getText(L"TXT_KEY_MAIN_MENU_OK"),
						});
					popup->optEnterSubmitBtnId = 0;
					popup->enableEscCancel = true;
					const std::optional<PopupReturn> result = InternalPopup::launchModal(std::move(popup));
					if (!result)
						return;
					const int n = result->getCurrentSpinBoxValue();
					if (n <= 0)
						return;
					offerItem.m_iData = n;
				}

				if (ui.getController().onClickTradeItem(offerItem, isOffer, isAIPlayer, dealId) == CvDiplomacyController::EOfferResult::OnlyOneSideMayOfferItems)
				{
					auto popup = std::make_unique<InternalPopup>();
					popup->bodyString = CvTranslator::getInstance().getText(L"TXT_KEY_PEACE_ERROR");
					popup->controls.push_back(CvPopup::Control{
						.type = CvPopup::EControlType::Button,
						.text = CvTranslator::getInstance().getText(L"TXT_KEY_MAIN_MENU_OK"),
						});
					popup->optEnterSubmitBtnId = 0;
					popup->enableEscCancel = true;
					(void)InternalPopup::launchModal(std::move(popup));
				}

				ui.updateUI();
				CvTuiInterface::getInstance().getTuiMainInterface()->onGameStateChanged(CvTuiMainInterface::EGameStateChangeReason::DiploEvent);
			}
		}

	protected:
		virtual hecktui::LayoutSizeInfo measureThis() const override
		{
			const heck::ivec2 size{ static_cast<int>(label.size()), 1 };
			return { .minimum = size, .preferred = size };
		}

		virtual void drawThis(heck::ivec2 offset, hecktui::Framebuffer& fb) override
		{
			const UISceneState state = getUISceneState();
			const hecktui::EColour textColour = !isDenial
				? (state.hover ? hecktui::EColour::Grey100 : theme::kTradeItemDefaultColour)
				: hecktui::EColour::Red1;
			fb.drawText(label, offset + getRect(), hecktui::EAlign::Left, hecktui::EAlign::Middle, { .text = textColour }, false);
		}
	};
};

struct CvTuiDiplomacyScreen::TradePanel : hecktui::Element
{
	std::shared_ptr<hecktui::Element> invListPanel;
	std::shared_ptr<hecktui::Element> offerListContainer;
	std::shared_ptr<hecktui::Element> offerListPanel;
	std::shared_ptr<hecktui::Label> ceaseFireHeader;

	std::vector<size_t> tradeCategoryCounts;
	std::vector<std::shared_ptr<hecktui::Element>> tradeCategoryGroupBoxes;
	std::vector<std::shared_ptr<Internals::TradeItemButton>> tradeInventoryButtons;

	TradePanel()
	{
		setLayout(std::make_unique<hecktui::TableLayout>(hecktui::TableLayoutConfig{
			.cols{ hecktui::TableLayoutConfig::kFlex },
			.rows{
				hecktui::TableLayoutConfig::kFlex,
				hecktui::TableLayoutConfig::kAutoSize,
				{.min = kOfferPanelHeight }
			},
			.cells {
				hecktui::TableLayoutConfig::Cell{.coord{ 0, 0 } },
				hecktui::TableLayoutConfig::Cell{.coord{ 0, 1 } },
				hecktui::TableLayoutConfig::Cell{.coord{ 0, 2 } },
			}
		}));

		const hecktui::FlowConfig listFlowLayout{
			.axis = hecktui::EAxis::Vertical,
			.linesCrosswiseJustilign = hecktui::EJustilign::Stretch,
		};

		auto invListScrollPanel = std::make_shared<hecktui::ScrollBarPanel>(hecktui::EAxis::Vertical);
		invListPanel = invListScrollPanel->getPanel();
		invListPanel->setLayout(std::make_unique<hecktui::FlowLayout>(listFlowLayout));
		addChild(std::move(invListScrollPanel));

		addChild(std::make_shared<hecktui::HorizontalRule>(hecktui::EBorderStyle::Double));
			
		auto offerListScrollPanel = std::make_shared<hecktui::ScrollBarPanel>(hecktui::EAxis::Vertical);
		offerListContainer = offerListScrollPanel->getPanel();
		offerListContainer->setLayout(std::make_unique<hecktui::FlowLayout>(listFlowLayout));
		addChild(std::move(offerListScrollPanel));

		ceaseFireHeader = std::make_shared<hecktui::Label>(CvTranslator::getInstance().getText(L"TXT_KEY_TRADE_CEASE_FIRE_STRING"));
		ceaseFireHeader->setVisible(false);
		offerListContainer->addChild(ceaseFireHeader);

		offerListPanel = std::make_shared<hecktui::Element>();
		offerListPanel->setLayout(std::make_unique<hecktui::FlowLayout>(listFlowLayout));
		offerListContainer->addChild(offerListPanel);
	}
};

CvTuiDiplomacyScreen::CvTuiDiplomacyScreen(std::unique_ptr<CvDiploParameters> diploParams)
	: CvDiplomacyScreen(std::move(diploParams))
	, CvGInterfaceScreen("Diplomacy Screen", cvengine::ECvScreen::DIPLOMACY_SCREEN)
{
	auto centerPanel = std::make_shared<hecktui::Element>();

	centerPanel->setLayout(std::make_unique<hecktui::TableLayout>(hecktui::TableLayoutConfig{
		.cols{ hecktui::TableLayoutConfig::kFlex },
		.rows{
			hecktui::TableLayoutConfig::kFlex,
		hecktui::TableLayoutConfig::kAutoSize,
		hecktui::TableLayoutConfig::kAutoSize,
		//hecktui::TableLayoutConfig::kAutoSize
		},
		.cells {
			hecktui::TableLayoutConfig::Cell{.coord{ 0, 0 }, .align{ hecktui::EJustilign::Stretch, hecktui::EJustilign::Center } },
			hecktui::TableLayoutConfig::Cell{.coord{ 0, 1 } },
			hecktui::TableLayoutConfig::Cell{.coord{ 0, 2 } },
			//hecktui::TableLayoutConfig::Cell{.coord{ 0, 3 } },
		},
		.gap{ 0, 1 },
		}));

	mLblLeaderheadText = std::make_shared<cvengine::RichLabel>(L"");
	mLblLeaderheadText->setLabelAlignment(hecktui::EAlign::Center);
	centerPanel->addChild(mLblLeaderheadText);

	//auto boxPanel = std::make_shared<hecktui::BoxPanel>(hecktui::EBorderStyle::None);
	//boxPanel->setLayout(makeBorderedFillLayout());
	mLblAIComment = std::make_shared<hecktui::Label>();
	//mLblAIComment->setLabelAlignment(hecktui::EAlign::Center);
	mLblAIComment->colouring.text = theme::kRichLabelDefaultColour;
	mLblAIComment->enableWrapping = true;
	//boxPanel->addChild(mLblAIComment);
	centerPanel->addChild(mLblAIComment);

	//centerPanel->addChild(std::make_shared<hecktui::HorizontalRule>(hecktui::EBorderStyle::None));

	//auto boxPanel = std::make_shared<hecktui::BoxPanel>(hecktui::EBorderStyle::None);
	//boxPanel->setLayout(makeBorderedFillLayout());
	mUserCommentsPanel = std::make_shared<hecktui::Element>();
	//mUserCommentsPanel->setLayout(std::make_unique<hecktui::FlowLayout>(hecktui::FlowConfig{
	//	.axis = hecktui::EAxis::Vertical,
	//	.itemsCrosswiseJustilign = hecktui::EJustilign::Center,
	//	.linesCrosswiseJustilign = hecktui::EJustilign::Center,
	//	}));
	mUserCommentsPanel->setLayout(std::make_unique<hecktui::TableLayout>(hecktui::TableLayoutConfig{
		.cols{ hecktui::TableLayoutConfig::kFlex },
		.rows{ hecktui::TableLayoutConfig::kAutoSize },
		.cells{ hecktui::TableLayoutConfig::Cell{.coord{ 0, 0 }, .align{ hecktui::EJustilign::Center, hecktui::EJustilign::Stretch } } },
		.gap{ 0, 0 },
		}));
	//boxPanel->addChild(mUserCommentsPanel);
	centerPanel->addChild(mUserCommentsPanel);

	mTradePanels[0] = std::make_shared<TradePanel>();
	mTradePanels[1] = std::make_shared<TradePanel>();
	getTuiRoot()->addChild(mTradePanels[0]);
	getTuiRoot()->addChild(centerPanel);
	getTuiRoot()->addChild(mTradePanels[1]);
	endTrade();

	updateUI();

	//getTuiRoot()->addChild(this);
	//getTuiRoot()->setLayout(std::make_unique<hecktui::FillLayout>());
}

void CvTuiDiplomacyScreen::startTrade()
{
	for (auto& panel : mTradePanels)
	{
		panel->invListPanel->removeAllChildren();
		panel->offerListPanel->removeAllChildren();
		panel->setVisible(true);
		panel->tradeCategoryCounts.clear();
		panel->tradeCategoryGroupBoxes.clear();
		panel->tradeInventoryButtons.clear();
	}

	getTuiRoot()->setLayout(std::make_unique<hecktui::TableLayout>(hecktui::TableLayoutConfig{
		.cols{
			{.weight = 1 },
			{.weight = 3 },
			{.weight = 1 },
		},
		.rows{ hecktui::TableLayoutConfig::kFlex },
		.cells {
			hecktui::TableLayoutConfig::Cell{.coord{ 0, 0 } },
			hecktui::TableLayoutConfig::Cell{.coord{ 1, 0 } },
			hecktui::TableLayoutConfig::Cell{.coord{ 2, 0 } },
		},
		.gap = 1,
		}));
}
void CvTuiDiplomacyScreen::setTradeInventory(int side, const CLinkList<TradeData>& items, const std::vector<std::wstring>& labels)
{
	auto& tradePanel = *mTradePanels[side];
	tradePanel.invListPanel->removeAllChildren();
	tradePanel.tradeCategoryCounts.clear();
	tradePanel.tradeCategoryGroupBoxes.clear();
	tradePanel.tradeInventoryButtons.clear();

	size_t numTradeItemTypes = 0;
	for (const auto& item : viewCLinkList(items))
		numTradeItemTypes = std::max<size_t>(numTradeItemTypes, item.m_eItemType + 1);
	
	tradePanel.tradeCategoryCounts.resize(numTradeItemTypes);
	tradePanel.tradeCategoryGroupBoxes.resize(numTradeItemTypes);

	const hecktui::FlowConfig listFlowLayout{
		.axis = hecktui::EAxis::Vertical,
		.linesCrosswiseJustilign = hecktui::EJustilign::Stretch,
	};

	CvString icon;

	for (size_t i = 0; i < numTradeItemTypes; ++i)
	{
		CvWString label;
		// Doesn't depend on player.
		if (!GET_PLAYER(PlayerTypes()).getHeadingTradeString(NO_PLAYER, static_cast<TradeableItems>(i), label, icon))
			label.clear();

		auto group = std::make_shared<hecktui::Element>();
		group->setLayout(std::make_unique<hecktui::FlowLayout>(listFlowLayout));
		group->setVisible(false);
		
		if (!label.empty())
		{
			auto lblHeader = std::make_shared<hecktui::Label>(std::move(label));
			lblHeader->setLabelAlignment(hecktui::EAlign::Center);
			group->addChild(std::move(lblHeader));
		}
		
		tradePanel.invListPanel->addChild(group);
		tradePanel.tradeCategoryGroupBoxes[i] = std::move(group);
	}

	std::vector<std::tuple<TradeData, std::wstring_view, size_t>> sortedList(std::from_range, std::views::zip(viewCLinkList(items), labels, range<size_t>(items.getLength())));
	std::ranges::stable_sort(sortedList, std::less<>(), [](const std::tuple<TradeData, std::wstring_view, size_t>& kv) { return std::get<0>(kv).m_eItemType; });

	tradePanel.tradeInventoryButtons.resize(items.getLength());

	for (const auto& [item, label, index] : sortedList)
	{
		auto btn = std::make_shared<Internals::TradeItemButton>(*this, item, std::wstring(label), false, side == 0);
		tradePanel.tradeCategoryGroupBoxes[item.m_eItemType]->addChild(btn);
		tradePanel.tradeInventoryButtons[index] = std::move(btn);

		tradePanel.tradeCategoryGroupBoxes[item.m_eItemType]->setVisible(true);
	}
}
void CvTuiDiplomacyScreen::updateTradeInventory(int side, const std::vector<DynamicTradeProps>& props)
{
	auto& tradePanel = *mTradePanels[side];

	assert(props.size() == tradePanel.tradeInventoryButtons.size());

	for (const auto& group : tradePanel.tradeCategoryGroupBoxes)
		group->setVisible(false);
	std::ranges::fill(tradePanel.tradeCategoryCounts, 0);

	for (const auto& [btn, itemProps] : std::views::zip(tradePanel.tradeInventoryButtons, props))
	{
		btn->isDenial = itemProps.denial;
		btn->setVisible(!itemProps.hidden);
		if (!itemProps.hidden)
		{
			tradePanel.tradeCategoryGroupBoxes[btn->tradeData.m_eItemType]->setVisible(true);
			++tradePanel.tradeCategoryCounts[btn->tradeData.m_eItemType];
		}
	}
}
void CvTuiDiplomacyScreen::setTradeOffer(int side, const CLinkList<TradeData>& items, const std::vector<std::wstring>& labels, const std::vector<int>& dealIds)
{
	auto& tradePanel = *mTradePanels[side];
	tradePanel.offerListPanel->removeAllChildren();

	for (const auto& [i, item] : viewCLinkList(items) | std::views::enumerate)
		tradePanel.offerListPanel->addChild(std::make_shared<Internals::TradeItemButton>(*this, item, labels[i], true, side == 0, dealIds.empty() ? -1 : dealIds[i]));
}

//void CvTUIDiplomacyScreen::addOfferTradeItem(int side, TradeData item, std::wstring label)
//{
//	mTradePanels[side]->offerListPanel->addChild(std::make_shared<Internals::TradeItemButton>(*this, item, label, true, side == 0, -1));
//}
//void CvTUIDiplomacyScreen::removeOfferTradeItem(int side, TradeData item)
//{
//	auto& tradePanel = *mTradePanels[side];
//	for (const auto& ctrl : tradePanel.offerListPanel->getChildren())
//	{
//		auto& btn = static_cast<Internals::TradeItemButton&>(*ctrl);
//		if (CvDeal::isMatchingTradeItem(btn.tradeData, item))
//		{
//			btn.orphan();
//			break;
//		}
//	}
//}

void CvTuiDiplomacyScreen::setCeaseFireVisible(bool visible)
{
	mTradePanels[0]->ceaseFireHeader->setVisible(visible);
	mTradePanels[1]->ceaseFireHeader->setVisible(visible);
}

void CvTuiDiplomacyScreen::setInventoriesVisible(bool visible)
{
	mTradePanels[0]->invListPanel->setVisible(visible);
	mTradePanels[1]->invListPanel->setVisible(visible);
}

void CvTuiDiplomacyScreen::endTrade()
{
	for (auto& panel : mTradePanels)
	{
		panel->invListPanel->removeAllChildren();
		panel->offerListPanel->removeAllChildren();
		panel->setVisible(false);
		panel->tradeCategoryCounts.clear();
		panel->tradeCategoryGroupBoxes.clear();
		panel->tradeInventoryButtons.clear();
	}

	// Layout does not currently ignore non-visible controls, so update the layout.
	getTuiRoot()->setLayout(std::make_unique<hecktui::TableLayout>(hecktui::TableLayoutConfig{
		.cols{
			hecktui::TableLayoutConfig::kAutoSize,
			{.weight = 3 },
			hecktui::TableLayoutConfig::kAutoSize,
		},
		.rows{ hecktui::TableLayoutConfig::kFlex },
		.cells {
			hecktui::TableLayoutConfig::Cell{.coord{ 0, 0 } },
			hecktui::TableLayoutConfig::Cell{.coord{ 1, 0 } },
			hecktui::TableLayoutConfig::Cell{.coord{ 2, 0 } },
		},
		.gap = 1,
		}));
}

void CvTuiDiplomacyScreen::rebuildPythonScreen()
{
}

void CvTuiDiplomacyScreen::updateFromGameState(hecktui::Window& wnd)
{
	//if (mUI)
	//	mUI->updateFromGameState(*this);

	CvWString text;
	CvGameTextMgr::GetInstance().getTradeScreenHeader(text, getController().getSide(kSideAI).playerI, getController().getSide(kSideActive).playerI, true);
	wnd.setWindowTitle(std::move(text));
}

std::shared_ptr<hecktui::Window> CvTuiDiplomacyScreen::createTuiWindow(bool passInput) const
{
	const hecktui::WindowConfig config{
		.isDefaultFocus = passInput,
		.isFullscreen = false,
		.isModal = true,
		.canClose = false,
		.borderStyle = hecktui::EBorderStyle::Rounded,
	};

	struct ScreenWindow : hecktui::Window
	{
		bool isPassInput = false;
		EAutoSizeBehaviour autoSizeBehaviour{};

		using hecktui::Window::Window;

		virtual void positionWindowInScene(heck::ivec2 sceneDim) override
		{
			if (getWindowConfig().isFullscreen)
				return Window::positionWindowInScene(sceneDim);

			// TODO: CvAppUI has similar code. Deduplicate.
			if (!wasMoved() && !wasResized())
			{
				//const heck::ivec2 measurement = getLayoutSizeInfo().preferred;

				const int widthFraction = gGlobals.getDiplomacyScreen()->getController().isTradeScreenActive() ? 90 : 50;
				const int heightFraction = 90;

				const heck::ivec2 size = (heck::ivec2(widthFraction, heightFraction) * sceneDim + 50) / 100;

				const heck::ivec2 position = (sceneDim - size) / 2;

				setRect(heck::iaabb2{
					.min = position,
					.max = position + size,
					}.intersection(heck::iaabb2{ .max = sceneDim }.shrunk({ 1, 1 })));
			}
		}
	};

	auto wnd = std::make_shared<ScreenWindow>(L" ", config);
	wnd->isPassInput = passInput;
	wnd->autoSizeBehaviour = EAutoSizeBehaviour::GrowOnly;
	wnd->setClientArea(getTuiRoot());
	return wnd;
}

void CvTuiDiplomacyScreen::regreet()
{
	CvDiplomacyScreen::regreet();
	updateUI();
}

void CvTuiDiplomacyScreen::endDiplomacy()
{
	CvDiplomacyScreen::endDiplomacy();
	updateUI();
}

void CvTuiDiplomacyScreen::updateUI()
{
	if (mAreTradePanelsActive != getController().isTradeScreenActive())
	{
		mAreTradePanelsActive = getController().isTradeScreenActive();
		if (mAreTradePanelsActive)
		{
			startTrade();
			rebuildTradeInventory();
		}
		else
			endTrade();
	}

	if (mAreTradePanelsActive)
		updateTradePanels();

	setInventoriesVisible(getController().isShowAllTrade());

	updateComments();
	updateLeaderHead();

	setWantClose(getController().isEndDiplomacy());

	if (auto popupInfo = getController().popPendingPopup())
		CvButtonPopup::launch(std::make_unique<CvButtonPopup>(std::move(popupInfo)));
}

// "isShowCurrent" is for when showing current deals only, not deal renegotiation.
static std::vector<std::wstring> getTradeItemLabels(PlayerTypes ownerPlayerI, PlayerTypes otherPlayerI, const CLinkList<TradeData>& items, bool isOffer, bool isShowCurrent)
{
	return { std::from_range, viewCLinkList(items) | std::views::transform([&](TradeData item) {
		return getTradeItemLabel(ownerPlayerI, otherPlayerI, item, isOffer, isShowCurrent);
		})
	};
}

void CvTuiDiplomacyScreen::rebuildTradeInventory()
{
	const bool isShowCurrent = !getController().isShowAllTrade();

	for (int i = 0; i < 2; ++i)
	{
		const CvDiplomacyController::Side& us = getController().getSide(static_cast<CvDiplomacyController::ESide>(i));
		const CvDiplomacyController::Side& them = getController().getSide(static_cast<CvDiplomacyController::ESide>(1 - i));
		setTradeInventory(i, us.inventory, getTradeItemLabels(us.playerI, them.playerI, us.inventory, false, isShowCurrent));
		//setTradeOffer(i, us.offer, getTradeItemLabels(us.playerI, them.playerI, us.offer, true), us.dealIds);
	}
}

void CvTuiDiplomacyScreen::updateTradePanels()
{
	const bool isShowCurrent = !getController().isShowAllTrade();

	for (int i = 0; i < 2; ++i)
	{
		const CvDiplomacyController::Side& us = getController().getSide(static_cast<CvDiplomacyController::ESide>(i));
		const CvDiplomacyController::Side& them = getController().getSide(static_cast<CvDiplomacyController::ESide>(1 - i));

		setTradeOffer(i, us.offer, getTradeItemLabels(us.playerI, them.playerI, us.offer, true, isShowCurrent), us.dealIds);

		//GET_PLAYER(us.playerI).updateTradeList(them.playerI, us.inventory, us.offer, them.offer);
		updateTradeInventory(i, { std::from_range, viewCLinkList(us.inventory) | std::views::transform([&](TradeData item) {
			return CvTuiDiplomacyScreen::DynamicTradeProps{
				.denial = GET_PLAYER(us.playerI).getTradeDenial(them.playerI, item) != NO_DENIAL,
				.hidden = item.m_bHidden || item.m_bOffering,
			};
			}) });
	}

	const TradeData peaceItem{ .m_eItemType = CvDeal::getPeaceItem(), .m_iData = -1, .m_bOffering{}, .m_bHidden{} };
	setCeaseFireVisible(getController().isWarDiplo() && (
		!CvDiplomacyController::Side::findMatchingTradeItem(getController().getSide(kSideAI).offer, peaceItem) ||
		!CvDiplomacyController::Side::findMatchingTradeItem(getController().getSide(kSideActive).offer, peaceItem)));
}

void CvTuiDiplomacyScreen::updateComments()
{
	mLblAIComment->setLabel(std::wstring(getController().getAICommentText()));

	for (const auto& btn : mUserCommentButtons)
		btn->orphan();
	mUserCommentButtons.clear();

	for (const auto& comment : getController().getUserComments())
	{
		auto btn = std::make_shared<Internals::UserCommentButton>(*this, comment);
		mUserCommentsPanel->addChild(btn);
		mUserCommentButtons.push_back(std::move(btn));
	}
}

void CvTuiDiplomacyScreen::updateLeaderHead()
{
	mLblLeaderheadText->widgetData = {
		.m_iData1 = getController().getSide(kSideAI).playerI,
		.m_iData2 = getController().getSide(kSideActive).playerI,
		.m_bOption = false,
		.m_eWidgetType = WIDGET_LEADERHEAD,
	};

	mLblLeaderheadText->setLabel(getController().getLeaderHeadText());
}