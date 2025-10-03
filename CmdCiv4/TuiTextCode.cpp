#include "TuiTextCode.h"
#include "CvTranslator.h"
#include "Common.h"
#include "CvApp.h"

#include <CvGlobals.h>
#include <CvInfos.h>

#include <CvDllTranslator.h>

#include <CommonStuff/StringConversion.h>

#include <algorithm>
#include <ranges>
#include <cwctype>

namespace
{
	struct TranslatorInfo
	{
		CvWString szTagStartIcon;
		CvWString szTagStartOur;
		CvWString szTagStartCT;
		CvWString szTagStartColour;
		CvWString szTagStartLink;
		CvWString szTagEndLink;
		CvWString szEndLinkReplacement;
		std::map<std::wstring, CvWString> aIconMap;
		std::map<std::wstring, CvWString> aColourMap;
	};

	static TranslatorInfo kInfo{};

	const TranslatorInfo& getTranslatorInfo()
	{
		return kInfo;
	}

	std::optional<uint8_t> tryConsumeUInt8(std::wstring_view& s)
	{
		int value = 0;
		while (!s.empty() && L'0' <= s.front() && s.front() <= L'9')
		{
			value *= 10;
			value += s.front() - L'0';
			s.remove_prefix(1);
			if (value > UINT8_MAX)
				return std::nullopt;
		}
		return static_cast<uint8_t>(value);
	}

	std::optional<hecktui::RGB8> tryConvertToColourRGB(std::wstring_view s)
	{
		const std::optional<uint8_t> r = tryConsumeUInt8(s);
		if (s.empty() || s.front() != L',')
			return std::nullopt;
		s.remove_prefix(1);
		const std::optional<uint8_t> g = tryConsumeUInt8(s);
		if (s.empty() || s.front() != L',')
			return std::nullopt;
		s.remove_prefix(1);
		const std::optional<uint8_t> b = tryConsumeUInt8(s);
		if (!r || !g || !b)
			return std::nullopt;

		if (s.empty())
			return hecktui::RGB8(*r, *g, *b);
		else if (s.front() == L',')
		{
			s.remove_prefix(1);
			if (tryConsumeUInt8(s) && s.empty())
				return hecktui::RGB8(*r, *g, *b);
		}

		return std::nullopt;
	}

