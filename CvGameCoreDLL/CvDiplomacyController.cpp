#include "CvDiplomacyController.h"
#include "CLinkListIterator.h"
#include "CvDiploParameters.h"
#include "CvDLLPythonIFaceBase.h"
#include "CvDLLUtilityIFaceBase.h"
#include "CvGameAI.h"
#include "CvGameCoreUtils.h"
#include "CvGlobals.h"
#include "CvPlayerAI.h"
#include "CvPopupInfo.h"
#include "CvTeamAI.h"
#include "CyArgsList.h"

#include <algorithm>
#include <ranges>

CvDiplomacyController::CvDiplomacyController(std::unique_ptr<CvDiploParameters> diploParams)
	: mDiploParams(std::move(diploParams))
	, mSides{ {
		Side{.playerI = mDiploParams->getWhoTalkingTo(), .teamI = GET_PLAYER(mDiploParams->getWhoTalkingTo()).getTeam() },
		Side{.playerI = GC.getGame().getActivePlayer(), .teamI = GC.getGame().getActiveTeam() },
	} }
{
}

void CvDiplomacyController::startDiplo(bool isRegreet)
{
	CvPlayerAI& aiPlayer = GET_PLAYER(mSides[kSideAI].playerI);

	mCurrentAIDiploComment = NO_DIPLOCOMMENT;

	if (!isRegreet)
	{
		mCurrentAIDiploComment = mDiploParams->getDiploComment();

		if (mCurrentAIDiploComment == gGlobals.getInfoTypeForString("AI_DIPLOCOMMENT_JOIN_WAR"))
		{
			// Check if we can actually declare war.
			const TeamTypes activeTeamI = GET_PLAYER(mSides[kSideActive].playerI).getTeam();
			const TeamTypes aiTeamI = GET_PLAYER(mSides[kSideAI].playerI).getTeam();
			const TeamTypes enemyTeamI = static_cast<TeamTypes>(mDiploParams->getData());

			const bool isValidJoinWar =
				GET_TEAM(activeTeamI).canDeclareWar(enemyTeamI)
				&& !GET_TEAM(activeTeamI).isAtWar(aiTeamI)
				&& GET_TEAM(aiTeamI).isAtWar(enemyTeamI);

			if (!isValidJoinWar)
			{
				endDiplomacy();
				return;
			}
		}
		else if (mDiploParams->getOurOfferList().getLength() > 0 || mDiploParams->getTheirOfferList().getLength() > 0) // "our" is the active player
		{
			setShowAllTrade(false);
			startTrade(false);
			const auto tryLoadOffer = [&](ESide side, const CLinkList<TradeData>& diploOffer) {
				for (const TradeData item : viewCLinkList(diploOffer))
				{
					if (GET_PLAYER(mSides[side].playerI).canTradeItem(mSides[1 - side].playerI, item, false))
					{
						(void)mSides[side].addToOffer(item);
						//if (mSides[side].addToOffer(item))
						//	mUI->addOfferTradeItem(side, item, getTradeItemLabel(mSides[side].playerI, mSides[1 - side].playerI, item, true, false));
					}
					else
						return false;
				}
				return true;
				};
			const bool canOffer = tryLoadOffer(kSideActive, mDiploParams->getOurOfferList())
				&& tryLoadOffer(kSideAI, mDiploParams->getTheirOfferList());


			if (canOffer && dealToEndWar()) // If this is a wartime offer from the AI, then check we can end the war.
			{
				updateTradeInventories();
				//updateTradeInventoriesUI();
			}
			else
			{
				// Autosave turn 40 BC-2400 demanded nothing.CivBeyondSwordSave: AI demanded map, but we couldn't trade that yet. Firaxis engine appears to cancel the diplo.
				endDiplomacy();
				return;
			}
		}
	}

	DiploEventTypes diploEvent = mDiploParams->getAIContact() ? DIPLOEVENT_AI_CONTACT : DIPLOEVENT_CONTACT;

	// Not sure what happens if AI sends diplo request when also refuses to talk...
	// UPDATE: Okay, load "Axolotl7 BC-0320 Cancel deals from AI" and press next turn.
	//         Napoleon will greet you and immediately demand you to cancel deals afterwards.
	//         If you immediately declare war on him when he greets you, the next diplo is still queued up.
	//         The Firaxis engine will bug out and let you continue diplo with him, with a buggy trade screen.
	//         Here, this won't happen. He'll tell you to get lost because he refuses to talk.
	// UPDATE: No, can't do that. When AI declares war on you, they will refuse to talk, but they will queue up a diplo (Axolotl7 AD-1510 AI declares war on me).
	// So, with the code below, "Axolotl7 BC-0320 Cancel deals from AI" will remain bugged, and you can access trade too...
	// So, for now, just abort diplo if refuses to talk and starting trade.
	if (mCurrentAIDiploComment == NO_DIPLOCOMMENT)
	{
		if (aiPlayer.AI_isWillingToTalk(mSides[kSideActive].playerI))
			mCurrentAIDiploComment = aiPlayer.AI_getGreeting(mSides[kSideActive].playerI);
		else
		{
			mCurrentAIDiploComment = static_cast<DiploCommentTypes>(gGlobals.getInfoTypeForString("AI_DIPLOCOMMENT_REFUSE_TO_TALK"));
			diploEvent = DIPLOEVENT_FAILED_CONTACT;
		}
	}

	{
		CyArgsList args;
		args.add(static_cast<int>(mCurrentAIDiploComment));
		args.add(static_cast<int>(mDiploParams->getDiploCommentArgs().size())); // zero args
		for (const FVariable& arg : mDiploParams->getDiploCommentArgs())
		{
			switch (arg.m_eType)
			{
			case FVARTYPE_WSTRING:
				args.add(arg.m_wszValue);
				break;
			default:
				std::abort();
			}
		}
		gGlobals.getDLLIFace()->getPythonIFace()->callFunction(PYDiplomacyModule, "beginDiplomacy", args.makeFunctionArgs());
	}

	if (!isRegreet)
		sendDiploEvent(diploEvent, -1, -1);
}

