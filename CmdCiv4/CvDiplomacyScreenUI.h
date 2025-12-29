#pragma once

#include <CvStructs.h>
#include <LinkedList.h>

#include <HeckTextUI/BasicControls.h>
#include <HeckTextUI/Window.h>

class CvDiplomacyScreen;

namespace cvengine
{
	class RichLabel;
}

class CvDiplomacyScreenUI : public hecktui::Element
{
public:
	CvDiplomacyScreenUI();

	void updateFromGameState(const CvDiplomacyScreen& screen);

	void setLeaderheadText(std::wstring text);
	void setLeaderheadTooltip(CvWidgetDataStruct widgetData);

	void setAICommentText(std::wstring text);
	void clearUserComments();
	void addUserComment(DiploCommentTypes eComment, int data1, int data2, std::wstring text);

	void startTrade();

	void setTradeInventory(int side, const CLinkList<TradeData>& items, const std::vector<std::wstring>& labels);
	
	struct DynamicTradeProps
	{
		bool denial = false;
		bool hidden = false;
	};
	
	void updateTradeInventory(int side, const std::vector<DynamicTradeProps>& props);

	//std::shared_ptr<TradeItemButton> addTradeInventoryButton(int side, TradeData item, std::wstring label);
	//void setTradeItemDenial(TradeItemButton&, bool denial);
	//void setTradeItemHidden(TradeItemButton&, bool hidden);

	void setTradeOffer(int side, const CLinkList<TradeData>& items, const std::vector<std::wstring>& labels, const std::vector<int>& dealIds);

	static bool isMatchingTradeItem(TradeData invItem, TradeData queryItem);

	void addOfferTradeItem(int side, TradeData item, std::wstring label);
	void removeOfferTradeItem(int side, TradeData item);
	
	void setCeaseFireVisible(bool visible);
	void setInventoriesVisible(bool visible);

	void endTrade();

private:
	std::shared_ptr<cvengine::RichLabel> mLblLeaderheadText;
	std::shared_ptr<hecktui::Label> mLblAIComment;
	std::shared_ptr<hecktui::Element> mUserCommentsPanel;
	std::vector<std::shared_ptr<hecktui::Button>> mUserCommentButtons;

	struct TradePanel;
	//struct TradeItemButton;

	std::array<std::shared_ptr<TradePanel>, 2> mTradePanels;
};