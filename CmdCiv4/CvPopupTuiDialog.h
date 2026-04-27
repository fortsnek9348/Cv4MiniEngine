#pragma once

#include <Cv4CommonEngineLib/CvPopup.h>
#include <Cv4CommonEngineLib/EngineSpecificsHeader.h>

#include <HeckTextUI/Window.h>

namespace cvengine
{
	class CvPopupTuiDialog : public hecktui::Window, public engine_specific::IPopupUIWindow
	{
	public:
		explicit CvPopupTuiDialog(CvPopup* popup);
	
		CvPopup& getPopup();
	
		virtual void positionWindowInScene(hecktui::ivec2 sceneDim) override;
	
		virtual bool onEvent(const hecktui::ConsoleEvent& e) override;

		virtual void setWantClose() override;
		virtual bool isWantClose() const override;
	
	private:
		CvPopup* mPopup;
		// Same indices.
		std::vector<hecktui::Element*> mPopupElements;
	
		bool submit(int btnId);
	};
	
	class InternalPopup : public CvPopup
	{
	public:
		static std::optional<PopupReturn> launchModal(std::unique_ptr<InternalPopup>);
	
		virtual void onPopupDisplayed() override;
		virtual void submit(const PopupReturn& result) override;
		virtual void escCancel() override;
	
		std::optional<PopupReturn> optResult;
	};
}