void CvDiplomacyController::addUserComment(DiploCommentTypes eComment, int iData1, int iData2,
	const std::wstring& szString, const std::vector<std::wstring>& txtArgs)
{
	const std::vector<CvDLLUtilityIFaceBase::TextArg> formatTextArgs(std::from_range, txtArgs);
	//mUI->addUserComment(eComment, iData1, iData2, MyCvDLLUtility::getInstance().getTextGeneric(szString, formatTextArgs));
	mUserComments.emplace_back(eComment, iData1, iData2, gGlobals.getDLLIFace()->getTextGeneric(szString, formatTextArgs));
}
bool CvDiplomacyController::isWarDiplo() const
{
	return ::atWar(mSides[kSideAI].teamI, mSides[kSideActive].teamI);
}
void CvDiplomacyController::clearUserComments()
{
	mUserComments.clear();
}
void CvDiplomacyController::endDiplomacy()
{
	assert(!mPendingPopup);
	mEndDiplomacy = true;
}
bool CvDiplomacyController::counterPropose()
{
	CLinkList<TradeData> counters[2];
	if (GET_PLAYER(mSides[kSideAI].playerI).AI_counterPropose(
		mSides[kSideActive].playerI,
		&mSides[kSideActive].offer,
		&mSides[kSideAI].offer,
		&mSides[kSideActive].inventory,
		&mSides[kSideAI].inventory,
		&counters[kSideActive],
		&counters[kSideAI]
	))
	{
		for (int i = 0; i < 2; ++i)
		{
			for (const TradeData item : viewCLinkList(counters[i]))
			{
				//if (mSides[i].addToOffer(item))
				//	mUI->addOfferTradeItem(i, item, getTradeItemLabel(mSides[i].playerI, mSides[1 - i].playerI, item, true, false));
				(void)mSides[i].addToOffer(item);

				// AI_counterPropose ignores dual requirement
				// My Game turn 248 AD-1690 incomplete peace treaty.CivBeyondSwordSave
				if (CvDeal::isDual(item.m_eItemType))
				{
					const int j = 1 - i;
					//if (mSides[j].addToOffer(item))
					//	mUI->addOfferTradeItem(j, item, getTradeItemLabel(mSides[j].playerI, mSides[1 - j].playerI, item, true, false));
					(void)mSides[j].addToOffer(item);
				}
			}
		}
		updateTradeInventories();
		//updateTradeInventoriesUI();
		mIsOfferFromAI = true;
		return true;
	}
	else
		return false;
}
void CvDiplomacyController::declareWar()
{
	//auto info = std::make_unique<CvPopupInfo>(BUTTONPOPUP_DECLAREWARMOVE, mSides[kSideAI].playerI, INT_MIN, INT_MIN, 1, false, false);
	//auto popup = std::make_unique<CvButtonPopup>(std::move(info));
	//CvButtonPopup::launch(std::move(popup));

	//GET_PLAYER(mSides[kSideActive].playerI).addPopup(info.release(), true);

	assert(!mPendingPopup);
	mPendingPopup = std::make_unique<CvPopupInfo>(BUTTONPOPUP_DECLAREWARMOVE, mSides[kSideAI].playerI, INT_MIN, INT_MIN, 1, false, false);
}
void CvDiplomacyController::sendDiploEvent(DiploEventTypes iDiploEvent, int iData1, int iData2)
{
	// TODO: Is this all that happens? Check when USER_DIPLOCOMMENT_ASK.
	GET_PLAYER(mSides[kSideAI].playerI).handleDiploEvent(iDiploEvent, mSides[kSideActive].playerI, iData1, iData2);
}

