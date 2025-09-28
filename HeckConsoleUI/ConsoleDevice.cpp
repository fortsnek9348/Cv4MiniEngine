#include "inc/HeckTextUI/HeckTextUI.h"
#include "AnsiParser.h"

#include <CommonStuff/StringConversion.h>

#include <iostream>
#include <algorithm>
#include <memory>
#include <string>
#include <mutex>
#include <chrono>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN 1
#define NOMINMAX 1
#include <Windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#include <termios.h>
#include <csignal> // Ooooo...
#include <poll.h>
#endif

using namespace hecktui;

//static constexpr bool kEnableRemapRightClickToShift = true;

#ifdef _WIN32
#define HECKTUI_USE_ANSI_INPUT 1
#else
#define HECKTUI_USE_ANSI_INPUT 1
#endif

using Clock = std::chrono::steady_clock;

#ifndef _WIN32
#if HECKTUI_USE_ANSI_INPUT
static void resetTerminal()
{
	// https://unix.stackexchange.com/questions/752901/is-there-a-way-to-have-reset-command-reset-input-settings-without-clearing-the-s/752906#752906
	//const std::string_view buf = "\033[?1006l\033[?1003l\033[?1002l\033[?1000l\033[?9l\x1b[?25h\x1b[3 q\n";
	const std::string_view buf = "\033[?1006l\033[?1003l\033[?1002l\033[?1000l\033[?9l\x1b[!p\n";
#ifdef _WIN32
	fwrite(buf.data(), buf.size(), 1, stdout);
#else
	write(0, buf.data(), buf.size());
	
	// Terminal non-canonical mode.
	struct termios raw {};
	tcgetattr(STDIN_FILENO, &raw);
	raw.c_lflag |= ECHO | ICANON;
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
#endif
}

static void signal_handler([[maybe_unused]] int sig)
{
	//resetTerminal();
	std::quick_exit(EXIT_FAILURE);
}
#endif
#endif

/*
static bool EnableVTMode()
{
	// Set output mode to handle virtual terminal sequences
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hOut == INVALID_HANDLE_VALUE)
		return false;

	HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
	if (hIn == INVALID_HANDLE_VALUE)
		return false;

	DWORD dwMode = 0;
	if (!GetConsoleMode(hOut, &dwMode))
		return false;

	dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	if (!SetConsoleMode(hOut, dwMode))
		return false;

	if (!GetConsoleMode(hIn, &dwMode))
		return false;

	dwMode |= ENABLE_VIRTUAL_TERMINAL_INPUT;
	dwMode &= ~ENABLE_LINE_INPUT;
	if (!SetConsoleMode(hIn, dwMode))
		return false;
	return true;
}*/

static bool EnableInputMode()
{
#ifdef _WIN32
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);

	DWORD dwMode = 0;
	if (!GetConsoleMode(hOut, &dwMode))
		return false;

	//dwMode |= ENABLE_WRAP_AT_EOL_OUTPUT;

	// This stops the flicker in the center of the console when using Windows Terminal, for some reason (and you need explicit newlines with this).
	dwMode &= ~ENABLE_WRAP_AT_EOL_OUTPUT;

	dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;

	if (!SetConsoleMode(hOut, dwMode))
		return false;

	HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
	if (hIn == INVALID_HANDLE_VALUE)
		return false;

	if (!GetConsoleMode(hIn, &dwMode))
		return false;

	dwMode = 0;
#if HECKTUI_USE_ANSI_INPUT
	dwMode |= ENABLE_VIRTUAL_TERMINAL_INPUT;
#else
	dwMode |= ENABLE_MOUSE_INPUT;
	dwMode |= ENABLE_WINDOW_INPUT;
#endif

	dwMode |= ENABLE_EXTENDED_FLAGS;
	//dwMode |= ENABLE_PROCESSED_INPUT;
	dwMode &= ~ENABLE_PROCESSED_INPUT;
	dwMode &= ~ENABLE_LINE_INPUT;
	dwMode &= ~ENABLE_ECHO_INPUT;
	dwMode &= ~ENABLE_QUICK_EDIT_MODE;
	if (!SetConsoleMode(hIn, dwMode))
		return false;
#endif

