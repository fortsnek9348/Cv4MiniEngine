#pragma once

#include <CommonStuff/CompilerMacros.h>
#include <CommonStuff/aabb.h>

#include <cassert>

#ifdef HECKCONSOLEUI_EXPORTS
#define HECKCONSOLEUI_EXPORT HECK_DLLEXPORT
#else
#define HECKCONSOLEUI_EXPORT HECK_DLLIMPORT
#endif

#include "Colour.h"

#include <vector>
#include <variant>
#include <string>
#include <span>

namespace hecktui
{
	using ivec2 = heck::ivec2;
	using iaabb2 = heck::iaabb2;

	struct PixelColouring
	{
		Colour text{ EColour::Silver };
		Colour back{ EColour::Black };
	};

	struct Pixel
	{
		wchar_t c = L' ';

		PixelColouring colour{};

		//bool blink : 1 = false;
		bool bold : 1 = false;
		//bool italic : 1 = false;
		bool underline : 1 = false;
		bool autoMergeBorder : 1 = false;
	};

	enum class EAxis
	{
		Horizontal,
		Vertical,
	};

	
	enum class EInputCaretStyle
	{
		None,
		Underline,
		Block,
		Bar,
	};

	struct InputCaret
	{
		EInputCaretStyle style{};
		ivec2 coord{};
	};

	enum class EBorderStyle
	{
		None,
		Thin,
		Thick,
		Double,
		Rounded,
		Bracketed,
	};

	enum class EAlign : uint8_t
	{
		Start,
		Center,
		End,

		Left = Start,
		Right = End,

		Top = Start,
		Middle = Center,
		Bottom = End,
	};

	std::wstring_view consumeWrappedTextLine(std::wstring_view& text, int width) noexcept;
	std::wstring_view consumeMultilineTextLine(std::wstring_view& text) noexcept;
	HECKCONSOLEUI_EXPORT ivec2 calcWrappedTextMinSize(std::wstring_view text) noexcept;
	// Note that the returned width may be slightly less.
	HECKCONSOLEUI_EXPORT ivec2 calcWrappedTextSizeForWidth(std::wstring_view text, int width) noexcept;
	HECKCONSOLEUI_EXPORT ivec2 calcMultilineTextSize(std::wstring_view text) noexcept;


	class HECKCONSOLEUI_EXPORT Framebuffer
	{
	public:
		explicit Framebuffer(ivec2 dim);

		ivec2 getDim() const;

		void setInputCaret(InputCaret);

		void clear();

		void setScissorRect(iaabb2) noexcept;
		const iaabb2& getScissorRect() const noexcept;
		iaabb2 intersectScissorRect(iaabb2) noexcept;

		void draw(ivec2 coord, const Pixel& pixel);
		void recolour(ivec2 coord, PixelColouring colouring);

		void drawHLine(ivec2 coord, int length, Pixel pixel);
		void drawVLine(ivec2 coord, int length, Pixel pixel);

		void drawHLine(ivec2 coord, int length, EBorderStyle style, PixelColouring colouring, bool autoMerge = true);
		void drawHLineColouring(ivec2 coord, int length, PixelColouring colouring);
		void drawVLine(ivec2 coord, int length, EBorderStyle style, PixelColouring colouring, bool autoMerge = true);
		void drawBox(iaabb2 rect, EBorderStyle style, PixelColouring colouring);
		static wchar_t getBorderChar(EBorderStyle style, ivec2 dir);

		void drawFilledBox(iaabb2 rect, EBorderStyle style, PixelColouring colouring);
		void drawFilledBox(iaabb2 rect, const Pixel& pixel);
		void drawFilledBoxColouring(iaabb2 rect, PixelColouring colouring);

		void drawText(std::wstring_view str, const iaabb2& rect, EAlign halign, EAlign valign, PixelColouring colouring, bool wrap);
		void drawUnderline(ivec2 coord, int length);

		void blit(ivec2 coord, std::span<const Pixel> src);
		void blit(iaabb2 rect, std::span<const Pixel> src, EAlign halign, EAlign valign, bool wrap);
		void blitWithDefaultColouring(ivec2 coord, std::span<const Pixel> src, PixelColouring defColouring);
		void blitWithDefaultColouring(iaabb2 rect, std::span<const Pixel> src, EAlign halign, EAlign valign, bool wrap, PixelColouring defColouring);
		void blitAutoContrastText(iaabb2 rect, std::span<const Pixel> src, EAlign halign, EAlign valign);

		void mergeBorders();

		InputCaret getInputCaret() const;
		
