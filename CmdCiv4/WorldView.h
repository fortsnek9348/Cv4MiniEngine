#pragma once

#include <CvGameCoreDLL/CommonShared.h>
#include <CvGameCoreDLL/CvEnums.h>
#include <CvGameCoreDLL/CvStructs.h>

#include <CommonStuff/aabb.h>

#include <string>
#include <span>
#include <vector>
#include <unordered_map>

class WorldViewFramebuffer;

class CvPlot;
class CvCity;

namespace hecktui
{
	enum class EColour : std::uint8_t;
}

namespace cvengine
{
	class WorldView
	{
	public:
		enum EPlotVisibility
		{
			kBlackened,
			kDarkened,
			kLightened,
		};

		// Used by minimap UI.
		static hecktui::EColour getPlotBackgroundColour(const CvPlot& plot, EPlotVisibility);

		struct ColouredPlotDesc
		{
			NiColorA colour{};
			PlotStyles plotStyle = PLOT_STYLE_NONE;
			PlotLandscapeLayers layer{};
		};


		void setWindowSize(heck::ivec2 windowDim);
		void setCenterPlot(heck::ivec2 coord);

		void scrollPlots(heck::ivec2 delta);
		void zoom(int levels);
		int getZoom() const;

		CvPlot* getLookAtPlot() const;
		heck::ivec2 getLookAtPlotCoord() const;

		CvPlot* getPlotAtWindowCoord(heck::ivec2 windowCoord) const;

		// Will auto-wrap to get as close to the center map cell coord as possible.
		heck::iaabb2 getPlotInnerRect(heck::ivec2 plotCoord) const;

		//void setHighlightedCity(CvCity*) noexcept;

		struct PathNode
		{
			heck::ivec2 coord{};
			// Stoppoints/waypoints are placed at turn increments.
			int turn = 0;
			bool red = false;

			friend bool operator==(const PathNode&, const PathNode&) = default;
		};
		void setDisplayedPath(std::span<const PathNode> path);

		enum class EDisplayMode
		{
			MainView,
			CityScreen,
		};

		void setDisplayMode(EDisplayMode mode);

		void setColouredPlot(heck::ivec2 coord, ColouredPlotDesc);
		void clearColouredPlots(PlotLandscapeLayers layer) noexcept;

		void addAreaBorderPlot(heck::ivec2, AreaBorderLayers, NiColorA colour);
		void clearAreaBorderPlots(AreaBorderLayers) noexcept;

		// Called from MyCvDLLEngineIFace, triggered by updateFog.
		void changePlotVisibility(heck::ivec2 coord, EPlotVisibility value);

		void invalidateAll();
		void invalidatePlot(heck::ivec2 coord);
		void updateFog();
		// This will clear and regenerate coloured plot infos.
		// This should not be called all the time as this will drop event plot circles.
		void updateColouredPlots();

		// "dirty" => draw() return value has changed.
		bool isDirty() const;
		void updateBeforeDraw();
		WorldViewFramebuffer draw() const;
		void clearDirty();

		// Does not wrap.
		heck::iaabb2 getDisplayedPlotsAABB() const;

		bool isPlotVisible(heck::ivec2 coord) const noexcept;

	private:
		int mZoom = 5;

		// This value should always be within map bounds, without wrapping.
		heck::ivec2 mCenterMapCellCoord{};

		heck::ivec2 mWindowDim{};

		EDisplayMode mDisplayMode = EDisplayMode::MainView;

		bool mInvalidated = true;

		std::vector<PathNode> mDisplayedPath;

		IDInfo mHighlightedCityId{};

		struct Hasher
		{
			size_t operator()(heck::ivec2 coord) const noexcept;
		};

		std::unordered_map<heck::ivec2, ColouredPlotDesc, Hasher> mColouredPlotsLookup;
		// "Displayed", in that we only need visibility info for displayed plots.
		std::unordered_map<heck::ivec2, EPlotVisibility, Hasher> mDisplayedVisibilityInfo;

		struct AreaBorderEntry
		{
			AreaBorderLayers layer{};
			NiColor colour{};
		};

		std::unordered_map<heck::ivec2, std::vector<AreaBorderEntry>, Hasher> mAreaBorderPlotsLookup;
	};
}