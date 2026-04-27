#pragma once

#include <CvEnums.h>
#include <CvStructs.h>

#include <pybind11/pybind11.h>

class CyDiplomacy
{
public:
	static void registerWithPython(const pybind11::module& m);

	void addUserComment(DiploCommentTypes eComment, int iData1, int iData2, const std::wstring& szString, const std::vector<std::wstring>& txtArgs);
	bool atWar();
	void clearUserComments();
	void closeScreen();
	bool counterPropose();
	void declareWar();
	void diploEvent(DiploEventTypes iDiploEvent, int iData1, int iData2);
	void endTrade();
	int getData();
	std::string getOpponentCivName();
	std::string getOpponentName();
	std::string getOurCivName();
	std::string getOurName();
	int getOurScore();
	std::optional<TradeData> getPlayerTradeOffer(int iIndex);
	int getTheirScore();
	std::optional<TradeData> getTheirTradeOffer(int iIndex);
	int getWhoTradingWith();
	bool hasAnnualDeal();
	void implementDeal();
	bool isAIOffer();
	bool isSeparateTeams();
	void makePeace();
	bool offerDeal();
	bool ourOfferEmpty();
	void performHeadAction(LeaderheadAction eAction);
	void setAIComment(int iComment);
	void setAIOffer(bool bOffer);
	void setAIString(const std::wstring& szString, const std::vector<std::wstring>& txtArgs);
	void showAllTrade(bool bShow);
	void startTrade(int iComment, bool bRenegotiate);
	bool theirOfferEmpty();
	bool theirVassalTribute();
};