#pragma once

#include <Cv4CommonEngineLib/CvEngineEnums.h>

#include <CvEnums.h>

#include <HeckTextUI/Control.h>

class CvMap;
class CvReplayInfo;

namespace cvengine
{
	class MinimapSectionTracking
	{
	public:

		// Called by CvInterface.
		void update();

		// This is always zero or positive, but may need to be wrapped.
		hecktui::iaabb2 getCurrentRect() const;

	private:
		hecktui::iaabb2 mRect{};
		std::vector<uint16_t> mCachedColAnyRevealedY; // [x] = y
		std::vector<uint16_t> mCachedRowAnyRevealedX; // [y] = x
	};

	hecktui::ivec2 getMinimapBaseTextureDim();

	// It's not very high definition, but it's there and you can click on it.
	class Minimap : public hecktui::Element
	{
	public:

		Minimap();

		cvengine::ECvScreen screenEventSource = cvengine::ECvScreen::NO_SCREEN;
		bool handleMouseMove = false;

		struct Marker
		{
			hecktui::ivec2 coord{};
			ColorTypes colourI = NO_COLOR;
			wchar_t c{};
		};

		void clearMinimapMarkers() noexcept;
		void addMinimapMarker(Marker);

		void setMinimapBaseTextureFromReplay(const CvReplayInfo&);
		void setPlotCulture(hecktui::ivec2 coord, ColorTypes colour);

		virtual bool onEvent(const hecktui::ConsoleEvent& e) override;

		struct fvec2
		{
			float x{};
			float y{};
		};

		// Map from render coords to plot coords.
		struct RenderCoordMapping
		{
			fvec2 scale{};
			fvec2 offset{};

			RenderCoordMapping() = default;

			explicit RenderCoordMapping(hecktui::iaabb2 renderRect, hecktui::iaabb2 mapRect);
			hecktui::ivec2 toPlotCoord(fvec2 renderCoord) const;
			hecktui::ivec2 toRenderCoord(fvec2 plotCoord) const;
		};

		// This is externally used to create a minimap texture for replays.
		// If teamI == NO_TEAM, then assume this is the replay minimap where every plot is revealed.
		static hecktui::RGBF computeMinimapBaseTexturePixelColour(const CvMap& map, TeamTypes teamI, RenderCoordMapping mapping, hecktui::ivec2 coord);

	protected:
		virtual hecktui::LayoutSizeInfo measureThis() const override;
		virtual void drawThis(hecktui::ivec2 offset, hecktui::Framebuffer& fb) override;

	private:
		std::vector<std::array<uint8_t, 3>> mReplayBaseTexture;
		std::vector<ColorTypes> mReplayCultureMap;
		hecktui::ivec2 mReplayMapSize{};
		std::vector<Marker> mMarkers;
	};
}