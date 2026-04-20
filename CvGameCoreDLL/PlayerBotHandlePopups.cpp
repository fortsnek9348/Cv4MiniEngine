#include "PlayerBotEnablement.h"

#if ENABLE_PLAYER_BOT

#include "PlayerBotUtil.h"
#include "PlayerBotGameInterface.h"

#include "CvPlayerAI.h"
#include "CvDiplomacyScreen.h"
#include "CvGlobals.h"
#include "CvPopupInfo.h"
#include "CvDLLButtonPopup.h"
#include "CvGameAI.h"
#include "CvInfos.h"
#include "CvMap.h"
#include "CvMessageControl.h"
#include "CvDiploParameters.h"

#include <PlayerBotGameBinding/IPlayerBot.h>
#include <PlayerBotGameBinding/GameStructs.h>

#include <CommonStuff/range.h>

#include <algorithm>
#include <ranges>

using namespace cvbot;

namespace
{
	struct PlayerBotDiplomacyScreen : CvDiplomacyScreen
	{
		using CvDiplomacyScreen::CvDiplomacyScreen;
	};

	void handleDiplo(CvPlayerAI& player, IBot& bot, std::unique_ptr<CvDiploParameters> diploParams)
	{
		//static int nesting = 0;
		//assert(nesting++ == 0);

		PlayerBotDiplomacyScreen diploScreen{ std::move(diploParams) };
		gGlobals.setDiplomacyScreen(&diploScreen);

		CvDiplomacyController& controller = diploScreen.getController();

		CvPlayerAI& them = GET_PLAYER(controller.getDiplomacyAIPlayer());

		const Game& game = Game::getInstance();

		struct DiploChoice
		{
			DiploCommentTypes type{};
			// Pretty much always -1.
			int data1 = -1;
			int data2 = -1;
		};

		while (!controller.isEndDiplomacy())
		{
			TradeList offerImports;
			TradeList offerExports;

			if (controller.isTradeScreenActive())
			{
				offerImports = convertTradeList(controller.getSide(CvDiplomacyController::kSideAI).offer, them, player);
				offerExports = convertTradeList(controller.getSide(CvDiplomacyController::kSideActive).offer, player, them);
			}

			const auto choices = controller.getUserComments()
				| std::views::transform([](const CvDiplomacyController::UserComment& c) { return DiploChoice{ c.type, c.data1, c.data2 }; })
				| std::ranges::to<std::vector>()
				;

			const DiploCommentTypes aiCommentType = controller.getAICommentType();

			// Only a hard-coded AI comment should be in diploParams as it was created by AI code.
			// So, see which one it is.
			// AI_DIPLOCOMMENT_OFFER_PEACE
			// AI_DIPLOCOMMENT_NO_VASSAL - trade cancelled, you are too weak
			// AI_DIPLOCOMMENT_CANCEL_DEAL - negotiate
			// AI_DIPLOCOMMENT_GIVE_HELP - help for human player
			// AI_DIPLOCOMMENT_OFFER_CITY - gift city to human player
			// AI_DIPLOCOMMENT_OFFER_DEAL
			// AI_DIPLOCOMMENT_OFFER_VASSAL
			// AI_DIPLOCOMMENT_RELIGION_PRESSURE - demand human player join religion
			// AI_DIPLOCOMMENT_CIVIC_PRESSURE - demand human player adopt civic
			// AI_DIPLOCOMMENT_JOIN_WAR - demand human player join war
			// AI_DIPLOCOMMENT_STOP_TRADING - demand human player to stop trading
			// AI_DIPLOCOMMENT_ASK_FOR_HELP - ask for help from human player, with offer
			// AI_DIPLOCOMMENT_DEMAND_TRIBUTE - tribute from human player
			// AI_DIPLOCOMMENT_DECLARE_WAR - AI declares war on you
			// AI_DIPLOCOMMENT_FIRST_CONTACT
			// Note that we should still go through onClickUserComment to let script call diplo events.

			const EPlayer aiPlayerI = static_cast<EPlayer>(controller.getDiplomacyAIPlayer());

			const char* choiceName{};

			if (aiCommentType == gGlobals.getInfoTypeForString("AI_DIPLOCOMMENT_OFFER_PEACE")
				|| aiCommentType == gGlobals.getInfoTypeForString("AI_DIPLOCOMMENT_OFFER_DEAL") || aiCommentType == gGlobals.getInfoTypeForString("AI_DIPLOCOMMENT_OFFER_VASSAL"))
			{
				// Ignore. Let bot make trade.
				choiceName = "USER_DIPLOCOMMENT_REJECT_OFFER";
			}
			else if (aiCommentType == gGlobals.getInfoTypeForString("AI_DIPLOCOMMENT_CANCEL_DEAL"))
			{
				// Ignore. Let bot make new trade.
				choiceName = "USER_DIPLOCOMMENT_NO_RENEGOTIATE";
			}
			else if (aiCommentType == gGlobals.getInfoTypeForString("AI_DIPLOCOMMENT_GIVE_HELP") || aiCommentType == gGlobals.getInfoTypeForString("AI_DIPLOCOMMENT_OFFER_CITY"))
			{
				if (bot.handleDiploGift(game, aiPlayerI, offerImports))
					choiceName = "USER_DIPLOCOMMENT_ACCEPT_OFFER";
				else
					choiceName = "USER_DIPLOCOMMENT_REJECT_OFFER";
			}
			else if (aiCommentType == gGlobals.getInfoTypeForString("AI_DIPLOCOMMENT_ASK_FOR_HELP"))
			{
				if (bot.handleDiploHelp(game, aiPlayerI, offerExports))
					choiceName = "USER_DIPLOCOMMENT_GIVE_HELP";
				else
					choiceName = "USER_DIPLOCOMMENT_REFUSE_HELP";
			}
			else if (aiCommentType == gGlobals.getInfoTypeForString("AI_DIPLOCOMMENT_DEMAND_TRIBUTE"))
			{
				if (bot.handleDiploDemandTribute(game, aiPlayerI, offerExports))
					choiceName = "USER_DIPLOCOMMENT_ACCEPT_DEMAND";
				else
					choiceName = "USER_DIPLOCOMMENT_REJECT_DEMAND";
			}
			else if (aiCommentType == gGlobals.getInfoTypeForString("AI_DIPLOCOMMENT_RELIGION_PRESSURE"))
			{
				if (bot.handleDiploReligionPressure(game, aiPlayerI, static_cast<EReligion>(them.getStateReligion())))
					choiceName = "USER_DIPLOCOMMENT_CONVERT";
				else
					choiceName = "USER_DIPLOCOMMENT_NO_CONVERT";
			}
			else if (aiCommentType == gGlobals.getInfoTypeForString("AI_DIPLOCOMMENT_CIVIC_PRESSURE"))
			{
				// The civic that the diplo event picks.
				const CivicTypes civic = static_cast<CivicTypes>(gGlobals.getLeaderHeadInfo(them.getPersonalityType()).getFavoriteCivic());
				const auto type = gGlobals.getCivicInfo(civic).getCivicOptionType();
				if (bot.handleDiploCivicPressure(game, aiPlayerI, static_cast<ECivicOptionType>(type), static_cast<ECivic>(civic)))
					choiceName = "USER_DIPLOCOMMENT_REVOLUTION";
				else
					choiceName = "USER_DIPLOCOMMENT_NO_REVOLUTION";
			}
			else if (aiCommentType == gGlobals.getInfoTypeForString("AI_DIPLOCOMMENT_JOIN_WAR"))
			{
				if (bot.handleDiploJoinWar(game, aiPlayerI, static_cast<ETeam>(diploScreen.getController().getDiploParams().getData())))
					choiceName = "USER_DIPLOCOMMENT_JOIN_WAR";
				else
					choiceName = "USER_DIPLOCOMMENT_NO_JOIN_WAR";
			}
			else if (aiCommentType == gGlobals.getInfoTypeForString("AI_DIPLOCOMMENT_STOP_TRADING"))
			{
				if (bot.handleDiploStopTrading(game, aiPlayerI, static_cast<ETeam>(diploScreen.getController().getDiploParams().getData())))
					choiceName = "USER_DIPLOCOMMENT_STOP_TRADING";
				else
					choiceName = "USER_DIPLOCOMMENT_NO_STOP_TRADING";
			}
			else if (aiCommentType == gGlobals.getInfoTypeForString("AI_DIPLOCOMMENT_DECLARE_WAR"))
			{
				bot.handleDiploWarDeclaration(game, static_cast<ETeam>(them.getTeam()));
				choiceName = "USER_DIPLOCOMMENT_WAR_RESPONSE";
			}
			else if (aiCommentType == gGlobals.getInfoTypeForString("AI_DIPLOCOMMENT_FIRST_CONTACT")
				|| aiCommentType == gGlobals.getInfoTypeForString("AI_DIPLOCOMMENT_NO_VASSAL"))
			{
				bot.handleDiploFirstContact(game, aiPlayerI);
				choiceName = "USER_DIPLOCOMMENT_PEACE";
			}
			else if (aiCommentType == gGlobals.getInfoTypeForString("AI_DIPLOCOMMENT_PEACE")
				|| aiCommentType == gGlobals.getInfoTypeForString("AI_DIPLOCOMMENT_THANKS")
				|| aiCommentType == gGlobals.getInfoTypeForString("AI_DIPLOCOMMENT_CIVIC_DENIED")
				|| aiCommentType == gGlobals.getInfoTypeForString("AI_DIPLOCOMMENT_JOIN_DENIED"))
			{
				choiceName = "USER_DIPLOCOMMENT_EXIT";
			}
			else
				throw BotFailure("Unhandled diplo.");

			const DiploChoice choice{
				.type = static_cast<DiploCommentTypes>(gGlobals.getInfoTypeForString(choiceName)),
			};

			// Sanity check the choice.
			if (!std::ranges::contains(choices, choice.type, &DiploChoice::type))
				throw BotFailure("Diplo failure. Chosen response not in user comments list.");

			//const DiploChoice choice = bot.handleDiplo(
			//	Game::getInstance(),
			//	aiPlayerI,
			//	static_cast<EDiploComment>(aiCommentType),
			//	choices,
			//	offerImports,
			//	offerExports
			//);
			
			controller.onClickUserComment(static_cast<DiploCommentTypes>(choice.type), choice.data1, choice.data2);
		}

		//--nesting;
	}

