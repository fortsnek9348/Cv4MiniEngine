#include "Minimap.h"
#include "Common.h"
#include "CvInterface.h"
#include "WorldView.h"
#include "TuiTextCode.h"
#include "DXT.h"

#include "DLLInterface/MyCvDLLPython.h"

#include <CvGlobals.h>
#include <CvMap.h>
#include <CvGameAI.h>
#include <CvPlayerAI.h>
#include <CvInfos.h>
#include <CyArgsList.h>
#include <CvReplayInfo.h>

#include <CommonStuff/range.h>

using namespace hecktui;
using namespace cvengine;
using heck::range;

static constexpr bool kAllVisible = false;

template<bool kCol>
static bool isPlotRevealed(const CvMap& map, TeamTypes teamI, int x, int y)
{
	if constexpr (!kCol)
		std::swap(x, y);
	return map.plot(x, y)->isRevealed(teamI, false);
}

template<bool kCol>
static bool grabStripIsRevealed(const CvMap& map, TeamTypes teamI, int length, int x, std::span<uint16_t> cachedPosition)
{
	if (cachedPosition[x] < length && isPlotRevealed<kCol>(map, teamI, x, cachedPosition[x]))
		return true;

	for (const int i : range(length))
	{
		if (isPlotRevealed<kCol>(map, teamI, x, i))
		{
			cachedPosition[x] = (uint16_t)i;
			return true;
		}
	}

	cachedPosition[x] = UINT16_MAX;
	return false;
}

template<bool kCol>
static std::pair<int, int> findRevealedSpan(const CvMap& map, TeamTypes teamI, bool wrapped, int width, int height, std::span<uint16_t> cachedPosition)
{
	const auto isRevealed = [&](int t) {
		return grabStripIsRevealed<kCol>(map, teamI, height, t, cachedPosition);
	};

	if (!wrapped)
	{
		// Find first and last revealed columns.
		int minX = 0;
		for (const int x : range(width))
			if (isRevealed(x))
			{
				minX = x;
				break;
			}

		int maxX = 0;
		for (const int x : range(width) | std::views::reverse)
			if (isRevealed(x))
			{
				maxX = x;
				break;
			}

		// If nothing is revealed, this will return { 0, 0 }.
		return { minX, maxX };
	}
	else
	{
		// Find the first unrevealed column and expand around it.
		int firstUnrevealedX = -1;
		for (const int x : range(width))
		{
			if (!isRevealed(x))
			{
				firstUnrevealedX = x;
				break;
			}
		}

		// Nothing revealed.
		if (firstUnrevealedX < 0)
			return { 0, 0 };

		// Find the left-most extent of revealed columns (first after unrevealed columns).
		int minX = width; // This is minX if firstUnrevealedX == width - 1.
		for (const int x : range(firstUnrevealedX + 1, width))
			if (isRevealed(x))
			{
				minX = x;
				break;
			}

		// Find the right-most extent of revealed columns (to the left of the gap).
		int maxX = -1;
		if (firstUnrevealedX > 0)
			maxX = firstUnrevealedX - 1; // Trivially.
		else
		{
			for (const int x : range(width) | std::views::reverse)
				if (isRevealed(x))
				{
					maxX = x - width;
					break;
				}
		}

		// Unwrap.
		if (minX < width)
		{
			assert(maxX < minX);
			maxX += width;
		}
		else
		{
			minX = 0;
		}

		return { minX, maxX };
	}
}

// Called by CvInterface.
void MinimapSectionTracking::update()
{
	const bool isRevealCentered = !gGlobals.getGame().shouldCenterMinimap();

	CvMap& map = gGlobals.getMap();
	const int mapW = map.getGridWidth();
	const int mapH = map.getGridHeight();

	if (!isRevealCentered || kAllVisible)
	{
		mRect = { .max{ mapW, mapH } };
		return;
	}

	const TeamTypes teamI = gGlobals.getGame().getActiveTeam();

	const bool isWrapX = map.isWrapX();
	const bool isWrapY = map.isWrapY();

	mCachedColAnyRevealedY.resize(mapW, 0);
	mCachedRowAnyRevealedX.resize(mapH, 0);

	const auto [minX, maxX] = findRevealedSpan<true>(map, teamI, isWrapX, mapW, mapH, mCachedColAnyRevealedY);
	const auto [minY, maxY] = findRevealedSpan<false>(map, teamI, isWrapY, mapH, mapW, mCachedRowAnyRevealedX);

	mRect = {
		.min{ minX, minY },
		.max = ivec2(maxX, maxY) + 1, // to exclusive bounds
	};
}


