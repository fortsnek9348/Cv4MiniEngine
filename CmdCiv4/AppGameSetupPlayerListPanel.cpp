#include "AppGameSetupPlayerListPanel.h"
#include "AppUIUtil.h"

#include "DLLInterface/MyCvDLLUtility.h"
#include "CvApp.h"

#include <CvGlobals.h>
#include <CvInitCore.h>
#include <CvInfos.h>
#include <CvGameTextMgr.h>

#include <CommonStuff/range.h>

namespace tui = hecktui;
using heck::range;

static bool isHumanPlayer(PlayerTypes playerI)
{
	return std::to_underlying(playerI) == 0;
}

static bool isPlayableCiv(CivilizationTypes civI, PlayerTypes playerI)
{
	const auto& info = gGlobals.getCivilizationInfo(civI);
	return isHumanPlayer(playerI) ? info.isPlayable() : info.isAIPlayable();
}

AppGameSetupPlayerListPanel::AppGameSetupPlayerListPanel(const CvInitCore& initCore) //: ScrollBarPanel(tui::EAxis::Vertical)
{
	buildCivLeaderData();

	Element* const client = this; //getPanel();

	const CvWString teamStr = MyCvDLLUtility::getInstance().getText(L"TXT_KEY_PITBOSS_TEAM");
	const std::wstring randomStr = cvengine::getRandomText();

	mNumPlayers = MAX_CIV_PLAYERS;

	for (const int i : range(MAX_CIV_PLAYERS))
	{
		PlayerRow row{};

		if (i == 0)
			row.name = mTxtPlayerName = std::make_shared<tui::Textbox>(initCore.getLeaderName(static_cast<PlayerTypes>(i)));
		else
			row.name = std::make_shared<tui::Label>(MyCvDLLUtility::getInstance().getText(L"TXT_KEY_MAIN_MENU_AI"));

		client->addChild(row.name);

		row.team = std::make_shared<tui::Combobox>(tui::EComboboxStyle::Compact);
		row.team->setListItems(range(MAX_CIV_TEAMS) | std::views::transform([&](int teamI) { return teamStr + L' ' + std::to_wstring(teamI + 1); })
			| std::ranges::to<std::vector>());
		row.team->setSelectionIndex(initCore.getTeam(static_cast<PlayerTypes>(i)));
		client->addChild(row.team);





		row.leader = std::make_shared<cvengine::MyComboBox>(*this, tui::EComboboxStyle::Compact);
		client->addChild(row.leader);

		row.civ = std::make_shared<cvengine::MyComboBox>(*this, tui::EComboboxStyle::Compact);
		client->addChild(row.civ);

		mPlayers.push_back(row);

		refillLeadersList(static_cast<PlayerTypes>(i));
		refillCivsList(static_cast<PlayerTypes>(i));
	}

	client->setLayout(std::make_unique<tui::TableLayout>(tui::TableLayoutConfig{
		.cols{ {.weight = 2 }, tui::TableLayoutConfig::kAutoSize, {.weight = 1 }, {.weight = 1 } },
		.rows{ tui::TableLayoutConfig::kAutoSize },
		.cells{
			tui::TableLayoutConfig::Cell{.coord{ 0, 0 } },
			tui::TableLayoutConfig::Cell{.coord{ 1, 0 } },
			tui::TableLayoutConfig::Cell{.coord{ 2, 0 } },
			tui::TableLayoutConfig::Cell{.coord{ 3, 0 } }
		},
		.gap{ 1, 0 },
		}));
}

void AppGameSetupPlayerListPanel::onPlayerCountChanged(int n)
{
	mNumPlayers = n;
	for (int i = 0; i < static_cast<int>(mPlayers.size()); ++i)
	{
		const bool visible = i < n;
		mPlayers[i].name->setVisible(visible);
		mPlayers[i].team->setVisible(visible);
		mPlayers[i].leader->setVisible(visible);
		mPlayers[i].civ->setVisible(visible);
	}
}

void AppGameSetupPlayerListPanel::onUnrestrictedLeadersChanged(bool enabled)
{
	mUnrestrictedLeaders = enabled;
	for (const PlayerTypes i : range<PlayerTypes>(MAX_CIV_PLAYERS))
		refillCivsList(i);
}

void AppGameSetupPlayerListPanel::onComboBoxSelectionChanged(tui::Combobox& ctrl)
{
	for (const PlayerTypes i : range<PlayerTypes>(MAX_CIV_PLAYERS))
	{
		if (mPlayers[i].leader.get() == &ctrl)
			refillCivsList(i);
		else if (mPlayers[i].civ.get() == &ctrl)
			refillLeadersList(i);
	}
}

void AppGameSetupPlayerListPanel::setupConfig(SimplifiedInitCore& config) const
{
	config.playerName = mTxtPlayerName->getText();

	for (const int i : range(mNumPlayers))
	{
		config.players.push_back(SimplifiedInitCore::Player{
			.team = static_cast<TeamTypes>(mPlayers[i].team->getSelectionIndex()),
			.leader = mPlayers[i].getSelectedLeader(),
			.civ = mPlayers[i].getSelectedCiv(),
			});
	}
}


