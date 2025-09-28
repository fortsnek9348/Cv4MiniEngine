#include "WorldView.h"
#include "Common.h"
#include "Graphics.h"
#include "CvInterface.h"
#include "CvApp.h"
#include "TuiTextCode.h"
#include "CvTranslator.h"
#include "WorldViewUtils.h"
#include "CvEngine.h"

#include <CvGlobals.h>
#include <CvMap.h>
#include <CvPlot.h>
#include <CvGameCoreUtils.h>
#include <CvUnit.h>
#include <CvCity.h>
#include <CvGameAI.h>
#include <CvTeamAI.h>
#include <CvPlayerAI.h>
#include <CvInfos.h>
#include <CvGameTextMgr.h>
#include <CvInitCore.h>
#include <CvInfos.h>

#include <CommonStuff/range.h>

#include <array>
#include <cwctype>
#include <iostream>

using heck::range;
using heck::ivec2;

// originally 9x5 before zooming
// NOTE: Round for wider plots because consoles are wide.
static constexpr ivec2 kDefaultPlotInnerDim{ 9, 4 }; 
// Assuming character cell center sampling.
static constexpr ivec2 kDefaultPlotCenterSampledDim = kDefaultPlotInnerDim + ivec2(1, 1);
// Including all borders.
static constexpr ivec2 kDefaultPlotOuterDim = kDefaultPlotInnerDim + ivec2(2, 2);

static constexpr int kDefaultMaxUnitNameLength = kDefaultPlotInnerDim.x + 1;

static constexpr int kMaxZoom = 20;

static constexpr bool kAllVisible = false;

// Window coord: Relative to window top-left, one unit per character cell.
// Map cell coord: Relative to map bottom-left, one unit per character cell.
// Plot coord: Relative to map bottom-left, one unit per plot.

static constexpr ivec2 kMapPerWindow{ 1, -1 };

static ivec2 getWindowTopLeftMapCellCoord(ivec2 windowDim, ivec2 centerMapCellCoord)
{
	return centerMapCellCoord - windowDim * kMapPerWindow / 2;
}

static ivec2 windowToMapCellCoord(ivec2 windowDim, ivec2 centerMapCellCoord, ivec2 windowCoord)
{
	const ivec2 topLeftMapCell = getWindowTopLeftMapCellCoord(windowDim, centerMapCellCoord);
	return topLeftMapCell + windowCoord * kMapPerWindow;
}

static ivec2 mapCellToWindowCoord(ivec2 windowDim, ivec2 centerMapCellCoord, ivec2 mapCellCoord)
{
	const ivec2 topLeftMapCell = getWindowTopLeftMapCellCoord(windowDim, centerMapCellCoord);
	return (mapCellCoord - topLeftMapCell) / kMapPerWindow;
}

static ivec2 makeZoomVec(int zoom)
{
	return { zoom, ((kDefaultPlotInnerDim.x + zoom) * kDefaultPlotInnerDim.y + kDefaultPlotInnerDim.x / 2) / kDefaultPlotInnerDim.x - kDefaultPlotInnerDim.y };
}

static ivec2 mapCellToPlotCoord(ivec2 mapCellCoord, int zoom)
{
	// Must round DOWN. Negatives are possible.
	const ivec2 plotDim = kDefaultPlotCenterSampledDim + makeZoomVec(zoom);
	ivec2 coord = mapCellCoord / plotDim;
	coord.x -= mapCellCoord.x < 0 && mapCellCoord.x % plotDim.x != 0;
	coord.y -= mapCellCoord.y < 0 && mapCellCoord.y % plotDim.y != 0;
	return coord;
}

static ivec2 wrapPlotCoord(ivec2 plotCoord)
{
	CvMap& map = gGlobals.getMap();
	return {
		coordRange(plotCoord.x, map.getGridWidth(), map.isWrapX()),
		coordRange(plotCoord.y, map.getGridHeight(), map.isWrapY())
	};
}

static ivec2 wrapOrClampMapCellCoord(ivec2 mapCellCoord, int zoom)
{
	const ivec2 plotDim = kDefaultPlotCenterSampledDim + makeZoomVec(zoom);
	CvMap& map = gGlobals.getMap();
	const hecktui::ivec2 dim = hecktui::ivec2(map.getGridWidth(), map.getGridHeight()) * plotDim;
	return {
		map.isWrapX() ? coordRange(mapCellCoord.x, dim.x, true) : std::clamp(mapCellCoord.x, 0, dim.x),
		map.isWrapY() ? coordRange(mapCellCoord.y, dim.y, true) : std::clamp(mapCellCoord.y, 0, dim.y),
	};
}

static ivec2 plotToBottomLeftMapCellCoord(ivec2 plotCoord, int zoom)
{
	return plotCoord * (kDefaultPlotCenterSampledDim + makeZoomVec(zoom));
}

static ivec2 plotToTopLeftMapCellCoord(ivec2 plotCoord, int zoom)
{
	const ivec2 plotDim = kDefaultPlotCenterSampledDim + makeZoomVec(zoom);
	return plotToBottomLeftMapCellCoord(plotCoord, zoom) + plotDim * ivec2(0, 1);
}

static ivec2 plotToCenterMapCellCoord(ivec2 plotCoord, int zoom)
{
	const ivec2 plotDim = kDefaultPlotCenterSampledDim + makeZoomVec(zoom);
	return plotToBottomLeftMapCellCoord(plotCoord, zoom) + plotDim / 2;
}

using EColour = WorldViewFramebuffer::EColour;

static ColorTypes getPlayerColour(PlayerTypes player)
{
	if (player == NO_PLAYER)
		return ColorTypes::NO_COLOR;
	return (ColorTypes)gGlobals.getPlayerColorInfo(gGlobals.getInitCore().getColor(player)).getColorTypePrimary();
}

static EColour pickVgaColour(PlayerTypes colour)
{
	return toTuiColour(getPlayerColour(colour));
}

//static EColour averageOwnerColour(PlayerTypes a, PlayerTypes b)
//{
//	const NiColorA colourA = gGlobals.getColorInfo(getPlayerColour(a)).getColor();
//	const NiColorA colourB = gGlobals.getColorInfo(getPlayerColour(b)).getColor();
//	return hecktui::toColourFromRGBF({
//		(colourA.r + colourB.r) * 0.5f,
//		(colourA.g + colourB.g) * 0.5f,
//		(colourA.b + colourB.b) * 0.5f
//		});
//}

void WorldView::setWindowSize(ivec2 windowDim)
{
	windowDim = vmax(ivec2(), windowDim);
	if (mWindowDim != windowDim)
	{
		mWindowDim = windowDim;
		invalidateAll();
	}
}

void WorldView::setCenterPlot(ivec2 coord)
{
	coord = plotToCenterMapCellCoord(wrapPlotCoord(coord), mZoom);
	if (mCenterMapCellCoord != coord)
	{
		mCenterMapCellCoord = wrapOrClampMapCellCoord(coord, mZoom);
		invalidateAll();
	}
}

void WorldView::scrollPlots(ivec2 delta)
{
	if (delta.x || delta.y)
	{
		const ivec2 plotDim = kDefaultPlotCenterSampledDim + makeZoomVec(mZoom);
		delta *= plotDim * kMapPerWindow;
		mCenterMapCellCoord = wrapOrClampMapCellCoord(mCenterMapCellCoord + delta, mZoom);
		invalidateAll();
	}
}

void WorldView::zoom(int levels)
{
	const ivec2 centerPlotCoord = mapCellToPlotCoord(mCenterMapCellCoord, mZoom);
	mZoom += levels;
	mZoom = std::clamp(mZoom, 0, kMaxZoom);
	setCenterPlot(centerPlotCoord);
	invalidateAll();
}

int WorldView::getZoom() const
{
	return mZoom;
}

CvPlot* WorldView::getLookAtPlot() const
{
	const CvMap& map = gGlobals.getMap();
	const ivec2 plotCoord = mapCellToPlotCoord(mCenterMapCellCoord, mZoom);
	return map.plot(plotCoord.x, plotCoord.y);
}

hecktui::ivec2 WorldView::getLookAtPlotCoord() const
{
	return mapCellToPlotCoord(mCenterMapCellCoord, mZoom);
}

CvPlot* WorldView::getPlotAtWindowCoord(ivec2 windowCoord) const
{
	const ivec2 mapCellCoord = windowToMapCellCoord(mWindowDim, mCenterMapCellCoord, windowCoord);
	const ivec2 plotCoord = mapCellToPlotCoord(mapCellCoord, mZoom);
	const CvMap& map = gGlobals.getMap();
	return map.plot(plotCoord.x, plotCoord.y);
}

static int autoWrap(int center, int x, int width)
{
	// min |plotCoord + width * k - centerPlotCoord|
	// min |offset + width * k|
	// k = floor/ceil(-offset / width)
	const int offset = x - center;
	const int dim = width;
	const int k = -offset / dim;
	const int dist[]{
		std::abs(offset + dim * (k - 1)),
		std::abs(offset + dim * k),
		std::abs(offset + dim * (k + 1)),
	};
	return x + dim * (int(std::ranges::min_element(dist) - dist) - 1);
}

hecktui::iaabb2 WorldView::getPlotInnerRect(ivec2 plotCoord) const
{
	// Need to adjust the plot coord to get as close as possible to the center coord, due to wrap.
	const ivec2 centerPlotCoord = mapCellToPlotCoord(mCenterMapCellCoord, mZoom);
	CvMap& map = gGlobals.getMap();
	if (map.isWrapX())
		plotCoord.x = autoWrap(centerPlotCoord.x, plotCoord.x, map.getGridWidth());
	if (map.isWrapY())
		plotCoord.y = autoWrap(centerPlotCoord.y, plotCoord.y, map.getGridHeight());

	const ivec2 mapCoord = plotToTopLeftMapCellCoord(plotCoord, mZoom);
	const ivec2 windowCoord = mapCellToWindowCoord(mWindowDim, mCenterMapCellCoord, mapCoord) + hecktui::ivec2(1, 1);
	return hecktui::iaabb2::sized(windowCoord, kDefaultPlotInnerDim + makeZoomVec(mZoom));
}

/*void WorldView::setHighlightedCity(CvCity* city) noexcept
{
	const IDInfo cityId = city ? city->getIDInfo() : IDInfo();
	if (cityId != mHighlightedCityId)
	{
		mHighlightedCityId = cityId;
		invalidateAll();
	}
}*/

void WorldView::setDisplayedPath(std::span<const PathNode> path)
{
	if (!std::ranges::equal(mDisplayedPath, path))
	{
		mDisplayedPath.assign_range(path);
		invalidateAll();
	}
}

void WorldView::setDisplayMode(EDisplayMode mode)
{
	if (mode != mDisplayMode)
	{
		mDisplayMode = mode;
		invalidateAll();
	}
}

size_t WorldView::Hasher::operator()(ivec2 coord) const noexcept
{
	return std::hash<uint32_t>()(uint32_t(coord.x) + uint32_t(coord.y) * 1721); // a prime
}

void WorldView::setColouredPlot(ivec2 coord, ColouredPlotDesc desc)
{
	mColouredPlotsLookup[coord] = std::move(desc);
	invalidatePlot(coord);
}
void WorldView::clearColouredPlots(PlotLandscapeLayers layer) noexcept
{
	if (layer == PLOT_LANDSCAPE_LAYER_ALL)
		mColouredPlotsLookup.clear();
	else
	{
		std::erase_if(mColouredPlotsLookup, [layer](const std::pair<ivec2, ColouredPlotDesc>& kv) {
			return kv.second.layer == layer;
		});
	}
}

void WorldView::addAreaBorderPlot(ivec2 p, AreaBorderLayers layer, NiColorA colour)
{
	std::vector<AreaBorderEntry>& list = mAreaBorderPlotsLookup[p];
	auto it = std::ranges::lower_bound(list, layer, std::less<>(), &AreaBorderEntry::layer);
	if (it == list.end() || it->layer != layer)
		it = list.insert(it, AreaBorderEntry{ .layer = layer });
	it->colour = { colour.r, colour.g, colour.b };
}
void WorldView::clearAreaBorderPlots(AreaBorderLayers layer) noexcept
{
	for (auto it = mAreaBorderPlotsLookup.begin(); it != mAreaBorderPlotsLookup.end(); )
	{
		std::vector<AreaBorderEntry>& list = it->second;
		if (std::erase_if(list, [layer](const AreaBorderEntry& entry) { return entry.layer == layer; }))
			invalidatePlot(it->first);
		if (list.empty())
			it = mAreaBorderPlotsLookup.erase(it);
		else
			++it;
	}
}