// This is always zero or positive, but may need to be wrapped.
iaabb2 MinimapSectionTracking::getCurrentRect() const
{
	return mRect;
}

///

hecktui::ivec2 cvengine::getMinimapBaseTextureDim()
{
	return {
		gGlobals.getDefineINT("MINIMAP_RENDER_SIZE"),
		gGlobals.getDefineINT("MINIMAP_RENDER_SIZE"),
	};
}

Minimap::Minimap()
{
	mIsMouseInteractable = true;
}

void Minimap::clearMinimapMarkers() noexcept
{
	mMarkers.clear();
}
void Minimap::addMinimapMarker(Marker marker)
{
	mMarkers.push_back(marker);
}

void Minimap::setMinimapBaseTextureFromReplay(const CvReplayInfo& replay)
{
	const auto dim = getMinimapBaseTextureDim();
	mReplayBaseTexture = decodeDXT1(dim, std::span(replay.getMinimapPixels(), dim.x * dim.y / 2));
	mReplayMapSize = { replay.getMapWidth(), replay.getMapHeight() };
	mReplayCultureMap.resize(mReplayMapSize.x * mReplayMapSize.y, NO_COLOR);
}

void Minimap::setPlotCulture(hecktui::ivec2 coord, ColorTypes colour)
{
	mReplayCultureMap[coord.x + coord.y * mReplayMapSize.x] = colour;
}

// renderRect -> mapRect
// [renderMinX..renderMaxX] -> [mapMinX..mapMaxX], round down
// [renderMinY..renderMaxY] -> [mapMaxY..mapMinY], round down. Note that mapMaxY won't be returned because pixels are sampled on the inside.

Minimap::RenderCoordMapping::RenderCoordMapping(hecktui::iaabb2 renderRect, hecktui::iaabb2 mapRect)
{
	// mapX = ((mapMinX-mapMaxX)*x+mapMaxX*renderMinX-mapMinX*renderMaxX) / (renderMinX - renderMaxX)
	// mapY = ((mapMinY-mapMaxY)*y+mapMaxY*renderMaxY-mapMinY*renderMinY) / (renderMaxY - renderMinY)
	scale = {
		(float)mapRect.size().x / renderRect.size().x,
		(float)mapRect.size().y / -renderRect.size().y,
	};

	offset = {
		float(mapRect.max.x*renderRect.min.x-mapRect.min.x*renderRect.max.x) / (renderRect.min.x - renderRect.max.x),
		float(mapRect.max.y*renderRect.max.y-mapRect.min.y*renderRect.min.y) / (renderRect.max.y - renderRect.min.y),
	};
}
hecktui::ivec2 Minimap::RenderCoordMapping::toPlotCoord(fvec2 renderCoord) const
{
	return {
		(int)std::floor(renderCoord.x * scale.x + offset.x),
		(int)std::floor(renderCoord.y * scale.y + offset.y),
	};
}
hecktui::ivec2 Minimap::RenderCoordMapping::toRenderCoord(fvec2 plotCoord) const
{
	return {
		(int)std::floor((plotCoord.x - offset.x) / scale.x),
		(int)std::floor((plotCoord.y - offset.y) / scale.y),
	};
}


namespace
{
	using fvec2 = Minimap::fvec2;

	struct UICoordMapping
	{
		// Panel coords: Screen cell coords.
		// Render coords: Panel coords with doubled Y.

		iaabb2 panelDrawRect{};
		ivec2 renderSize{};
		Minimap::RenderCoordMapping renderCoordMapping;