#if HECKTUI_USE_ANSI_INPUT
	std::cout << "\033[?1000h";
	std::cout << "\033[?1003h";
	std::cout << "\033[?1015h";
	std::cout << "\033[?1006h";
	std::cout << "\033>?1s" << std::flush;
#endif

#ifndef _WIN32
	// Terminal non-canonical mode.
	struct termios raw {};
	tcgetattr(STDIN_FILENO, &raw);
	raw.c_iflag &= ~IXON; // IXON? What's that? It's the thing that stops Ctrl+S from working.
	raw.c_lflag &= ~(ECHO | ICANON);
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

	// Do everything we can to reset the terminal on exit.
	std::once_flag flag;
	std::call_once(flag, [] {
		std::atexit(resetTerminal);
		std::at_quick_exit(resetTerminal);
		static const std::terminate_handler prevHandler = std::set_terminate([] {
			resetTerminal();
			return prevHandler();
			});
		signal(SIGINT, signal_handler);
		signal(SIGTERM, signal_handler);
		});
	
#endif

	return true;
}

ConsoleDevice::ConsoleDevice()
{
	if (!EnableInputMode())
		throw std::runtime_error("Could not enable console input modes.");
	//std::wcout << L"\x1b[?1003h" << std::flush;
	// Alt screen buffer. Disables the scrollbar, but leaves a gap instead.
	// If only there was a way to "Disable Scroll Forward" programmatically.
	//std::wcout << L"\x1b[?1049h" << std::flush;
}

ivec2 ConsoleDevice::getDim() const
{
#ifdef _WIN32
	CONSOLE_SCREEN_BUFFER_INFO screenBufferInfo{};
	if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &screenBufferInfo))
		return { screenBufferInfo.srWindow.Right - screenBufferInfo.srWindow.Left + 1, screenBufferInfo.srWindow.Bottom - screenBufferInfo.srWindow.Top + 1 };
#else
	// https://stackoverflow.com/questions/23369503/get-size-of-terminal-window-rows-columns
	struct winsize w {};
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0 && w.ws_row > 0)
		return { w.ws_col, w.ws_row };
#endif

	return { 80, 25 };
}

void ConsoleDevice::setShiftClickHackEnabled(bool x)
{
	mShiftClickHackEnabled = x;
}

static void formatTo(std::wstring& str, std::wstring_view s)
{
	str += s;
}

static void formatTo(std::wstring& str, wchar_t s)
{
	str += s;
}

static void formatTo(std::wstring& str, int x)
{
	char buf[std::numeric_limits<int>::digits10 + 1];
	const auto [ptr, ec] = std::to_chars(buf, std::end(buf), x);
	wchar_t wbuf[std::size(buf)];
	std::ranges::copy(buf, wbuf);
	str += std::wstring_view(wbuf, ptr - buf);
}

static void formatTo(std::wstring& str, const auto&... x) requires (sizeof...(x) >= 2)
{
	(formatTo(str, x), ...);
}