void WorldView::changePlotVisibility(ivec2 coord, EPlotVisibility value)
{
	coord = wrapPlotCoord(coord);

	// Only update if the plot was displayed (rendering will add to this lookup, so if a plot isn't here, it doesn't need invalidation).
	if (const auto it = mDisplayedVisibilityInfo.find(coord); it != mDisplayedVisibilityInfo.end() && it->second != value)
	{
		it->second = value;
		invalidatePlot(coord);
	}
}

void WorldView::invalidateAll()
{
	mInvalidated = true;
}

void WorldView::invalidatePlot(ivec2)
{
	invalidateAll();
}

void WorldView::updateFog()
{
	const hecktui::iaabb2 worldRect = getDisplayedPlotsAABB();
	CvMap& map = gGlobals.getMap();
	for (const int y : range(worldRect.min.y, worldRect.max.y))
		for (const int x : range(worldRect.min.x, worldRect.max.x))
			if (CvPlot* const plot = map.plot(x, y))
				plot->updateFog();
}
void WorldView::updateColouredPlots()
{
	gGlobals.getGame().updateColoredPlots();
}

bool WorldView::isDirty() const
{
	return mInvalidated;
}



static constexpr std::array kGrassColours{
	EColour(28 + 6 * -1),
	EColour(28 + 6 * 0),
	EColour(28 + 6 * 1),
	EColour(28 + 6 * 2),
	//EColour(28 + 6 * 3),
};

static constexpr std::array kPlainsColours{
	EColour(137 - 36 * 1 + 0),
	EColour(179 - 36 * 1 + 0),
	EColour(185 - 36 * 1 + 0),
	EColour(227 - 36 * 1 + 0),
};

static constexpr std::array kDesertColours{
	//EColour(178), // darken(EColour(229), 1)?
	//EColour(184),
	//EColour(226),
	//EColour(229),


	EColour(136),
	EColour(179),
	EColour(185),
	EColour(227),
};

static constexpr std::array kTundraColours{
	EColour::Gray,
	EColour::LightSlateGrey,
	EColour::Silver,
	EColour::White,
};

static constexpr std::array kSnowColours{
	//WorldViewFramebuffer::LightBlue,
	//WorldViewFramebuffer::LightCyan,

	EColour(250),
	EColour(152),
	EColour(195),
	EColour(231),
};

namespace
{


	struct TerrainArt
	{
		using ColourArray = std::remove_cv_t<decltype(kSnowColours)>;
		ColourArray coloursLight{};
		ColourArray coloursDark{};

		explicit TerrainArt(const ColourArray& colours) : coloursLight(colours)
		{
			for (auto&& [light, dark] : std::views::zip(coloursLight, coloursDark))
				dark = darken(light, 2);
		}
	};

	const std::array kTerrainArts = std::to_array<TerrainArt>({
		TerrainArt(kGrassColours),
		TerrainArt(kPlainsColours),
		TerrainArt(kDesertColours),
		TerrainArt(kTundraColours),
		TerrainArt(kSnowColours),
	});
}

static WorldViewFramebuffer::Cell lightCellMapping(char c)
{
	switch (c)
	{
	case 'g': return { .c = u'▒', .textColour = WorldViewFramebuffer::LightGreen, .backColour = WorldViewFramebuffer::Green };
	case 'p': return { .c = u'▒', .textColour = EColour(76), .backColour = EColour(185) };
	case 'd': return { .c = u'▒', .textColour = darken(EColour(229), 0), .backColour = darken(EColour(229), 1) };
	case 't': return { .c = u'▒', .textColour = WorldViewFramebuffer::Silver, .backColour = WorldViewFramebuffer::Gray };
	case 's': return { .c = u'▓', .textColour = WorldViewFramebuffer::White, .backColour = WorldViewFramebuffer::Gray };
	case 'w': return { .c = u'~', .textColour = WorldViewFramebuffer::LightCyan, .backColour = WorldViewFramebuffer::LightBlue };
	default: std::unreachable();
			//CellMapping{ u'w', {.c = u" ︢︣", .textColour = WorldViewFramebuffer::LightCyan, .backColour = WorldViewFramebuffer::LightBlue } },
	}
}

static WorldViewFramebuffer::Cell darkCellMapping(char c)
{
	switch (c)
	{
	case 'g': return { .c = u'░', .textColour = WorldViewFramebuffer::Black, .backColour = WorldViewFramebuffer::DarkGreen };
	case 'p': return { .c = u'░', .textColour = darken(EColour(76), 1), .backColour = darken(EColour(185), 1) };
	case 'd': return { .c = u'░', .textColour = darken(EColour(229), 1), .backColour = darken(EColour(229), 2) };
	case 't': return { .c = u'░', .textColour = WorldViewFramebuffer::Gray, .backColour = WorldViewFramebuffer::Grey23 };
	case 's': return { .c = u'▓', .textColour = WorldViewFramebuffer::Silver, .backColour = WorldViewFramebuffer::Gray };
	case 'w': return { .c = u'~', .textColour = WorldViewFramebuffer::Blue, .backColour = WorldViewFramebuffer::DarkBlue };
	default: std::unreachable();
		//CellMapping{ u'w', {.c = u" ︢︣", .textColour = WorldViewFramebuffer::LightCyan, .backColour = WorldViewFramebuffer::LightBlue } },
	}
}

using TerrainTemplate = SpriteTemplate<1, 1>;
using PlotInnerTemplate = SpriteTemplate<kDefaultPlotInnerDim.x, kDefaultPlotInnerDim.y>;

static const auto kTerrainSpritesLight = std::to_array({
	TerrainTemplate("g") | lightCellMapping, // grass
	TerrainTemplate("p") | lightCellMapping, // plains
	TerrainTemplate("d") | lightCellMapping, // desert
	TerrainTemplate("t") | lightCellMapping, // tundra
	TerrainTemplate("s") | lightCellMapping, // snow
});

static const auto kTerrainSpritesDark = std::to_array({
	TerrainTemplate("g") | darkCellMapping, // grass
	TerrainTemplate("p") | darkCellMapping, // plains
	TerrainTemplate("d") | darkCellMapping, // desert
	TerrainTemplate("t") | darkCellMapping, // tundra
	TerrainTemplate("s") | darkCellMapping, // snow
});

static EColour getWaterBackgroundColour(bool lit)
{
	return lit ? EColour::LightBlue : EColour(17);
}

[[nodiscard]] static EColour drawWater(WorldViewFramebuffer& fb, ivec2 position, ivec2 size, bool lit)
{
	WorldViewFramebuffer::Cell cell{
		.c = L'~',
		.textColour = lit ? EColour::LightCyan : EColour::Blue,
		.backColour = getWaterBackgroundColour(lit),
	};
	fb.fill(position, position + size, cell);
	return cell.backColour;
}

static EColour getPeakBackgroundColour(bool lit)
{
	return lit ? EColour::Gray : EColour::Black;
}

[[nodiscard]] static EColour drawPeak(WorldViewFramebuffer& fb, ivec2 position, [[maybe_unused]] ivec2 size, [[maybe_unused]] bool lit, const WorldViewFramebuffer& peakFB)
{
	//WorldViewFramebuffer::Cell cell{ .c = L' ', .textColour = EColour::White, .backColour = getPeakBackgroundColour(lit) };
	//const ivec2 max = position + size - ivec2(1, 1);
	//fb.fill(position, position + size, cell);
	//cell.c = L'\\';
	//fb.drawStippleLine(position, max, cell, "#");
	//cell.c = L'/';
	//fb.drawStippleLine({ max.x, position.y }, { position.x, max.y }, cell, "#");
	//cell.c = L'#';
	//fb.drawFilledEllipse(hecktui::iaabb2::sized(position, size).shrunk(size / 4), cell);
	//cell.c = L'▲';
	//fb.drawFilledEllipse(hecktui::iaabb2::sized(position, size).shrunk(size / 3), cell);
	////cell.c = L'X';
	////fb.drawCell(position + size / 2, cell);
	fb.drawSprite(position, peakFB.getCells());
	return peakFB.getCells()[{ 0, 0 }].backColour;
}

template<bool kLight>
static constexpr EColour lighten(EColour c)
{
	if constexpr (kLight)
		c = EColour(int(c) | 0b1000);
	return c;
}

template<bool kLight>
static constexpr WorldViewFramebuffer::Cell miscCellMapping(char c)
{
	switch (c)
	{
	case '+': return { .c = kLight ? u'▓' : u'▒', .ignoreTextColour = true, .ignoreBackColour = true };
	case '-': return { .c = u' ', .ignoreTextColour = true, .ignoreBackColour = true, .ignoreChar = true };
	case ' ': return { .c = u' ', .ignoreTextColour = true, .ignoreBackColour = true, .ignoreChar = true };
	case '/': return { .c = u'/', .ignoreTextColour = true, .ignoreBackColour = true };
	case '\\': return { .c = u'\\', .ignoreTextColour = true, .ignoreBackColour = true };
	case '#': return { .c = u'▓', .textColour = lighten<kLight>(EColour::Silver), .backColour = lighten<kLight>(EColour::Cyan) }; // Ξ
	case 'I': return { .c = u'♦', .textColour = lighten<kLight>(EColour::Silver), .backColour = lighten<kLight>(EColour::Cyan) }; // Ξ
	//case 'j': return { .c = u'♠', .textColour = lighten<kLight>(EColour::Green), .backColour = kLight ? EColour::Green : EColour::Black };
	//case 'J': return { .c = u'J', .textColour = lighten<kLight>(EColour::Green), .backColour = kLight ? EColour::Green : EColour::Black };
	case 'j': return { .c = u'♠', .textColour = EColour(46 - !kLight * 12 + 36), .ignoreBackColour = true };
	case 'J': return { .c = u'♠', .textColour = EColour(46 - !kLight * 12 + 36), .ignoreBackColour = true };
	case 'f': return { .c = u'♠', .textColour = EColour(22), .ignoreBackColour = true };
	case 'F': return { .c = u'♣', .textColour = EColour(22), .ignoreBackColour = true };
	//case 'f': return { .c = u'꜡', .textColour = lighten<kLight>(EColour::LightGreen), .ignoreBackColour = true };	
	default: std::unreachable();
	}
}

static constexpr WorldViewFramebuffer::Cell falloutCellMapping(char c)
{
	switch (c)
	{
	case '>': return { .c = u'►', .textColour = EColour::Black, .backColour = EColour::Yellow1 };
	case '<': return { .c = u'◄', .textColour = EColour::Black, .backColour = EColour::Yellow1 };
	case ' ': return { .c = u' ', .ignoreTextColour = true, .ignoreBackColour = true, .ignoreChar = true };
	default: std::unreachable();
	}
}

static void drawHill(WorldViewFramebuffer& fb, ivec2 position, [[maybe_unused]] ivec2 size, bool lit, TerrainTypes terrainTypeI, const WorldViewFramebuffer& renderedHillsFB)
{
	//if (terrainTypeI != 0)
	//{
	//	fb.drawFilledEllipse(hecktui::iaabb2::sized(position, size)/*.shrunk({ 1, 1 })*/, {
	//		.c = lit ? u'▓' : u'▒',
	//		//.c = u'▓',
	//		.ignoreTextColour = true,
	//		.ignoreBackColour = true
	//		});
	//}
	//else
	{
		fb.drawSpriteWithColourRemap(position, renderedHillsFB.getCells(),
			lit ? kTerrainArts[(int)terrainTypeI].coloursLight : kTerrainArts[(int)terrainTypeI].coloursDark);
	}


}

template<bool kLight>
static constexpr auto kFeature_Ice = SpriteTemplate<2, 2>(
	R"(#I)"
	R"(I#)"
) | miscCellMapping<kLight>;

