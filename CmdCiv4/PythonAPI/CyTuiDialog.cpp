#include "CyTuiDialog.h"
#include "CvPythonExtensions.h"
#include "../Common.h"
#include "../TuiTextCode.h"
#include "../MainInterfaceControls.h"
#include "../DLLInterface/MyCvDLLPython.h"
#include "../CvApp.h"

#include <CyArgsList.h>

#include <HeckTextUI/BasicControls.h>
#include <HeckTextUI/Combobox.h>

#include <pybind11/stl.h>

using namespace hecktui;

void CyTuiDialog::registerWithPython(const pybind11::module& m)
{
#define R(x, ...) def(#x, &CyTuiDialog::x __VA_OPT__(,) __VA_ARGS__)
#define V(ns, x) value(#x, ns::x)
#define M(ns, x) def_readwrite(#x, &ns::x)

	pybind11::class_<CyTuiDialog>(m, "CyTuiDialog")
		.def(pybind11::init<>())
		//.R(clear)
		//.R(setParent)
		//.R(moveToFirst)
		//.R(delByPrefix)
		//.R(show)
		//.R(hide)
		//.R(setVisible)
		//.R(disable)
		//.R(setEnabled)
		.R(setInitialTitle)
		//.def("setAutoSizeBehaviour", [](CyTuiDialog& self, CvGInterfaceScreen::EAutoSizeBehaviour b) { self.impl.setAutoSizeBehaviour(b); })
		//.def("setModal", [](CyTuiDialog& self, bool b) { self.impl.setModal(b); })
		//.R(setControlRect)
		//.R(isActive)
		//.R(setSound)
		//.R(newLabel, "Create a new label with optional widget data",
		//	pybind11::arg("parent"), pybind11::arg("name"), pybind11::arg("text"),
		//	pybind11::arg("widgetType") = WidgetTypes::WIDGET_GENERAL,
		//	pybind11::arg("actionIndex") = -1,
		//	pybind11::arg("data2") = -1
		//)
		//.R(setLabelText)
		//.R(setLabelHAlign)
		//.R(setLabelWidgetData)
		//.R(setLabelEnableWrapping)
		.R(newPanel)
		.R(setPanelBackgroundColour)
		//.R(newBoxPanel, "Create a bordered panel.",
		//	pybind11::arg("parent"), pybind11::arg("name"), pybind11::arg("contentPanelName"),
		//	pybind11::arg("borderStyle"), pybind11::arg("borderColour") = EColour::Silver
		//)

		.R(newButton)
		
		.R(newCheckBox)
		.R(setCheckBoxValue)
		.R(getCheckBoxValue)
		
		.R(newCombobox)
		.R(setComboboxSelectedIndex)
		.R(getComboboxSelectedIndex)

		.R(newHRule, "Create a new horizontal separator",
			pybind11::arg("parent"),
			pybind11::arg("name"),
			pybind11::arg("borderStyle") = hecktui::EBorderStyle::Thick
		)
		
		.R(setFillLayout)
		.R(setHFlowLayout, "Set layout to HFlow.", pybind11::arg("name"), pybind11::arg("halign") = EJustilign::Start, pybind11::arg("valign") = EJustilign::Stretch)
		//.R(setHWrapLayout)
		.R(setVFlowLayout)
		//.R(setTableLayout)
		.R(showAsModalDialog)
		.R(destroy)
		;
}

void CyTuiDialog::setInitialTitle(std::wstring title)
{
	impl.setInitialTitle(std::move(title));
}


void CyTuiDialog::newControl(const std::string& parent, std::string name, std::shared_ptr<hecktui::Element> ctrl)
{
	impl.at(parent)->addChild(ctrl);
	impl.set(std::move(name), std::move(ctrl));
}

namespace
{
	struct Panel : Element
	{
		std::optional<PixelColouring> colouring{};

		virtual void drawThis(ivec2 offset, hecktui::Framebuffer& fb) override
		{
			// If not set, don't overwrite the background.
			if (colouring)
				fb.drawFilledBox(offset + getRect(), { .colour = *colouring });
		}
	};
}

void CyTuiDialog::newPanel(std::string parent, std::string name)
{
	newControl(parent, name, std::make_shared<Panel>());
}
void CyTuiDialog::setPanelBackgroundColour(std::string name, std::optional<hecktui::EColour> colour)
{
	static_cast<Panel&>(*impl.at(name)).colouring = PixelColouring{ .back = colour ? *colour : kTransparent };
}

void CyTuiDialog::newButton(std::string parent, std::string name, std::wstring label, std::string callbackInterfaceName, std::string callbackFunctionName)
{
	auto ctrl = makeActionButtonWithManualLabel(label, { .m_iData1{}, .m_iData2{}, .m_bOption{}, .m_eWidgetType = WIDGET_GENERAL }, [=] {
		CyArgsList pyArgs;
		pyArgs.add(name.c_str());
		long result = 0;
		MyCvDLLPython().callFunction(callbackInterfaceName.c_str(), callbackFunctionName.c_str(), pyArgs.makeFunctionArgs(), &result);
		});
	
	newControl(parent, name, std::move(ctrl));
}

namespace
{
	class PythonCheckBox : public hecktui::Checkbox
	{
	public:
		explicit PythonCheckBox(std::wstring label, std::string name, std::string callbackInterfaceName, std::string callbackFunctionName)
			: hecktui::Checkbox(hecktui::ECheckStyle::AsciiX, label)
			, mName(std::move(name))
			, mCallbackInterfaceName(std::move(callbackInterfaceName))
			, mCallbackFunctionName(std::move(callbackFunctionName))
		{
		}

