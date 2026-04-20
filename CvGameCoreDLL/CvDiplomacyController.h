#pragma once

// fortsnek: This was part of the engine, but now moving it to CvGameCoreDLL to support player bots.

#include "LinkedList.h"
#include "CvStructs.h"

#include <CommonStuff/NoCopy.h>

#include <memory>
#include <vector>

class CvDiploParameters;
class CvPopupInfo;

class DllExportForInterface CvDiplomacyController : public heck::NoCopyNoMove
{
public:
	explicit CvDiplomacyController(std::unique_ptr<CvDiploParameters> diploParams);
	~CvDiplomacyController() noexcept;

	void startDiplo(bool isRegreet);

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

	std::wstring_view getLeaderHeadText() const;
	std::wstring_view getAICommentText() const;
	DiploCommentTypes getAICommentType() const;

	struct UserComment
	{
		DiploCommentTypes type{};
		int data1{};
		int data2{};
		std::wstring text;
	};

	std::span<const UserComment> getUserComments() const;

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
		void clearOffer();
		[[nodiscard]] bool hasOfferIgnoringPeaceTreaty() const;

		DllExportForInterface static const TradeData* findMatchingTradeItem(const CLinkList<TradeData>& list, TradeData query);
		DllExportForInterface static TradeData* findMatchingTradeItem(CLinkList<TradeData>& list, TradeData query);
	};

	const Side& getSide(ESide) const;
	bool isTradeScreenActive() const;
	bool isShowAllTrade() const;
	bool isEndDiplomacy() const;

	void onClickUserComment(DiploCommentTypes eComment, int iData1, int iData2);

	enum class [[nodiscard]] EOfferResult
	{
		Success,
		CantEndWar,
		OnlyOneSideMayOfferItems,
		DualFailure,
	};

	EOfferResult onClickTradeItem(TradeData item, bool isOffer, bool isAIPlayer, int dealId);

	const CvDiploParameters& getDiploParams() const;

	// Diplo popups don't go through CvPlayer::addPopup, as those are deferred by diplo, and we don't want to directly inject a popup into CvInterface from here.
	// Instead, let the diplo screen implementation handle it.
	[[nodiscard]] std::unique_ptr<CvPopupInfo> popPendingPopup();

private:
	std::unique_ptr<CvDiploParameters> mDiploParams;

	std::wstring mLeaderHeadText;

	DiploCommentTypes mCurrentAIDiploComment = NO_DIPLOCOMMENT;
	std::vector<UserComment> mUserComments;
	std::wstring mAICommentText;

	std::array<Side, 2> mSides;

	bool mIsTradeUIActive = false;
	bool mIsShowAllTrade = true;
	bool mIsOfferFromAI = false;
	bool mEndDiplomacy = false;

	std::unique_ptr<CvPopupInfo> mPendingPopup;
	
	[[nodiscard]] bool canOffer(ESide sideI, TradeData item) const;
	[[nodiscard]] bool dealToEndWar();
	void refreshDiplo();

	void updateTradeInventories();
};