template<bool kLight>
static constexpr auto kFeature_Jungle = SpriteTemplate<4, 2>(
	R"( j J)"
	R"(J j )"
) | miscCellMapping<kLight>;

template<bool kLight>
static constexpr auto kFeature_FloodPlains = SpriteTemplate<2, 2>(
	R"(  )"
	R"(  )"
) | miscCellMapping<kLight>;

template<bool kLight>
static constexpr auto kFeature_Forest = SpriteTemplate<4, 2>(
	R"( F f)"
	R"(f F )"
) | miscCellMapping<kLight>;

static constexpr auto kFeature_Fallout = SpriteTemplate<10, 10>(
	R"( ><       )"
	R"(          )"
	R"(          )"
	R"(    ><    )"
	R"(        ><)"
	R"(          )"
	R"(  ><      )"
	R"(          )"
	R"(          )"
	R"(          )"
) | falloutCellMapping;

static void drawFloodplain(WorldViewFramebuffer& fb, ivec2 position, ivec2 size, bool lit, std::array<bool, 4> cardinalRiversNESW, std::array<bool, 4> cardinalRiversNESWDiags)
{
	fb.fillPattern(position, position + size, lit ? kFeature_FloodPlains<true> : kFeature_FloodPlains<false>);

	const WorldViewFramebuffer::Cell bank{
		.c = L'▒',
		.textColour = lit ? lighten<true>(EColour::LightGreen) : EColour::LightGreen,
		//.backColour = lit ? EColour::LightGreen : EColour::Green,
		.ignoreBackColour = true,
	};

	if (cardinalRiversNESW[CARDINALDIRECTION_WEST]) fb.drawVLine(position, size.y, bank);
	if (cardinalRiversNESW[CARDINALDIRECTION_NORTH]) fb.drawHLine(position, size.x, bank);
	if (cardinalRiversNESW[CARDINALDIRECTION_EAST]) fb.drawVLine(position + ivec2(size.x - 1, 0), size.y, bank);
	if (cardinalRiversNESW[CARDINALDIRECTION_SOUTH]) fb.drawHLine(position + ivec2(0, size.y - 1), size.x, bank);

	if (cardinalRiversNESWDiags[0]) fb.drawCell(position + ivec2(size.x - 1, 0), bank);
	if (cardinalRiversNESWDiags[1]) fb.drawCell(position + size - ivec2(1, 1), bank);
	if (cardinalRiversNESWDiags[2]) fb.drawCell(position + ivec2(0, size.y - 1), bank);
	if (cardinalRiversNESWDiags[3]) fb.drawCell(position, bank);

	// Extend the banks into the plot gutters.
	// This could maybe alternatively be done by gutter painting.
	if (cardinalRiversNESWDiags[0] && cardinalRiversNESW[CARDINALDIRECTION_WEST] != cardinalRiversNESW[CARDINALDIRECTION_NORTH])
		fb.drawCell(position + (cardinalRiversNESW[CARDINALDIRECTION_WEST] ? ivec2(0, -1) : ivec2(-1, 0)), bank);
	if (cardinalRiversNESWDiags[1] && cardinalRiversNESW[CARDINALDIRECTION_NORTH] != cardinalRiversNESW[CARDINALDIRECTION_EAST])
		fb.drawCell(position + (cardinalRiversNESW[CARDINALDIRECTION_NORTH] ? ivec2(size.x, 0) : ivec2(size.x - 1, -1)), bank);
	if (cardinalRiversNESWDiags[2] && cardinalRiversNESW[CARDINALDIRECTION_EAST] != cardinalRiversNESW[CARDINALDIRECTION_SOUTH])
		fb.drawCell(position + (cardinalRiversNESW[CARDINALDIRECTION_EAST] ? ivec2(size.x - 1, size.y) : ivec2(size.x, size.y - 1)), bank);
	if (cardinalRiversNESWDiags[3] && cardinalRiversNESW[CARDINALDIRECTION_SOUTH] != cardinalRiversNESW[CARDINALDIRECTION_WEST])
		fb.drawCell(position + (cardinalRiversNESW[CARDINALDIRECTION_SOUTH] ? ivec2(-1, size.y - 1) : ivec2(0, size.y)), bank);
}

template<bool kLight>
static constexpr WorldViewFramebuffer::Cell improvementCellMapping(char c)
{
	switch (c)
	{
	case '?': return { .c = u'?', .textColour = EColour::White, .backColour = EColour::Black };
	default: std::unreachable();
	}
}

static void drawCity(WorldViewFramebuffer& fb, ivec2 position, ivec2 size, [[maybe_unused]] bool lit)
{
	fb.drawBox(position, size, {
		.c = L'Ξ',
		.textColour = EColour::Black,
		.ignoreBackColour = true,
		});
	fb.drawCell(position + ivec2(size.x / 2, 0), { .c = L' ', .ignoreTextColour = true, .ignoreBackColour = true });
	fb.drawCell(position + ivec2(0, size.y / 2), { .c = L' ', .ignoreTextColour = true, .ignoreBackColour = true });
	fb.drawCell(position + size - ivec2(1, 1) - ivec2(size.x / 2, 0), { .c = L' ', .ignoreTextColour = true, .ignoreBackColour = true });
	fb.drawCell(position + size - ivec2(1, 1) - ivec2(0, size.y / 2), { .c = L' ', .ignoreTextColour = true, .ignoreBackColour = true });
}

static std::wstring getShortString(std::wstring desc, int zoom)
{
	if (static_cast<int>(desc.size()) > kDefaultMaxUnitNameLength + zoom)
		std::erase(desc, u' ');
	if (static_cast<int>(desc.size()) > kDefaultMaxUnitNameLength + zoom)
	{
		std::erase_if(desc, [](wchar_t c) {
			switch (c)
			{
			case u'a':
			case u'e':
			case u'i':
			case u'o':
			case u'u':
				return true;
			default:
				return false;
			}
		});
	}
	return desc.substr(0, std::min<size_t>(desc.size(), kDefaultMaxUnitNameLength + zoom));
}

//static void shorten(std::vector<WorldViewFramebuffer::Cell>& desc, int zoom)
//{
//	if (static_cast<int>(desc.size()) > kDefaultMaxUnitNameLength + zoom)
//		std::erase_if(desc, [](WorldViewFramebuffer::Cell cell) { return cell.c == u' '; });
//	if (static_cast<int>(desc.size()) > kDefaultMaxUnitNameLength + zoom)
//	{
//		std::erase_if(desc, [](WorldViewFramebuffer::Cell cell) {
//			switch (cell.c)
//			{
//			case u'a':
//			case u'e':
//			case u'i':
//			case u'o':
//			case u'u':
//				return true;
//			default:
//				return false;
//			}
//		});
//	}
//	if (static_cast<int>(desc.size()) > kDefaultMaxUnitNameLength + zoom)
//		desc.resize(kDefaultMaxUnitNameLength + zoom);
//}

static const CvPlot* getRelPlot(const CvPlot* plot, hecktui::ivec2 relCoord)
{
	if (relCoord ==  hecktui::ivec2())
		return plot;
	return plot ? gGlobals.getMap().plot(plot->getX() + relCoord.x, plot->getY() + relCoord.y) : nullptr;
}

static bool hasCardinalRiver(const CvPlot* plot, hecktui::ivec2 relCoord, CardinalDirectionTypes dir)
{
	plot = getRelPlot(plot, relCoord);
	switch (dir)
	{
	case CARDINALDIRECTION_NORTH:
		plot = getRelPlot(plot, { 0, 1 });
		return plot && plot->isNOfRiver();
	case CARDINALDIRECTION_EAST:
		return plot && plot->isWOfRiver();
	case CARDINALDIRECTION_SOUTH:
		return plot && plot->isNOfRiver();
	case CARDINALDIRECTION_WEST:
		plot = getRelPlot(plot, { -1, 0 });
		return plot && plot->isWOfRiver();
	default:
		std::unreachable();
	}
}

EColour WorldView::getPlotBackgroundColour(const CvPlot& plot, WorldView::EPlotVisibility visibility)
{
	const TerrainTypes terrain = plot.getTerrainType();
	const PlotTypes plotType = plot.getPlotType();
	//const TeamTypes team = gGlobals.getGame().getActiveTeam();

	const bool light = visibility == WorldView::kLightened;
	const bool revealed = visibility >= WorldView::kDarkened;

	if (!revealed)
		return EColour::Black;

	EColour plotBackgroundColour = EColour::Black;

	if ((plotType == PLOT_HILLS || plotType == PLOT_LAND) && std::to_underlying(terrain) < static_cast<int>(std::size(kTerrainSpritesDark)))
	{
		const auto& pattern = (light ? kTerrainSpritesLight : kTerrainSpritesDark)[terrain];
		plotBackgroundColour = pattern.cells[0].backColour;
	}

	if (plotType == PLOT_HILLS)
		;
	else if (plotType == PLOT_PEAK)
		plotBackgroundColour = getPeakBackgroundColour(light);
	else if (plotType == PLOT_OCEAN)
		plotBackgroundColour = getWaterBackgroundColour(light);

	return plotBackgroundColour;
}

// Returns terrain background colour.
static EColour paintInnerPlot(WorldViewFramebuffer& fb, ivec2 coord, ivec2 zoomVec, const CvPlot& plot, WorldView::EPlotVisibility visibility,
	const WorldViewFramebuffer& renderedHillsFB, const WorldViewFramebuffer& renderedPeakFB)
{
	const ivec2 innerCoord = coord + ivec2{ 1, 1 };

	const TerrainTypes terrain = plot.getTerrainType();
	const PlotTypes plotType = plot.getPlotType();
	//const TeamTypes team = gGlobals.getGame().getActiveTeam();

	const bool light = visibility == WorldView::kLightened;
	const bool revealed = visibility >= WorldView::kDarkened;

	if (!revealed)
		return EColour::Black;

	EColour plotBackgroundColour = EColour::Black;

	const ivec2 plotInnerDim = kDefaultPlotInnerDim + zoomVec;

	if ((plotType == PLOT_HILLS || plotType == PLOT_LAND) && std::to_underlying(terrain) < static_cast<int>(std::size(kTerrainSpritesDark)))
	{
		const auto& pattern = (light ? kTerrainSpritesLight : kTerrainSpritesDark)[terrain];
		fb.fillPattern(innerCoord, innerCoord + plotInnerDim, pattern);
		plotBackgroundColour = pattern.cells[0].backColour;
	}

	if (plotType == PLOT_HILLS)
		drawHill(fb, innerCoord, plotInnerDim, light, terrain, renderedHillsFB);
	else if (plotType == PLOT_PEAK)
		plotBackgroundColour = drawPeak(fb, innerCoord, plotInnerDim, light, renderedPeakFB);
	else if (plotType == PLOT_OCEAN)
		plotBackgroundColour = drawWater(fb, innerCoord, plotInnerDim, light);

	return plotBackgroundColour;
}