		virtual void onCheckChanged() override
		{
			CyArgsList pyArgs;
			pyArgs.add(value);
			pyArgs.add(mName.c_str());
			long result = 0;
			MyCvDLLPython().callFunction(mCallbackInterfaceName.c_str(), mCallbackFunctionName.c_str(), pyArgs.makeFunctionArgs(), &result);
		}

	private:
		std::string mName;
		std::string mCallbackInterfaceName;
		std::string mCallbackFunctionName;
	};

	class PythonComboBox : public hecktui::Combobox
	{
	public:
		explicit PythonComboBox(hecktui::EComboboxStyle style, std::string name, std::string callbackInterfaceName, std::string callbackFunctionName)
			: hecktui::Combobox(style)
			, mName(std::move(name))
			, mCallbackInterfaceName(std::move(callbackInterfaceName))
			, mCallbackFunctionName(std::move(callbackFunctionName))
		{
		}

		virtual void onSelectionChanged() override
		{
			CyArgsList pyArgs;
			pyArgs.add(static_cast<int>(getSelectionIndex()));
			pyArgs.add(mName.c_str());
			long result = 0;
			MyCvDLLPython().callFunction(mCallbackInterfaceName.c_str(), mCallbackFunctionName.c_str(), pyArgs.makeFunctionArgs(), &result);
		}

	private:
		std::string mName;
		std::string mCallbackInterfaceName;
		std::string mCallbackFunctionName;
	};
}

void CyTuiDialog::newCheckBox    /**/(std::string parent, std::string name, std::wstring label, std::string callbackInterfaceName, std::string callbackFunctionName)
{
	newControl(parent, name, std::make_shared<PythonCheckBox>(std::move(label), name, std::move(callbackInterfaceName), std::move(callbackFunctionName)));
}

void CyTuiDialog::setCheckBoxValue     /**/(std::string name, bool value)
{
	static_cast<PythonCheckBox&>(*impl.at(name)).value = value;
}
bool CyTuiDialog::getCheckBoxValue     /**/(std::string name)
{
	return static_cast<PythonCheckBox&>(*impl.at(name)).value;
}

void CyTuiDialog::newCombobox(std::string parent, std::string name, std::vector<std::wstring> items, std::string callbackInterfaceName, std::string callbackFunctionName)
{
	auto ctrl = std::make_shared<PythonComboBox>(hecktui::EComboboxStyle::Bulky, name, std::move(callbackInterfaceName), std::move(callbackFunctionName));
	ctrl->setListItems(std::move(items));
	newControl(parent, name, std::move(ctrl));
}
void CyTuiDialog::setComboboxSelectedIndex(std::string name, int i)
{
	static_cast<hecktui::Combobox&>(*impl.at(name)).setSelectionIndex(i);
}
int CyTuiDialog::getComboboxSelectedIndex(std::string name)
{
	return static_cast<int>(static_cast<const hecktui::Combobox&>(*impl.at(name)).getSelectionIndex());
}

void CyTuiDialog::newHRule(std::string parent, std::string name, hecktui::EBorderStyle style)
{
	newControl(parent, name, std::make_unique<hecktui::HorizontalRule>(style));
}


void CyTuiDialog::setFillLayout(std::string ctrlName)
{
	impl.at(ctrlName)->setLayout(std::make_unique<hecktui::FillLayout>());
}
void CyTuiDialog::setHFlowLayout(std::string ctrlName, EJustilign halign, EJustilign valign)
{
	impl.at(ctrlName)->setLayout(std::make_unique<hecktui::FlowLayout>(hecktui::FlowConfig{
		.axis = hecktui::EAxis::Horizontal,
		.itemsFlowwiseJustilign = halign,
		.itemsCrosswiseJustilign = valign,
		}));
}
//void CyTuiDialog::setHWrapLayout(std::string ctrlName)
//{
//	impl.at(ctrlName)->setLayout(std::make_shared<hecktui::FlowLayout>(hecktui::FlowConfig{
//		.axis = hecktui::EAxis::Horizontal,
//		.wrap = true,
//		.itemsCrosswiseJustilign = EJustilign::Stretch,
//		}));
//}
void CyTuiDialog::setVFlowLayout(std::string ctrlName)
{
	impl.at(ctrlName)->setLayout(std::make_unique<hecktui::FlowLayout>(hecktui::FlowConfig{
		.axis = hecktui::EAxis::Vertical,
		.linesCrosswiseJustilign = EJustilign::Stretch
		}));
}
//void CyTuiDialog::setTableLayout(std::string ctrlName, hecktui::TableLayoutConfig config)
//{
//	auto layout = std::make_shared<hecktui::TableLayout>(std::move(config));
//	if (ctrlName == "" && impl.getKind() == CvEngineEnums::ECvScreen::SPACE_SHIP_SCREEN)
//		layout->debug = true;
//	impl.at(ctrlName)->setLayout(std::move(layout));
//}



void CyTuiDialog::showAsModalDialog()
{
	if (window)
		return;

	window = impl.createTuiWindow(false, {
		.isDefaultFocus = true,
		.isFullscreen = false,
		.isModal = true,
		.canClose = false,
		.isFocusContext = false,
		.borderStyle = hecktui::EBorderStyle::Rounded,
		});

	CvApp::getInstance().getUI().pushWindow(window);
}

void CyTuiDialog::destroy()
{
	if (window)
	{
		window->setWantClose();
		CvApp::getInstance().getUI().removeWindow(window.get());
		window = nullptr;
	}
}