		const Pixel& getPixel(ivec2 coord) const;
		Pixel& getPixel(ivec2 coord);

	private:
		ivec2 mDim{};
		iaabb2 mStencil{};
		std::vector<Pixel> mPixels;

		InputCaret mInputCaret{};
	};

	enum class EConsoleEventType
	{
		None,
		MouseMove,
		MouseButtonDown,
		MouseButtonDoubleClick,
		MouseButtonUp,
		MouseScroll,
		ActionKeyPressed,
		CharKeyPressed,

		MouseCapture,
	};

	enum class EActionKey : uint8_t
	{
		None,
		Esc,
		F1,
		F2,
		F3,
		F4,
		F5,
		F6,
		F7,
		F8,
		F9,
		F10,
		F11,
		F12,
		Tab,
		Backspace,
		Enter,
		Delete,
		Insert,
		Home,
		End,
		PageUp,
		PageDown,
		Up,
		Down,
		Left,
		Right,
	};

	struct ConsoleEvent
	{
		EConsoleEventType type{};

		explicit operator bool() const
		{
			return type != EConsoleEventType::None;
		}
	};

	struct ModifierKeyState
	{
		bool shift = false;
		bool ctrl = false;
		bool alt = false;
	};

	enum class EMouseButton
	{
		None,
		Left,
		Middle,
		Right,
	};

	struct MouseEvent : ConsoleEvent, ModifierKeyState
	{
		ivec2 coord{};
		bool left = false;
		bool middle = false;
		bool right = false;
	};

	struct MouseMoveEvent : MouseEvent
	{
	};

	struct MouseButtonEvent : MouseEvent
	{
		EMouseButton button{};
	};

	struct MouseCaptureEvent : MouseEvent
	{
	};

	struct MouseScrollEvent : MouseEvent
	{
		int scroll = 0;
	};

	struct KeyEvent : ConsoleEvent, ModifierKeyState
	{
	};

	struct ActionKeyEvent : KeyEvent
	{
		EActionKey key{};
	};

	struct CharKeyEvent : KeyEvent
	{
		wchar_t c{};
	};

	struct EventStorage
	{
		std::variant<
			MouseMoveEvent,
			MouseButtonEvent,
			MouseCaptureEvent,
			MouseScrollEvent,
			ActionKeyEvent,
			CharKeyEvent
		> var;

		operator const ConsoleEvent* () const&
		{
			return std::visit([](const auto& e) -> const ConsoleEvent* { return e.type != EConsoleEventType::None ? &e : nullptr; }, var);
		}

		const ConsoleEvent& get() const &
		{
			return std::visit([](const ConsoleEvent& e) -> const ConsoleEvent& { return e; }, var);
		}
	};


	inline auto dispatch(const ConsoleEvent& e, auto f)
	{
		switch (e.type)
		{
		case EConsoleEventType::MouseMove:
			return f(static_cast<const MouseMoveEvent&>(e));
		case EConsoleEventType::MouseButtonDown:
		case EConsoleEventType::MouseButtonDoubleClick:
		case EConsoleEventType::MouseButtonUp:
			return f(static_cast<const MouseButtonEvent&>(e));
		case EConsoleEventType::MouseCapture:
			return f(static_cast<const MouseCaptureEvent&>(e));
		case EConsoleEventType::MouseScroll:
			return f(static_cast<const MouseScrollEvent&>(e));
		case EConsoleEventType::ActionKeyPressed:
			return f(static_cast<const ActionKeyEvent&>(e));
		case EConsoleEventType::CharKeyPressed:
			return f(static_cast<const CharKeyEvent&>(e));
		default:
			return f(e);
		}
	}


	class HECKCONSOLEUI_EXPORT ConsoleDevice
	{
	public:
		ConsoleDevice();

		ivec2 getDim() const;

		// Right-click mapped to shift-left-click.
		void setShiftClickHackEnabled(bool);

		void print(const Framebuffer& fb);

		EventStorage poll();

		void wait();

	private:
		std::vector<EventStorage> mEventQueue;

		MouseEvent mLastMouseState{};
		bool mLastPhysicalRightButtonState = false;

		bool mShiftClickHackEnabled = false;

		int64_t mLastLeftClickMouseStateTimestamp{};
	};

	

	HECKCONSOLEUI_EXPORT void debugLog(std::wstring);
	HECKCONSOLEUI_EXPORT void checkDebugBreakKey();
	HECKCONSOLEUI_EXPORT uint32_t getDoubleClickMilliseconds() noexcept;
	HECKCONSOLEUI_EXPORT int getDefaultVerticalScrollAmount() noexcept;
	
}