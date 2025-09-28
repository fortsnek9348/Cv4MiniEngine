#include "AnsiParser.h"

#include <iostream>
#include <cwctype>

using namespace hecktui;

namespace
{
	enum EState
	{
		Accept,
		EscapeStarted,
		CSI,
		MouseButtons,
		MouseX,
		MouseY,
		ActionKey0,
		ActionKey1,
		SS3FuncKey,
	};

	// https://www.chiark.greenend.org.uk/~sgtatham/coroutines.html
#define YIELD(state) do { mState = state; return; case state:; } while(0)
#define BAD_INPUT() do { mState = Accept; return; } while(0)
#define ACCEPT() do { mState = Accept; return; } while(0)
#define PARSE_INTEGER(var, init, state) do { \
	var = init; \
	mState = state; \
	return; \
case state: \
	if (L'0' <= c && c <= L'9') \
	{ \
		var = var * 10 + (c - L'0'); \
		return; \
	} \
	else \
		break; \
} while(0)

}

AnsiParser::AnsiParser(MouseEvent& mouseState) : mMouseState(mouseState)
{
}

void AnsiParser::inject(wchar_t c, std::vector<EventStorage>& out)
{
	const auto emitActionKey = [&](EActionKey key) {
		ActionKeyEvent e{ KeyEvent{ {}, mMouseState } };
		e.type = EConsoleEventType::ActionKeyPressed;
		e.key = key;
		out.emplace_back(e);
		};

	switch (mState)
	{
	case Accept:
		if (c == L'\x1b')
		{
			YIELD(EscapeStarted);
			if (c == L'[')
			{
				YIELD(CSI);
				if (c == L'<')
				{
					// https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h3-Any-event-tracking
					PARSE_INTEGER(mData0, 0, MouseButtons);
					if (c == L';')
					{
						PARSE_INTEGER(mData1, 0, MouseX);
						if (c == L';')
						{
							PARSE_INTEGER(mData2, 0, MouseY);
							if (c == L'm' || c == L'M')
							{
								//std::cerr << mData0 << ' ' << mData1 << ' ' << mData2 << std::endl;
								const int btnEncoding = mData0;

								const bool isMouseMotion = (btnEncoding & 32) != 0;
								const bool isScrollOrAnyButtonPressed = c == L'M';
								const bool isMouseWheel = isScrollOrAnyButtonPressed && (btnEncoding & 64) != 0;

								mMouseState.shift = false; // This stalls terminal
								mMouseState.alt = (btnEncoding & 8) != 0;
								mMouseState.ctrl = (btnEncoding & 16) != 0;

								const auto releaseAllButtons = [&] {
									if (mMouseState.left)
									{
										mMouseState.left = false;
										MouseButtonEvent e{ mMouseState };
										e.type = EConsoleEventType::MouseButtonUp;
										e.button = EMouseButton::Left;
										out.emplace_back(e);
									}
									if (mMouseState.middle)
									{
										mMouseState.middle = false;
										MouseButtonEvent e{ mMouseState };
										e.type = EConsoleEventType::MouseButtonUp;
										e.button = EMouseButton::Middle;
										out.emplace_back(e);
									}
									if (mMouseState.right)
									{
										mMouseState.right = false;
										MouseButtonEvent e{ mMouseState };
										e.type = EConsoleEventType::MouseButtonUp;
										e.button = EMouseButton::Right;
										out.emplace_back(e);
									}
									};

								if (isMouseWheel)
								{
									MouseScrollEvent e{ mMouseState };
									e.type = EConsoleEventType::MouseScroll;
									e.scroll = btnEncoding & 1 ? 1 : -1;
									out.emplace_back(e);
								}
								else if (isMouseMotion)
								{
									if (!isScrollOrAnyButtonPressed)
										releaseAllButtons();

									mMouseState.coord = ivec2(mData1, mData2) - 1;
									MouseMoveEvent e{ mMouseState };
									e.type = EConsoleEventType::MouseMove;
									out.emplace_back(e);

									//std::clog << mMouseState.coord << std::endl;
								}
								else
								{
									// Probably a button.
									EMouseButton btn{};

									switch (btnEncoding & 3)
									{
									case 0: btn = EMouseButton::Left; mMouseState.left = isScrollOrAnyButtonPressed; break;
									case 1: btn = EMouseButton::Middle; mMouseState.middle = isScrollOrAnyButtonPressed; break;
									case 2: btn = EMouseButton::Right; mMouseState.right = isScrollOrAnyButtonPressed; break;
									default:
									{
										releaseAllButtons();
										break;
									}
									}

									MouseButtonEvent e{ mMouseState };
									e.type = isScrollOrAnyButtonPressed ? EConsoleEventType::MouseButtonDown : EConsoleEventType::MouseButtonUp;
									e.button = btn;

									//if (isScrollOrAnyButtonPressed && btn == EMouseButton::Right)
									//{
									//	e.button = EMouseButton::Left;
									//	e.shift = true;
									//}
									out.emplace_back(e);
								}

								ACCEPT();
							}
						}
					}
				}
				else
				{
					// When you use modifier keys with the various action keys, they get prefixed with stuff.
					mData0 = 0;
					mData1 = 0;
					mData2 = 0;
					if (L'0' <= c && c <= L'9')
					{
						mData2 = 1;
						PARSE_INTEGER(mData0, c - L'0', ActionKey0);
						if (c == L';')
						{
							PARSE_INTEGER(mData1, 0, ActionKey1);
						}
					}

					mMouseState.shift = mData1 > 0 && ((mData1 - 1) & 1) != 0;
					mMouseState.alt = mData1 > 0 && ((mData1 - 1) & 2) != 0;
					mMouseState.ctrl = mData1 > 0 && ((mData1 - 1) & 4) != 0;

					// You can look at https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h3-PC-Style-Function-Keys, but it's incomplete.
					// You can only really get this stuff by testing a terminal.

					if (c == L'~')
					{
						EActionKey key = EActionKey::None;
						switch (mData0)
						{
						case 2: key = EActionKey::Insert; break;
						case 3: key = EActionKey::Delete; break;
						case 5: key = EActionKey::PageUp; break;
						case 6: key = EActionKey::PageDown; break;
						case 15: key = EActionKey::F5; break;
						case 17: key = EActionKey::F6; break;
						case 18: key = EActionKey::F7; break;
						case 19: key = EActionKey::F8; break;
						case 20: key = EActionKey::F9; break;
						case 21: key = EActionKey::F10; break;
						case 23: key = EActionKey::F11; break;
						case 24: key = EActionKey::F12; break;
						default:
							break;
						}
						if (key != EActionKey::None)
						{
							emitActionKey(key);
							ACCEPT();
						}
					}
					else if (mData2 == 0 /* there were no other args */ || mData0 == 1)
					{
						EActionKey key{};
						switch (c)
						{
						case L'A': key = EActionKey::Up; break;
						case L'B': key = EActionKey::Down; break;
						case L'C': key = EActionKey::Right; break;
						case L'D': key = EActionKey::Left; break;
						case L'H': key = EActionKey::Home; break;
						case L'F': key = EActionKey::End; break;
							// These don't happen without a modifier key.
						case L'P': key = EActionKey::F1; break;
						case L'Q': key = EActionKey::F2; break;
						case L'R': key = EActionKey::F3; break;
						case L'S': key = EActionKey::F4; break;
						default:
							break;
						}
						if (key != EActionKey::None)
						{
							emitActionKey(key);
							ACCEPT();
						}
					}
				}
			}
			else if (c == L'O')
			{
				// https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h3-PC-Style-Function-Keys
				// SS3
				YIELD(SS3FuncKey);
				mMouseState.shift = false;
				mMouseState.alt = false;
				mMouseState.ctrl = false;
				if (c == L'P') { emitActionKey(EActionKey::F1); ACCEPT(); }
				else if (c == L'Q') { emitActionKey(EActionKey::F2); ACCEPT(); }
				else if (c == L'R') { emitActionKey(EActionKey::F3); ACCEPT(); }
				else if (c == L'S') { emitActionKey(EActionKey::F4); ACCEPT(); }
			}
			else if (1 <= c && c <= 26)
			{
				// Ctrl+Alt+Letter.
				static_cast<ModifierKeyState&>(mMouseState) = { .ctrl = true, .alt = true };
				CharKeyEvent e{ KeyEvent{ {}, mMouseState } };
				e.type = EConsoleEventType::CharKeyPressed;
				e.c = L'a' + (c - 1);
				out.emplace_back(e);
			}
			else if (c >= 0x80)
			{
				// Typed extended ASCII character. Ctrl+Alt+Letter.
				static_cast<ModifierKeyState&>(mMouseState) = { .ctrl = true, .alt = true };
				CharKeyEvent e{ KeyEvent{ {}, mMouseState } };
				e.type = EConsoleEventType::CharKeyPressed;
				e.c = c;
				out.emplace_back(e);
			}
			else if (c >= 32)
			{
				// Alt+Letter
				static_cast<ModifierKeyState&>(mMouseState) = { .shift = std::iswupper(c) != 0, .alt = true };
				CharKeyEvent e{ KeyEvent{ {}, mMouseState } };
				e.type = EConsoleEventType::CharKeyPressed;
				e.c = c;
				out.emplace_back(e);
			}
		}
		else // No escape.
		{
			// Typed character.

			static_cast<ModifierKeyState&>(mMouseState) = {};

			EActionKey key = EActionKey::None;
			switch (c)
			{
				// Why?
#ifdef _WIN32
			case '\r':
#else
			case '\n':
#endif
				key = EActionKey::Enter;
				break;
			case '\t': key = EActionKey::Tab; break;
			case '\x7F': key = EActionKey::Backspace; break;
			default:
				break;
			}
			if (key != EActionKey::None)
			{
				ActionKeyEvent e{ KeyEvent{ {}, mMouseState } };
				e.type = EConsoleEventType::ActionKeyPressed;
				e.key = key;
				out.emplace_back(e);
			}
			else
			{
				if (1 <= c && c <= 26)
				{
					// Ctrl+Letter. Can't detect Shift.
					// Can't detect Ctrl+M either!
					static_cast<ModifierKeyState&>(mMouseState) = { .ctrl = true };
					CharKeyEvent e{ KeyEvent{ {}, mMouseState } };
					e.type = EConsoleEventType::CharKeyPressed;
					e.c = L'a' + (c - 1);
					out.emplace_back(e);
				}
				else if (c >= 32)
				{
					// Normal character.
					mMouseState.shift = std::iswupper(c) != 0;
					CharKeyEvent e{ KeyEvent{ {}, mMouseState } };
					e.type = EConsoleEventType::CharKeyPressed;
					e.c = c;
					out.emplace_back(e);
				}
			}
		}

		BAD_INPUT();
		break;

		// How would you get here... I don't know.
	default:
		BAD_INPUT();
		break;
	}
}