void ConsoleDevice::print(const Framebuffer& fb)
{
	//static bool xxx = false;

	const ivec2 dim = fb.getDim();
	std::wstring output;
	output.reserve(size_t(dim.x) * dim.y * 2);

	//if (xxx)
	//	std::wcout << L"\x1b""7";

#ifdef _WIN32
	const HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
	(void)SetConsoleCursorPosition(console, {});
#else
	output += L"\x1b[1;1H";
#endif
	
	
	

	Pixel last{
		.colour = { EColour::Silver, EColour::Black },
	};

	//auto backInserter = std::back_inserter(output);

	// Reset.
	output += L"\x1b[38;0m";
	// https://en.wikipedia.org/wiki/ANSI_escape_code#8-bit

	formatTo(output, L"\x1b[38;5;", (int)last.colour.text.code, L"m\x1b[48;5;", (int)last.colour.back.code, L'm');

	for (int y = 0; y < dim.y; ++y)
	{
		if (y > 0)
			output += L'\n';

		for (int x = 0; x < dim.x; ++x)
		{
			const Pixel& pixel = fb.getPixel({ x, y });

			if (pixel.colour.text.code != last.colour.text.code)
				formatTo(output, L"\x1b[38;5;", (int)pixel.colour.text.code, L'm');
			if (pixel.colour.back.code != last.colour.back.code)
				formatTo(output, L"\x1b[48;5;", (int)pixel.colour.back.code, L'm');
			if (pixel.bold != last.bold)
				output += pixel.bold ? L"\x1b[1m" : L"\x1b[22m";
			//if (pixel.italic != last.italic)
			//	output += pixel.italic ? L"\x1b[3m" : L"\x1b[23m";
			if (pixel.underline != last.underline)
				output += pixel.underline ? L"\x1b[4m" : L"\x1b[24m";
			//if (pixel.blink != last.blink)
			//	output += pixel.blink ? L"\x1b[5m" : L"\x1b[25m";

			output += pixel.c;

			last = pixel;
		}
	}

	
	//if (!xxx)
	{
		//xxx = true;
		const InputCaret caret = fb.getInputCaret();

		if (caret.style == EInputCaretStyle::None)
			output += L"\x1b[?25l";
		else
		{
			output += L"\x1b[?25h";
			switch (caret.style)
			{
			default:
			case EInputCaretStyle::Block:
				output += L"\x1b[1 q";
				break;
			case EInputCaretStyle::Underline:
				output += L"\x1b[3 q";
				break;
			case EInputCaretStyle::Bar:
				output += L"\x1b[5 q";
				break;
			}
		}

		formatTo(output, L"\x1b[", caret.coord.y + 1, L';', caret.coord.x + 1, L'H');
	}
	//else
	//	output += L"\x1b""8";
	
#ifdef _WIN32
	DWORD numWritten = 0;
	WriteConsoleW(console, output.data(), (DWORD)output.size(), &numWritten, nullptr);
#else
	//fputws(output.c_str(), stdout);
	fputs(heck::toUtf8(output).c_str(), stdout);
	fflush(stdout);
#endif
}

EventStorage ConsoleDevice::poll()
{
	if (mEventQueue.empty())
		return {};
	else
	{
		EventStorage e = mEventQueue.back();
		mEventQueue.pop_back();
		return e;
	}
}