bool CvDiplomacyController::hasAnnualDeal() const
{
	// Logs show that the engine does go through all deals.
	std::array<PlayerTypes, 2> diploPlayerList{ mSides[0].playerI, mSides[1].playerI };
	std::ranges::sort(diploPlayerList);

	CvGame& game = gGlobals.getGame();
	int itIndex{};
	for (const CvDeal* deal = game.firstDeal(&itIndex); deal; deal = game.nextDeal(&itIndex))
	{
		std::array<PlayerTypes, 2> dealPlayerList{ deal->getFirstPlayer(), deal->getSecondPlayer() };
		std::ranges::sort(dealPlayerList);
		if (diploPlayerList == dealPlayerList)
		{
			for (auto* node = deal->headFirstTradesNode(); node; node = deal->nextFirstTradesNode(node))
				if (CvDeal::isAnnual(node->m_data.m_eItemType))
					return true;

			for (auto* node = deal->headSecondTradesNode(); node; node = deal->nextSecondTradesNode(node))
				if (CvDeal::isAnnual(node->m_data.m_eItemType))
					return true;
		}
	}

	return false;
}
void CvDiplomacyController::implementDeal()
{
	gGlobals.getGame().implementDeal(mSides[kSideActive].playerI, mSides[kSideAI].playerI, &mSides[kSideActive].offer, &mSides[kSideAI].offer, false);
	//CvInterface::getInstance().onGameStateChanged(CvInterface::EGameStateChangeReason::Deal);

}
bool CvDiplomacyController::isAIOffer() const
{
	return mIsOfferFromAI;
}
bool CvDiplomacyController::offerDeal()
{
	if (GET_PLAYER(mSides[kSideAI].playerI).AI_considerOffer(mSides[kSideActive].playerI, &mSides[kSideActive].offer, &mSides[kSideAI].offer, 1))
	{
		implementDeal();
		return true;
	}
	else
		return false;
}
bool CvDiplomacyController::isOurOfferEmpty() const
{
	return mSides[kSideActive].offer.getLength() <= 0;
}
bool CvDiplomacyController::isTheirOfferEmpty() const
{
	return mSides[kSideAI].offer.getLength() <= 0;
}
void CvDiplomacyController::performHeadAction(LeaderheadAction eAction)
{
	static constexpr std::array kLabels = std::to_array<std::wstring_view>({
		L"greeting",
		L"friendly",
		L"pleased",
		L"cautious",
		L"annoyed",
		L"furious",
		L"disagree",
		L"agree",
		});

	mLeaderHeadText = L'[' + std::wstring(GET_PLAYER(mSides[kSideAI].playerI).getName()) + L"'s " + kLabels.at(eAction) + L" face]";
}
void CvDiplomacyController::setAIComment(DiploCommentTypes iComment)
{
	mCurrentAIDiploComment = iComment;
	gGlobals.getGame().handleDiplomacySetAIComment(iComment);
}
void CvDiplomacyController::setIsAIOffer(bool bOffer)
{
	mIsOfferFromAI = bOffer;
}
void CvDiplomacyController::setAIString(std::wstring text)
{
	//mUI->setAICommentText(std::move(text));
	mAICommentText = std::move(text);
}
void CvDiplomacyController::setShowAllTrade(bool bShow)
{
	mIsShowAllTrade = bShow;

	//mUI->setInventoriesVisible(bShow);
}

