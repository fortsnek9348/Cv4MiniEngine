#pragma once

#include "CvGInterfaceScreen.h"

#include <CvStructs.h>
#include <CvDiplomacyScreen.h>
#include <LinkedList.h>

#include <HeckTextUI/BasicControls.h>
#include <HeckTextUI/Window.h>

#include <CommonStuff/NoCopy.h>

namespace cvengine
{
	class RichLabel;

	class CvTuiDiplomacyScreen : public CvDiplomacyScreen, public CvGInterfaceScreen, public heck::NoCopyNoMove // self-referencing
	{
	public:
		explicit CvTuiDiplomacyScreen(std::unique_ptr<CvDiploParameters> diploParams);

		virtual void rebuildPythonScreen() override;
		virtual void updateFromGameState(hecktui::Window& window) override;
		virtual std::shared_ptr<hecktui::Window> createTuiWindow(bool passInput) const override;

		virtual void regreet() override;
		virtual void endDiplomacy() override;

	private:
		struct Internals;

		std::shared_ptr<cvengine::RichLabel> mLblLeaderheadText;
		std::shared_ptr<hecktui::Label> mLblAIComment;
		std::shared_ptr<hecktui::Element> mUserCommentsPanel;
		std::vector<std::shared_ptr<hecktui::Button>> mUserCommentButtons;

		struct TradePanel;

		std::array<std::shared_ptr<TradePanel>, 2> mTradePanels;

		bool mAreTradePanelsActive = false;

		void startTrade();

		void setTradeInventory(int side, const CLinkList<TradeData>& items, const std::vector<std::wstring>& labels);

		struct DynamicTradeProps
		{
			bool denial = false;
			bool hidden = false;
		};

		void updateTradeInventory(int side, const std::vector<DynamicTradeProps>& props);

		void setTradeOffer(int side, const CLinkList<TradeData>& items, const std::vector<std::wstring>& labels, const std::vector<int>& dealIds);

		void setCeaseFireVisible(bool visible);
		void setInventoriesVisible(bool visible);

		void endTrade();

		void updateUI();
		void rebuildTradeInventory();
		void updateTradePanels();
		void updateComments();
		void updateLeaderHead();
	};
}