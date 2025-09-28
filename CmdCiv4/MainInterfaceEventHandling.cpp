#include "MainInterface.h"
#include "Common.h"
#include "CvInterface.h"

#include "DLLInterface/MyCvDLLPython.h"

#include <CvGameAI.h>
#include <CvGlobals.h>
#include <CvInfos.h>
#include <FInputDevice.h>

#include <HeckTextUI/Core.h>

#include <CommonStuff/range.h>

using namespace hecktui;
using heck::range;

// NOTE: Known console input quirks.
// Windows Terminal and Xfce terminal totally hijack the shift key (so you can't shift-click).
// Can't handle tab in Windows Terminal (no Ctrl-Tab for event log).
//   *Can you handle Tab with ANSI input?

namespace
{
	static int findActionByKeyInput(FInputDevice::InputType c, bool ctrl, bool alt, bool shift)
	{
		CvGame& game = gGlobals.getGame();
		int bestPriority = -1;
		int bestIndex = -1;
		for (const auto index : range(gGlobals.getNumActionInfos()))
		{
			const CvActionInfo& info = gGlobals.getActionInfo(index);
			const bool checkA = info.getHotKeyVal() == c && info.getHotKeyPriority() > bestPriority && info.isShiftDown() == shift && info.isCtrlDown() == ctrl && info.isAltDown() == alt;
			const bool checkB = info.getHotKeyValAlt() == c && info.getHotKeyPriorityAlt() > bestPriority && info.isShiftDownAlt() == shift && info.isCtrlDownAlt() == ctrl && info.isAltDownAlt() == alt;
			if (checkA || checkB)
			{
				if (game.canHandleAction(index, nullptr, false, false))
				{
					bestPriority = std::max(
						checkA ? info.getHotKeyPriority() : bestPriority,
						checkB ? info.getHotKeyPriorityAlt() : bestPriority
					);
					bestIndex = index;
				}
			}
		}

		return bestIndex;
	}

	bool isPlayableAction(int index)
	{
		return gGlobals.getActionInfo(index).isVisible() && gGlobals.getGame().canHandleAction(index, CvInterface::getInstance().getGotoPlot(), true, true);
	}
}

bool ::handleMainInterfaceConsoleEvent(const ConsoleEvent& e, CvInterface& interfaceController)
{
	struct Handler
	{
		CvInterface& interfaceController;
		WorldView& worldView = interfaceController.getWorldView();

		bool tryKey(FInputDevice::InputType key, const hecktui::ModifierKeyState& modifiers) const
		{
			if (key != FInputDevice::NONE)
			{
				if (key == FInputDevice::KB_A && modifiers.ctrl && modifiers.alt)
				{
					// Cv4MiniEngine key
					interfaceController.activateAIAutoPlay(10'000);
				}
				else if (const int actionI = findActionByKeyInput(key, modifiers.ctrl, modifiers.alt, modifiers.shift); actionI != -1)
				{
					gGlobals.getGame().handleAction(actionI);
					interfaceController.onGameStateChanged(CvInterface::EGameStateChangeReason::Action);
					return true;
				}
			}

			return false;
		}

		bool operator()(const ActionKeyEvent& e) const
		{
			if (e.key == EActionKey::F5 && e.ctrl)
				MyCvDLLPython().restart();
			else if (!interfaceController.isInCityScreen())
			{
				switch (e.key)
				{
				case EActionKey::Down:
					worldView.scrollPlots({ 0, 1 });
					break;
				case EActionKey::Up:
					worldView.scrollPlots({ 0, -1 });
					break;
				case EActionKey::Left:
					worldView.scrollPlots({ -1, 0 });
					break;
				case EActionKey::Right:
					worldView.scrollPlots({ 1, 0 });
					break;
				case EActionKey::Enter:
					return tryKey(FInputDevice::KB_RETURN, e);
				default:
					if (EActionKey::F1 <= e.key && e.key <= EActionKey::F12)
						return tryKey(FInputDevice::InputType((int)FInputDevice::KB_F1 + ((int)e.key - (int)EActionKey::F1)), e);
					break;
				}
			}
			else
			{
				switch (e.key)
				{
				case EActionKey::Esc:
					interfaceController.exitCityScreen();
					break;
				default:
					break;
				}
			}
			return true;
		}

		bool operator()(const CharKeyEvent& e) const
		{
			if (e.c == 'd' && e.ctrl && !e.shift && !e.alt)
			{
				interfaceController.showDebugScreen();
				return true;
			}

			FInputDevice::InputType key = FInputDevice::NONE;
			if ('a' <= e.c && e.c <= 'z')
				key = FInputDevice::InputType(FInputDevice::KB_A + (e.c - 'a'));
			if ('A' <= e.c && e.c <= 'Z')
				key = FInputDevice::InputType(FInputDevice::KB_A + (e.c - 'A'));
			if (e.c == '#')
				key = FInputDevice::KB_APOSTROPHE;

			
			
			return tryKey(key, e);
		}

		bool operator()(const ConsoleEvent&) const
		{
			return false;
		}
	};

	return dispatch(e, Handler(interfaceController));
}

std::vector<int>(::buildListOfActionsToShow)()
{
	gGlobals.getGame().setupActionCache();
	return range(gGlobals.getNumActionInfos()) | std::views::filter(isPlayableAction) | std::ranges::to<std::vector>();
}