void CvDiplomacyController::startTrade(bool bRenegotiate)
{
	if (mIsTradeUIActive)
		return;

	// Sanity check for the bug described above.
	if (!GET_PLAYER(mSides[kSideAI].playerI).AI_isWillingToTalk(mSides[kSideActive].playerI))
	{
		endDiplomacy();
		return;
	}

	for (int i = 0; i < 2; ++i)
	{
		mSides[i].inventory.clear();
		mSides[i].offer.clear();
		GET_PLAYER(mSides[i].playerI).buildTradeTable(mSides[1 - i].playerI, mSides[i].inventory);
	}



	mIsTradeUIActive = true;
	//mIsShowAllTrade = !bRenegotiate;
	//mIsOfferFromAI = false;

	/*for (auto& side : mSides)
	{
		side.inventory.clear();
		side.offer.clear();
	}*/

	// "bRenegotiate" here actually means "show current deals".
	if (!bRenegotiate)
	{
		;
	}
	else
	{
		// Logs show that the engine does go through all deals.
		std::array<PlayerTypes, 2> diploPlayerList{ mSides[0].playerI, mSides[1].playerI };
		std::ranges::sort(diploPlayerList);

		CvGame& game = gGlobals.getGame();
		int itIndex{};
		for (const CvDeal* deal = game.firstDeal(&itIndex); deal; deal = game.nextDeal(&itIndex))
		{
			std::array<PlayerTypes, 2> dealPlayerList{ deal->getFirstPlayer(), deal->getSecondPlayer() };
			std::ranges::sort(dealPlayerList);
			if (diploPlayerList == dealPlayerList)
			{
				Side& firstPlayerSide = mSides[deal->getFirstPlayer() != mSides[0].playerI];
				Side& secondPlayerSide = mSides[deal->getSecondPlayer() != mSides[0].playerI];

				auto& firstPlayerDealIds = mSides[deal->getFirstPlayer() != mSides[0].playerI].dealIds;
				auto& secondPlayerDealIds = mSides[deal->getSecondPlayer() != mSides[0].playerI].dealIds;

				for (auto* node = deal->headFirstTradesNode(); node; node = deal->nextFirstTradesNode(node))
				{
					firstPlayerSide.offer.insertAtEnd(node->m_data);
					firstPlayerDealIds.push_back(deal->getID());
				}

				for (auto* node = deal->headSecondTradesNode(); node; node = deal->nextSecondTradesNode(node))
				{
					secondPlayerSide.offer.insertAtEnd(node->m_data);
					secondPlayerDealIds.push_back(deal->getID());
				}
			}
		}
	}

	updateTradeInventories();
}