void ConsoleDevice::wait()
{
	if (!mEventQueue.empty())
		return;

#if !HECKTUI_USE_ANSI_INPUT
	(void)WaitForSingleObject(GetStdHandle(STD_INPUT_HANDLE), INFINITE);

	const HANDLE console = GetStdHandle(STD_INPUT_HANDLE);

	ivec2 origin{};

	CONSOLE_SCREEN_BUFFER_INFO screenBufferInfo{};
	if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &screenBufferInfo))
		origin = { screenBufferInfo.srWindow.Left, screenBufferInfo.srWindow.Top };

	while (mEventQueue.size() < 20)
	{
		DWORD numEvents = 0;
		if (!GetNumberOfConsoleInputEvents(console, &numEvents) || numEvents == 0)
			break;

		INPUT_RECORD record{};
		DWORD numEventsRead = 0;
		if (!ReadConsoleInputW(console, &record, 1, &numEventsRead) || numEventsRead == 0)
			break;

		switch (record.EventType)
		{
		case FOCUS_EVENT:
			continue;
		case KEY_EVENT:
		{
			const auto& e = record.Event.KeyEvent;
			if (!e.bKeyDown)
				continue;

			EActionKey key{};

			switch (e.wVirtualKeyCode)
			{
			case VK_ESCAPE: key = EActionKey::Esc; break;
			case VK_F1: key = EActionKey::F1; break;
			case VK_F2: key = EActionKey::F2; break;
			case VK_F3: key = EActionKey::F3; break;
			case VK_F4: key = EActionKey::F4; break;
			case VK_F5: key = EActionKey::F5; break;
			case VK_F6: key = EActionKey::F6; break;
			case VK_F7: key = EActionKey::F7; break;
			case VK_F8: key = EActionKey::F8; break;
			case VK_F9: key = EActionKey::F9; break;
			case VK_F10: key = EActionKey::F10; break;
			case VK_F11: key = EActionKey::F11; break;
			case VK_F12: key = EActionKey::F12; break;
			case VK_TAB: key = EActionKey::Tab; break;
			case VK_BACK: key = EActionKey::Backspace; break;
			case VK_RETURN: key = EActionKey::Enter; break;
			case VK_DELETE: key = EActionKey::Delete; break;
			case VK_INSERT: key = EActionKey::Insert; break;
			case VK_HOME: key = EActionKey::Home; break;
			case VK_END: key = EActionKey::End; break;
			case VK_PRIOR: key = EActionKey::PageUp; break;
			case VK_NEXT: key = EActionKey::PageDown; break;
			case VK_UP: key = EActionKey::Up; break;
			case VK_DOWN: key = EActionKey::Down; break;
			case VK_LEFT: key = EActionKey::Left; break;
			case VK_RIGHT: key = EActionKey::Right; break;
			default:
				//if (L'A' <= e.wVirtualKeyCode && e.wVirtualKeyCode <= L'Z')
				//	c = wchar_t(e.wVirtualKeyCode);
				//if (L'0' <= e.wVirtualKeyCode && e.wVirtualKeyCode <= L'9')
				//	c = wchar_t(e.wVirtualKeyCode);
				break;
			}

			KeyEvent genericKeyEvent{ {}, ModifierKeyState{
					.shift = bool(e.dwControlKeyState & SHIFT_PRESSED),
					.ctrl = bool(e.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)),
					.alt = bool(e.dwControlKeyState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)),
				}
			};

			// Detect AltGr
			if ((e.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_ALT_PRESSED)) == (LEFT_CTRL_PRESSED | RIGHT_ALT_PRESSED))
				genericKeyEvent.ctrl = false;

			if (key != EActionKey::None)
			{
 				genericKeyEvent.type = EConsoleEventType::ActionKeyPressed;
				mEventQueue.push_back(EventStorage{ .var = ActionKeyEvent{ genericKeyEvent, key } });
			}
			else if (e.uChar.UnicodeChar)
			{
				genericKeyEvent.type = EConsoleEventType::CharKeyPressed;

				// HACK: The ANSI ctrl combo offset.
				// Note that Ctrl+digit doesn't seem to result in any input.
				wchar_t c = e.uChar.UnicodeChar;
				if (genericKeyEvent.ctrl && 1 <= c && c <= 1 + (L'Z' - L'A'))
					c += L'A' - 1;

				if (genericKeyEvent.ctrl && genericKeyEvent.alt)
				{
					// Hack some keys. Hopefully, this won't affect typing these characters on other keyboard layouts.
					switch (c)
					{
					case L'á': c = L'a'; break;
					case L'é': c = L'e'; break;
					case L'í': c = L'i'; break;
					case L'ó': c = L'o'; break;
					case L'ú': c = L'u'; break;
					default:
						break;
					}
				}

				mEventQueue.push_back(EventStorage{ .var = CharKeyEvent{ genericKeyEvent, c } });
			}
			// else, unrecognised key

			break;
		}
		case MOUSE_EVENT:
		{
			const auto& e = record.Event.MouseEvent;

			

			MouseEvent genericMouseEvent{ {}, ModifierKeyState{
					.shift = bool(e.dwControlKeyState & SHIFT_PRESSED),
					.ctrl = bool(e.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)),
					.alt = bool(e.dwControlKeyState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)),
				},
				ivec2{ e.dwMousePosition.X, e.dwMousePosition.Y } - origin,
				bool(e.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED),
				bool(e.dwButtonState & FROM_LEFT_2ND_BUTTON_PRESSED),
				bool(e.dwButtonState & RIGHTMOST_BUTTON_PRESSED),
			};

			//if constexpr (kEnableRemapRightClickToShift)
			//	genericMouseEvent.shift |= genericMouseEvent.right;

			//const bool newShiftRightButtonState = bool(e.dwButtonState & RIGHTMOST_BUTTON_PRESSED) && genericMouseEvent.shift;
			//
			//if (e.dwButtonState & RIGHTMOST_BUTTON_PRESSED)
			//	std::clog << "XXX" << std::endl;
			//if (newShiftRightButtonState)
			//	std::clog << "YYY" << std::endl;
			//
			//genericMouseEvent.left |= newShiftRightButtonState;

			if (mShiftClickHackEnabled)
			{
				// On right mouse down or up
				const bool lastPhysicalRightButtonState = mLastPhysicalRightButtonState;
				mLastPhysicalRightButtonState = genericMouseEvent.right;
				if (genericMouseEvent.right || (lastPhysicalRightButtonState && !genericMouseEvent.right))
				{
					genericMouseEvent.left = genericMouseEvent.right;
					genericMouseEvent.shift = true;
					genericMouseEvent.right = false;
				}
			}
			else
				mLastPhysicalRightButtonState = false;

			// Detect AltGr
			if ((e.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_ALT_PRESSED)) == (LEFT_CTRL_PRESSED | RIGHT_ALT_PRESSED))
				genericMouseEvent.ctrl = false;

			// If it was none of those, assume it was a mouse button event.
			if (genericMouseEvent.left != mLastMouseState.left)
			{
				genericMouseEvent.type = genericMouseEvent.left ? EConsoleEventType::MouseButtonDown : EConsoleEventType::MouseButtonUp;
				mEventQueue.push_back(EventStorage{ .var = MouseButtonEvent{ genericMouseEvent, EMouseButton::Left } });
			}
			if (genericMouseEvent.middle != mLastMouseState.middle)
			{
				genericMouseEvent.type = genericMouseEvent.middle ? EConsoleEventType::MouseButtonDown : EConsoleEventType::MouseButtonUp;
				mEventQueue.push_back(EventStorage{ .var = MouseButtonEvent{ genericMouseEvent, EMouseButton::Middle } });
			}
			if (genericMouseEvent.right != mLastMouseState.right)
			{
				genericMouseEvent.type = genericMouseEvent.right ? EConsoleEventType::MouseButtonDown : EConsoleEventType::MouseButtonUp;
				mEventQueue.push_back(EventStorage{ .var = MouseButtonEvent{ genericMouseEvent, EMouseButton::Right } });
			}

			if (e.dwEventFlags & MOUSE_MOVED)
			{
				if (genericMouseEvent.coord != mLastMouseState.coord)
				{
					genericMouseEvent.type = EConsoleEventType::MouseMove;
					mEventQueue.push_back(EventStorage{ .var = MouseScrollEvent{ genericMouseEvent } });
				}
			}
			if (e.dwEventFlags & MOUSE_WHEELED)
			{
				genericMouseEvent.type = EConsoleEventType::MouseScroll;
				mEventQueue.push_back(EventStorage{ .var = MouseScrollEvent{ genericMouseEvent, -(std::int16_t)HIWORD(e.dwButtonState) / WHEEL_DELTA } });
			}
			if (e.dwEventFlags & DOUBLE_CLICK)
			{
				genericMouseEvent.type = EConsoleEventType::MouseButtonDoubleClick;
				if (genericMouseEvent.left != mLastMouseState.left)
					mEventQueue.push_back(EventStorage{ .var = MouseButtonEvent{ genericMouseEvent, EMouseButton::Left } });
				if (genericMouseEvent.middle != mLastMouseState.middle)
					mEventQueue.push_back(EventStorage{ .var = MouseButtonEvent{ genericMouseEvent, EMouseButton::Middle } });
				if (genericMouseEvent.right != mLastMouseState.right)
					mEventQueue.push_back(EventStorage{ .var = MouseButtonEvent{ genericMouseEvent, EMouseButton::Right } });
			}

			mLastMouseState = genericMouseEvent;
			//mShiftRightButtonState = newShiftRightButtonState;
			break;
		}
		// Ah yes, buffer size, not window size.
		case WINDOW_BUFFER_SIZE_EVENT:
			mEventQueue.emplace_back();
			//	mEventQueue.push_back(EventStorage{ .var = ConsoleResizeEvent{ { EConsoleEventType::Resize },{
			//							  record.Event.WindowBufferSizeEvent.dwSize.X,
			//							  record.Event.WindowBufferSizeEvent.dwSize.Y,
			//						  } } });
			break;
		default:
			break;
		}
	}
