#pragma once

#include "inc/HeckTextUI/Core.h"

class AnsiParser
{
public:
	explicit AnsiParser(hecktui::MouseEvent& mouseState);
	void inject(wchar_t c, std::vector<hecktui::EventStorage>& out);
	[[nodiscard]] bool isComplete() const noexcept;
	void flush(std::vector<hecktui::EventStorage>& out);

private:
	unsigned int mState = 0;
	int mData0 = 0;
	int mData1 = 0;
	int mData2 = 0;
	hecktui::MouseEvent& mMouseState;
};