bool AnsiParser::isComplete() const noexcept
{
	return mState == 0;
}

// How do you distinguish between ESC and the start of an escape sequence? A dirty hack. The user can't type faster than the input loop, right?
// Ugh... ANSI input isn't elegant.
// This also assumes the terminal doesn't overflow the input buffer, as the input loop might find that there's no more input? (race condition)
void AnsiParser::flush(std::vector<EventStorage>& out)
{
	switch (mState)
	{
	case EscapeStarted:
	{
		ActionKeyEvent e{ KeyEvent{ {}, mMouseState } };
		e.type = EConsoleEventType::ActionKeyPressed;
		e.key = EActionKey::Esc;
		out.emplace_back(e);
		break;
	}
	case CSI:
	{
		// Alt+[
		static_cast<ModifierKeyState&>(mMouseState) = { .alt = true };
		CharKeyEvent e{ KeyEvent{ {}, mMouseState } };
		e.type = EConsoleEventType::CharKeyPressed;
		e.c = L'[';
		out.emplace_back(e);
		break;
	}
	case SS3FuncKey:
	{
		// Alt+Shift+O
		static_cast<ModifierKeyState&>(mMouseState) = { .shift = true, .alt = true };
		CharKeyEvent e{ KeyEvent{ {}, mMouseState } };
		e.type = EConsoleEventType::CharKeyPressed;
		e.c = L'O';
		out.emplace_back(e);
		break;
	}
	default:
		break;
	}

	mState = Accept;
}