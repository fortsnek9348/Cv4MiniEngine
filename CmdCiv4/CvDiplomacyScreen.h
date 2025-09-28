#pragma once

#include "CvGInterfaceScreen.h"

#include <LinkedList.h>
#include <CvStructs.h>

#include <CommonStuff/NoCopy.h>

#include <vector>

class CvDiploParameters;
class CvDiplomacyScreenUI;

class CvDiplomacyScreen : public CvGInterfaceScreen, public heck::NoCopyNoMove
{
public:
	explicit CvDiplomacyScreen(std::unique_ptr<CvDiploParameters> diploParams);

	// CyDiplomacy stuff
	void addUserComment(DiploCommentTypes eComment, int iData1, int iData2, const std::wstring& szString, const std::vector<std::wstring>& txtArgs);
	bool isWarDiplo() const;
	void clearUserComments();
	void endDiplomacy();
	bool counterPropose();
	void declareWar();
	void sendDiploEvent(DiploEventTypes iDiploEvent, int iData1, int iData2);
	void endTrade();
	bool hasAnnualDeal() const;
	void implementDeal();
	bool isAIOffer() const;
	bool offerDeal(); // Return true if deal implemented
	bool isOurOfferEmpty() const;
	void performHeadAction(LeaderheadAction eAction);
	void setAIComment(DiploCommentTypes iComment);
	void setIsAIOffer(bool bOffer);
	void setAIString(std::wstring text);
	void setShowAllTrade(bool bShow);
	void startTrade(bool bRenegotiate);
	bool isTheirOfferEmpty() const;
	bool isTheirOfferAVassalTribute();

	// After cancelling a deal.
	void regreet();

	// Called by MyCvDLLUtility
	bool isDiplomacyActive() const;
	PlayerTypes getDiplomacyAIPlayer() const;

	// Event handlers from CvDiplomacyScreenUI.
	void onClickUserComment(DiploCommentTypes eComment, int iData1, int iData2);
	void onClickTradeItem(TradeData item, bool isOffer, bool isAIPlayer, int dealId);

	// Called by us.
	bool isTradeScreenActive() const;

	const CvDiploParameters& getDiploParams() const;

	virtual void rebuildPythonScreen() override;
	virtual void updateFromGameState(hecktui::Window& window) override;
	virtual std::shared_ptr<hecktui::Window> createTuiWindow(bool passInput) const override;

	virtual ~CvDiplomacyScreen() override;

private:
	std::unique_ptr<CvDiploParameters> mDiploParams;
	std::shared_ptr<CvDiplomacyScreenUI> mUI;

	DiploCommentTypes mCurrentAIDiploComment = NO_DIPLOCOMMENT;

	enum ESide
	{
		kSideAI,
		kSideActive,
	};

	struct Side
	{
		PlayerTypes playerI{};
		TeamTypes teamI{};
		CLinkList<TradeData> inventory{};
		CLinkList<TradeData> offer{};
		std::vector<int> dealIds{};

		// Returns true iff newly added to offer.
		[[nodiscard]] bool addToOffer(TradeData);
		[[nodiscard]] bool removeFromOffer(TradeData);
		[[nodiscard]] bool hasOfferIgnoringPeaceTreaty() const;
	};

	std::array<Side, 2> mSides;

	bool mIsTradeUIActive = false;
	bool mIsShowAllTrade = true;
	bool mIsOfferFromAI = false;

	void startDiplo(bool isRegreet);
	[[nodiscard]] bool canOffer(ESide sideI, TradeData item) const;
	[[nodiscard]] bool dealToEndWar();
	void refreshDiplo();
	
	void showPeaceDealError();
	
	void updateTradeInventories();
	void updateTradeInventoriesUI();
};