static void paintPlotFeature(WorldViewFramebuffer& fb, ivec2 coord, ivec2 zoomVec, const CvPlot& plot, WorldView::EPlotVisibility visibility)
{
	const ivec2 innerCoord = coord + ivec2{ 1, 1 };

	const bool light = visibility == WorldView::kLightened;
	const bool revealed = visibility >= WorldView::kDarkened;

	if (!revealed)
		return;

	const ivec2 plotInnerDim = kDefaultPlotInnerDim + zoomVec;

	
	switch ((int)plot.getFeatureType())
	{
	case 0: // Ice
		fb.fillPattern(innerCoord, innerCoord + plotInnerDim, light ? kFeature_Ice<true> : kFeature_Ice<false>);
		break;
	case 1: // Jungle
		fb.fillPattern(innerCoord, innerCoord + plotInnerDim, light ? kFeature_Jungle<true> : kFeature_Jungle<false>);
		break;
	case 2: // Oasis
		fb.drawFilledEllipse(hecktui::iaabb2::sized(innerCoord + (plotInnerDim * ivec2(5, 1) + ivec2(5, 5)) / 10, (plotInnerDim * ivec2(4, 4) + ivec2(5, 5)) / 10), {
			.c = L' ',
			.backColour = EColour::Blue,
			.ignoreTextColour = true,
			});
		break;
	case 3: // Flood plains
	{
		const std::array<bool, 4> cardinalRivers{
			hasCardinalRiver(&plot, {}, CARDINALDIRECTION_NORTH),
			hasCardinalRiver(&plot, {}, CARDINALDIRECTION_EAST),
			hasCardinalRiver(&plot, {}, CARDINALDIRECTION_SOUTH),
			hasCardinalRiver(&plot, {}, CARDINALDIRECTION_WEST),
		};
		drawFloodplain(fb, innerCoord, plotInnerDim, light, cardinalRivers, {
			cardinalRivers[CARDINALDIRECTION_NORTH] || cardinalRivers[CARDINALDIRECTION_EAST] || hasCardinalRiver(&plot, { +1, +1 }, CARDINALDIRECTION_WEST) || hasCardinalRiver(&plot, { +1, +1 }, CARDINALDIRECTION_SOUTH), // NE
			cardinalRivers[CARDINALDIRECTION_SOUTH] || cardinalRivers[CARDINALDIRECTION_EAST] || hasCardinalRiver(&plot, { +1, -1 }, CARDINALDIRECTION_WEST) || hasCardinalRiver(&plot, { +1, -1 }, CARDINALDIRECTION_NORTH), // SE
			cardinalRivers[CARDINALDIRECTION_SOUTH] || cardinalRivers[CARDINALDIRECTION_WEST] || hasCardinalRiver(&plot, { -1, -1 }, CARDINALDIRECTION_EAST) || hasCardinalRiver(&plot, { -1, -1 }, CARDINALDIRECTION_NORTH), // SW
			cardinalRivers[CARDINALDIRECTION_NORTH] || cardinalRivers[CARDINALDIRECTION_WEST] || hasCardinalRiver(&plot, { -1, +1 }, CARDINALDIRECTION_EAST) || hasCardinalRiver(&plot, { -1, +1 }, CARDINALDIRECTION_SOUTH), // NW
			});
		break;
	}
	case 4: // Forest
		fb.fillPattern(innerCoord, innerCoord + plotInnerDim, light ? kFeature_Forest<true> : kFeature_Forest<false>);
		break;
	case 5: // Fallout
		fb.fillPattern(innerCoord, innerCoord + plotInnerDim, kFeature_Fallout);
		break;

	default:
		break;
	}
}

static void richPrint(std::vector<WorldViewFramebuffer::Cell>& cells, const WorldViewFramebuffer::Cell& attribs, std::wstring_view str)
{
	cells.append_range(str | std::views::transform([&attribs](wchar_t c) {
		auto attribs2 = attribs;
		attribs2.c = c;
		return attribs2;
	}));
}

