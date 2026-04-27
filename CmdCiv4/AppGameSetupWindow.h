#pragma once

#include "AppUIUtil.h"
#include "AppGameSetupMapScript.h"

#include <HeckTextUI/Window.h>

namespace cvengine
{
	class AppGameSetupPlayerListPanel;
	class AppGameSetupConfigPanel;
	class CvApp;

	class AppGameSetupWindow : public hecktui::Window
	{
	public:
		explicit AppGameSetupWindow(CvApp& app);

		virtual void drawThis(hecktui::ivec2 offset, hecktui::Framebuffer& fb) override;

		void onPlayerCountChanged();

		void onUnrestrictedLeadersOptionChanged(bool value);

		const std::vector<AppGameSetupMapScriptInfo>& getMapScripts() const;

		void launch();

	private:
		std::vector<AppGameSetupMapScriptInfo> mMapScriptsList;
		std::shared_ptr<AppGameSetupPlayerListPanel> mPlayerListPanel;
		std::shared_ptr<AppGameSetupConfigPanel> mGameSetupBottomPanel;
	};
}