void CvDiplomacyController::endTrade()
{
	mIsTradeUIActive = false;
	for (auto& side : mSides)
	{
		for (TradeData& item : viewCLinkList(side.inventory))
			item.m_bOffering = false;
		side.offer.clear();
	}
}

bool CvDiplomacyController::isTheirOfferAVassalTribute()
{
	// The diplo text changes to "Time for your tribute" when specifically demanding resources only.
	return isOurOfferEmpty() && CvDeal::isVassalTributeDeal(&mSides[kSideAI].offer);
}

void CvDiplomacyController::regreet()
{
	endTrade();
	startDiplo(true);
}

bool CvDiplomacyController::isDiplomacyActive() const
{
	return true;
}

PlayerTypes CvDiplomacyController::getDiplomacyAIPlayer() const
{
	return mSides[kSideAI].playerI;
}

std::wstring_view CvDiplomacyController::getLeaderHeadText() const
{
	return mLeaderHeadText;
}

std::wstring_view CvDiplomacyController::getAICommentText() const
{
	return mAICommentText;
}

DiploCommentTypes CvDiplomacyController::getAICommentType() const
{
	return mCurrentAIDiploComment;
}

std::span<const CvDiplomacyController::UserComment> CvDiplomacyController::getUserComments() const
{
	return mUserComments;
}

const CvDiplomacyController::Side& CvDiplomacyController::getSide(ESide i) const
{
	return mSides[i];
}

void CvDiplomacyController::onClickUserComment(DiploCommentTypes eComment, int iData1, int iData2)
{
	CyArgsList args;
	args.add(static_cast<int>(eComment));
	args.add(iData1);
	args.add(iData2);
	gGlobals.getDLLIFace()->getPythonIFace()->callFunction(PYDiplomacyModule, "handleUserResponse", args.makeFunctionArgs());
}

const TradeData* CvDiplomacyController::Side::findMatchingTradeItem(const CLinkList<TradeData>& list, TradeData query)
{
	for (TradeData& item : viewCLinkList(list))
		if (CvDeal::isMatchingTradeItem(item, query))
			return &item;
	return nullptr;
}
TradeData* CvDiplomacyController::Side::findMatchingTradeItem(CLinkList<TradeData>& list, TradeData query)
{
	for (TradeData& item : viewCLinkList(list))
		if (CvDeal::isMatchingTradeItem(item, query))
			return &item;
	return nullptr;
}

static bool hasDualDenial(PlayerTypes playerA, PlayerTypes playerB, TradeData item)
{
	return GET_PLAYER(playerA).getTradeDenial(playerB, item) != NO_DENIAL || GET_PLAYER(playerB).getTradeDenial(playerA, item) != NO_DENIAL;
}

