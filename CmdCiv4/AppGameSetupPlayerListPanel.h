#pragma once

#include "AppUIUtil.h"

#include <CvGameCoreDLL/CvEnums.h>

#include <HeckTextUI/BasicControls.h>
#include <HeckTextUI/Combobox.h>
#include <HeckTextUI/Textbox.h>

#include <unordered_set>

class CvInitCore;

namespace cvengine
{
	namespace app
	{
		struct SimplifiedInitCore;
	}

	class AppGameSetupPlayerListPanel : public hecktui::Element, public cvengine::IWindowChildEventHandler
	{
	public:
		explicit AppGameSetupPlayerListPanel(const CvInitCore& initCore);

		void onPlayerCountChanged(int n);

		void onUnrestrictedLeadersChanged(bool enabled);

		virtual void onComboBoxSelectionChanged(hecktui::Combobox& ctrl) override;

		void setupConfig(app::SimplifiedInitCore& config) const;


	private:
		std::shared_ptr<hecktui::Textbox> mTxtPlayerName;
		int mNumPlayers = 0;

		struct PlayerRow
		{
			std::shared_ptr<hecktui::Element> name; // textbox
			std::shared_ptr<hecktui::Combobox> team;
			std::shared_ptr<hecktui::Combobox> leader;
			std::shared_ptr<hecktui::Combobox> civ;

			std::vector<LeaderHeadTypes> leaderListData;
			std::vector<CivilizationTypes> civListData;

			LeaderHeadTypes getSelectedLeader() const;
			CivilizationTypes getSelectedCiv() const;
		};

		std::vector<PlayerRow> mPlayers;

		std::unordered_set<LeaderHeadTypes> mHumanPlayableLeaders;
		std::unordered_set<LeaderHeadTypes> mAIPlayableLeaders;
		std::vector<std::wstring> mLeaderLabels;

		bool mUnrestrictedLeaders = false;

		void buildCivLeaderData();
		static std::wstring makeLeaderText(LeaderHeadTypes leader);
		void refillLeadersList(PlayerTypes player);
		void refillCivsList(PlayerTypes player);
	};
}