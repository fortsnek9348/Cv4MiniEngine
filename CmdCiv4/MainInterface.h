#pragma once

#include <memory>
#include <vector>

class WorldView;
class CvInterface;

// Implementation mirroring CvMainInterface.py. Handles the world view, unit selection, minimap, etc, *and* the city screen.
//class MainInterface : public ftxui::ComponentBase
//{
//public:
//	explicit MainInterface(WorldView& view, std::function<void()> updatePopupUI);
//
//	// Renders the component.
//	virtual ftxui::Element Render() override;
//
//	virtual bool OnEvent(ftxui::Event) override;
//
//private:
//	WorldView& mWorldView;
//};

//// Input to mainInterface.
//extern const ftxui::Event kUIEvent_InvalidateMainInterface;
//extern const ftxui::Event kUIEvent_InterfaceModeChanged;
//// Output from mainInterface.
//extern const ftxui::Event kUIEvent_UnitActionExecuted;
//extern const ftxui::Event kUIEvent_NewTurn;
//extern const ftxui::Event kUIEvent_KeyActionExecuted;
//extern const ftxui::Event kUIEvent_UnitSelectionChanged;
//extern const ftxui::Event kUIEvent_GameStateChanged;
//extern const ftxui::Event kUIEvent_CityScreen;
//
//extern const ftxui::ButtonOption kDefaultButtonStyle;
//
//using GlobalEventHandlerSet = std::set<std::weak_ptr<ftxui::ComponentBase>, std::owner_less<>>;

namespace hecktui
{
	class Element;
	struct ConsoleEvent;
}

std::shared_ptr<hecktui::Element> buildMainInterfaceWorldViewComponent(WorldView& worldView);

//std::shared_ptr<hecktui::Element> createMainInterfaceRootElement();

bool handleMainInterfaceConsoleEvent(const hecktui::ConsoleEvent&, CvInterface& interfaceController);

std::vector<int> buildListOfActionsToShow();