	// Why does Civ4 have two layers of textcode. Square brackets or XML, pick one!
	std::wstring lowerToXml(std::wstring_view textcode)
	{
		// This is called during initialisation before `CvDllTranslator::initializeTags` can work.
		// At least for keybindings.
		// In such cases, check that there's no tags.
		if (CvApp::getInstance().symbols.empty())
		{
			assert(!textcode.contains(L'['));
			return std::wstring(textcode);
		}

		const TranslatorInfo& kTranslatorInfo = getTranslatorInfo();

		// TODO: Where the heck does this value come from. It might be stored in string form somewhere.
		constexpr int uiForm = 0;

		std::wstring xml;

		CvWString replacement;

		while (textcode.size())
		{
			const size_t textSpan = std::min(textcode.find_first_of(L"["), textcode.size());
			xml += textcode.substr(0, textSpan);
			textcode.remove_prefix(textSpan);

			if (textcode.empty())
				break;

			// Escape to XML.
			//switch (textcode.front())
			//{
			//case L'<': xml += L"&lt;"; textcode.remove_prefix(1); continue;
			//case L'>': xml += L"&gt;"; textcode.remove_prefix(1); continue;
			//case L'&': xml += L"&amp;"; textcode.remove_prefix(1); continue;
			//default:
			//	break;
			//}

			const size_t tagSpan = textcode.find(L']');

			if (tagSpan == textcode.npos)
			{
				// Could not find the end bracket. Just dump the rest of the string.
				xml += textcode;
				break;
			}

			const CvWString tagWithRBracket(textcode.substr(0, tagSpan + 1));
			const CvWString tag(textcode.substr(0, tagSpan));

			if (tag == L"[SPACE")
				xml += L' ';
			else if (tag == L"[NEWLINE")
				xml += L'\n';
			else if (tag == L"[TAB")
				xml += L"    ";
			else if (tag.starts_with(kTranslatorInfo.szTagStartIcon))
			{
				// Auto-insert a space before icons. We're doing the same in CvTranslator.
				if (xml.size() && !std::iswspace(xml.back()))
					xml += L' ';
				xml += kTranslatorInfo.aIconMap.at(tagWithRBracket);
			}
			else if (tag.starts_with(kTranslatorInfo.szTagStartOur))
			{
				if (CvDllTranslator::replaceOur(tag, uiForm, replacement))
					xml += replacement;
				else
					xml += tag;
			}
			else if (tag.starts_with(kTranslatorInfo.szTagStartCT))
			{
				if (CvDllTranslator::replaceCt(tag, uiForm, replacement))
					xml += replacement;
				else
					xml += tag;
			}
			else if (tag.starts_with(kTranslatorInfo.szTagStartColour))
				xml += kTranslatorInfo.aColourMap.at(tagWithRBracket);
			// Cv4MiniEngine extension: Background colour
			else if (tag.starts_with(L"[BG:COLOR_"))
			{
				// Modify the returned XML.
				std::wstring xmlPiece = kTranslatorInfo.aColourMap.at(L'[' + tagWithRBracket.substr(4));
				if (xmlPiece.starts_with(L"<color"))
					xmlPiece.insert(1, L"bg");
				else if (xmlPiece.starts_with(L"</color"))
					xmlPiece.insert(2, L"bg");
				else
					std::abort();
				xml += xmlPiece;
			}
			else if (tag.starts_with(kTranslatorInfo.szTagStartLink))
			{
				const std::wstring_view linkContents = std::wstring_view(tag).substr(kTranslatorInfo.szTagStartLink.size());

				if (linkContents.size() && linkContents.front() == '=')
				{
					xml += L"<link='";
					xml += linkContents.substr(1);
					xml += L"'>";
				}
				else
					xml += tagWithRBracket;
			}
			else if (tag == kTranslatorInfo.szTagEndLink)
				xml += kTranslatorInfo.szEndLinkReplacement;
			else if (tag == L"[BOLD")
				xml += L"<b>";
			else if (tag == L"[\\BOLD")
				xml += L"</b>";
			else if (tag.starts_with(L"[PARAGRAPH:"))
				xml += L"\n\n";
			else
				xml += tagWithRBracket;

			textcode.remove_prefix(tagSpan + 1);
		}

		return xml;
	}

	// For XML tags only.
	//void toAsciiLower(std::wstring& s)
	//{
	//	for (wchar_t& c : s)
	//		if (L'A' <= c && c <= L'Z')
	//			c += L'a' - L'A';
	//}


