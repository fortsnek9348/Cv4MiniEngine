#include "CvDiplomacyScreenUI.h"
#include "Common.h"
#include "CvDiplomacyScreen.h"
#include "CLinkListIterator.h"
#include "MainInterfaceControls.h"
#include "DLLInterface/MyCvDLLUtility.h"
#include "UITheme.h"

#include <CvGlobals.h>
#include <CvPlayerAI.h>
#include <CvDLLWidgetData.h>

#include <HeckTextUI/BasicControls.h>

#include <CommonStuff/range.h>

using heck::range;

static constexpr int kOfferPanelHeight = 7;
static constexpr hecktui::EBorderStyle kUserCommentBorderStyle = hecktui::EBorderStyle::Rounded;

namespace
{
	struct UserCommentButton : hecktui::Button
	{
		DiploCommentTypes eComment{};
		int data1{};
		int data2{};

		explicit UserCommentButton(DiploCommentTypes eComment, int data1, int data2, std::wstring text)
			: Button(std::move(text))
			, eComment(eComment)
			, data1(data1)
			, data2(data2)
		{
			setLabelColour(theme::kButtonDefaultLabelColour);
			setBorderStyle(kUserCommentBorderStyle);
		}

		virtual void onClick(hecktui::ModifierKeyState modifierKeyState) override
		{
			Button::onClick(modifierKeyState);
			gGlobals.getDiplomacyScreen()->onClickUserComment(eComment, data1, data2);
		}
	};

	struct TradeItemButton : hecktui::EmptyButton
	{
		//PlayerTypes fromPlayerI{};
		//PlayerTypes toPlayerI{};
		TradeData tradeData{};
		std::wstring label;
		bool isOffer{};
		bool isAIPlayer = false;
		bool isDenial{};
		int dealId = -1;

		explicit TradeItemButton(TradeData tradeData, std::wstring label, bool isOffer, bool isAIPlayer, int dealId = -1)
			: tradeData(tradeData), label(std::move(label)), isOffer(isOffer), isAIPlayer(isAIPlayer), dealId(dealId)
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
			return createWidgetTooltip({
				.m_iData1 = tradeData.m_eItemType,
				.m_iData2 = tradeData.m_iData,
				.m_bOption = isAIPlayer,
				.m_eWidgetType = WIDGET_TRADE_ITEM,
				});
		}

		virtual void onClick(hecktui::ModifierKeyState) override
		{
			if (!isDenial)
				gGlobals.getDiplomacyScreen()->onClickTradeItem(tradeData, isOffer, isAIPlayer, dealId);
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
}

struct CvDiplomacyScreenUI::TradePanel : hecktui::Element
{
	std::shared_ptr<hecktui::Element> invListPanel;
	std::shared_ptr<hecktui::Element> offerListContainer;
	std::shared_ptr<hecktui::Element> offerListPanel;
	std::shared_ptr<hecktui::Label> ceaseFireHeader;

	std::vector<size_t> tradeCategoryCounts;
	std::vector<std::shared_ptr<hecktui::Element>> tradeCategoryGroupBoxes;
	std::vector<std::shared_ptr<TradeItemButton>> tradeInventoryButtons;
	//std::vector<size_t> tradeInventoryButtons;

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

		ceaseFireHeader = std::make_shared<hecktui::Label>(MyCvDLLUtility::getInstance().getTextGeneric(L"TXT_KEY_TRADE_CEASE_FIRE_STRING", {}));
		ceaseFireHeader->setVisible(false);
		offerListContainer->addChild(ceaseFireHeader);

		offerListPanel = std::make_shared<hecktui::Element>();
		offerListPanel->setLayout(std::make_unique<hecktui::FlowLayout>(listFlowLayout));
		offerListContainer->addChild(offerListPanel);
	}
};

/*static auto makeBorderedFillLayout()
{
	auto layout = std::make_unique<hecktui::FillLayout>();
	layout->marginTopLeft = layout->marginBottomRight = 1;
	return layout;
}*/

CvDiplomacyScreenUI::CvDiplomacyScreenUI()
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

	mLblLeaderheadText = std::make_shared<RichLabel>(L"");
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
	addChild(mTradePanels[0]);
	addChild(centerPanel);
	addChild(mTradePanels[1]);
	endTrade();
}