static void paintPlotText(WorldViewFramebuffer& fb, ivec2 coord, ivec2 zoomVec, const CvPlot& plot, WorldView::EPlotVisibility visibility)
{
	const ivec2 innerCoord = coord + ivec2{ 1, 1 };

	const TeamTypes team = gGlobals.getGame().getActiveTeam();

	const bool light = visibility >= WorldView::kLightened;
	const bool revealed = visibility >= WorldView::kDarkened;

	if (!revealed)
		return;

	const auto plotCenterSampledDim = kDefaultPlotCenterSampledDim + zoomVec;
	const auto plotInnerDim = kDefaultPlotInnerDim + zoomVec;
	//const auto plotOuterDim = kDefaultPlotOuterDim + zoomVec;

	// Bonus/Improvement
	std::vector<WorldViewFramebuffer::Cell> cells;
	std::vector<WorldViewFramebuffer::Cell> cellsAbove;
	const ImprovementTypes improvementI = plot.getRevealedImprovementType(team, false);
	const BonusTypes bonusI = plot.getBonusType(team);
	if (improvementI != NO_IMPROVEMENT || bonusI != NO_BONUS)
	{
		//const EColour backColour = light ? EColour::Grey30 : EColour::Black;
		const EColour backColour = EColour::Black;
		cells.reserve(20);
		if (bonusI != NO_BONUS)
		{
			const std::string_view name = gGlobals.getBonusInfo(bonusI).getType();
			const std::wstring_view desc = gGlobals.getBonusInfo(bonusI).getDescription();
			
			if (name == "BONUS_COAL")
				richPrint(cells, { .textColour = EColour::Grey0, .backColour = EColour::Grey }, desc);
			else if (name == "BONUS_COPPER")
				richPrint(cells, { .textColour = EColour::White, .backColour = EColour(166) }, desc);
			else if (name == "BONUS_HORSE")
				richPrint(cells, { .textColour = EColour(166), .backColour = EColour::Black }, desc);
			else if (name == "BONUS_IRON")
				richPrint(cells, { .textColour = EColour::Grey0, .backColour = EColour::Grey62 }, desc);
			else if (name == "BONUS_GEMS")
			{
				const EColour colours[]{
					EColour::White,
					EColour(198),
					EColour(46),
					EColour(33),
					EColour(207),
				};
				for (size_t i = 0; i < desc.size(); ++i)
					richPrint(cells, { .textColour = colours[i % std::size(colours)], .backColour = backColour }, desc.substr(i, 1));
			}
			else if (name == "BONUS_URANIUM")
				richPrint(cells, { .textColour = EColour(220), .backColour = EColour::Black }, desc);
			else if (name == "BONUS_BANANA")
				richPrint(cells, { .textColour = EColour::Yellow1, .backColour = backColour }, desc);
			else if (name == "BONUS_CLAM")
				richPrint(cells, { .textColour = EColour::Black, .backColour = EColour(222) }, desc);
			else if (name == "BONUS_CORN")
				richPrint(cells, { .textColour = EColour::Yellow1, .backColour = EColour(22) }, desc);
			else if (name == "BONUS_COW")
				richPrint(cells, { .textColour = EColour::Black, .backColour = EColour(106) }, desc);
			else if (name == "BONUS_CRAB")
				richPrint(cells, { .textColour = EColour::Black, .backColour = EColour(70) }, desc);
			else if (name == "BONUS_DEER")
				richPrint(cells, { .textColour = EColour(130), .backColour = EColour::Black }, desc);
			else if (name == "BONUS_FISH")
				richPrint(cells, { .textColour = EColour::Black, .backColour = EColour(51) }, desc);
			else if (name == "BONUS_PIG")
				richPrint(cells, { .textColour = EColour(224), .backColour = EColour::Black }, desc);
			else if (name == "BONUS_RICE")
				richPrint(cells, { .textColour = EColour::White, .backColour = EColour(88) }, desc);
			else if (name == "BONUS_SHEEP")
				richPrint(cells, { .textColour = EColour::White, .backColour = EColour(22) }, desc);
			else if (name == "BONUS_WHEAT")
				richPrint(cells, { .textColour = EColour(220), .backColour = EColour::Black }, desc);
			else if (name == "BONUS_DYE")
				richPrint(cells, { .textColour = EColour::White, .backColour = EColour(135) }, desc);
			else if (name == "BONUS_FUR")
				richPrint(cells, { .textColour = EColour::Black, .backColour = EColour(130) }, desc);
			else if (name == "BONUS_GOLD")
				richPrint(cells, { .textColour = EColour(221), .backColour = EColour::Black }, desc);
			else if (name == "BONUS_IVORY")
				richPrint(cells, { .textColour = EColour::Grey74, .backColour = EColour::Black }, desc);
			else if (name == "BONUS_SILK")
				richPrint(cells, { .textColour = EColour(223), .backColour = EColour::Black }, desc);
			else if (name == "BONUS_SILVER")
				richPrint(cells, { .textColour = EColour(253), .backColour = EColour::Black }, desc);
			else if (name == "BONUS_SPICES")
				richPrint(cells, { .textColour = EColour(221), .backColour = EColour::Black }, desc);
			else
				richPrint(cells, { .textColour = EColour::White, .backColour = backColour }, desc);

			///
			
			if (name == "BONUS_ALUMINUM")
			{
				richPrint(cells, { .textColour = EColour::Grey, .backColour = EColour::Black }, L" ");
				richPrint(cells, { .textColour = EColour::Grey, .backColour = EColour::Black }, L"█");
				richPrint(cells, { .textColour = EColour::Grey, .backColour = EColour::Black }, L" ");
			}
			else if (name == "BONUS_COAL")
				richPrint(cells, { .textColour = EColour::Grey0, .backColour = EColour::Grey }, L"█▄");
			else if (name == "BONUS_COPPER")
				richPrint(cells, { .textColour = EColour(166), .backColour = EColour::Black }, L"█▄");
			else if (name == "BONUS_HORSE")
			{
				cellsAbove.insert(cellsAbove.end(), cells.size(), WorldViewFramebuffer::Cell{ .ignoreTextColour = true, .ignoreBackColour = true, .ignoreChar = true });
				richPrint(cellsAbove, { .textColour = EColour(166), .backColour = EColour::Black }, LR"(/\  _)");
				richPrint(cells     , { .textColour = EColour(166), .backColour = EColour::Black }, LR"( /▀▀\)");
			}
			else if (name == "BONUS_IRON")
				richPrint(cells, { .textColour = EColour::Grey62, .backColour = EColour::Grey0 }, L"█▄");
			else if (name == "BONUS_MARBLE")
				richPrint(cells, { .textColour = EColour::Grey62, .backColour = EColour::Black }, L"♦");
			else if (name == "BONUS_OIL")
				richPrint(cells, { .textColour = EColour::Black, .backColour = EColour::Grey62 }, L"▐█▌");
			else if (name == "BONUS_STONE")
				richPrint(cells, { .textColour = EColour::Grey78, .backColour = EColour::Black }, L"█▄");
			else if (name == "BONUS_URANIUM")
				richPrint(cells, { .textColour = EColour(196), .backColour = EColour(220) }, L"►‼◄");
			else if (name == "BONUS_BANANA")
				richPrint(cells, { .textColour = EColour::Yellow1, .backColour = EColour::Black, .underline = true }, L"///");
			else if (name == "BONUS_CLAM")
			{
				richPrint(cells, { .textColour = EColour(172), .backColour = EColour::Black }, L"Ͼ");
				richPrint(cells, { .textColour = EColour(222), .backColour = EColour::Black }, L"Ͻ");
				//richPrint(cells, { .textColour = EColour(222), .backColour = EColour::Black }, L"/");
			}
			else if (name == "BONUS_CORN")
			{
				for (int i = 0; i < 2; ++i)
				{
					richPrint(cells, { .textColour = EColour(40), .backColour = EColour(22) }, L"/");
					richPrint(cells, { .textColour = EColour::Yellow1, .backColour = EColour(22) }, L"'");
				}
			}
			else if (name == "BONUS_COW")
			{
				richPrint(cells     /**/, { .textColour = EColour::Black, .backColour = EColour(106) }, LR"(‾|)");
				richPrint(cells     /**/, { .textColour = EColour::White, .backColour = EColour(106) }, LR"(▀)");
				richPrint(cells     /**/, { .textColour = EColour::Black, .backColour = EColour(106) }, LR"(▀|)");
			}
			else if (name == "BONUS_CRAB")
				richPrint(cells, { .textColour = EColour(70), .backColour = EColour::Black }, L" Ж");
			else if (name == "BONUS_DEER")
			{
				cellsAbove.insert(cellsAbove.end(), cells.size(), WorldViewFramebuffer::Cell{ .ignoreTextColour = true, .ignoreBackColour = true, .ignoreChar = true });
				richPrint(cellsAbove/**/, { .textColour = EColour(130), .backColour = EColour::Black }, LR"(‾\   )");
				richPrint(cells     /**/, { .textColour = EColour(130), .backColour = EColour::Black }, LR"( /▀▀\)");
			}
			else if (name == "BONUS_FISH")
				richPrint(cells, { .textColour = EColour(51), .backColour = EColour::Black }, L"<°((><");
			else if (name == "BONUS_PIG")
			{
				cellsAbove.insert(cellsAbove.end(), cells.size(), WorldViewFramebuffer::Cell{ .ignoreTextColour = true, .ignoreBackColour = true, .ignoreChar = true });
				richPrint(cellsAbove/**/, { .textColour = EColour(219), .backColour = EColour::Black }, LR"(<(,  ,)>)");
				richPrint(cells     /**/, { .textColour = EColour(219), .backColour = EColour::Black }, LR"(( (∙∙) ))");
			}
			else if (name == "BONUS_RICE")
				richPrint(cells     /**/, { .textColour = EColour::White, .backColour = EColour::Black }, L"¤");
			else if (name == "BONUS_SHEEP")
			{
				//cellsAbove.insert(cellsAbove.end(), cells.size(), WorldViewFramebuffer::Cell{ .ignoreTextColour = true, .ignoreBackColour = true, .ignoreChar = true });
				richPrint(cells/**/, { .textColour = EColour::White, .backColour = EColour(22) }, LR"(‾)");
				richPrint(cells/**/, { .textColour = EColour::Black, .backColour = EColour(22) }, LR"(|)");
				richPrint(cells/**/, { .textColour = EColour::White, .backColour = EColour(22) }, LR"(▀▀)");
				richPrint(cells/**/, { .textColour = EColour::Black, .backColour = EColour(22) }, LR"(|)");
			}
			else if (name == "BONUS_WHEAT")
				richPrint(cells, { .textColour = EColour(220), .backColour = EColour::Black }, L" Ж");
			else if (name == "BONUS_DYE")
				richPrint(cells, { .textColour = EColour(135), .backColour = EColour::Black }, L"♦");
			else if (name == "BONUS_FUR")
				richPrint(cells, { .textColour = EColour(130), .backColour = EColour::Black }, L"<'");
			else if (name == "BONUS_GEMS")
				richPrint(cells, { .textColour = EColour::White, .backColour = EColour::Black }, L"♦♦♦");
			else if (name == "BONUS_GOLD")
				richPrint(cells, { .textColour = EColour(221), .backColour = EColour::Black }, L"█▄");
			else if (name == "BONUS_INCENSE")
				richPrint(cells, { .textColour = EColour::White, .backColour = EColour::Black }, L"≈");
			else if (name == "BONUS_IVORY")
			{
				cellsAbove.insert(cellsAbove.end(), cells.size(), WorldViewFramebuffer::Cell{ .ignoreTextColour = true, .ignoreBackColour = true, .ignoreChar = true });
				richPrint(cellsAbove/**/, { .textColour = EColour::Grey74, .backColour = EColour::Black }, LR"(/(▄▄ )");
				richPrint(cells     /**/, { .textColour = EColour::Grey74, .backColour = EColour::Black }, LR"( |▀▀|)");
			}
			else if (name == "BONUS_SILK")
			{
				richPrint(cells, { .textColour = EColour(137), .backColour = EColour::Black }, L"▄");
				richPrint(cells, { .textColour = EColour(223), .backColour = EColour::Black }, L"▀");
			}
			else if (name == "BONUS_SILVER")
				richPrint(cells, { .textColour = EColour(253), .backColour = EColour::Black }, L"█▄");
			else if (name == "BONUS_SPICES")
				richPrint(cells, { .textColour = EColour(214), .backColour = EColour(124) }, L"▀▄");
			else if (name == "BONUS_SUGAR")
				richPrint(cells, { .textColour = EColour::White, .backColour = EColour::Grey70 }, L"░░");
			else if (name == "BONUS_WINE")
				richPrint(cells, { .textColour = EColour(28), .backColour = EColour::White }, L"◘");
			else if (name == "BONUS_WHALE")
				richPrint(cells, { .textColour = EColour(33), .backColour = EColour::Black }, L"<(((><");
				
		}

		if (improvementI != NO_IMPROVEMENT)
			richPrint(cells, { .textColour = EColour::White, .backColour = backColour }, gGlobals.getImprovementInfo(improvementI).getDescription());

		//shorten(cells, zoomVec.x);

		cellsAbove.insert(cellsAbove.end(), cells.size() - cellsAbove.size(), WorldViewFramebuffer::Cell{ .ignoreTextColour = true, .ignoreBackColour = true, .ignoreChar = true });
		fb.drawText(innerCoord + ivec2(0, plotInnerDim.y / 2 + 0), plotInnerDim.x,
			cellsAbove,
			WorldViewFramebuffer::EHAlign::Center
		);
		fb.drawText(innerCoord + ivec2(0, plotInnerDim.y / 2 + 1), plotInnerDim.x,
			cells,
			WorldViewFramebuffer::EHAlign::Center
		);

		//cells.clear();
		//fb.drawText(innerCoord + ivec2(0, plotInnerDim.y / 2 + 1), plotInnerDim.x,
		//	cells,
		//	WorldViewFramebuffer::EHAlign::Center
		//);
	}

	//static constexpr std::wstring_view kCountCharacters = L"⓪①②③④⑤⑥⑦⑧⑨⑩⑪⑫⑬⑭⑮⑯⑰⑱⑲⑳∞";
	[[maybe_unused]] static constexpr std::wstring_view kCountCharacters = L"0123456789⑩⑪⑫⑬⑭⑮⑯⑰⑱⑲⑳∞";
	[[maybe_unused]] static constexpr std::wstring_view kSuperScriptCharacters = L"⁰¹²³⁴⁵⁶⁷⁸⁹⑩⑪⑫⑬⑭⑮⑯⑰⑱⑲⑳∞";

	// Yields
	{
		cells.clear();
		//const EColour backColour = light ? EColour::Grey30 : EColour::Black;
		[[maybe_unused]] static const EColour kColoursB[3]{ EColour(22), EColour(31), darken(darken(EColour(214))) };
		static const EColour kColoursA[3]{ EColour(185), EColour(153), EColour(226) };
		[[maybe_unused]] static const EColour kColoursADarkened[3]{ darken(kColoursA[0]), (darken(kColoursA[1])), (darken(kColoursA[2])) };
		const bool isWorked = plot.isVisibleWorked();
		if (light && isWorked)
			richPrint(cells, { .textColour = EColour::White, .backColour = EColour::Black, }, L"[");
		for (const int yieldI : range(gGlobals.getNUM_YIELD_TYPES()))
		{
			const int x = plot.calculateYield((YieldTypes)yieldI, true);
			if (x)
			{
				richPrint(cells, {
					//.textColour = kColoursB[yieldI],
					//.backColour = light && isWorked ? kColoursA[yieldI] : kColoursADarkened[yieldI],
					//.textColour = light && isWorked ? EColour::Black : EColour::White,
					//.backColour = light && isWorked ? EColour::White : EColour::Black,
					.textColour = EColour::White,
					.backColour = EColour::Black,
					.underline = false, //isWorked,
					},
					//x >= 0 ? std::wstring_view(&(isWorked ? kCountCharacters : kSuperScriptCharacters)[std::clamp<size_t>(x, 0, kCountCharacters.size() - 1)], 1) : L"-"
					x >= 0 ? std::wstring_view(&kCountCharacters[std::clamp<size_t>(x, 0, kCountCharacters.size() - 1)], 1) : L"-"
					);

				const CvYieldInfo& info = gGlobals.getYieldInfo((YieldTypes)yieldI);
				const wchar_t c = (wchar_t)info.getChar();
				const std::wstring_view str(&c, 1);
				const std::vector<hecktui::Pixel> symPixels = renderCiv4TextCode(str, {});
				for (const auto& pixel : symPixels)
					cells.push_back({ .c = pixel.c, .textColour = pixel.colour.text.code, .backColour = pixel.colour.back.code, .ignoreBackColour = pixel.colour.back.transparent });
			}
		}
		if (light && isWorked)
			richPrint(cells, { .textColour = EColour::White, .backColour = EColour::Black, }, L"]");

		// One row above unit text.
		fb.drawText(innerCoord + ivec2(0, plotInnerDim.y / 2 - 1), plotInnerDim.x,
			cells,
			WorldViewFramebuffer::EHAlign::Center
		);
	}

	// City text
	if (CvCity* const city = plot.getPlotCity(); city && city->isRevealed(team, kAllVisible))
	{
	//	//fb.drawSprite(innerCoord, kCity);
		drawCity(fb, innerCoord, plotInnerDim, light);
	//	//if (city->isStarCity())
	//	//	fb.drawSprite(innerCoord, kCityStar);
	//
	//	const ivec2 cityInnerTL = innerCoord + ivec2(1, 1);
	//	const ivec2 cityInnerBR = innerCoord + plotInnerDim - ivec2(1, 2);
	//	const ivec2 cityInnerTR(cityInnerBR.x, cityInnerTL.y);
	//	const ivec2 cityInnerBL(cityInnerTL.x, cityInnerBR.y);
	//	const int cityInnerWidth = plotCenterSampledDim.x - 3;
	//
	//	const int pop = city->getPopulation();
	//	const int turnsToGrow = city->getFoodTurnsLeft();
	//	const bool isStarving = city->foodDifference(true) < 0;
	//
	//	// Can't see happiness/healthiness without full city visibility.
	//	// See CvGameTextMgr::buildCityBillboardIconString for logic.
	//	const bool fullCityVisibility = city->canBeSelected();
	//	const bool angry = fullCityVisibility && city->angryPopulation() > 0;
	//	const bool sick = fullCityVisibility && city->healthRate() < 0;
	//	const EColour popBackColour = angry ? EColour::Maroon : EColour::Black;
	//	const EColour popColour = sick ? EColour::Lime : EColour::White;
	//	fb.drawText(cityInnerTL, std::format(L"{}☻", pop), { .textColour = popColour, .backColour = popBackColour }, WorldViewFramebuffer::EHAlign::Left);
	//
	//	if (fullCityVisibility)
	//	{
	//		if (turnsToGrow > 1 && !city->AI_isEmphasizeAvoidGrowth() && turnsToGrow < INT_MAX)
	//			fb.drawText(cityInnerTR, std::format(L"{}", turnsToGrow), { .textColour = isStarving ? EColour::Red : EColour::LightCyan, .backColour = EColour::Black }, WorldViewFramebuffer::EHAlign::Right);
	//
	//		if (city->getOrderQueueLength() > 0)
	//		{
	//			CvWString productionName = city->getProductionName();
	//			if (productionName.size() > cityInnerWidth)
	//				productionName.resize(cityInnerWidth);
	//
	//			fb.drawText(cityInnerBL, productionName, { .textColour = EColour::White, .backColour = EColour::Black }, WorldViewFramebuffer::EHAlign::Left);
	//
	//			const int productionTurnsLeft = city->getProductionTurnsLeft();
	//			if (productionTurnsLeft < INT_MAX)
	//				fb.drawText(cityInnerBR, std::format(L"{}", productionTurnsLeft), { .textColour = EColour::LightCyan, .backColour = EColour::Black }, WorldViewFramebuffer::EHAlign::Right);
	//		}
	//	}
	}

	// Units.
	if (light)
	{
		if (const CvUnit* const centerUnit = kAllVisible ? plot.getDebugCenterUnit() : plot.getCenterUnit())
		{
			std::wstring unitDesc = getShortString(centerUnit->getName(), zoomVec.x);

			const int numUnitsOnPlot = plot.getNumVisibleUnits(gGlobals.getGame().getActivePlayer());

			const PlayerColorTypes playerColourI = CvPlayerAI::getPlayerNonInl(centerUnit->getVisualOwner()).getPlayerColor();
			const EColour civColour = toTuiColour((ColorTypes)gGlobals.getPlayerColorInfo(playerColourI).getTextColorType());

			const bool selected = CvInterface::getInstance().getSelectionGroup().plot() == &plot;
			const WorldViewFramebuffer::EColour textColour = civColour; // selected ? WorldViewFramebuffer::White : WorldViewFramebuffer::Silver;

			const std::wstring countStr = std::wstring(1, kCountCharacters[std::min<size_t>(numUnitsOnPlot, kCountCharacters.size() - 1)]);

			// TODO: Deduplicate. Same logic as PlotListUnitButton.
			const EColour countColour =
				centerUnit->getTeam() != team || centerUnit->isWaiting() ? EColour::Grey30 :
				centerUnit->canMove() ? (centerUnit->hasMoved() ? EColour::DarkOrange : EColour::Lime) :
				EColour::Red;

			const bool isEnemy = centerUnit->shouldShowEnemyGlow(team);

			// Perfect centering padding.
			if ((countStr.size() + unitDesc.size()) % 2 != (plotCenterSampledDim.x - 2) % 2)
				unitDesc = L" " + unitDesc;

			cells.clear();
			if (selected)
				richPrint(cells, WorldViewFramebuffer::Cell{ .textColour = EColour::Grey100, .backColour = EColour::Black }, L"[");
			if (isEnemy)
			{
				richPrint(cells, WorldViewFramebuffer::Cell{ .textColour = EColour::White, .backColour = EColour(196) }, countStr);
				richPrint(cells, WorldViewFramebuffer::Cell{ .textColour = countColour, .backColour = EColour(196) }, L"●");
			}
			else
			{
				richPrint(cells, WorldViewFramebuffer::Cell{ .textColour = EColour::White, .backColour = EColour::Black }, countStr);
				richPrint(cells, WorldViewFramebuffer::Cell{ .textColour = countColour, .backColour = EColour::Black }, L"●");
			}
			// EColour(196)
			richPrint(cells, WorldViewFramebuffer::Cell{ .textColour = textColour, .backColour = EColour::Black/*, .underline = selected*/ }, unitDesc);
			if (selected)
				richPrint(cells, WorldViewFramebuffer::Cell{ .textColour = EColour::Grey100, .backColour = EColour::Black }, L"]");

			fb.drawText(innerCoord + ivec2(0, plotInnerDim.y / 2 + 0), plotInnerDim.x, cells, WorldViewFramebuffer::EHAlign::Center);
		}
	}
}

