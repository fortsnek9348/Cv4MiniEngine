#pragma once

#include <CvPopupReturn.h>
#include <CvStructs.h>

#include <HeckTextUI/Window.h>

#include <string>
#include <vector>
#include <optional>

class PopupReturn;

class CvPopupTuiDialog;

class CvPopup
{
public:
	enum class EControlType : uint8_t
	{
		None,
		Label,
		Button,
		Separator,
		Image,
		ListBox,
		ComboBox,
		CheckBoxGroup,
		RadioButtonGroup,
		EditBox,
		SpinBox,
	};

	struct Control
	{
		EControlType type{};
		bool enabled = true;
		std::wstring text{};
		std::string action{};
		int iGroup{}; // For buttons, we're using this as an id (getButtonClicked). OK button is -1.
		int sepSize{};
		int numControls{};
		int editBoxMaxLength{};
		int spinBoxMax{};
		std::vector<std::pair<std::string, int>> listItems{};

		CvWidgetDataStruct widgetData{
			.m_iData1{},
			.m_iData2{},
			.m_bOption{},
			.m_eWidgetType = WIDGET_GENERAL,
		};
	};

	std::wstring headerString;
	std::wstring bodyString;
	std::vector<Control> controls;
	std::optional<int> optEnterSubmitBtnId;
	PopupEventTypes eventType = POPUPEVENT_NONE; // What's this used for?
	PopupStates state{}; // Set by CvInterface.
	// This behaviour appears to be hard-coded in the EXE, not controlled by the DLL.
	bool enableEscCancel = false;

	std::shared_ptr<CvPopupTuiDialog> window;

	Control& findControl(EControlType type, int iGroup);

	// Civ4 DLL calls this "focus".
	// Used for things like looking at a city for a production popup.
	virtual void onPopupDisplayed() = 0;
	virtual void submit(const PopupReturn& result) = 0;
	virtual void escCancel() = 0;

	virtual ~CvPopup() = default;
};

class CvPopupTuiDialog : public hecktui::Window
{
public:
	explicit CvPopupTuiDialog(CvPopup* popup);

	CvPopup& getPopup();

	virtual void positionWindowInScene(hecktui::ivec2 sceneDim) override;

	virtual bool onEvent(const hecktui::ConsoleEvent& e) override;

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
