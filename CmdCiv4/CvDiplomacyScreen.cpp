#include "CvDiplomacyScreen.h"
#include "CvDiplomacyScreenUI.h"
#include "DLLInterface/MyCvDLLPython.h"
#include "CvTranslator.h"
#include "CLinkListIterator.h"
#include "DLLInterface/MyCvDLLUtility.h"
#include "DLLMessageQueue.h"
#include "CvButtonPopup.h"
#include "Common.h"
#include "CvInterface.h"

#include <CvDLLButtonPopup.h>
#include <CvDiploParameters.h>
#include <CvGlobals.h>
#include <CvGameCoreUtils.h>
#include <CvGameAI.h>
#include <CvPlayerAI.h>
#include <CvGameTextMgr.h>
#include <CyArgsList.h>
#include <CvMessageData.h>
#include <CvTeamAI.h>
#include <CvDLLWidgetData.h>

#include <HeckTextUI/Window.h>

#include <ranges>

static std::wstring getTradeItemLabel(PlayerTypes ownerPlayerI, PlayerTypes otherPlayerI, TradeData item, bool isOffer, bool isShowCurrent)
{
	if (isOffer)
		std::swap(ownerPlayerI, otherPlayerI); // WEIRD!

	CvString icon;
	CvWString label;
	if (!GET_PLAYER(ownerPlayerI).getItemTradeString(otherPlayerI, isOffer, isShowCurrent, item, label, icon))
		label = L"[UNKNOWN]";
	return std::move(label);
}

CvDiplomacyScreen::CvDiplomacyScreen(std::unique_ptr<CvDiploParameters> diploParams)
	: CvGInterfaceScreen("Diplomacy Screen", CvEngineEnums::ECvScreen::DIPLOMACY_SCREEN)
	, mDiploParams(std::move(diploParams))
	, mUI(std::make_shared<CvDiplomacyScreenUI>())
	, mSides{ {
		Side{.playerI = mDiploParams->getWhoTalkingTo(), .teamI = GET_PLAYER(mDiploParams->getWhoTalkingTo()).getTeam() },
		Side{.playerI = GC.getGame().getActivePlayer(), .teamI = GC.getGame().getActiveTeam() },
	} }
{
	getTuiRoot()->addChild(mUI);
	getTuiRoot()->setLayout(std::make_unique<hecktui::FillLayout>());

	gGlobals.setDiplomacyScreen(this);

	startDiplo(false);
}

void CvDiplomacyScreen::startDiplo(bool isRegreet)
{
	mUI->setLeaderheadTooltip({
		.m_iData1 = mSides[kSideAI].playerI,
		.m_iData2 = mSides[kSideActive].playerI,
		.m_bOption = false,
		.m_eWidgetType = WIDGET_LEADERHEAD,
		});

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
						if (mSides[side].addToOffer(item))
							mUI->addOfferTradeItem(side, item, getTradeItemLabel(mSides[side].playerI, mSides[1 - side].playerI, item, true, false));
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
				updateTradeInventoriesUI();
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
		(void)MyCvDLLPython().callFunction(PYDiplomacyModule, "beginDiplomacy", args.makeFunctionArgs());
	}

	if (!isRegreet)
		sendDiploEvent(diploEvent, -1, -1);
}