	PopupReturn makePopupReturnButtonClicked(int groupId, int value)
	{
		PopupReturn r;
		r.setButtonClicked(value, groupId);
		return r;
	}

	void handlePopupInfo(CvPlayerAI& player, IBot& bot, std::unique_ptr<CvPopupInfo> info)
	{
		if (!CvDLLButtonPopup::getInstance().OnHeadlessFocus(*info))
			return; // Popup cancelled.

		const IGame& game = Game::getInstance();

		switch (info->getButtonPopupType())
		{
		case BUTTONPOPUP_TEXT:
		case BUTTONPOPUP_MAIN_MENU:
			break;
		
		
		case BUTTONPOPUP_DECLAREWARMOVE:
		{
			// No, don't declare war.
			PopupReturn ret = makePopupReturnButtonClicked(0, 1);
			CvDLLButtonPopup::getInstance().OnOkClicked(nullptr, &ret, *info);
			break;
		}

		case BUTTONPOPUP_CONFIRMCOMMAND:
		{
			// Yes, do it.
			PopupReturn ret = makePopupReturnButtonClicked(0, 0);
			CvDLLButtonPopup::getInstance().OnOkClicked(nullptr, &ret, *info);
			break;
		}
		
		case BUTTONPOPUP_CHOOSETECH:
			// Ignore.
			break;

		case BUTTONPOPUP_RAZECITY:
		{
			const CvCity& city = *player.getCity(info->getData1());
			const auto choice = bot.handleCityCaptureChoice(game, { static_cast<int16_t>(city.getX_INLINE()), static_cast<int16_t>(city.getY_INLINE()) });
			PopupReturn ret = makePopupReturnButtonClicked(0, static_cast<int>(choice));
			CvDLLButtonPopup::getInstance().OnOkClicked(nullptr, &ret, *info);
			break;
		}

		case BUTTONPOPUP_DISBANDCITY:
		{
			const CvCity& city = *player.getCity(info->getData1());
			const auto choice = bot.handleCityAcquireChoice(game, { static_cast<int16_t>(city.getX_INLINE()), static_cast<int16_t>(city.getY_INLINE()) });
			PopupReturn ret = makePopupReturnButtonClicked(0, static_cast<int>(choice));
			CvDLLButtonPopup::getInstance().OnOkClicked(nullptr, &ret, *info);
			break;
		}

		case BUTTONPOPUP_CHOOSEPRODUCTION:
			// Ignore.
			break;

		case BUTTONPOPUP_CHANGECIVIC:
		case BUTTONPOPUP_CHANGERELIGION:
			// Ignore.
			break;

		case BUTTONPOPUP_CHOOSEELECTION:
			// Ignore.
			// TODO: Let bot handle this
			break;
		
		case BUTTONPOPUP_DIPLOVOTE:
		{
			// TODO: Let bot handle this
			PopupReturn ret = makePopupReturnButtonClicked(0, PLAYER_VOTE_ABSTAIN);
			CvDLLButtonPopup::getInstance().OnOkClicked(nullptr, &ret, *info);
			break;
		}
			
		case BUTTONPOPUP_ALARM:
			// Ignore.
			break;

		case BUTTONPOPUP_DEAL_CANCELED:
		{
			// Yes, cancel deal.
			PopupReturn ret = makePopupReturnButtonClicked(0, 0);
			CvDLLButtonPopup::getInstance().OnOkClicked(nullptr, &ret, *info);
			break;
		}

		case BUTTONPOPUP_PYTHON:
		case BUTTONPOPUP_PYTHON_SCREEN:
			// Ignore? Mainly used for victory screens.
			break;

		case BUTTONPOPUP_DETAILS:
		case BUTTONPOPUP_ADMIN:
		case BUTTONPOPUP_ADMIN_PASSWORD:
			// Ignore.
			break;

		case BUTTONPOPUP_EXTENDED_GAME:
		{
			// Just one more turn
			PopupReturn ret = makePopupReturnButtonClicked(0, 0);
			CvDLLButtonPopup::getInstance().OnOkClicked(nullptr, &ret, *info);
			break;
		}

		case BUTTONPOPUP_DIPLOMACY:
		case BUTTONPOPUP_ADDBUDDY:
		case BUTTONPOPUP_FORCED_DISCONNECT:
		case BUTTONPOPUP_PITBOSS_DISCONNECT:
		case BUTTONPOPUP_KICKED:
		case BUTTONPOPUP_VASSAL_DEMAND_TRIBUTE:
			// Ignore.
			break;

		case BUTTONPOPUP_EVENT:
		{
			// Copying logic from CvDLLButtonPopup.

			const EventTriggeredData* const pTriggeredData = player.getEventTriggered(info->getData1());
			if (!pTriggeredData)
				break;

			if (pTriggeredData->m_eTrigger == NO_EVENTTRIGGER)
				break;

			const CvEventTriggerInfo& trigger = gGlobals.getEventTriggerInfo(pTriggeredData->m_eTrigger);

			std::vector<EEvent> choices;

			for (const int i : heck::range(trigger.getNumEvents()))
				if (player.canDoEvent(static_cast<EventTypes>(trigger.getEvent(i)), *pTriggeredData))
					choices.push_back(static_cast<EEvent>(trigger.getEvent(i)));

			if (choices.empty())
				break;

			std::optional<i16vec2> optCity;

			if (trigger.isPickCity())
				if (const CvCity* const city = player.getCity(pTriggeredData->m_iCityId))
					optCity.emplace(static_cast<int16_t>(city->getX_INLINE()), static_cast<int16_t>(city->getY_INLINE()));

			std::optional<i16vec2> optCoord;

			if (trigger.isShowPlot())
				if (const CvPlot* const plot = gGlobals.getMapINLINE().plotINLINE(pTriggeredData->m_iPlotX, pTriggeredData->m_iPlotY))
					optCoord.emplace(static_cast<int16_t>(plot->getX_INLINE()), static_cast<int16_t>(plot->getY_INLINE()));

			const EEvent choice = bot.handleRandomEvent(game, static_cast<EEventTrigger>(pTriggeredData->m_eTrigger), optCity, optCoord, choices);

			CvMessageControl::getInstance().sendEventTriggered(GC.getGameINLINE().getActivePlayer(), static_cast<EventTypes>(choice), info->getData1());

			break;
		}


		case BUTTONPOPUP_FREE_COLONY:
			// Ignore.
			break;

		case BUTTONPOPUP_LAUNCH:
			// Ignore.
			// TODO: Need to add launch to IGame.
			break;

		case BUTTONPOPUP_FOUND_RELIGION:
		{
			// Select first choice.
			for (const auto i : heck::range<ReligionTypes>(gGlobals.getNumReligionInfos()))
			{
				if (!gGlobals.getGameINLINE().isReligionFounded(i))
				{
					CvMessageControl::getInstance().sendFoundReligion(GC.getGameINLINE().getActivePlayer(), i, static_cast<ReligionTypes>(info->getData1()));
					break;
				}
			}
			break;
		}

		case BUTTONPOPUP_CONFIRM_MENU: // Launched by BUTTONPOPUP_MAIN_MENU only
		case BUTTONPOPUP_LOADUNIT: // Bot shouldn't trigger. Load into a specific unit only.
			// These are not currently handled. Do they need to be?
		case BUTTONPOPUP_LEADUNIT:
		case BUTTONPOPUP_DOESPIONAGE:
		case BUTTONPOPUP_DOESPIONAGE_TARGET:
		case BUTTONPOPUP_VASSAL_GRANT_TRIBUTE: // This would happen if another player demends tribute from you?
		default:
			throw BotFailure("Unhandled button popup type.");
		}
	}
}

void cvbot::handlePopups(CvPlayerAI& player, IBot& bot)
{
	for (;;)
	{
		if (auto* const diplo = player.popFrontDiplomacy())
		{
			handleDiplo(player, bot, std::unique_ptr<CvDiploParameters>(diplo));
			continue;
		}

		if (std::unique_ptr<CvPopupInfo> info{ player.popFrontPopup() })
		{
			handlePopupInfo(player, bot, std::move(info));
			continue;
		}

		break;
	}
}

#endif