CvDiplomacyController::EOfferResult CvDiplomacyController::onClickTradeItem(TradeData item, bool isOffer, bool isAIPlayer, int dealId)
{
	const int sideI = !isAIPlayer;
	const int otherSideI = isAIPlayer;
	auto& side = mSides[sideI];
	auto& otherSide = mSides[otherSideI];

	if (isOffer && dealId >= 0)
	{
		//auto info = std::make_unique<CvPopupInfo>(ButtonPopupTypes::BUTTONPOPUP_DEAL_CANCELED, dealId, -1, -1, 0, true);
		//auto popup = std::make_unique<CvButtonPopup>(std::move(info));
		//CvButtonPopup::launch(std::move(popup));

		//GET_PLAYER(mSides[kSideActive].playerI).addPopup(info.release(), true);
		assert(!mPendingPopup);
		mPendingPopup = std::make_unique<CvPopupInfo>(ButtonPopupTypes::BUTTONPOPUP_DEAL_CANCELED, dealId, -1, -1, 0, true);
	}
	else if (TradeData* const invItem = Side::findMatchingTradeItem(side.inventory, item))
	{
		const bool isAddingToOffer = !isOffer;

		if (isAddingToOffer)
		{
			if (isWarDiplo())
			{
				const bool side0Gifting = mSides[0].hasOfferIgnoringPeaceTreaty() || (sideI == 0 && invItem->m_eItemType != CvDeal::getPeaceItem());
				const bool side1Gifting = mSides[1].hasOfferIgnoringPeaceTreaty() || (sideI == 1 && invItem->m_eItemType != CvDeal::getPeaceItem());
				if (side0Gifting && side1Gifting)
					return EOfferResult::OnlyOneSideMayOfferItems;
			}

			if (!CvDeal::isEndWar(invItem->m_eItemType))
			{
				if (!dealToEndWar())
					return EOfferResult::CantEndWar;
			}

			TradeData offerItem = *invItem;
			if (CvDeal::isGold(offerItem.m_eItemType))
			{
				// Caller should have filled this in.
				offerItem.m_iData = item.m_iData;
				assert(offerItem.m_iData > 0);
			}
			else if (CvDeal::isDual(offerItem.m_eItemType))
			{
				if (hasDualDenial(side.playerI, otherSide.playerI, offerItem))
					return EOfferResult::DualFailure;

				// We need the other guy to offer it too.
				TradeData* const otherGuyInvItem = Side::findMatchingTradeItem(otherSide.inventory, item);
				if (!otherGuyInvItem)
					return EOfferResult::DualFailure;

				if (!otherGuyInvItem->m_bOffering)
				{
					otherGuyInvItem->m_bOffering = true;
					if (otherSide.addToOffer(*otherGuyInvItem))
					{
						//mUI->addOfferTradeItem(otherSideI, *otherGuyInvItem, getTradeItemLabel(otherSide.playerI, side.playerI, *otherGuyInvItem, true, false));
						mIsOfferFromAI = false;
					}
				}
			}

			if (side.addToOffer(offerItem))
			{
				//mUI->addOfferTradeItem(sideI, offerItem, getTradeItemLabel(side.playerI, otherSide.playerI, offerItem, true, false));
				mIsOfferFromAI = false;
			}
		}
		else
		{
			if (CvDeal::isDual(invItem->m_eItemType))
			{
				if (TradeData* const otherGuyInvItem = Side::findMatchingTradeItem(otherSide.inventory, item))
					otherGuyInvItem->m_bOffering = false;
				if (otherSide.removeFromOffer(*invItem))
					mIsOfferFromAI = false;
				//mUI->removeOfferTradeItem(otherSideI, *invItem);
			}

			if (side.removeFromOffer(*invItem))
				mIsOfferFromAI = false;
			//mUI->removeOfferTradeItem(sideI, *invItem);

			if (isWarDiplo())
			{
				// If gifting items, ensure there's a peace deal.
				const bool side0Gifting = mSides[0].hasOfferIgnoringPeaceTreaty();
				const bool side1Gifting = mSides[1].hasOfferIgnoringPeaceTreaty();
				if (side0Gifting || side1Gifting)
				{
					// Reset to cease fire.
					mSides[0].clearOffer();
					mSides[1].clearOffer();
					// This is the message that BTS displays.
					return EOfferResult::OnlyOneSideMayOfferItems;
				}
			}
		}

		updateTradeInventories();
		//updateTradeInventoriesUI();
		refreshDiplo();
	}

	return EOfferResult::Success;
}

bool CvDiplomacyController::isTradeScreenActive() const
{
	return mIsTradeUIActive;
}

bool CvDiplomacyController::isShowAllTrade() const
{
	return mIsShowAllTrade;
}

bool CvDiplomacyController::isEndDiplomacy() const
{
	return mEndDiplomacy;
}

const CvDiploParameters& CvDiplomacyController::getDiploParams() const
{
	return *mDiploParams;
}

std::unique_ptr<CvPopupInfo> CvDiplomacyController::popPendingPopup()
{
	return std::move(mPendingPopup);
}