void CvDiplomacyScreen::addUserComment(DiploCommentTypes eComment, int iData1, int iData2,
	const std::wstring& szString, const std::vector<std::wstring>& txtArgs)
{
	const std::vector<CvDLLUtilityIFaceBase::TextArg> formatTextArgs(std::from_range, txtArgs);
	mUI->addUserComment(eComment, iData1, iData2, MyCvDLLUtility::getInstance().getTextGeneric(szString, formatTextArgs));
}
bool CvDiplomacyScreen::isWarDiplo() const
{
	return ::atWar(mSides[kSideAI].teamI, mSides[kSideActive].teamI);
}
void CvDiplomacyScreen::clearUserComments()
{
	mUI->clearUserComments();
}
void CvDiplomacyScreen::endDiplomacy()
{
	setWantClose(true);
}
bool CvDiplomacyScreen::counterPropose()
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
				if (mSides[i].addToOffer(item))
					mUI->addOfferTradeItem(i, item, getTradeItemLabel(mSides[i].playerI, mSides[1 - i].playerI, item, true, false));

				// AI_counterPropose ignores dual requirement
				// My Game turn 248 AD-1690 incomplete peace treaty.CivBeyondSwordSave
				if (CvDeal::isDual(item.m_eItemType))
				{
					const int j = 1 - i;
					if (mSides[j].addToOffer(item))
						mUI->addOfferTradeItem(j, item, getTradeItemLabel(mSides[j].playerI, mSides[1 - j].playerI, item, true, false));
				}
			}
		}
		updateTradeInventories();
		updateTradeInventoriesUI();
		mIsOfferFromAI = true;
		return true;
	}
	else
		return false;
}
void CvDiplomacyScreen::declareWar()
{
	auto info = std::make_unique<CvPopupInfo>(BUTTONPOPUP_DECLAREWARMOVE, mSides[kSideAI].playerI, INT_MIN, INT_MIN, 1, false, false);
	auto popup = std::make_unique<CvButtonPopup>(std::move(info));
	CvButtonPopup::launch(std::move(popup));
}
void CvDiplomacyScreen::sendDiploEvent(DiploEventTypes iDiploEvent, int iData1, int iData2)
{
	// TODO: Is this all that happens? Check when USER_DIPLOCOMMENT_ASK.
	GET_PLAYER(mSides[kSideAI].playerI).handleDiploEvent(iDiploEvent, mSides[kSideActive].playerI, iData1, iData2);
	// Update UI after cancelling deals.
	CvInterface::getInstance().onGameStateChanged(CvInterface::EGameStateChangeReason::DiploEvent);
}

bool CvDiplomacyScreen::hasAnnualDeal() const
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
void CvDiplomacyScreen::implementDeal()
{
	gGlobals.getGame().implementDeal(mSides[kSideActive].playerI, mSides[kSideAI].playerI, &mSides[kSideActive].offer, &mSides[kSideAI].offer, false);
	CvInterface::getInstance().onGameStateChanged(CvInterface::EGameStateChangeReason::Deal);

}
bool CvDiplomacyScreen::isAIOffer() const
{
	return mIsOfferFromAI;
}
bool CvDiplomacyScreen::offerDeal()
{
	if (GET_PLAYER(mSides[kSideAI].playerI).AI_considerOffer(mSides[kSideActive].playerI, &mSides[kSideActive].offer, &mSides[kSideAI].offer, 1))
	{
		implementDeal();
		return true;
	}
	else
		return false;
}
bool CvDiplomacyScreen::isOurOfferEmpty() const
{
	return mSides[kSideActive].offer.getLength() <= 0;
}
bool CvDiplomacyScreen::isTheirOfferEmpty() const
{
	return mSides[kSideAI].offer.getLength() <= 0;
}
void CvDiplomacyScreen::performHeadAction(LeaderheadAction eAction)
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

	mUI->setLeaderheadText(L'[' + std::wstring(GET_PLAYER(mSides[kSideAI].playerI).getName()) + L"'s " + kLabels.at(eAction) + L" face]");
}
void CvDiplomacyScreen::setAIComment(DiploCommentTypes iComment)
{
	mCurrentAIDiploComment = iComment;
	gGlobals.getGame().handleDiplomacySetAIComment(iComment);
}
void CvDiplomacyScreen::setIsAIOffer(bool bOffer)
{
	mIsOfferFromAI = bOffer;
}
void CvDiplomacyScreen::setAIString(std::wstring text)
{
	mUI->setAICommentText(std::move(text));
}
void CvDiplomacyScreen::setShowAllTrade(bool bShow)
{
	mIsShowAllTrade = bShow;

	mUI->setInventoriesVisible(bShow);
}

void CvDiplomacyScreen::startTrade(bool bRenegotiate)
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

	const bool isShowCurrent = !mIsShowAllTrade;

	// "isShowCurrent" is for when showing current deals only, not deal renegotiation.
	const auto getTradeItemLabels = [isShowCurrent](PlayerTypes ownerPlayerI, PlayerTypes otherPlayerI, const CLinkList<TradeData>& items, bool isOffer) {
		return std::vector<std::wstring>(std::from_range, viewCLinkList(items) | std::views::transform([&](TradeData item) {
			return getTradeItemLabel(ownerPlayerI, otherPlayerI, item, isOffer, isShowCurrent);
			}));
		};

	mUI->startTrade();

	for (int i = 0; i < 2; ++i)
	{
		auto& us = mSides[i];
		auto& them = mSides[1 - i];
		mUI->setTradeInventory(i, us.inventory, getTradeItemLabels(us.playerI, them.playerI, us.inventory, false));
		mUI->setTradeOffer(i, us.offer, getTradeItemLabels(us.playerI, them.playerI, us.offer, true), mSides[i].dealIds);
	}

	updateTradeInventoriesUI();
}