void WorldView::updateBeforeDraw()
{
	const hecktui::iaabb2 displayPlotsAABB = getDisplayedPlotsAABB();

	//CvMap& map = gGlobals.getMap();

	// Update fog.
	for (const int y : range(displayPlotsAABB.min.y, displayPlotsAABB.max.y))
		for (const int x : range(displayPlotsAABB.min.x, displayPlotsAABB.max.x))
		{
			// Force an entry.
			(void)mDisplayedVisibilityInfo[wrapPlotCoord({ x, y })];
		}

	// Remove entries outside viewport.
	std::erase_if(mDisplayedVisibilityInfo, [this](const std::pair<const ivec2, EPlotVisibility>& kv) {
		return !isPlotVisible(kv.first);
	});

	updateFog();
}

WorldViewFramebuffer WorldView::draw() const
{
	WorldViewFramebuffer fb(mWindowDim);
	const CvMap& map = gGlobals.getMap();

	const hecktui::iaabb2 displayPlotsAABB = getDisplayedPlotsAABB();

	const int visibleMinXPlot = displayPlotsAABB.min.x;
	const int visibleMaxXPlot = displayPlotsAABB.max.x;
	const int visibleMinYPlot = displayPlotsAABB.min.y;
	const int visibleMaxYPlot = displayPlotsAABB.max.y;

	const ivec2 zoomVec = makeZoomVec(mZoom);
	const ivec2 plotCenterSampledDim = kDefaultPlotCenterSampledDim + zoomVec;
	const ivec2 plotOuterDim = kDefaultPlotOuterDim + zoomVec;
	const ivec2 plotInnerDim = kDefaultPlotInnerDim + zoomVec;

	const int visibleWidthPlots = visibleMaxXPlot - visibleMinXPlot;
	const int visibleHeightPlots = visibleMaxYPlot - visibleMinYPlot;
	std::vector<EColour> plotBackgroundColours(visibleWidthPlots * visibleHeightPlots);
	SpriteSpan<EColour> plotBackgroundColoursMD(plotBackgroundColours, { visibleWidthPlots, visibleHeightPlots });

	const WorldViewFramebuffer renderedHillsFB = WorldViewUtils::renderHill(plotInnerDim, { EColour(0), EColour(1), EColour(2), EColour(3) });
	const WorldViewFramebuffer renderedPeakFB = WorldViewUtils::renderPeak(plotInnerDim,
		{
			EColour(232 + 6 * 0),
			EColour(232 + 6 * 1),
			EColour(232 + 6 * 2),
			EColour(232 + 6 * 3),
		},
		{
			EColour(255 - 2 * 0),
			EColour(255 - 2 * 1),
			EColour(255 - 2 * 2),
			EColour(255 - 2 * 3),
		}
	);

	// Render terrain
	for (int yplots = visibleMinYPlot; yplots < visibleMaxYPlot; ++yplots)
	{
		for (int xplots = visibleMinXPlot; xplots < visibleMaxXPlot; ++xplots)
		{
			if (const CvPlot* const plot = map.plot(xplots, yplots))
			{
				plotBackgroundColoursMD[{ xplots - visibleMinXPlot, yplots - visibleMinYPlot }] =
					paintInnerPlot(fb, mapCellToWindowCoord(mWindowDim, mCenterMapCellCoord, plotToTopLeftMapCellCoord(ivec2(xplots, yplots), mZoom)),
					zoomVec,
					*plot, kAllVisible ? EPlotVisibility::kLightened : mDisplayedVisibilityInfo.at(wrapPlotCoord({ xplots, yplots })), renderedHillsFB, renderedPeakFB);
			}
		}
	}

	/* 
	* Draw edges and corners. East edge, south edge, and the SE corner.
	* 
	* A border is when adjacent cells have a different owner (including no owner).
	* A double border is a border where both owners are players.
	*
	* *No idea if the below is up to date.
	* 
	* Edge rules:
	*     PlotBackground-PlotBackground: Blend with ▒.
	*     River, no borders: ≈≈≈≈
	*     Single border: Thick █.
	*     Double border: Both with half-block characters.
	*     River + single border: Stipple thick █.
	*     River + double border: Stipple with half-block characters.
	* Corner rules:
	*     No borders, no rivers, 1x (4) plot distinct background: Solid █.
	*     No borders, no rivers, 2x (2+2) plot distinct background: Blend with ▒.
	*     No borders, no rivers, 2x (1+3) plot distinct background: Solid █. Pick the most common colour.
	*     No borders, no rivers, 3x (2+1+1) plot distinct background: Solid █. Pick the colour that occurs twice.
	*     No borders, no rivers, 4x (1+1+1+1) plot distinct background: Blend with ▒. Pick opposing diagonals based on coordinate.
	*     No borders, 1x river: River.
	*     No borders, 2x river straight: River.
	*     No borders, 2x river bend: River.
	*     No borders, 3x river: River.
	*     No borders, 4x river: River.
	*     1x border edge: Impossible.
	*     2x border edge, 1 owner: Thick █.
	*     2x border edge, 2 owners, straight: Half-block character, same as edges.
	*     2x border edge, 2 owners, bend: Thick █ for most common plot owner.
	*     3x border edge, 2 owners (1 unowned plot): Half-block character, same as middle edge.
	*     3x border edge, 3 owners: Thick █ for most common plot owner.
	*     4x border edge, 1 unowned plot: Blend borders of unowned plot with ▒. The unowned plot will always have thick borders, so this is the tie-breaker.
	*     4x border edge, all plots owned, 2 owners: The owners diagonally oppose. Thick █, pick owner based on coordinate.
	*     4x border edge, all plots owned, 3 owners: One owner owns two diagonally opposing plots. Thick █.
	*     4x border edge, all plots owned, 4 owners: Blend with ▒. Pick opposing diagonals based on coordinate.
	* 
	*     Border rules above can be generalised:
	*         Set all quadrants of a corner to their plot owner.
	*         Perform border thickening:
	*         If 1 plot unowned, return Blend unowned plot borders with ▒ (can't do thick borders).
	*         If 2 plots unowned and they are cardinal, copy owners to the other side (all quadrants get an owner).
	*         If 2 plots unowned and they are diagonal, return Blend the two owners with ▒.
	*         If 3 plots unowned, copy the one owner to other quadrants.
	*         
	*         If solid or cardinal, half-block character.
	*         If the most common colour is unambiguous, Thick █.
	*         If there are two unique colours, Blend with ▒ (they are also diagonally opposing and both occur twice).
	*         There can't be three unique colours as one of them would have to occur twice.
	*         There are four unique colours. Blend opposing diagonals with ▒ based on coordinate.
	*         
	*/

	static constexpr char16_t kHalfBlendChar = u'▒';
	static constexpr char16_t kThickBorderChar = u'█';
	static constexpr char16_t kRiverChar = u'≈';
	static constexpr char16_t kHalfBorderChars[]{ u'▌', u'▀' }; // [horiz]
	static constexpr WorldViewFramebuffer::Cell kRiverCell{ .c = kRiverChar, .textColour = EColour::LightCyan, .backColour = EColour::LightBlue };

	const TeamTypes team = gGlobals.getGame().getActiveTeam();

	const auto plotIfRevealed = [team](const CvPlot* plot) {
		return plot && plot->isRevealed(team, kAllVisible) ? plot : nullptr;
	};

	// Plot gutters.
	for (int yplots = visibleMinYPlot; yplots < visibleMaxYPlot; ++yplots)
	{
		for (int xplots = visibleMinXPlot; xplots < visibleMaxXPlot; ++xplots)
		{
			const CvPlot* const plot = plotIfRevealed(map.plot(xplots, yplots));
			if (!plot)
				continue;

			enum EPlotOrder
			{
				kTL, kTR, kBR, kBL,
			};

			enum EEdgeOrder
			{
				kNorth, kEast, kSouth, kWest
			};

			const int rightX = xplots + 1 - visibleMinXPlot;
			const int bottomY = yplots - 1 - visibleMinYPlot;

			struct PlotGutterColouringInfo
			{
				const CvPlot* plot{};
				EColour background{};
				PlayerTypes owner{};
				EColour ownerColour{};

				PlotGutterColouringInfo() = default;
				explicit PlotGutterColouringInfo(const CvPlot* plot, EColour background, TeamTypes activeTeamI) : plot(plot), background(background),
					owner(plot ? plot->getRevealedOwner(activeTeamI, kAllVisible) : NO_PLAYER), ownerColour(pickVgaColour(owner))
				{
				}
			};

			const EColour plotBackgroundColoursTLCW[]{
				plotBackgroundColoursMD[{ xplots - visibleMinXPlot, yplots - visibleMinYPlot }],
				rightX < visibleWidthPlots ? plotBackgroundColoursMD[{ rightX, yplots - visibleMinYPlot }] : EColour::Black,
				rightX < visibleWidthPlots && bottomY >= 0 ? plotBackgroundColoursMD[{ rightX, bottomY }] : EColour::Black,
				bottomY >= 0 ? plotBackgroundColoursMD[{ xplots - visibleMinXPlot, bottomY }] : EColour::Black,
			};

			// Looking at plots even they're not revealed. Needed for the same border info as Civ4 (using getRevealedOwner).
			const PlotGutterColouringInfo plotInfosTLCW[5]{
				PlotGutterColouringInfo{ plot, plotBackgroundColoursTLCW[0], team },
				PlotGutterColouringInfo{ map.plot(xplots + 1, yplots), plotBackgroundColoursTLCW[1], team },
				PlotGutterColouringInfo{ map.plot(xplots + 1, yplots - 1), plotBackgroundColoursTLCW[2], team },
				PlotGutterColouringInfo{ map.plot(xplots, yplots - 1), plotBackgroundColoursTLCW[3], team },
				plotInfosTLCW[0] // for wrapping
			};

			const bool cornerRiversNESW[]{
				plot->isWOfRiver(),
				plotInfosTLCW[kTR].plot && plotInfosTLCW[kTR].plot->isNOfRiver(),
				plotInfosTLCW[kBL].plot && plotInfosTLCW[kBL].plot->isWOfRiver(),
				plot->isNOfRiver(),
			};

			static constexpr auto solidCell = [](EColour colour) {
				return WorldViewFramebuffer::Cell{ .c = kThickBorderChar, .textColour = colour, .backColour = colour };
			};

			static constexpr auto doubleBlockCell = [](bool horiz, EColour colourA, EColour colourB) {
				return WorldViewFramebuffer::Cell{ .c = kHalfBorderChars[horiz], .textColour = colourA, .backColour = colourB };
			};

			static constexpr auto blendCell = [](EColour colourA, EColour colourB) {
				return WorldViewFramebuffer::Cell{ .c = kHalfBlendChar, .textColour = colourA, .backColour = colourB };
			};

			static constexpr auto decideEdgeCell = [](bool horiz, EColour plotBackground, EColour adjPlotBackground, bool river, PlayerTypes owner, PlayerTypes neighbour, int i) {
				bool border = owner != neighbour;
				if (river && border)
				{
					river &= i % 2 == 0;
					border &= i % 2 != 0;
				}

				if (border)
				{
					if ((owner != NO_PLAYER) != (neighbour != NO_PLAYER))
					{
						if (owner != NO_PLAYER)
							neighbour = owner;
						else
							owner = neighbour;
					}
					return doubleBlockCell(horiz, pickVgaColour(owner), pickVgaColour(neighbour));
				}
				else if (river)
					return kRiverCell;
				else
					return blendCell(plotBackground, adjPlotBackground);
			};

			static constexpr auto isDifferent = [](auto p) {
				return std::get<0>(p) != std::get<1>(p);
			};

			static constexpr auto decideCornerCell = [](const bool(&riversNESW)[4], const PlotGutterColouringInfo(&plotInfosTLCW)[5], int tieBreaker) {

				// A corner is split into quadrants.
				// The layers are logically painted in this order:
				// 1. Paint the quadrants with the plot backgrounds.
				// 2. If river anywhere, paint all quadrants with river.
				// 3. Paint /cardinal/ border /owners/ in the corresponding halfs. Borders are keyed by owner, not colour. Border corners are ignored.
				//
				// Now with the quadrant colours, they need to be resolved to a Cell.
				// If there are rivers, resolve to river cell.
				// If there are borders, resolve to half/half cell. If one half has conflicting colours, pick by tie breaker.
				// Else, only plot backgrounds remain.
				// Else, if there are two unique colours, resolve to blend cell.
				// Else, if there are three unique colours, and one pair is cardinal, resolve to half/half cell with tie breaker for the non-uniform half.
				// Else, if there are three unique colours, and one pair is diagonal, resolve to blend cell and use the tie breaker for the other diagonal.
				// Else, if there are four unique colours, pick a diagonal by tie breaker and resolve to blend cell.

				if (std::ranges::count(riversNESW, true))
					return kRiverCell;

				std::array<EColour, 4> quadrants{};
				for (const int i : range(4))
					quadrants[i] = plotInfosTLCW[i].background;

				bool hasBorders = false;
				int borderQuadrantI = 0;

				for (const int i : range(4))
				{
					const int j = (i + 1) % 4;
					const int oppI = (i + 3) % 4;
					const int oppJ = (j + 1) % 4;
					if (plotInfosTLCW[i].owner == plotInfosTLCW[j].owner &&
						plotInfosTLCW[i].owner != plotInfosTLCW[oppI].owner &&
						plotInfosTLCW[j].owner != plotInfosTLCW[oppJ].owner &&
						plotInfosTLCW[i].owner != NO_PLAYER) // Cardinal only
					{
						quadrants[i] = plotInfosTLCW[i].ownerColour;
						quadrants[j] = plotInfosTLCW[i].ownerColour;
						quadrants[oppI] = plotInfosTLCW[i].ownerColour;
						quadrants[oppJ] = plotInfosTLCW[i].ownerColour;
						if (plotInfosTLCW[oppI].owner == plotInfosTLCW[oppJ].owner &&
							plotInfosTLCW[oppI].owner != NO_PLAYER)
						{
							quadrants[oppI] = plotInfosTLCW[oppI].ownerColour;
							quadrants[oppJ] = plotInfosTLCW[oppI].ownerColour;
						}
						borderQuadrantI = i;
						hasBorders = true;
						break;
					}
				}

				std::array<EColour, 4> orderedQuadrants = quadrants;
				std::ranges::sort(orderedQuadrants);

				const auto pairwise = orderedQuadrants | std::views::pairwise;

				const size_t numDistinctBackgrounds = 1 + std::ranges::count_if(pairwise, isDifferent);

				switch (numDistinctBackgrounds)
				{
				case 1:
					return solidCell(quadrants[0]);
				case 2:
					if (hasBorders)
					{
						if (borderQuadrantI % 2 == 0)
							return doubleBlockCell(borderQuadrantI % 2 == 0, quadrants[tieBreaker % 2], quadrants[2 + tieBreaker / 2 % 2]);
						else
							return doubleBlockCell(borderQuadrantI % 2 == 0, quadrants[(3 + tieBreaker / 2 % 2) % 4], quadrants[1 + tieBreaker % 2]);
					}
					//if (backgrounds[0] == backgrounds[1] && backgrounds[2] == backgrounds[3])
					//	return blendCell(backgrounds[0], backgrounds[2]);
					// 1222
					// 2221
					//else
					//	return solidCell(backgrounds[backgrounds[0] != backgrounds[1]]);
					return blendCell(orderedQuadrants[1], orderedQuadrants[2]);
				case 3:
				{
					if (hasBorders)
					{
						if (borderQuadrantI % 2 == 0)
							return doubleBlockCell(borderQuadrantI % 2 == 0, quadrants[tieBreaker % 2], quadrants[2 + tieBreaker / 2 % 2]);
						else
							return doubleBlockCell(borderQuadrantI % 2 == 0, quadrants[(3 + tieBreaker / 2 % 2) % 4], quadrants[1 + tieBreaker % 2]);
					}
					const size_t commonOrderedI = std::ranges::find_if_not(pairwise, isDifferent) - pairwise.begin();
					std::ranges::rotate(orderedQuadrants, orderedQuadrants.begin() + commonOrderedI);
					return blendCell(orderedQuadrants[0], orderedQuadrants[2 + tieBreaker % 2]);
				}
				default: // 4
					return blendCell(orderedQuadrants[tieBreaker % 2], orderedQuadrants[tieBreaker / 2 % 2]);
				}
			};

			// bool river, bool border, bool hasAdjOwner, EColour ownerColour, EColour neighbourColour, int i
			const WorldViewFramebuffer::Cell edges0[]{
				decideEdgeCell(false, plotBackgroundColoursTLCW[0], plotBackgroundColoursTLCW[1], cornerRiversNESW[0], plotInfosTLCW[0].owner, plotInfosTLCW[1].owner, 0),
				//decideEdgeCell(false, plotBackgroundColoursTLCW[1].background, plotBackgroundColoursTLCW[2].background, cornerRiversNESW[1], plotBackgroundColoursTLCW[1].ownerColour, plotBackgroundColoursTLCW[2].ownerColour, 0),
				//decideEdgeCell(false, plotBackgroundColoursTLCW[2].background, plotBackgroundColoursTLCW[3].background, cornerRiversNESW[2], plotBackgroundColoursTLCW[2].ownerColour, plotBackgroundColoursTLCW[3].ownerColour, 0),
				decideEdgeCell(true, plotBackgroundColoursTLCW[0], plotBackgroundColoursTLCW[3], cornerRiversNESW[3], plotInfosTLCW[0].owner, plotInfosTLCW[3].owner, 0),
			};

			const WorldViewFramebuffer::Cell edges1[]{
				decideEdgeCell(false, plotBackgroundColoursTLCW[0], plotBackgroundColoursTLCW[1], cornerRiversNESW[0], plotInfosTLCW[0].owner, plotInfosTLCW[1].owner, 1),
				//decideEdgeCell(false, plotBackgroundColoursTLCW[1].background, plotBackgroundColoursTLCW[2].background, cornerRiversNESW[1], plotBackgroundColoursTLCW[1].ownerColour, plotBackgroundColoursTLCW[2].ownerColour, 1),
				//decideEdgeCell(false, plotBackgroundColoursTLCW[2].background, plotBackgroundColoursTLCW[3].background, cornerRiversNESW[2], plotBackgroundColoursTLCW[2].ownerColour, plotBackgroundColoursTLCW[3].ownerColour, 1),
				decideEdgeCell(true, plotBackgroundColoursTLCW[0], plotBackgroundColoursTLCW[3], cornerRiversNESW[3], plotInfosTLCW[0].owner, plotInfosTLCW[3].owner, 1),
			};

			const WorldViewFramebuffer::Cell corner = decideCornerCell(cornerRiversNESW, plotInfosTLCW, xplots + yplots);

			const ivec2 cornerCoord = mapCellToWindowCoord(mWindowDim, mCenterMapCellCoord, plotToTopLeftMapCellCoord(ivec2(xplots, yplots), mZoom)) + plotCenterSampledDim;

			fb.drawStippleLine(cornerCoord - ivec2(0, 1), cornerCoord - plotCenterSampledDim * ivec2(0, 1) + ivec2(0, 1), edges0[0], " #");
			fb.drawStippleLine(cornerCoord - ivec2(0, 1), cornerCoord - plotCenterSampledDim * ivec2(0, 1) + ivec2(0, 1), edges1[0], "# ");

			fb.drawStippleLine(cornerCoord - ivec2(1, 0), cornerCoord - plotCenterSampledDim * ivec2(1, 0) + ivec2(1, 0), edges0[1], " #");
			fb.drawStippleLine(cornerCoord - ivec2(1, 0), cornerCoord - plotCenterSampledDim * ivec2(1, 0) + ivec2(1, 0), edges1[1], "# ");

			fb.drawCell(cornerCoord, corner);
		}
	}

	// Plot features.
	for (int yplots = visibleMinYPlot; yplots < visibleMaxYPlot; ++yplots)
	{
		for (int xplots = visibleMinXPlot; xplots < visibleMaxXPlot; ++xplots)
		{
			if (const CvPlot* const plot = map.plot(xplots, yplots))
			{
				paintPlotFeature(fb, mapCellToWindowCoord(mWindowDim, mCenterMapCellCoord, plotToTopLeftMapCellCoord(ivec2(xplots, yplots), mZoom)),
					zoomVec,
					*plot, kAllVisible ? EPlotVisibility::kLightened : mDisplayedVisibilityInfo.at(wrapPlotCoord({ xplots, yplots })));
			}
		}
	}

	// Routes
	for (int yplots = visibleMinYPlot; yplots < visibleMaxYPlot; ++yplots)
	{
		for (int xplots = visibleMinXPlot; xplots < visibleMaxXPlot; ++xplots)
		{
			const CvPlot* const plot = plotIfRevealed(map.plot(xplots, yplots));
			if (!plot)
				continue;

			const RouteTypes route = plot->getRevealedRouteType(team, kAllVisible);

			if (route == NO_ROUTE)
				continue;

			const auto hasRoute = [team, xplots, yplots, &plotIfRevealed, &map](ivec2 d) {
				const CvPlot* const plot = plotIfRevealed(map.plot(xplots + d.x, yplots + d.y));
				if (!plot)
					return false;

				return plot->getRevealedRouteType(team, kAllVisible) != NO_ROUTE;
			};

			const bool L = hasRoute({ -1, 0 });
			const bool B = hasRoute({ 0, -1 });
			const bool R = hasRoute({ +1, 0 });
			const bool T = hasRoute({ 0, +1 });
			const bool TL = hasRoute({ -1, +1 }) || (T && L);
			const bool BL = hasRoute({ -1, -1 }) || (B && L);
			const bool BR = hasRoute({ +1, -1 }) || (B && R);
			const bool TR = hasRoute({ +1, +1 }) || (T && R);

			// Half-coords with (0,0) at cell center.
			const ivec2 coordTL = mapCellToWindowCoord(mWindowDim, mCenterMapCellCoord, plotToTopLeftMapCellCoord(ivec2(xplots, yplots), mZoom)) * 2;
			const ivec2 coordBL = coordTL + plotCenterSampledDim * 2 * ivec2(0, 1);
			const ivec2 coordBR = coordTL + plotCenterSampledDim * 2;
			const ivec2 coordTR = coordTL + plotCenterSampledDim * 2 * ivec2(1, 0);
			const ivec2 coordL = coordTL + plotCenterSampledDim * ivec2(0, 1);
			const ivec2 coordB = coordBL + plotCenterSampledDim * ivec2(1, 0);
			const ivec2 coordR = coordTR + plotCenterSampledDim * ivec2(0, 1);
			const ivec2 coordT = coordTL + plotCenterSampledDim * ivec2(1, 0);
			const ivec2 coordCenter = coordTL + plotCenterSampledDim;

			const EColour colour = route == 0 ? EColour::Black : EColour::Grey50;

			const WorldViewFramebuffer::Cell cellFwd{
				.c = L'/',
				.textColour = colour,
				.ignoreBackColour = true,
			};

			const WorldViewFramebuffer::Cell cellBack{
				.c = L'\\',
				.textColour = colour,
				.ignoreBackColour = true,
			};

			const WorldViewFramebuffer::Cell cellH{
				.c = L'-',
				.textColour = colour,
				.ignoreBackColour = true,
			};

			const WorldViewFramebuffer::Cell cellV{
				.c = L'|',
				.textColour = colour,
				.ignoreBackColour = true,
			};

			static constexpr auto floorHalfs = [](int x) {
				return x - (x % 2 != 0);
			};

			static constexpr auto ceilHalfs = [](int x) {
				return x + (x % 2 != 0);
			};

			// A diagonal where each row has one plotted cell.
			// The drawn line is also symmetric if the input is symmetric.
			const auto drawVerticalDiagonal = [&fb](hecktui::ivec2 a, hecktui::ivec2 b, WorldViewFramebuffer::Cell defPixel) {

				const int minY = std::min(a.y, b.y);
				const int maxY = std::max(a.y, b.y);

				const int intMinY = ceilHalfs(minY) / 2;
				const int intMaxY = floorHalfs(maxY) / 2;

				if (intMinY >= intMaxY) [[unlikely]]
				{
					fb.drawCell(a / 2, defPixel);
					return;
				}

				// Symmetric tie-breaking. Round in the other direction if x < a.x and x falls precisely on a cell boundary.
				const bool tiebreakRoundDown = b.x < a.x;

				for (const int y : range(intMinY, intMaxY + 1))
				{
					// ax + (bx - ax) * tNumer / tDenom
					// realX = lerp(ax, bx, (y*2 - ay) / (by - ay)).
					const int realDXNumer = (b.x - a.x) * (y * 2 - a.y);
					const int realDXDenom = b.y - a.y;
					const int xNumer = realDXNumer + a.x*realDXDenom + realDXDenom;
					const int xDenom = 2*realDXDenom;
					int x = xNumer / xDenom;
					if (xNumer % xDenom == 0 && tiebreakRoundDown)
						--x;
					fb.drawCell({ x, y }, defPixel);
				}
			};

			if (TL)
				drawVerticalDiagonal(coordCenter, coordTL, cellBack);
			if (BL)
				drawVerticalDiagonal(coordCenter, coordBL, cellFwd);
			if (BR)
				drawVerticalDiagonal(coordCenter, coordBR, cellBack);
			if (TR)
				drawVerticalDiagonal(coordCenter, coordTR, cellFwd);

			if (L && !TL && !BL)
				fb.drawStippleLine(coordCenter / 2, coordL / 2, cellH, "#");
			if (R && !TR && !BR)
				fb.drawStippleLine(coordCenter / 2, coordR / 2, cellH, "#");
			if (T && !TL && !TR)
				fb.drawStippleLine(coordCenter / 2, coordT / 2, cellV, "#");
			if (B && !BL && !BR)
				fb.drawStippleLine(coordCenter / 2, coordB / 2, cellV, "#");

			//if (coordCenter.x % 2 != 0 || coordCenter.y != 0)
			//{
			//	//fb.drawCell(coordCenter / 2, { .c = L'\\', .textColour = colour, .ignoreBackColour = true });
			//	//fb.drawCell(coordCenter / 2 + ivec2(1, 0), { .c = L'/', .textColour = colour, .ignoreBackColour = true });
			//	//fb.drawCell(coordCenter / 2 + ivec2(1, 1), { .c = L'\\', .textColour = colour, .ignoreBackColour = true });
			//	//fb.drawCell(coordCenter / 2 + ivec2(0, 1), { .c = L'/', .textColour = colour, .ignoreBackColour = true });
			//}
			//else

			// Handle this specific case to hide the "//" pattern in the middle.
			if (coordCenter.x % 2 != 0 && coordCenter.y % 2 == 0)
			{
				fb.drawCell(coordCenter / 2, { .c = u'>', .textColour = colour, .ignoreBackColour = true, });
				fb.drawCell(coordCenter / 2 + ivec2(1, 0), { .c = u'<', .textColour = colour, .ignoreBackColour = true, });
			}
			// No adj roads, draw something.
			else if (!TL && !BL && !BR && !TR && !L && !R && !T && !B)
			{
				fb.drawCell(coordCenter / 2, {
					.c = u'x',
					.textColour = colour,
					.ignoreBackColour = true,
					});
			}
		}
	}

	// Coloured plots
	for (const auto& [coord, desc] : mColouredPlotsLookup)
	{
		const ivec2 windowCoord = mapCellToWindowCoord(mWindowDim, mCenterMapCellCoord, plotToTopLeftMapCellCoord(coord, mZoom));
		/*fb.drawBox(windowCoord + ivec2(1, 1), kPlotOuterDim - ivec2(2, 2), WorldViewFramebuffer::Cell{
				.c = u' ',
				.backColour = toTuiColour(desc.colour),
			});*/
		
		hecktui::iaabb2 rect = hecktui::iaabb2::sized(windowCoord, plotCenterSampledDim);
		rect.max += hecktui::ivec2(1, 1); // Overlap into the opposite plot gutters.
		fb.drawStippleEllipse(rect, WorldViewFramebuffer::Cell{
				.c = u' ',
				.backColour = toTuiColour(desc.colour),
			},
			"#");
	}

	for (int yplots = visibleMinYPlot; yplots < visibleMaxYPlot; ++yplots)
	{
		for (int xplots = visibleMinXPlot; xplots < visibleMaxXPlot; ++xplots)
		{
			if (const CvPlot* const plot = map.plot(xplots, yplots))
			{
				paintPlotText(fb, mapCellToWindowCoord(mWindowDim, mCenterMapCellCoord, plotToTopLeftMapCellCoord(ivec2(xplots, yplots), mZoom)),
					zoomVec,
					*plot, kAllVisible ? EPlotVisibility::kLightened : mDisplayedVisibilityInfo.at(wrapPlotCoord({ xplots, yplots })));
			}
		}
	}

	// Area fills.
	for (int yplots = visibleMinYPlot; yplots < visibleMaxYPlot; ++yplots)
	{
		for (int xplots = visibleMinXPlot; xplots < visibleMaxXPlot; ++xplots)
		{
			if (const auto it = mAreaBorderPlotsLookup.find({ xplots, yplots }); it != mAreaBorderPlotsLookup.end())
			{
				const NiColor colour = it->second.back().colour;
				const hecktui::ivec2 plotCoord = mapCellToWindowCoord(mWindowDim, mCenterMapCellCoord, plotToTopLeftMapCellCoord(ivec2(xplots, yplots), mZoom));
				fb.drawBox(plotCoord, plotOuterDim, {
					.c = L'█',
					.textColour = toTuiColour(NiColorA{ colour.r, colour.g, colour.b, 1.0f }),
					.backColour = EColour::Black
					});
			}
		}
	}

	// Plot signs
	for (int yplots = visibleMinYPlot; yplots < visibleMaxYPlot; ++yplots)
	{
		for (int xplots = visibleMinXPlot; xplots < visibleMaxXPlot; ++xplots)
		{
			if (const std::wstring_view label = CvEngine::getInstance().findSignTextAt(GC.getGame().getActivePlayer(), { xplots, yplots }); label.size())
			{
				fb.drawText(mapCellToWindowCoord(mWindowDim, mCenterMapCellCoord, plotToTopLeftMapCellCoord({ xplots, yplots }, mZoom)),
					plotOuterDim.x,
					label,
					{ .textColour = WorldViewFramebuffer::White, .backColour = WorldViewFramebuffer::Black },
					WorldViewFramebuffer::EHAlign::Center
				);
			}
		}
	}

	// Path segments between waypoints
	for (const auto& [prev, node] : mDisplayedPath | std::views::pairwise)
	{
		//fb.drawCell(mapCellToWindowCoord(mWindowDim, mCenterMapCellCoord, plotToTopLeftMapCellCoord(node.coord)),
		//	{ .c = '#', .textColour = node.red ? WorldViewFramebuffer::Red : WorldViewFramebuffer::White, .backColour = WorldViewFramebuffer::Black });
		
		//fb.drawBox(mapCellToWindowCoord(mWindowDim, mCenterMapCellCoord, plotToTopLeftMapCellCoord(node.coord)),
		//	kPlotOuterDim,
		//	{ .c = u'█', .textColour = node.red ? WorldViewFramebuffer::Red : WorldViewFramebuffer::White, .backColour = WorldViewFramebuffer::Black }
		//);

		fb.drawStippleLine(
			mapCellToWindowCoord(mWindowDim, mCenterMapCellCoord, plotToCenterMapCellCoord(prev.coord, mZoom)),
			mapCellToWindowCoord(mWindowDim, mCenterMapCellCoord, plotToCenterMapCellCoord(node.coord, mZoom)),
			{ .c = u'*', .textColour = WorldViewFramebuffer::White, .backColour = WorldViewFramebuffer::Black },
			"  #"
		);
	}

	// Draw the waypoints after.
	for (const size_t i : range(mDisplayedPath.size()))
	{
		const auto& end = mDisplayedPath[i];
		//if (i > 0 && mDisplayedPath[i].turn != mDisplayedPath[i - 1].turn || i == mDisplayedPath.size() - 1)
		if (i == mDisplayedPath.size() - 1 || mDisplayedPath[i + 1].turn != mDisplayedPath[i].turn)
		{
			fb.drawText(
				mapCellToWindowCoord(mWindowDim, mCenterMapCellCoord, plotToTopLeftMapCellCoord(end.coord, mZoom)) + plotOuterDim * ivec2(0, 1) / 2,
				plotOuterDim.x,
				end.turn >= 0 ? std::format(L"▐{}▌", end.turn) : L"▐⬤▌",
				{ .textColour = end.red ? WorldViewFramebuffer::Red : WorldViewFramebuffer::White, .backColour = WorldViewFramebuffer::Black },
				WorldViewFramebuffer::EHAlign::Center
			);
		}
	}

	return fb;
}