		explicit UICoordMapping(iaabb2 panelRect, ivec2 aspectRatio, iaabb2 textureRect)
		{
			//const iaabb2 mapRect = CvInterface::getInstance().getMinimapSectionRect();
			const ivec2 panelSize = panelRect.size();
			const ivec2 renderSpace = panelSize * ivec2(1, 2);

			if (aspectRatio.y * renderSpace.x > renderSpace.y * aspectRatio.x)
			{
				// Constrain to height.
				const int uiWidthNeededToMatchHeight = std::lround((float)aspectRatio.x / aspectRatio.y * renderSpace.y);
				renderSize = { uiWidthNeededToMatchHeight, renderSpace.y };
			}
			else
			{
				// Constrain to width.
				// Height must be rounded to nearest two.
				const int uiHeightNeededToMatchWidth = std::lround((float)aspectRatio.y / aspectRatio.x * renderSpace.x / 2) * 2;
				renderSize = { renderSpace.x, uiHeightNeededToMatchWidth };
			}

			panelDrawRect = hecktui::justilignRect(panelRect, renderSize / ivec2(1, 2), RectJustilign{ .halign = EJustilign::Center, .valign = EJustilign::Center });
			
			renderCoordMapping = Minimap::RenderCoordMapping(iaabb2::sized({}, renderSize), textureRect);
		}

		ivec2 toPlotCoord(fvec2 renderCoord) const
		{
			return renderCoordMapping.toPlotCoord(renderCoord);
		}

		ivec2 toRenderCoord(fvec2 plotCoord) const
		{
			return renderCoordMapping.toRenderCoord(plotCoord);
		}
	};
}

bool Minimap::onEvent(const hecktui::ConsoleEvent& e)
{
	const auto lookAt = [this](ivec2 panelCoord) {

		CvInterface::getInstance().exitCityScreen();

		const iaabb2 rect{ .max = getRect().size() };
		const iaabb2 mapRect = CvInterface::getInstance().updateAndGetMinimapSectionRect();
		const UICoordMapping mapping(rect, mapRect.size(), mapRect);
		if (mapping.panelDrawRect.contains(panelCoord))
		{
			const ivec2 renderCoord = (panelCoord - mapping.panelDrawRect.min) * ivec2(1, 2);
			const ivec2 plotCoord = mapping.toPlotCoord({ (float)renderCoord.x + 0.5f, (float)renderCoord.y + 1.0f });
			CvInterface::getInstance().lookAt(plotCoord);

			CyArgsList pyArgs;
			pyArgs.add((int)screenEventSource);
			(void)MyCvDLLPython().callFunction(PYScreensModule, "minimapClicked", pyArgs.makeFunctionArgs());
		}
	};

	if (mReplayBaseTexture.empty())
	{
		if (handleMouseMove)
		{
			if (e.type == EConsoleEventType::MouseMove || e.type == EConsoleEventType::MouseButtonDown)
			{
				const auto& e2 = static_cast<const MouseEvent&>(e);
				if (e2.left)
					lookAt(e2.coord);

				return true;
			}
		}
		else
		{
			if (e.type == EConsoleEventType::MouseButtonUp && getUISceneState().capture)
			{
				const auto& e2 = static_cast<const MouseButtonEvent&>(e);
				if (e2.button == EMouseButton::Left)
				{
					lookAt(e2.coord);


				}

				return true;
			}
		}
	}

	return false;
}

LayoutSizeInfo Minimap::measureThis() const
{
	const ivec2 v{ 3, 3 };
	return { v, v };
}

[[maybe_unused]] static constexpr fvec2 k1xMSAA[]{ { 0.5f, 0.5f } };
[[maybe_unused]] static constexpr fvec2 k2xMSAA0[]{ { 0.25f, 0.25f }, { 0.75f, 0.75f } };
[[maybe_unused]] static constexpr fvec2 k2xMSAA1[]{ { 0.25f, 0.75f }, { 0.75f, 0.25f } };
[[maybe_unused]] static constexpr fvec2 k4xMSAA[]{ { 6 / 16.0f, 2 / 16.0f }, { 14 / 16.0f, 6 / 16.0f }, { 2 / 16.0f, 10 / 16.0f }, { 10 / 16.0f, 14 / 16.0f } };

static constexpr auto& kSamplePatternA = k2xMSAA0;
static constexpr auto& kSamplePatternB = k2xMSAA1;

static constexpr float kCultureAlpha = 0.6f;