	// The most rough XML parsing. In case of error, just spit out the rest of the string.
	std::vector<hecktui::Pixel> renderCiv4Xml(std::wstring_view remaining, hecktui::Pixel defPixel)
	{
		[[maybe_unused]] const std::wstring_view input = remaining; // for debugging
		std::vector<hecktui::Pixel> pixels;
		pixels.reserve(remaining.size());

		static thread_local std::vector<heck::ci_wstring_view> tlsTagStack;
		static thread_local std::vector<hecktui::Pixel> tlsAttribStack;

		static std::vector<heck::ci_wstring_view>& tagStack = tlsTagStack;
		static std::vector<hecktui::Pixel>& attribStack = tlsAttribStack;
		tagStack.clear();
		attribStack.assign({ defPixel });

		std::wstring temp;

		const auto write = [&](std::wstring_view str) {
			pixels.append_range(str | std::views::transform([attribs = attribStack.back()](wchar_t c) {
				hecktui::Pixel p = attribs;
				p.c = c;
				return p;
			}));
		};

		//if (remaining.contains(L"60%"))
		//	_CrtDbgBreak();

		while (remaining.size())
		{
			const size_t textSpan = std::min(remaining.find_first_of(L"<&"), remaining.size());
			write(remaining.substr(0, textSpan));
			remaining.remove_prefix(textSpan);

			if (remaining.empty())
				break;

			const wchar_t leading = remaining.front();
			remaining.remove_prefix(1);

			if (leading == L'<')
			{
				const size_t tagSpan = remaining.find(L'>');
				if (tagSpan >= remaining.npos)
				{
					// Unterminated tag.
					write(remaining);
					break;
				}

				temp = remaining.substr(0, tagSpan);
				//toAsciiLower(temp);
				heck::ci_wstring_view tagContents(temp.c_str(), temp.size());

				bool isBGColour = false;

				if (tagContents.starts_with(L"/"))
				{
					tagContents.remove_prefix(1);
					if (tagStack.size() && tagStack.back() == tagContents)
					{
						if (tagContents.contains(heck::ci_wstring_view(L"color")) || tagContents == L"u")
							attribStack.pop_back();
						tagStack.pop_back();
					}
					else
					{
						// Malformed XML.
						write(remaining);
						break;
					}
				}
				else if (tagContents.starts_with(L"color=") || (isBGColour = tagContents.starts_with(L"bgcolor="), isBGColour))
				{
					const std::wstring_view matchedTag = isBGColour ? L"bgcolor" : L"color";
					tagContents.remove_prefix(matchedTag.size() + 1);
					//int r{}, g{}, b{}, a{};
					//int n{};
					//if (_snwscanf_s(tagContents.data(), tagContents.size(), L"%i,%i,%i,%i%n", &r, &g, &b, &a, &n) == 4 ||
					//	(a = 255, _snwscanf_s(tagContents.data(), tagContents.size(), L"%i,%i,%i%n", &r, &g, &b, &n) == 3))
					if (const std::optional<hecktui::RGB8> colour = tryConvertToColourRGB(std::wstring_view(tagContents)))
					{
						//tagContents.remove_prefix(n);
						//if (!tagContents.empty() || std::max({ unsigned(r), unsigned(g), unsigned(b) }) > UINT8_MAX)
						//{
						//	// Malformed colour tag.
						//	write(remaining);
						//	break;
						//}

						tagStack.emplace_back(matchedTag);
						defPixel = attribStack.back();
						//(isBGColour ? defPixel.colour.back : defPixel.colour.text) = hecktui::toColourFromRGB8({ uint8_t(r), uint8_t(g), uint8_t(b) });
						(isBGColour ? defPixel.colour.back : defPixel.colour.text) = hecktui::toColourFromRGB8(*colour);
						attribStack.push_back(defPixel);
					}
					// Cv4MiniEngine extension: Named colours in XML.
					//else if (const int colour = gGlobals.getInfoTypeForString(CvString(CvWString(std::wstring_view(tagContents.data(), tagContents.size()))).c_str()); colour != -1)
					//{
					//	tagStack.emplace_back(matchedTag);
					//	defPixel = attribStack.back();
					//	(isBGColour ? defPixel.colour.back : defPixel.colour.text) = toTuiColour(ColorTypes(colour));
					//	attribStack.push_back(defPixel);
					//}
					else
					{
						// Malformed colour tag.
						write(remaining);
						break;
					}
				}
				// Cv4MiniEngine extension(?): Underline
				else if (tagContents == L"u")
				{
					tagStack.push_back(L"u");
					defPixel = attribStack.back();
					defPixel.underline = true;
					attribStack.push_back(defPixel);
				}
				// This shows up in Next War, CvDawnOfMan.
				else if (tagContents == L"b")
				{
					tagStack.push_back(L"b");
					defPixel = attribStack.back();
					defPixel.bold = true;
					attribStack.push_back(defPixel);
				}
				else if (tagContents == L"tab")
				{
					write(L"    ");
				}
				else if (tagContents.starts_with(L"font="))
				{
					// Ignore.
					tagStack.push_back(L"font");
				}
				else if (tagContents.starts_with(L"link="))
				{
					tagContents.remove_prefix(std::wstring_view(L"link=").size());
					tagStack.push_back(L"link");
				}
				else if (tagContents.starts_with(L"img="))
				{
					// Image tags. Used in the great general unit attach dialog.
					//tagContents.remove_prefix(std::wstring_view(L"link=").size());

					tagContents.remove_prefix(4);
					const size_t spaceI = std::min(tagContents.find(L' '), tagContents.size());
					const std::wstring_view imgSpec(tagContents.substr(0, spaceI));
					std::wstring_view path;
					if (imgSpec.starts_with(L','))
					{
						const size_t atlasStart = std::min(imgSpec.find(L',', 1), imgSpec.size()) + 1;
						path = imgSpec.substr(atlasStart);
					}
					else
						path = imgSpec;

					static const std::unordered_map<std::wstring_view, std::wstring_view> kImgMap{
						{ L"Art/Interface/Buttons/Promotions_Atlas.dds,8,2", L"✶" },
						{ L"Art/Interface/Buttons/Promotions_Atlas.dds,1,6", L"✶" },
						{ L"Art/Interface/Buttons/Promotions_Atlas.dds,2,6", L"✶" },
						{ L"Art/Interface/Buttons/Promotions_Atlas.dds,3,6", L"✶" },
						{ L"Art/Interface/Buttons/Promotions_Atlas.dds,4,6", L"✶" },
						{ L"Art/Interface/Buttons/Promotions_Atlas.dds,2,5", L"cover" },
						{ L"Art/Interface/Buttons/Promotions_Atlas.dds,4,5", L"shock" },
						{ L"Art/Interface/Buttons/Promotions_Atlas.dds,3,5", L"pinch" },
						{ L"Art/Interface/Buttons/Promotions_Atlas.dds,1,2", L"form" },
						{ L"Art/Interface/Buttons/Promotions_Atlas.dds,1,5", L"charge" },
						{ L"Art/Interface/Buttons/Promotions_Atlas.dds,3,1", L"ambush" }, // Ambush
						{ L"Art/Interface/Buttons/Promotions_Atlas.dds,3,1", L"≈" }, // AMPH
						{ L"Art/Interface/Buttons/Promotions_Atlas.dds,2,4", L"march" },
						{ L"Art/Interface/Buttons/Promotions_Atlas.dds,8,1", L"blitz" },
						{ L"Art/Interface/Buttons/Promotions_Atlas.dds,1,3", L"commando" },
						{ L"Art/Interface/Buttons/Promotions_Atlas.dds,3,4", L"+" },
						{ L"Art/Interface/Buttons/Promotions_Atlas.dds,4,4", L"+" },
						{ L"Art/Interface/Buttons/Promotions_Atlas.dds,5,5", L"H1" },
						{ L"Art/Interface/Buttons/Promotions_Atlas.dds,6,5", L"H2" },
						{ L"Art/Interface/Buttons/Promotions_Atlas.dds,7,5", L"H3" },
						{ L"Art/Interface/Buttons/Promotions_Atlas.dds,6,6", L"F1" },
						{ L"Art/Interface/Buttons/Promotions_Atlas.dds,7,6", L"F2" },
						{ L"Art/Interface/Buttons/Promotions_Atlas.dds,8,6", L"F3" },
						{ L"Art/Interface/Buttons/Promotions_Atlas.dds,5,2", L">Ξ<" }, // CR
						{ L"Art/Interface/Buttons/Promotions_Atlas.dds,6,2", L">Ξ<" }, // CR
						{ L"Art/Interface/Buttons/Promotions_Atlas.dds,7,2", L">Ξ<" }, // CR
						{ L"Art/Interface/Buttons/Promotions_Atlas.dds,2,2", L"Ξ+" }, // CG
						{ L"Art/Interface/Buttons/Promotions_Atlas.dds,3,2", L"Ξ+" }, // CG
						{ L"Art/Interface/Buttons/Promotions_Atlas.dds,4,2", L"Ξ+" }, // CG
						{ L"Art/Interface/Buttons/Promotions_Atlas.dds,2,3", L"»" },
						{ L"Art/Interface/Buttons/Promotions_Atlas.dds,3,3", L"»" },
						{ L"Art/Interface/Buttons/Promotions_Atlas.dds,4,3", L"»" },
						{ L"Art/Interface/Buttons/Promotions_Atlas.dds,5,3", L"»" },
						{ L"Art/Interface/Buttons/Promotions_Atlas.dds,4,1", L"/ϡ" }, // Barrage
						{ L"Art/Interface/Buttons/Promotions_Atlas.dds,5,1", L"/ϡ" }, // Barrage
						{ L"Art/Interface/Buttons/Promotions_Atlas.dds,6,1", L"/ϡ" }, // Barrage
						{ L"Art/Interface/Buttons/Promotions_Atlas.dds,1,1", L"/Ξ" }, // Accuracy
						{ L"Art/Interface/Buttons/Promotions_Atlas.dds,6,3", L"↑+↓" }, // flanking 1
						{ L"Art/Interface/Buttons/Promotions_Atlas.dds,7,3", L"↑+↓" }, // flanking 2
						{ L"Art/Interface/Buttons/Promotions_Atlas.dds,5,6", L"ʃ+" }, // sentry
						{ L"Art/Interface/Buttons/Promotions_Atlas.dds,5,4", L"↔" },
						{ L"Art/Interface/Buttons/Promotions_Atlas.dds,7,4", L"→" }, // nav 1
						{ L"Art/Interface/Buttons/Promotions_Atlas.dds,8,4", L"→" }, // nav 2
						{ L"Art/Interface/Buttons/Warlords_Atlas_1.dds,5,10", L"✶ϡ✶" }, // PROMOTION_LEADER
						{ L"Art/Interface/Buttons/Warlords_Atlas_1.dds,5,16", L"↑ϡ↑" }, // PROMOTION_LEADERSHIP
						{ L"Art/Interface/Buttons/Warlords_Atlas_1.dds,6,16", L"→*" }, // PROMOTION_TACTICS
						{ L"Art/Interface/Buttons/Warlords_Atlas_1.dds,6,15", L"✶" }, // PROMOTION_COMBAT6
						{ L"Art/Interface/Buttons/Warlords_Atlas_1.dds,7,16", L"☻→" }, // PROMOTION_MORALE
						{ L"Art/Interface/Buttons/Warlords_Atlas_1.dds,8,16", L"+" }, // PROMOTION_MEDIC3
						{ L"Art/Interface/Buttons/Beyond_the_Sword_Atlas.dds,1,15", L"○+" }, // PROMOTION_RANGE1
						{ L"Art/Interface/Buttons/Beyond_the_Sword_Atlas.dds,2,15", L"○+" }, // PROMOTION_RANGE2
						{ L"Art/Interface/Buttons/Beyond_the_Sword_Atlas.dds,3,15", L"ϔ" }, // PROMOTION_INTERCEPTION1
						{ L"Art/Interface/Buttons/Beyond_the_Sword_Atlas.dds,4,15", L"ϔ" }, // PROMOTION_INTERCEPTION2
						{ L"Art/Interface/Buttons/Beyond_the_Sword_Atlas.dds,5,15", L"↑ϔ↓" }, // PROMOTION_ACE
						{ L"Art/Interface/Buttons/NextWar_Atlas.dds,7,1", L"BIO" }, // PROMOTION_ANTIBIOLOGICAL
						{ L"Art/Interface/Buttons/NextWar_Atlas.dds,8,1", L"NANO" }, // PROMOTION_NANOIDS
					};

					if (const auto it = kImgMap.find(path); it != kImgMap.end())
					{
						write(L"[");
						write(it->second);
						write(L"]");
					}
					else
						write(L"<img>");

					tagStack.push_back(L"img");
				}
				else
					std::abort();

				remaining.remove_prefix(tagSpan + 1);

			}
			else // '&'
			{
				if (remaining.starts_with(L"lt;"))
					write(L"<");
				else if (remaining.starts_with(L"gt;"))
					write(L">");
				else
				{
					// Malformed char entity.
					write(remaining);
					break;
				}
				
				remaining.remove_prefix(remaining.find(L';') + 1);
			}
		}

		if (tagStack.size())
			write(L"[Unterminated tags]");

		return pixels;
	}

	
	//std::vector<hecktui::Pixel> replaceSymbols(std::vector<hecktui::Pixel> str)
	//{
	//	if (std::ranges::any_of(str, [](const hecktui::Pixel& p) { return p.c >= CvTranslator::kFirstSymbolCode; }))
	//	{
	//		std::vector<hecktui::Pixel> output;
	//		output.reserve(str.size());
	//
	//		for (const auto& pixel : str)
	//		{
	//			if (pixel.c >= CvTranslator::kFirstSymbolCode)
	//			{
	//				const std::wstring repl = CvTranslator().lookupSymbolChar(pixel.c);
	//				// Force spacing before symbols.
	//				if (output.size() && !std::iswspace(uint16_t(output.back().c)))
	//				{
	//					hecktui::Pixel dst = pixel;
	//					dst.c = L' ';
	//					output.push_back(dst);
	//				}
	//				output.append_range(repl | std::views::transform([pixel](wchar_t c) {
	//					hecktui::Pixel dst = pixel;
	//					dst.c = c;
	//					return dst;
	//				}));
	//			}
	//			else
	//				output.push_back(pixel);
	//		}
	//
	//		return output;
	//	}
	//	else
	//		return str;
	//}