LeaderHeadTypes AppGameSetupPlayerListPanel::PlayerRow::getSelectedLeader() const
{
	return leaderListData.empty() ? NO_LEADER : leaderListData[leader->getSelectionIndex()];
}

CivilizationTypes AppGameSetupPlayerListPanel::PlayerRow::getSelectedCiv() const
{
	return civListData.empty() ? NO_CIVILIZATION : civListData[civ->getSelectionIndex()];
}

void AppGameSetupPlayerListPanel::buildCivLeaderData()
{
	std::vector<std::wstring> leaderNames{};
	std::vector<std::wstring> civNames{};
	for (const auto civI : range<CivilizationTypes>(gGlobals.getNumCivilizationInfos()))
	{
		const auto& civ = gGlobals.getCivilizationInfo(civI);
		civNames.push_back(civ.getDescription());
		for (const auto leaderI : range<LeaderHeadTypes>(gGlobals.getNumLeaderHeadInfos()))
		{
			if (civ.isLeaders(leaderI))
			{
				if (gGlobals.getCivilizationInfo(civI).isPlayable())
					mHumanPlayableLeaders.insert(leaderI);
				if (gGlobals.getCivilizationInfo(civI).isAIPlayable())
					mAIPlayableLeaders.insert(leaderI);
			}
		}
	}

	mLeaderLabels.assign_range(range<LeaderHeadTypes>(gGlobals.getNumLeaderHeadInfos()) | std::views::transform(makeLeaderText));
}

std::wstring AppGameSetupPlayerListPanel::makeLeaderText(LeaderHeadTypes leader)
{
	CvWStringBuffer name;
	name.append(gGlobals.getLeaderHeadInfo(leader).getDescription());
	name.append(L" (");
	CvGameTextMgr::GetInstance().parseLeaderShortTraits(name, leader);
	name.append(L")");
	return name.getCString();
}

void AppGameSetupPlayerListPanel::refillLeadersList(PlayerTypes player)
{
	const auto selectedCiv = mPlayers[player].getSelectedCiv();

	std::vector<LeaderHeadTypes> leaders{};

	const auto& playableSet = isHumanPlayer(player) ? mHumanPlayableLeaders : mAIPlayableLeaders;
	for (const auto leaderI : range<LeaderHeadTypes>(gGlobals.getNumLeaderHeadInfos()))
		if (selectedCiv == NO_CIVILIZATION || mUnrestrictedLeaders ? playableSet.contains(leaderI) : gGlobals.getCivilizationInfo(selectedCiv).isLeaders(leaderI))
			leaders.push_back(leaderI);

	std::vector<std::wstring> names(std::from_range, leaders | std::views::transform([this](LeaderHeadTypes i) {
		return mLeaderLabels[i];
		}));

	std::ranges::sort(std::views::zip(names, leaders));

	leaders.insert(leaders.begin(), NO_LEADER);
	names.insert(names.begin(), cvengine::getRandomText());

	tui::Combobox& lst = *mPlayers[player].leader;
	const LeaderHeadTypes selectedLeader = mPlayers[player].leaderListData.size() ? mPlayers[player].leaderListData[lst.getSelectionIndex()] : NO_LEADER;
	lst.setListItems(std::move(names));
	if (const auto it = std::ranges::find(leaders, selectedLeader); it != leaders.end())
		lst.setSelectionIndex(it - leaders.begin());
	else
		lst.setSelectionIndex(0);

	mPlayers[player].leaderListData = std::move(leaders);
}

void AppGameSetupPlayerListPanel::refillCivsList(PlayerTypes player)
{
	const auto selectedLeader = mPlayers[player].getSelectedLeader();

	std::vector<CivilizationTypes> civs{};

	for (const auto civI : range<CivilizationTypes>(gGlobals.getNumCivilizationInfos()))
		if ((mUnrestrictedLeaders || selectedLeader == NO_LEADER || gGlobals.getCivilizationInfo(civI).isLeaders(selectedLeader)) && isPlayableCiv(civI, player))
			civs.push_back(civI);

	std::vector<std::wstring> names(std::from_range, civs | std::views::transform([](CivilizationTypes i) {
		return gGlobals.getCivilizationInfo(i).getDescription();
		}));

	std::ranges::sort(std::views::zip(names, civs));

	civs.insert(civs.begin(), NO_CIVILIZATION);
	names.insert(names.begin(), cvengine::getRandomText());

	tui::Combobox& lst = *mPlayers[player].civ;
	const CivilizationTypes selectedCiv = mPlayers[player].civListData.size() ? mPlayers[player].civListData[lst.getSelectionIndex()] : NO_CIVILIZATION;
	lst.setListItems(std::move(names));
	if (civs.size() == 2)
		lst.setSelectionIndex(1); // Only one choice.
	else if (const auto it = std::ranges::find(civs, selectedCiv); it != civs.end())
		lst.setSelectionIndex(it - civs.begin());
	else
		lst.setSelectionIndex(0);

	mPlayers[player].civListData = std::move(civs);
}