void CvDiplomacyScreen::endTrade()
{
	mUI->endTrade();
	mIsTradeUIActive = false;
	for (auto& side : mSides)
	{
		for (TradeData& item : viewCLinkList(side.inventory))
			item.m_bOffering = false;
		side.offer.clear();
	}
}

bool CvDiplomacyScreen::isTheirOfferAVassalTribute()
{
	// The diplo text changes to "Time for your tribute" when specifically demanding resources only.
	return isOurOfferEmpty() && CvDeal::isVassalTributeDeal(&mSides[kSideAI].offer);
}

void CvDiplomacyScreen::regreet()
{
	endTrade();
	startDiplo(true);
}

bool CvDiplomacyScreen::isDiplomacyActive() const
{
	return true;
}

PlayerTypes CvDiplomacyScreen::getDiplomacyAIPlayer() const
{
	return mSides[kSideAI].playerI;
}

void CvDiplomacyScreen::onClickUserComment(DiploCommentTypes eComment, int iData1, int iData2)
{
	CyArgsList args;
	args.add(static_cast<int>(eComment));
	args.add(iData1);
	args.add(iData2);
	(void)MyCvDLLPython().callFunction(PYDiplomacyModule, "handleUserResponse", args.makeFunctionArgs());
}

static TradeData* findMatchingTradeItem(CLinkList<TradeData>& list, TradeData query)
{
	for (TradeData& item : viewCLinkList(list))
		if (CvDiplomacyScreenUI::isMatchingTradeItem(item, query))
			return &item;
	return nullptr;
}

static bool hasDualDenial(PlayerTypes playerA, PlayerTypes playerB, TradeData item)
{
	return GET_PLAYER(playerA).getTradeDenial(playerB, item) != NO_DENIAL || GET_PLAYER(playerB).getTradeDenial(playerA, item) != NO_DENIAL;
}

