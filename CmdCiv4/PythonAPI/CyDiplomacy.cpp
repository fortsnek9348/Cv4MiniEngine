#include "CyDiplomacy.h"
#include "../Common.h"
#include "../CvInterface.h"
#include "../DLLInterface/MyCvDLLUtility.h"
#include "../TuiTextCode.h"

#include <CvGlobals.h>
#include <CvDiplomacyScreen.h>
#include <CvDiploParameters.h>

#include <pybind11/stl.h>

static CvDiplomacyController& getDiplo()
{
	//return CvInterface::getInstance().tryGetActiveDiplomacy();
	return gGlobals.getDiplomacyScreen()->getController();
}

void CyDiplomacy::registerWithPython(const pybind11::module& m)
{
#define R(x) def(#x, &CyDiplomacy::x)

	pybind11::class_<CyDiplomacy>(m, "CyDiplomacy")
		.def(pybind11::init())
		.R(addUserComment)
		.R(atWar)
		.R(clearUserComments)
		.R(closeScreen)
		.R(counterPropose)
		.R(declareWar)
		.R(diploEvent)
		.R(endTrade)
		.R(getData)
		.R(getOpponentCivName)
		.R(getOpponentName)
		.R(getOurCivName)
		.R(getOurName)
		.R(getOurScore)
		.R(getPlayerTradeOffer)
		.R(getTheirScore)
		.R(getTheirTradeOffer)
		.R(getWhoTradingWith)
		.R(hasAnnualDeal)
		.R(implementDeal)
		.R(isAIOffer)
		.R(isSeparateTeams)
		.R(makePeace)
		.R(offerDeal)
		.R(ourOfferEmpty)
		.R(performHeadAction)
		.R(setAIComment)
		.R(setAIOffer)
		.R(setAIString)
		.R(showAllTrade)
		.R(startTrade)
		.R(theirOfferEmpty)
		.R(theirVassalTribute)
		;
}

void CyDiplomacy::addUserComment(DiploCommentTypes eComment, int iData1, int iData2, const std::wstring& szString, const std::vector<std::wstring>& txtArgs)
{
	getDiplo().addUserComment(eComment, iData1, iData2, szString, txtArgs);
}

bool CyDiplomacy::atWar()
{
	return getDiplo().isWarDiplo();
}

void CyDiplomacy::clearUserComments()
{
	getDiplo().clearUserComments();
}

void CyDiplomacy::closeScreen()
{
	getDiplo().endDiplomacy();
}

bool CyDiplomacy::counterPropose()
{
	return getDiplo().counterPropose();
}

void CyDiplomacy::declareWar()
{
	return getDiplo().declareWar();
}

void CyDiplomacy::diploEvent(DiploEventTypes iDiploEvent, int iData1, int iData2)
{
	return getDiplo().sendDiploEvent(iDiploEvent, iData1, iData2);
}

void CyDiplomacy::endTrade()
{
	getDiplo().endTrade();
}

int CyDiplomacy::getData()
{
	return getDiplo().getDiploParams().getData();
}

std::string CyDiplomacy::getOpponentCivName()
{
	std::abort();
}

std::string CyDiplomacy::getOpponentName()
{
	std::abort();
}

std::string CyDiplomacy::getOurCivName()
{
	std::abort();
}

std::string CyDiplomacy::getOurName()
{
	std::abort();
}

int CyDiplomacy::getOurScore()
{
	std::abort();
}

std::optional<TradeData> CyDiplomacy::getPlayerTradeOffer(int iIndex)
{
	// Called by BUG for various AI comments.
	// Assuming the initial offer.
	if (auto* const node = getDiplo().getDiploParams().getOurOfferList().nodeNum(iIndex))
		return node->m_data;
	else
		return std::nullopt;
}

int CyDiplomacy::getTheirScore()
{
	std::abort();
}

std::optional<TradeData> CyDiplomacy::getTheirTradeOffer(int iIndex)
{
	// Called by BUG for various AI comments.
	// Assuming the initial offer.
	if (auto* const node = getDiplo().getDiploParams().getTheirOfferList().nodeNum(iIndex))
		return node->m_data;
	else
		return std::nullopt;
}

// Must return int. BUG expects it.
int CyDiplomacy::getWhoTradingWith()
{
	return getDiplo().getDiploParams().getWhoTalkingTo();
}

bool CyDiplomacy::hasAnnualDeal()
{
	return getDiplo().hasAnnualDeal();
}

void CyDiplomacy::implementDeal()
{
	getDiplo().implementDeal();
}

bool CyDiplomacy::isAIOffer()
{
	return getDiplo().isAIOffer();
}

bool CyDiplomacy::isSeparateTeams()
{
	std::abort();
}

void CyDiplomacy::makePeace()
{
	std::abort();
}

bool CyDiplomacy::offerDeal()
{
	return getDiplo().offerDeal();
}

bool CyDiplomacy::ourOfferEmpty()
{
	return getDiplo().isOurOfferEmpty();
}

void CyDiplomacy::performHeadAction(LeaderheadAction eAction)
{
	getDiplo().performHeadAction(eAction);
}

void CyDiplomacy::setAIComment(int iComment)
{
	getDiplo().setAIComment((DiploCommentTypes)iComment);
}

void CyDiplomacy::setAIOffer(bool bOffer)
{
	getDiplo().setIsAIOffer(bOffer);
}

void CyDiplomacy::setAIString(const std::wstring& szString, const std::vector<std::wstring>& txtArgs)
{
	// TODO: txtArgs can come from beginPythonDiplomacy. That's the types it contains. See if these args are used anywhere.
	return getDiplo().setAIString(MyCvDLLUtility::getInstance().getTextGeneric(szString, std::vector<CvDLLUtilityIFaceBase::TextArg>(std::from_range, txtArgs)));
}

void CyDiplomacy::showAllTrade(bool bShow)
{
	getDiplo().setShowAllTrade(bShow);
}

void CyDiplomacy::startTrade(int iComment, bool bRenegotiate)
{
	getDiplo().setAIComment(DiploCommentTypes(iComment));
	getDiplo().startTrade(bRenegotiate);
}

bool CyDiplomacy::theirOfferEmpty()
{
	return getDiplo().isTheirOfferEmpty();
}

bool CyDiplomacy::theirVassalTribute()
{
	return getDiplo().isTheirOfferAVassalTribute();
}