	std::wstring replaceSymbolsAsXml(std::wstring str)
	{
		if (std::ranges::any_of(str, [](wchar_t c) { return c >= CvTranslator::kFirstSymbolCode; }))
		{
			std::wstring output;
			output.reserve(str.size() + 20);

			for (const wchar_t c : str)
			{
				if (c >= CvTranslator::kFirstSymbolCode)
				{
					output += CvTranslator().lookupSymbolChar(c);
					// Force spacing before symbols.
					//if (output.size() && !std::iswspace(uint16_t(output.back().c)))
					//{
					//	hecktui::Pixel dst = pixel;
					//	dst.c = L' ';
					//	output.push_back(dst);
					//}
					//output.append_range(repl | std::views::transform([pixel](wchar_t c) {
					//	hecktui::Pixel dst = pixel;
					//	dst.c = c;
					//	return dst;
					//}));
				}
				else
					output += c;
			}

			return output;
		}
		else
			return str;
	}
}

void ::initialiseTextCodeTags()
{
	kInfo = {};
	CvDllTranslator::initializeTags(
		kInfo.szTagStartIcon, kInfo.szTagStartOur, kInfo.szTagStartCT, kInfo.szTagStartColour, kInfo.szTagStartLink,
		kInfo.szTagEndLink, kInfo.szEndLinkReplacement, kInfo.aIconMap, kInfo.aColourMap
	);
}

