#include "MainInterface.h"
#include "CvTuiInterface.h"
#include "CvTuiMainInterface.h"

#include <Cv4CommonEngineLib/MyCvDLLPython.h>

#include <CvGameCoreDLL/CvGameAI.h>
#include <CvGameCoreDLL/CvGlobals.h>
#include <CvGameCoreDLL/CvInfos.h>
#include <CvGameCoreDLL/FInputDevice.h>

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
		if (c == FInputDevice::KB_RETURN && ctrl && !alt && !shift)
		{
			// Handle as CONTROL_FORCE_ENDTURN instead. Game core DLL will handle a player bot turn.
			shift = true;
			ctrl = false;
		}

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

	
}

bool cvengine::handleMainInterfaceConsoleEvent(const ConsoleEvent& e)
{
	struct Handler
	{
		CvTuiMainInterface& interfaceController = *CvTuiInterface::getInstance().getTuiMainInterface();
		WorldView& worldView = interfaceController.getWorldView();

		bool tryKey(FInputDevice::InputType key, const hecktui::ModifierKeyState& modifiers) const
		{
			if (key != FInputDevice::NONE)
			{
				// Was Ctrl+Alt+A, but can't get that through ANSI input.
				if (key == FInputDevice::KB_B && modifiers.ctrl && modifiers.alt)
				{
					// Cv4MiniEngine key
					interfaceController.activateAIAutoPlay(10'000);
				}
				else if (const int actionI = findActionByKeyInput(key, modifiers.ctrl, modifiers.alt, modifiers.shift); actionI != -1)
				{
					gGlobals.getGame().handleAction(actionI);
					interfaceController.onGameStateChanged(CvTuiMainInterface::EGameStateChangeReason::Action);
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

	return dispatch(e, Handler());
}