void CvDiplomacyScreen::onClickTradeItem(TradeData item, bool isOffer, bool isAIPlayer, int dealId)
{
	const int sideI = !isAIPlayer;
	const int otherSideI = isAIPlayer;
	auto& side = mSides[sideI];
	auto& otherSide = mSides[otherSideI];

	if (isOffer && dealId >= 0)
	{
		auto info = std::make_unique<CvPopupInfo>(ButtonPopupTypes::BUTTONPOPUP_DEAL_CANCELED, dealId, -1, -1, 0, true);
		auto popup = std::make_unique<CvButtonPopup>(std::move(info));
		CvButtonPopup::launch(std::move(popup));
	}
	else if (TradeData* const invItem = findMatchingTradeItem(side.inventory, item))
	{
		const bool isAddingToOffer = !isOffer;

		if (isAddingToOffer)
		{
			if (!CvDeal::isEndWar(invItem->m_eItemType))
			{
				if (!dealToEndWar())
					return;
			}

			if (isWarDiplo())
			{
				const bool side0Gifting = mSides[0].hasOfferIgnoringPeaceTreaty() || (sideI == 0 && invItem->m_eItemType != CvDeal::getPeaceItem());
				const bool side1Gifting = mSides[1].hasOfferIgnoringPeaceTreaty() || (sideI == 1 && invItem->m_eItemType != CvDeal::getPeaceItem());
				if (side0Gifting && side1Gifting)
				{
					showPeaceDealError();
					return;
				}
			}

			TradeData offerItem = *invItem;
			if (CvDeal::isGold(offerItem.m_eItemType))
			{
				const bool isGpt = CvDeal::getGoldPerTurnItem() == offerItem.m_eItemType;
				const int max = isGpt
					? GET_PLAYER(mSides[sideI].playerI).AI_maxGoldPerTurnTrade(mSides[otherSideI].playerI)
					: GET_PLAYER(mSides[sideI].playerI).AI_maxGoldTrade(mSides[otherSideI].playerI);

				auto popup = std::make_unique<InternalPopup>();
				popup->headerString = MyCvDLLUtility::getInstance().getTextGeneric(L"TXT_KEY_TRADE_TITLE_GOLD", {});
				popup->bodyString = MyCvDLLUtility::getInstance().getTextGeneric(
					isAIPlayer
					? isGpt ? L"TXT_KEY_TRADE_GPT_FROM_THEM" : L"TXT_KEY_TRADE_GOLD_FROM_THEM"
					: isGpt ? L"TXT_KEY_TRADE_GPT_TO_OFFER" : L"TXT_KEY_TRADE_GOLD_TO_OFFER",
					{});
				popup->controls.push_back(CvPopup::Control{
					.type = CvPopup::EControlType::SpinBox,
					.text = std::to_wstring(max),
					.spinBoxMax = max,
					});
				popup->controls.push_back(CvPopup::Control{
					.type = CvPopup::EControlType::Button,
					.text = MyCvDLLUtility::getInstance().getTextGeneric(L"TXT_KEY_MAIN_MENU_OK", {}),
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
			else if (CvDeal::isDual(offerItem.m_eItemType))
			{
				if (hasDualDenial(side.playerI, otherSide.playerI, offerItem))
					return;

				// We need the other guy to offer it too.
				TradeData* const otherGuyInvItem = findMatchingTradeItem(otherSide.inventory, item);
				if (!otherGuyInvItem)
					return; // ???

				if (!otherGuyInvItem->m_bOffering)
				{
					otherGuyInvItem->m_bOffering = true;
					if (otherSide.addToOffer(*otherGuyInvItem))
					{
						mUI->addOfferTradeItem(otherSideI, *otherGuyInvItem, getTradeItemLabel(otherSide.playerI, side.playerI, *otherGuyInvItem, true, false));
						mIsOfferFromAI = false;
					}
				}
			}

			if (side.addToOffer(offerItem))
			{
				mUI->addOfferTradeItem(sideI, offerItem, getTradeItemLabel(side.playerI, otherSide.playerI, offerItem, true, false));
				mIsOfferFromAI = false;
			}
		}
		else
		{
			if (CvDeal::isDual(invItem->m_eItemType))
			{
				if (TradeData* const otherGuyInvItem = findMatchingTradeItem(otherSide.inventory, item))
					otherGuyInvItem->m_bOffering = false;
				if (otherSide.removeFromOffer(*invItem))
					mIsOfferFromAI = false;
				mUI->removeOfferTradeItem(otherSideI, *invItem);
			}

			if (side.removeFromOffer(*invItem))
				mIsOfferFromAI = false;
			mUI->removeOfferTradeItem(sideI, *invItem);
		}

		updateTradeInventories();
		updateTradeInventoriesUI();
		refreshDiplo();
	}
}

bool CvDiplomacyScreen::isTradeScreenActive() const
{
	return mIsTradeUIActive;
}

const CvDiploParameters& CvDiplomacyScreen::getDiploParams() const
{
	return *mDiploParams;
}



void CvDiplomacyScreen::rebuildPythonScreen()
{
}

void CvDiplomacyScreen::updateFromGameState(hecktui::Window& wnd)
{
	if (mUI)
		mUI->updateFromGameState(*this);
	
	CvWString text;
	CvGameTextMgr::GetInstance().getTradeScreenHeader(text, mSides[kSideAI].playerI, mSides[kSideActive].playerI, true);
	wnd.setWindowTitle(std::move(text));
}

std::shared_ptr<hecktui::Window> CvDiplomacyScreen::createTuiWindow(bool passInput) const
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

				const int widthFraction = gGlobals.getDiplomacyScreen()->isTradeScreenActive() ? 90 : 50;
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

bool CvDiplomacyScreen::Side::addToOffer(TradeData item)
{
	// Mark as offered.
	for (TradeData& invItem : viewCLinkList(inventory))
	{
		if (CvDiplomacyScreenUI::isMatchingTradeItem(invItem, item))
		{
			invItem.m_bOffering = true;
			break;
		}
	}

	// Add to offer list if not already in it.
	if (!std::ranges::any_of(viewCLinkList(offer), std::bind_back(CvDiplomacyScreenUI::isMatchingTradeItem, item)))
	{
		offer.insertAtEnd(item);
		return true;
	}
	else
		return false;
}

bool CvDiplomacyScreen::Side::removeFromOffer(TradeData item)
{
	for (TradeData& invItem : viewCLinkList(inventory))
	{
		if (CvDiplomacyScreenUI::isMatchingTradeItem(invItem, item))
		{
			invItem.m_bOffering = false;
			break;
		}
	}

	if (auto* const node = std::ranges::find_if(viewCLinkList(offer), std::bind_back(CvDiplomacyScreenUI::isMatchingTradeItem, item)).node)
	{
		offer.deleteNode(node);
		return true;
	}
	else
		return false;
}

bool CvDiplomacyScreen::Side::hasOfferIgnoringPeaceTreaty() const
{
	return !std::ranges::empty(viewCLinkList(offer) | std::views::filter([](TradeData item) { return item.m_eItemType != CvDeal::getPeaceItem(); }));
}

bool CvDiplomacyScreen::canOffer(ESide sideI, TradeData item) const
{
	const auto view = viewCLinkList(mSides[sideI].inventory);
	const auto it = std::ranges::find_if(view, std::bind_back(CvDiplomacyScreenUI::isMatchingTradeItem, item));
	if (it != view.end())
		return !it->m_bHidden && GET_PLAYER(mSides[sideI].playerI).getTradeDenial(mSides[1 - sideI].playerI, item) == NO_DENIAL;
	return false;
}

bool CvDiplomacyScreen::dealToEndWar()
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
			if (mSides[i].addToOffer(defPeaceItem))
				mUI->addOfferTradeItem(i, defPeaceItem, getTradeItemLabel(mSides[i].playerI, mSides[1 - i].playerI, defPeaceItem, true, false));
		return true;
	}

	return false;
}

void CvDiplomacyScreen::refreshDiplo()
{
	CyArgsList args;
	args.add(static_cast<int>(mCurrentAIDiploComment));
	(void)MyCvDLLPython().callFunction(PYDiplomacyModule, "refresh", args.makeFunctionArgs());
}

void CvDiplomacyScreen::showPeaceDealError()
{
	auto popup = std::make_unique<InternalPopup>();
	popup->bodyString = MyCvDLLUtility::getInstance().getTextGeneric(L"TXT_KEY_PEACE_ERROR", {});
	popup->controls.push_back(CvPopup::Control{
		.type = CvPopup::EControlType::Button,
		.text = MyCvDLLUtility::getInstance().getTextGeneric(L"TXT_KEY_MAIN_MENU_OK", {}),
	});
	popup->optEnterSubmitBtnId = 0;
	popup->enableEscCancel = true;
	(void)InternalPopup::launchModal(std::move(popup));
}

void CvDiplomacyScreen::updateTradeInventories()
{
	for (int i = 0; i < 2; ++i)
		GET_PLAYER(mSides[i].playerI).updateTradeList(mSides[1 - i].playerI, mSides[i].inventory, mSides[i].offer, mSides[1 - i].offer);
}

void CvDiplomacyScreen::updateTradeInventoriesUI()
{
	for (int i = 0; i < 2; ++i)
	{
		auto& us = mSides[i];
		auto& them = mSides[1 - i];

		//GET_PLAYER(us.playerI).updateTradeList(them.playerI, us.inventory, us.offer, them.offer);
		mUI->updateTradeInventory(i, { std::from_range, viewCLinkList(us.inventory) | std::views::transform([&](TradeData item) {
			return CvDiplomacyScreenUI::DynamicTradeProps{
				.denial = GET_PLAYER(us.playerI).getTradeDenial(them.playerI, item) != NO_DENIAL,
				.hidden = item.m_bHidden || item.m_bOffering,
			};
			}) });
	}

	const TradeData peaceItem{ .m_eItemType = CvDeal::getPeaceItem(), .m_iData = -1, .m_bOffering{}, .m_bHidden{} };
	mUI->setCeaseFireVisible(isWarDiplo() && (!findMatchingTradeItem(mSides[0].offer, peaceItem) || !findMatchingTradeItem(mSides[1].offer, peaceItem)));
}

CvDiplomacyScreen::~CvDiplomacyScreen()
{
	assert(gGlobals.getDiplomacyScreen() == this);
	gGlobals.setDiplomacyScreen(nullptr);
}