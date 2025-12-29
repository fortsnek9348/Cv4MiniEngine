#pragma once

#include "AppUIUtil.h"

#include <HeckTextUI/Textbox.h>

#include <CvEnums.h>

class AppGameSetupWindow;
class AppGameSetupPlayerListPanel;
struct SimplifiedInitCore;
struct AppGameSetupMapScriptInfo;

class AppGameSetupConfigPanel : public hecktui::ScrollBarPanel, public cvengine::IWindowChildEventHandler
{
public:
	explicit AppGameSetupConfigPanel(const std::vector<AppGameSetupMapScriptInfo>& mapScripts, AppGameSetupWindow& singlePlayerCustomWindow, std::shared_ptr<AppGameSetupPlayerListPanel> playerListPanel);

	virtual void onComboBoxSelectionChanged(hecktui::Combobox& ctrl) override;
	virtual void onCheckBoxValueChanged(hecktui::Checkbox&) override;

	// Returns nullopt if unknown.
	std::optional<int> getNumPlayers() const;

	void setupConfig(const std::vector<AppGameSetupMapScriptInfo>& mapScripts, SimplifiedInitCore& config) const;

private:
	AppGameSetupWindow& mSinglePlayerCustomWindow;

	std::shared_ptr<hecktui::Combobox> mLstMapScript;
	std::shared_ptr<hecktui::Combobox> mLstNumPlayers;
	std::shared_ptr<hecktui::Combobox> mLstDifficulty;
	std::shared_ptr<hecktui::Combobox> mLstWorldSize;
	std::shared_ptr<hecktui::Combobox> mLstWorldSizeMultiplier;
	std::shared_ptr<hecktui::Combobox> mLstClimate;
	std::shared_ptr<hecktui::Combobox> mLstSeaLevel;
	std::shared_ptr<hecktui::Combobox> mLstEra;
	std::shared_ptr<hecktui::Combobox> mLstSpeed;
	std::vector<GameSpeedTypes> mGameSpeedOrder;

	std::shared_ptr<hecktui::Element> mLblClimate;
	std::shared_ptr<hecktui::Element> mLblSeaLevel;

	struct CustomOption
	{
		std::shared_ptr<hecktui::Element> lbl;
		std::shared_ptr<hecktui::Combobox> lst;
		bool isRandomisable = false;
	};
	std::vector<CustomOption> mCustomOptions;

	std::shared_ptr<hecktui::Element> mSettingsPanel;

	std::shared_ptr<hecktui::Textbox> mTxtGameName;
	std::shared_ptr<hecktui::Textbox> mTxtMapSeed;
	std::shared_ptr<hecktui::Textbox> mTxtSyncSeed;
	std::shared_ptr<hecktui::Label> mLblMapSeed;
	std::shared_ptr<hecktui::Label> mLblSyncSeed;

	std::vector<std::shared_ptr<hecktui::Checkbox>> mChkGameOptions;
	std::vector<std::shared_ptr<hecktui::Checkbox>> mChkVictoryOptions;

	void onMapScriptChanged();

	void addCustomMapOptionControls(const AppGameSetupMapScriptInfo& mapScript);
};