void WorldView::clearDirty()
{
	mInvalidated = false;
}

hecktui::iaabb2 WorldView::getDisplayedPlotsAABB() const
{
	const ivec2 topLeftMapCellCoord = getWindowTopLeftMapCellCoord(mWindowDim, mCenterMapCellCoord);
	const ivec2 bottomRightMapCellCoord = topLeftMapCellCoord + mWindowDim * kMapPerWindow;
	const ivec2 topLeftPlotCoord = mapCellToPlotCoord(topLeftMapCellCoord, mZoom);
	const ivec2 bottomRightPlotCoord = mapCellToPlotCoord(bottomRightMapCellCoord, mZoom);

	const int visibleMinXPlot = topLeftPlotCoord.x;
	const int visibleMaxXPlot = bottomRightPlotCoord.x + 1;
	const int visibleMinYPlot = bottomRightPlotCoord.y;
	const int visibleMaxYPlot = topLeftPlotCoord.y + 1;

	return {
		.min{ visibleMinXPlot, visibleMinYPlot },
		.max{ visibleMaxXPlot, visibleMaxYPlot },
	};
}

bool WorldView::isPlotVisible(ivec2 coord) const noexcept
{
	CvMap& map = gGlobals.getMap();

	const hecktui::iaabb2 rect = getDisplayedPlotsAABB();

	coord = wrapPlotCoord(coord);

	return wrapCoordDifference(coord.x - rect.min.x, map.getGridWidth(), map.isWrapX()) < rect.size().x
		&& wrapCoordDifference(coord.y - rect.min.y, map.getGridHeight(), map.isWrapY()) < rect.size().y;

}