hecktui::RGBF Minimap::computeMinimapBaseTexturePixelColour(const CvMap& map, TeamTypes teamI, RenderCoordMapping mapping, hecktui::ivec2 coord)
{
	float r = 0;
	float g = 0;
	float b = 0;

	for (const fvec2& p : coord.y % 2 == 0 ? kSamplePatternA : kSamplePatternB)
	{
		const ivec2 plotCoord = mapping.toPlotCoord({ coord.x + p.x, coord.y + p.y });
		const CvPlot* const plot = map.plot(plotCoord.x, plotCoord.y);
		const bool isRevealedPlot = teamI == NO_TEAM || (plot && plot->isRevealed(teamI, false));
		const EColour colour = plot ? (WorldView::getPlotBackgroundColour(*plot,
			teamI == NO_TEAM || plot->isActiveVisible(false) ? WorldView::EPlotVisibility::kLightened :
			isRevealedPlot ? WorldView::EPlotVisibility::kDarkened :
			WorldView::EPlotVisibility::kBlackened)) : EColour::Grey0
			;
		RGBF rgb = toRGBF(colour);
		//if constexpr (!kDoSingleSampleCulture)
		//{
		//	if (isRevealedPlot && teamI != NO_TEAM)
		//	{
		//		const PlayerTypes ownerPlayerI = plot->getRevealedOwner(teamI, false);
		//		if (ownerPlayerI != NO_PLAYER)
		//		{
		//			const PlayerColorTypes ownerPlayerColourI = CvPlayerAI::getPlayerNonInl(ownerPlayerI).getPlayerColor();
		//			const ColorTypes borderColour = (ColorTypes)gGlobals.getPlayerColorInfo(ownerPlayerColourI).getColorTypePrimary();
		//			const NiColorA borderColourRGB = gGlobals.getColorInfo(borderColour).getColor();
		//			rgb[0] += (borderColourRGB.r - rgb[0]) * cultureAlpha;
		//			rgb[1] += (borderColourRGB.g - rgb[1]) * cultureAlpha;
		//			rgb[2] += (borderColourRGB.b - rgb[2]) * cultureAlpha;
		//		}
		//	}
		//}

		r += rgb[0];
		g += rgb[1];
		b += rgb[2];
	}

	r *= 1.0f / std::size(kSamplePatternA);
	g *= 1.0f / std::size(kSamplePatternA);
	b *= 1.0f / std::size(kSamplePatternA);

	//if constexpr (kDoSingleSampleCulture)
	//{
	//	const fvec2 p{ 0.5f, 0.5f };
	//	const ivec2 plotCoord = mapping.toPlotCoord({ coord.x + p.x, coord.y + p.y });
	//	const CvPlot* const plot = map.plot(plotCoord.x, plotCoord.y);
	//	const bool isRevealedPlot = teamI != NO_TEAM && plot && plot->isRevealed(teamI, false);
	//
	//	if (isRevealedPlot)
	//	{
	//		const PlayerTypes ownerPlayerI = plot->getRevealedOwner(teamI, false);
	//		if (ownerPlayerI != NO_PLAYER)
	//		{
	//			const PlayerColorTypes ownerPlayerColourI = CvPlayerAI::getPlayerNonInl(ownerPlayerI).getPlayerColor();
	//			const ColorTypes borderColour = (ColorTypes)gGlobals.getPlayerColorInfo(ownerPlayerColourI).getColorTypePrimary();
	//			const NiColorA borderColourRGB = gGlobals.getColorInfo(borderColour).getColor();
	//			r += (borderColourRGB.r - r) * cultureAlpha;
	//			g += (borderColourRGB.g - g) * cultureAlpha;
	//			b += (borderColourRGB.b - b) * cultureAlpha;
	//		}
	//	}
	//}

	return { r, g, b };
}