void CvDiplomacyScreenUI::updateFromGameState(const CvDiplomacyScreen&)
{
}

void CvDiplomacyScreenUI::setLeaderheadText(std::wstring text)
{
	mLblLeaderheadText->setLabel(std::move(text));
}

void CvDiplomacyScreenUI::setLeaderheadTooltip(CvWidgetDataStruct widgetData)
{
	mLblLeaderheadText->widgetData = widgetData;
}

void CvDiplomacyScreenUI::setAICommentText(std::wstring text)
{
	mLblAIComment->setLabel(std::move(text));
}

void CvDiplomacyScreenUI::clearUserComments()
{
	for (const auto& btn : mUserCommentButtons)
		btn->orphan();
	mUserCommentButtons.clear();
}
void CvDiplomacyScreenUI::addUserComment(DiploCommentTypes eComment, int data1, int data2, std::wstring text)
{
	auto btn = std::make_shared<UserCommentButton>(eComment, data1, data2, std::move(text));
	mUserCommentsPanel->addChild(btn);
	mUserCommentButtons.push_back(std::move(btn));
}

void CvDiplomacyScreenUI::startTrade()
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

	setLayout(std::make_unique<hecktui::TableLayout>(hecktui::TableLayoutConfig{
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
void CvDiplomacyScreenUI::setTradeInventory(int side, const CLinkList<TradeData>& items, const std::vector<std::wstring>& labels)
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
		auto btn = std::make_shared<TradeItemButton>(item, std::wstring(label), false, side == 0);
		tradePanel.tradeCategoryGroupBoxes[item.m_eItemType]->addChild(btn);
		tradePanel.tradeInventoryButtons[index] = std::move(btn);

		tradePanel.tradeCategoryGroupBoxes[item.m_eItemType]->setVisible(true);
	}
}
void CvDiplomacyScreenUI::updateTradeInventory(int side, const std::vector<DynamicTradeProps>& props)
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
void CvDiplomacyScreenUI::setTradeOffer(int side, const CLinkList<TradeData>& items, const std::vector<std::wstring>& labels, const std::vector<int>& dealIds)
{
	auto& tradePanel = *mTradePanels[side];
	tradePanel.offerListPanel->removeAllChildren();

	for (const auto& [item, label, dealId] : std::views::zip(viewCLinkList(items), labels, dealIds))
		tradePanel.offerListPanel->addChild(std::make_shared<TradeItemButton>(item, label, true, side == 0, dealId));
}

bool CvDiplomacyScreenUI::isMatchingTradeItem(TradeData item, TradeData query)
{
	return query.m_eItemType == item.m_eItemType
		// Match data, unless the data is quantity.
		&& (CvDeal::isGold(query.m_eItemType) || !CvDeal::hasData(item.m_eItemType) || query.m_iData == item.m_iData);
}

void CvDiplomacyScreenUI::addOfferTradeItem(int side, TradeData item, std::wstring label)
{
	mTradePanels[side]->offerListPanel->addChild(std::make_shared<TradeItemButton>(item, label, true, side == 0, -1));
}
void CvDiplomacyScreenUI::removeOfferTradeItem(int side, TradeData item)
{
	auto& tradePanel = *mTradePanels[side];
	for (const auto& ctrl : tradePanel.offerListPanel->getChildren())
	{
		auto& btn = static_cast<TradeItemButton&>(*ctrl);
		if (isMatchingTradeItem(btn.tradeData, item))
		{
			btn.orphan();
			break;
		}
	}
}

void CvDiplomacyScreenUI::setCeaseFireVisible(bool visible)
{
	mTradePanels[0]->ceaseFireHeader->setVisible(visible);
	mTradePanels[1]->ceaseFireHeader->setVisible(visible);
}

void CvDiplomacyScreenUI::setInventoriesVisible(bool visible)
{
	mTradePanels[0]->invListPanel->setVisible(visible);
	mTradePanels[1]->invListPanel->setVisible(visible);
}

void CvDiplomacyScreenUI::endTrade()
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
	setLayout(std::make_unique<hecktui::TableLayout>(hecktui::TableLayoutConfig{
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