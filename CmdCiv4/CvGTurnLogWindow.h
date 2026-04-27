#pragma once

#include <CvGameCoreDLL/CvEnums.h>

#include <HeckTextUI/BasicControls.h>
#include <HeckTextUI/Window.h>

namespace cvengine
{
	// Class name in Civ4 is "CvGTurnLog". I guess this is what it's for.
	class CvGTurnLogWindow : public hecktui::Window
	{
	public:
		// First called when toggling the event log on (when the game has an active player).
		CvGTurnLogWindow();

		virtual void positionWindowInScene(heck::ivec2 sceneDim) override;

		void dirtyTurnLog(PlayerTypes ePlayer);

	private:
		struct EventLogScrollPanel;

		std::shared_ptr<EventLogScrollPanel> mEventLogScrollPanel;

		enum ETab
		{
			kEvents,
			kChat,
			kCombat,
			kQuests,
			kNumTabs,
		};

		std::array<std::shared_ptr<hecktui::RadioButton>, kNumTabs> mTabBtns;
	};
}