bool CvDiplomacyController::Side::addToOffer(TradeData item)
{
	// Mark as offered.
	for (TradeData& invItem : viewCLinkList(inventory))
	{
		if (CvDeal::isMatchingTradeItem(invItem, item))
		{
			invItem.m_bOffering = true;
			break;
		}
	}

	// Add to offer list if not already in it.
	if (!std::ranges::any_of(viewCLinkList(offer), std::bind_back(CvDeal::isMatchingTradeItem, item)))
	{
		offer.insertAtEnd(item);
		return true;
	}
	else
		return false;
}

bool CvDiplomacyController::Side::removeFromOffer(TradeData item)
{
	for (TradeData& invItem : viewCLinkList(inventory))
	{
		if (CvDeal::isMatchingTradeItem(invItem, item))
		{
			invItem.m_bOffering = false;
			break;
		}
	}

	if (auto* const node = std::ranges::find_if(viewCLinkList(offer), std::bind_back(CvDeal::isMatchingTradeItem, item)).node)
	{
		offer.deleteNode(node);
		return true;
	}
	else
		return false;
}

void CvDiplomacyController::Side::clearOffer()
{
	for (TradeData& invItem : viewCLinkList(inventory))
		invItem.m_bOffering = false;
	
	offer.clear();
}

bool CvDiplomacyController::Side::hasOfferIgnoringPeaceTreaty() const
{
	return !std::ranges::empty(viewCLinkList(offer) | std::views::filter([](TradeData item) { return item.m_eItemType != CvDeal::getPeaceItem(); }));
}

bool CvDiplomacyController::canOffer(ESide sideI, TradeData item) const
{
	const auto view = viewCLinkList(mSides[sideI].inventory);
	const auto it = std::ranges::find_if(view, std::bind_back(CvDeal::isMatchingTradeItem, item));
	if (it != view.end())
		return !it->m_bHidden && GET_PLAYER(mSides[sideI].playerI).getTradeDenial(mSides[1 - sideI].playerI, item) == NO_DENIAL;
	return false;
}

bool CvDiplomacyController::dealToEndWar()
{
	// If not at war, then we don't need to do anything.
	if (!isWarDiplo())
		return true;

	// Else, ensure there's something in the offer that ends the war (other than a cease fire).
	// Either a peace treaty or vassalage.

	for (const Side& side : mSides)
		for (const TradeData& item : viewCLinkList(side.offer))
			if (CvDeal::isEndWar(item.m_eItemType))
				return true;

	// Need to add peace to the offer.

	const TradeData defPeaceItem{
		.m_eItemType = CvDeal::getPeaceItem(),
		.m_iData = -1,
		.m_bOffering{},
		.m_bHidden{},
	};

	// Check denial.
	if (canOffer(kSideAI, defPeaceItem) && canOffer(kSideActive, defPeaceItem))
	{
		for (int i = 0; i < 2; ++i)
		{
			//if (mSides[i].addToOffer(defPeaceItem))
			//	mUI->addOfferTradeItem(i, defPeaceItem, getTradeItemLabel(mSides[i].playerI, mSides[1 - i].playerI, defPeaceItem, true, false));
			(void)mSides[i].addToOffer(defPeaceItem);
		}
		return true;
	}

	return false;
}

void CvDiplomacyController::refreshDiplo()
{
	CyArgsList args;
	args.add(static_cast<int>(mCurrentAIDiploComment));
	gGlobals.getDLLIFace()->getPythonIFace()->callFunction(PYDiplomacyModule, "refresh", args.makeFunctionArgs());
}

void CvDiplomacyController::updateTradeInventories()
{
	for (int i = 0; i < 2; ++i)
		GET_PLAYER(mSides[i].playerI).updateTradeList(mSides[1 - i].playerI, mSides[i].inventory, mSides[i].offer, mSides[1 - i].offer);
}
