#include <HeckTextUI/Core.h>

#include <array>
#include <sstream>
#include <ranges>
#include <limits.h>

using namespace hecktui;

static std::wostream& operator<<(std::wostream& os, const ConsoleEvent& e)
{
	switch (e.type)
	{
		using enum EConsoleEventType;
	case None:					 /**/ os << L"None"; break;
	case MouseMove:				 /**/ os << L"MouseMove"; break;
	case MouseButtonDown:		 /**/ os << L"MouseButtonDown"; break;
	case MouseButtonDoubleClick: /**/ os << L"MouseButtonDoubleClick"; break;
	case MouseButtonUp:			 /**/ os << L"MouseButtonUp"; break;
	case MouseScroll:			 /**/ os << L"MouseScroll"; break;
	case ActionKeyPressed:		 /**/ os << L"ActionKeyPressed"; break;
	case CharKeyPressed:		 /**/ os << L"CharKeyPressed"; break;
	case MouseCapture:			 /**/ os << L"MouseCapture"; break;
	default: os << L"Unknown"; break;
	}
	return os;
}

static std::wostream& operator<<(std::wostream& os, const ModifierKeyState& e)
{
	return os << L"ctrl=" << e.ctrl << L",shift=" << e.shift << L",alt=" << e.alt;
}

static std::wostream& operator<<(std::wostream& os, const MouseEvent& e)
{
	return os << static_cast<const ConsoleEvent&>(e) << L' ' << static_cast<const ModifierKeyState&>(e) << L' ' << e.coord << L' ' << e.left << e.middle << e.right;
}

static std::wostream& operator<<(std::wostream& os, const MouseButtonEvent& e)
{
	return os << static_cast<const MouseEvent&>(e) << L' ' << (int)e.button;
}

static std::wostream& operator<<(std::wostream& os, const MouseScrollEvent& e)
{
	return os << static_cast<const MouseEvent&>(e) << L' ' << e.scroll;
}

static std::wostream& operator<<(std::wostream& os, const KeyEvent& e)
{
	return os << static_cast<const ConsoleEvent&>(e) << L' ' << static_cast<const ModifierKeyState&>(e);
}

static std::wostream& operator<<(std::wostream& os, const ActionKeyEvent& e)
{
	static constexpr auto kNames = std::to_array<std::wstring_view>({
		L"None",
		L"Esc",
		L"F1",
		L"F2",
		L"F3",
		L"F4",
		L"F5",
		L"F6",
		L"F7",
		L"F8",
		L"F9",
		L"F10",
		L"F11",
		L"F12",
		L"Tab",
		L"Backspace",
		L"Enter",
		L"Delete",
		L"Insert",
		L"Home",
		L"End",
		L"PageUp",
		L"PageDown",
		L"Up",
		L"Down",
		L"Left",
		L"Right",
		});
	return os << static_cast<const KeyEvent&>(e) << L' ' << kNames[std::to_underlying(e.key)];
}

static std::wostream& operator<<(std::wostream& os, const CharKeyEvent& e)
{
	os << static_cast<const KeyEvent&>(e) << L" '";
	if (e.c < 32)
	{
		os << L"\\x" << std::hex;
		os.width(2);
		os.fill(L'0');
		os << (int)e.c;
		os.width(0);
		os.fill(L' ');
		os << std::dec;
	}
	else
		os << e.c;
	return os << L'\'';
}

int main()
{
	ConsoleDevice dev;
	Framebuffer fb(dev.getDim());

	std::vector<std::wstring> lines;

	for (;;)
	{
		fb.clear();
		for (const auto& [i, line] : lines | std::views::drop(std::max(0, (int)lines.size() - fb.getDim().y)) | std::views::enumerate)
			fb.drawText(line, iaabb2::sized({ 0, (int)i }, { INT_MAX, 1 }), EAlign::Left, EAlign::Top, {}, false);

		dev.print(fb);
		dev.wait();

		while (const EventStorage storage = dev.poll())
		{
			std::wostringstream os;
			dispatch(storage.get(), [&](const auto& e) { os << e; });
			lines.push_back(os.str());
		}
	}
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
