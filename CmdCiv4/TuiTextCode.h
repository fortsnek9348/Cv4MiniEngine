#pragma once

#include <HeckTextUI/Core.h>

#include <CvEnums.h>

void initialiseTextCodeTags();

// Processes the bracket tags of CvDllTranslator
// Processes the resulting XML too.
std::vector<hecktui::Pixel> renderCiv4TextCode(std::wstring_view textcode, hecktui::Pixel defPixel);
std::wstring lowerCiv4TextCodeToXml(std::wstring_view textcode);
std::wstring stripPixels(std::span<const hecktui::Pixel> pixels);

class NiColorA;

hecktui::EColour toTuiColour(ColorTypes civ4Colour);
hecktui::EColour toTuiColour(NiColorA civ4Colour);
hecktui::EColour darken(hecktui::EColour, int n = 1);
hecktui::EColour lighten(hecktui::EColour, int n = 1);