void Minimap::drawThis(ivec2 offset, hecktui::Framebuffer& fb)
{
	const iaabb2 rect = getRect() + offset;

	const ivec2 panelDim = rect.size();

	if (panelDim.x <= 0 || panelDim.y <= 0)
		return;

	const iaabb2 mapRect = mReplayBaseTexture.empty() ? CvInterface::getInstance().updateAndGetMinimapSectionRect() : iaabb2::sized({}, mReplayMapSize);
	const iaabb2 textureRect = mReplayBaseTexture.empty() ? mapRect : iaabb2::sized({}, getMinimapBaseTextureDim());
	const UICoordMapping mapping(rect, mapRect.size(), textureRect);
	const UICoordMapping plotMapping(rect, mapRect.size(), mapRect);

	const TeamTypes teamI = gGlobals.getGame().getActiveTeam();

	const auto blendCulture = [teamI, mapping, plotMapping, &map = gGlobals.getMap(), this](hecktui::RGBF baseColour, ivec2 renderCoord) {
		const fvec2 p{ 0.5f, 0.5f };
		ColorTypes colour = NO_COLOR;
		if (mReplayBaseTexture.empty())
		{
			const ivec2 plotCoord = mapping.toPlotCoord({ renderCoord.x + p.x, renderCoord.y + p.y });
			const CvPlot* const plot = map.plot(plotCoord.x, plotCoord.y);
			const bool isRevealedPlot = teamI != NO_TEAM && plot && plot->isRevealed(teamI, false);

			if (isRevealedPlot)
			{
				const PlayerTypes ownerPlayerI = plot->getRevealedOwner(teamI, false);
				if (ownerPlayerI != NO_PLAYER)
				{
					const PlayerColorTypes ownerPlayerColourI = CvPlayerAI::getPlayerNonInl(ownerPlayerI).getPlayerColor();
					colour = (ColorTypes)gGlobals.getPlayerColorInfo(ownerPlayerColourI).getColorTypePrimary();
				}
			}
		}
		else
		{
			const ivec2 plotCoord = plotMapping.toPlotCoord({ renderCoord.x + p.x, renderCoord.y + p.y });
			colour = mReplayCultureMap[plotCoord.x + plotCoord.y * mReplayMapSize.x];
		}

		if (colour != NO_COLOR)
		{
			const NiColorA borderColourRGB = gGlobals.getColorInfo(colour).getColor();
			baseColour[0] += (borderColourRGB.r - baseColour[0]) * kCultureAlpha;
			baseColour[1] += (borderColourRGB.g - baseColour[1]) * kCultureAlpha;
			baseColour[2] += (borderColourRGB.b - baseColour[2]) * kCultureAlpha;
		}
		return baseColour;
	};

	const auto getColour = [teamI, &mapping, &map = gGlobals.getMap(), textureRect, blendCulture, this](ivec2 renderCoord) {
		RGBF colour;
		if (mReplayBaseTexture.empty())
			colour = computeMinimapBaseTexturePixelColour(map, teamI, mapping.renderCoordMapping, renderCoord);
		else
		{
			// Sample from replay texture.
			int r = 0;
			int g = 0;
			int b = 0;
			for (const fvec2& p : renderCoord.y % 2 == 0 ? kSamplePatternA : kSamplePatternB)
			{
				const ivec2 plotCoord = mapping.toPlotCoord({ renderCoord.x + p.x, renderCoord.y + p.y });
				const hecktui::RGB8 baseColour = mReplayBaseTexture[plotCoord.x + (textureRect.max.y - 1 - plotCoord.y) * textureRect.max.x];
				r += baseColour[0];
				g += baseColour[1];
				b += baseColour[2];
			}

			constexpr float n = (float)std::size(kSamplePatternA) * 255.0f;

			colour = {
				r / n,
				g / n,
				b / n,
			};
		}

		return hecktui::toColourFromRGBF(blendCulture(colour, renderCoord));
	};

	

	hecktui::Pixel pixel{
		.c = L'▀',
		.colour{.text = EColour::White, .back = EColour::Black },
	};

	// If not the main interface minimap...
	if (!handleMouseMove)
		fb.drawBox(mapping.panelDrawRect.shrunk({ -1, -1 }), EBorderStyle::Thin, {});

	for (const int uiY : range(mapping.renderSize.y / 2))
	{
		for (const int uiX : range(mapping.renderSize.x))
		{
			pixel.colour.text = getColour({ uiX, uiY * 2 });
			pixel.colour.back = getColour({ uiX, uiY * 2 + 1 });
			fb.draw(mapping.panelDrawRect.min + ivec2(uiX, uiY), pixel);
		}
	}

	const auto drawMarker = [&](const Marker& marker) {
		const ivec2 uiCoord = plotMapping.toRenderCoord({ marker.coord.x + 0.5f, marker.coord.y + 0.5f }) / ivec2(1, 2);

		pixel.c = marker.c;
		pixel.colour.text = cvengine::toTuiColour(marker.colourI);
		pixel.colour.back = EColour::Black;
		fb.draw(mapping.panelDrawRect.min + uiCoord, pixel);
	};

	for (const Marker& marker : mMarkers)
		drawMarker(marker);

	if (mReplayBaseTexture.empty())
	{
		drawMarker({
			.coord = CvInterface::getInstance().getWorldView().getLookAtPlotCoord(),
			.colourI = (ColorTypes)gGlobals.getInfoTypeForString("COLOR_WHITE"),
			.c = L'@',
			});
	}
}