#else
	
	
#ifdef _WIN32
	const HANDLE console = GetStdHandle(STD_INPUT_HANDLE);
	const auto readChar = [console] {
		for (;;)
		{
			INPUT_RECORD record{};
			DWORD numEventsRead = 0;
			if (!ReadConsoleInputW(console, &record, 1, &numEventsRead))
			{
				//std::clog << GetLastError() << std::endl;
				throw std::runtime_error("Console input failure.");
			}
			if (numEventsRead > 0) // For some odd reason, ReadConsoleInputW returns no events when you press the Ctrl key in Windows Terminal.
				if (record.EventType == KEY_EVENT && record.Event.KeyEvent.uChar.UnicodeChar && record.Event.KeyEvent.bKeyDown)
					return record.Event.KeyEvent.uChar.UnicodeChar;
		}
		};

	const auto tryReadChar = [&, console] {
		DWORD n{};
		if (GetNumberOfConsoleInputEvents(console, &n) && n > 0)
			return readChar();
		else
			return L'\0';
		};
	
#else
	static_assert(WCHAR_MAX >= 0x10'0000);
	unsigned char buf[10]{};
	size_t bufStart = 0;
	size_t bufEnd = 0;
	const auto readByte = [&] {
		if (bufStart >= bufEnd)
		{
			bufStart = 0;
			bufEnd = read(STDIN_FILENO, buf, std::size(buf));
			if (bufEnd <= 0 || std::size(buf) < bufEnd)
				throw std::runtime_error("Console input failure.");
		}
		return buf[bufStart++];
		};
	const auto readChar = [&]() -> wchar_t {
		// Conversion from UTF-8!
		const char8_t firstByte = readByte();
		//return static_cast<wchar_t>(firstByte);
		uint32_t x = 0;
		switch (const int numBytes = static_cast<int>(std::countl_one(static_cast<uint8_t>(firstByte))))
		{
		case 4:
		case 3:
		case 2:
			x |= (firstByte & ((1 << (7 - numBytes)) - 1)) << (6 * (numBytes - 1));
			for (int i = numBytes - 2; i >= 0; --i)
			{
				const char8_t extByte = readByte();
				if ((extByte & 0b11'00'0000) != 0b10'00'0000)
					return  L'�';
				x |= (extByte & 0b111111) << (6 * i);
			}
			break;
		case 0:
			x = firstByte;
			break;
		default:
			x = L'�';
			break;
		}
		
		return static_cast<wchar_t>(x);
		
		};
	const auto hasPendingInput = [] {
		struct pollfd fd {};
		fd.fd = STDIN_FILENO;
		fd.events = POLLIN; // "There is data to read."
		fd.revents = 0;
		int ret = ::poll(&fd, 1, 0);
		return ret > 0 && (fd.revents & POLLIN) != 0;
		};
	const auto tryReadChar = [&]() {
		//int n{};
		if (bufStart < bufEnd || hasPendingInput())
			return readChar();
		else
			return L'\0';
		};
#endif
	AnsiParser parser(mLastMouseState);
	parser.inject(readChar(), mEventQueue);
	while (const wchar_t c = tryReadChar())
		parser.inject(c, mEventQueue);
	while (!parser.isComplete())
	{
		// AnsiParser has a /possibly/ incomplete sequence. Wait a little and read more.
#ifdef _WIN32
		std::this_thread::yield();
#else
		usleep(100);
#endif
		bool progress = false;
		while (const wchar_t c = tryReadChar())
		{
			progress = true;
			parser.inject(c, mEventQueue);
		}
		if (!progress)
			break;
	}
	parser.flush(mEventQueue);

	//std::clog << std::endl;

	const bool hasLeftClick = std::ranges::contains(mEventQueue, true, [](const EventStorage& storage) {
		const ConsoleEvent* const e = storage;
		return e->type == EConsoleEventType::MouseButtonDown && static_cast<const MouseButtonEvent&>(*e).left;
		});

	if (hasLeftClick)
	{
		const int64_t timestamp = std::chrono::duration_cast<std::chrono::duration<int64_t, std::milli>>(Clock::now().time_since_epoch()).count();
		if (mLastLeftClickMouseStateTimestamp)
		{
			if (timestamp - mLastLeftClickMouseStateTimestamp <= getDoubleClickMilliseconds())
			{
				MouseButtonEvent e{ mLastMouseState };
				e.type = EConsoleEventType::MouseButtonDoubleClick;
				e.button = EMouseButton::Left;
				mEventQueue.emplace_back(e);
			}
		}
		mLastLeftClickMouseStateTimestamp = timestamp;
	}
	
	mLastPhysicalRightButtonState = false;
#endif

	std::ranges::reverse(mEventQueue);
}