std::vector<hecktui::Pixel> (::renderCiv4TextCode)(std::wstring_view textcode, hecktui::Pixel defPixel)
{
	//return renderCiv4Xml(replaceSymbolsAsXml(lowerToXml(textcode)), defPixel);
	return renderCiv4Xml(replaceSymbolsAsXml(std::wstring(textcode)), defPixel);
}

std::wstring (::lowerCiv4TextCodeToXml)(std::wstring_view textcode)
{
	return lowerToXml(textcode);
}

hecktui::EColour (::toTuiColour)(ColorTypes civ4Colour)
{
	// I don't like this special case, but WorldView does.
	if (civ4Colour == ColorTypes::NO_COLOR)
		return hecktui::EColour::Black;
	return toTuiColour(gGlobals.getColorInfo(civ4Colour).getColor());
}

hecktui::EColour(::toTuiColour)(NiColorA civ4Colour)
{
	return hecktui::toColourFromRGBF({ civ4Colour.r, civ4Colour.g, civ4Colour.b });
}

hecktui::EColour (::darken)(hecktui::EColour colour, int n)
{
	if (n <= 0)
		return colour;

	if (colour <= hecktui::EColour::White)
		colour = toNonSysColour(colour);


	
	int index = int(colour);
	/*if (index < 8)
		;
	else if (index < 16)
		index -= 8; // Convert intense colours to standard colours.
	else*/ if (colour < hecktui::EColour::Grey3)
	{
		// return EColour(int(EColour::Grey0) + 6 * 6 * r + 6 * g + b);
		const auto [r, g, b] = hecktui::decodeCubeColour(colour);
		colour = hecktui::encodeCubeColour({ std::max(0, r - n), std::max(0, g - n), std::max(0, b - n) });
	}
	else
	{
		index -= 4 * n;
		if (index < 0)
			colour = hecktui::EColour::Grey0;
		else
			colour = hecktui::EColour(index);
	}
	return colour;
}

hecktui::EColour(::lighten)(hecktui::EColour colour, int n)
{
	if (n <= 0)
		return colour;

	if (colour <= hecktui::EColour::White)
		colour = toNonSysColour(colour);

	if (n <= 0)
		return colour;
	int index = int(colour);
	/*if (index < 8)
		index += 8; // Convert standard colours to intense colours.
	else if (index < 16)
		; // Do nothing to intense colours.
	else*/ if (colour < hecktui::EColour::Grey3)
	{
		// return EColour(int(EColour::Grey0) + 6 * 6 * r + 6 * g + b);
		const auto [r, g, b] = hecktui::decodeCubeColour(colour);
		colour = hecktui::encodeCubeColour({ std::max(0, r + n), std::max(0, g + n), std::max(0, b + n) });
	}
	else
	{
		index += 4 * n;
		if (index >= 256)
			colour = hecktui::EColour::Grey100;
		else
			colour = hecktui::EColour(index);
	}
	return colour;
}

std::wstring (::stripPixels)(std::span<const hecktui::Pixel> pixels)
{
	return pixels | std::views::transform(&hecktui::Pixel::c) | std::ranges::to<std::wstring>();
}