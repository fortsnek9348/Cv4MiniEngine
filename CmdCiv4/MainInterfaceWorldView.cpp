#include "MainInterface.h"
#include "WorldView.h"
#include "Graphics.h"
#include "CvInterface.h"
#include "Common.h"
#include "TuiTextCode.h"
#include "RichGuage.h"
#include "CvApp.h"

#include <CvMap.h>
#include <CvCity.h>
#include <CvGlobals.h>
#include <CvGameTextMgr.h>
#include <CvGameCoreUtils.h>
#include <CvGameAI.h>

#include <HeckTextUI/BasicControls.h>

#include <CommonStuff/range.h>

#include <array>
#include <ranges>

using heck::range;

namespace
{
	using namespace hecktui;

	class WorldViewUIComponent;

	struct WorldViewUIInnerLayout : Layout
	{
		WorldViewUIComponent& container;

		explicit WorldViewUIInnerLayout(WorldViewUIComponent& container) : container(container)
		{
		}

		virtual LayoutSizeInfo layoutPhase1Measure(ChildList) const override
		{
			return {};
		}

		// We will abuse the layout system to update city billboard panels.
		// This is called after the WorldViewUIComponent is positioned, every frame.
		virtual void layoutPhase2Apply(ivec2, ChildList) const override;
	};

	struct CityBillboard : std::enable_shared_from_this<CityBillboard>, EmptyButton
	{
		std::vector<hecktui::Pixel> iconLineText;
		NiColorA popBackColour{};
		std::vector<hecktui::Pixel> popText;
		std::vector<hecktui::Pixel> nameText;
		std::vector<hecktui::Pixel> prodText;

		static constexpr int kFakeBarMax = 1000;
		std::array<EColour, 4> foodBarTuiColours{};
		std::array<EColour, 4> prodBarTuiColours{};
		std::array<int, 4> foodBarRatios{};
		std::array<int, 4> prodBarRatios{};

		bool isSelected = false;
		IDInfo idInfo{};

		static constexpr int kBorderThickness = 1;
		static constexpr int popWidth = 3;

		explicit CityBillboard(IDInfo id) : idInfo(id)
		{
			setBorderStyle(EBorderStyle::None);
			setBackgroundColour(EColour::Black);
		}

		virtual bool onEvent(const ConsoleEvent& e) override
		{
			bool handled = false;
			// Handle MouseButtonDown instead of onClick so that city selection isn't cleared on mouse up after MouseButtonDoubleClick.
			//if (e.type == EConsoleEventType::MouseButtonDown)
			//{
			//	const auto e2 = static_cast<const MouseButtonEvent&>(e);
			//	if (e2.button == EMouseButton::Left)
			//	{
			//		gGlobals.getGame().selectCity(getCity(idInfo), e2.ctrl, e2.alt, e2.shift);
			//		CvInterface::getInstance().onGameStateChanged(CvInterface::EGameStateChangeReason::CitySelection);
			//		handled = true;
			//	}
			//}
			if (e.type == EConsoleEventType::MouseButtonDoubleClick)
			{
				if (static_cast<const MouseButtonEvent&>(e).button == EMouseButton::Left)
				{
					// Treat as double clicking on the plot.
					if (CvCity* const city = getCity(idInfo))
						CvInterface::getInstance().onWorldViewDoubleClickPlot(city->plot());
					handled = true;
				}
			}

			return EmptyButton::onEvent(e) || handled;
		}

		virtual void onClick(ModifierKeyState modifierKeys) override
		{
			// Minor HACK: Double clicking will bring up the city screen, and then mouse up will reset city selection, without this check.
			//if (!CvInterface::getInstance().isInCityScreen())
				gGlobals.getGame().selectCity(getCity(idInfo), modifierKeys.ctrl, modifierKeys.alt, modifierKeys.shift);
		}

		virtual void onBeginMouseHover() override
		{
			CvInterface::getInstance().onCityBillboardMouseOver(shared_from_this(), idInfo);
		}

		void updateCityBillboard(const iaabb2& plotInnerRect, CvCity& city)
		{
			// The IDInfo can change when the owner changes.
			idInfo = city.getIDInfo();

			auto& textMgr = CvGameTextMgr::GetInstance();
			CvWStringBuffer buf;

			const Pixel defPixel{
				.c = L' ',
				.colour{ EColour::White, kTransparent }
			};

			textMgr.buildCityBillboardIconString(buf, &city);
			std::wstring str = std::wstring(buf.getCString());
			// Stick the star in front if this is the capital.
			if (city.isCapital())
				str.insert(str.begin(), CvApp::getInstance().symbols[STAR_CHAR]);
			iconLineText = renderCiv4TextCode(str, defPixel);
			
			NiColorA textColour{};
			city.getCityBillboardSizeIconColors(popBackColour, textColour);

			textMgr.buildCityBillboardCitySizeString(buf, &city, textColour);
			popText = renderCiv4TextCode(buf.getCString(), defPixel);

			textMgr.buildCityBillboardCityNameString(buf, &city);
			nameText = renderCiv4TextCode(buf.getCString(), defPixel);

			//iFoodDifference = pHeadSelectedCity.foodDifference(True)

			if (city.canBeSelected()) // buildCityBillboardProductionString doesn't seem to check this.
			{
				std::vector<NiColorA> colours;
				std::vector<float> stackedPercentages;

				textMgr.getCityBillboardFoodbarColors(&city, colours);
				std::ranges::copy(colours | std::views::transform([](NiColorA c) { return toTuiColour(c); }), foodBarTuiColours.begin());
				city.getFoodBarPercentages(stackedPercentages);
				foodBarRatios = {
					static_cast<int>(std::lround(stackedPercentages[INFOBAR_STORED] * kFakeBarMax)),
					static_cast<int>(std::lround(stackedPercentages[INFOBAR_RATE] * kFakeBarMax)),
					static_cast<int>(std::lround(stackedPercentages[INFOBAR_RATE_EXTRA] * kFakeBarMax)),
				};

				textMgr.buildCityBillboardProductionString(buf, &city);
				prodText = renderCiv4TextCode(buf.getCString(), defPixel);
				textMgr.getCityBillboardProductionbarColors(&city, colours);
				std::ranges::copy(colours | std::views::transform([](NiColorA c) { return toTuiColour(c); }), prodBarTuiColours.begin());
				city.getProductionBarPercentages(stackedPercentages);
				prodBarRatios = {
					static_cast<int>(std::lround(stackedPercentages[INFOBAR_STORED] * kFakeBarMax)),
					static_cast<int>(std::lround(stackedPercentages[INFOBAR_RATE] * kFakeBarMax)),
					static_cast<int>(std::lround(stackedPercentages[INFOBAR_RATE_EXTRA] * kFakeBarMax)),
				};
			}
			else
			{
				prodText.clear();
				foodBarRatios = {};
				prodBarRatios = {};
			}

			isSelected = CvInterface::getInstance().isCitySelected(&city);

			const int maxLineWidth = (int)std::max({
				iconLineText.size(),
				popWidth + nameText.size(),
				prodText.size(),
				});

			int billboardWidth = maxLineWidth;
			// Ensure perfect centering.
			if ((billboardWidth - plotInnerRect.size().x) % 2 != 0)
				++billboardWidth;

			// Ensure min width.
			constexpr int additionalWidth = 3;
			billboardWidth = std::max(billboardWidth, plotInnerRect.size().x + additionalWidth * 2);

			setRect(iaabb2::sized({ ((plotInnerRect.min.x + plotInnerRect.max.x) - billboardWidth) / 2, plotInnerRect.max.y - 1 }, { billboardWidth, 3 })
				.shrunk({ -kBorderThickness, -kBorderThickness })
			);
		}

		virtual void drawThis(ivec2 offset, hecktui::Framebuffer& fb) override
		{
			//EmptyButton::drawThis(offset, fb);

			// Completely taking over drawing.

			iaabb2 rect = (offset + getRect());

			if (isSelected)
				fb.drawBox(rect, EBorderStyle::Rounded, { .text = EColour::White, .back = EColour::Black });

			rect = rect.shrunk({ kBorderThickness, kBorderThickness });

			const iaabb2 oldStencilRect = fb.intersectScissorRect(rect);


			fb.drawFilledBox(rect, { .colour = getCurrentPixelColouring() });

			fb.blit(rect, iconLineText, EAlign::Center, EAlign::Top, false);
			rect.min.y += 1;

			

			const iaabb2 popRect = iaabb2::sized(rect.min, { popWidth, 1 });
			fb.drawFilledBox(popRect, Pixel{ .c = L' ', .colour{ .back = toTuiColour(popBackColour) } });
			fb.blit(popRect, popText, EAlign::Right, EAlign::Center, false);
			rect.min.x += popWidth;
			RichGuage::drawBareGuage(fb, rect, foodBarTuiColours, foodBarRatios, kFakeBarMax);
			fb.blit(rect, nameText, static_cast<int>(nameText.size()) <= rect.size().x ? EAlign::Center : EAlign::Right, EAlign::Top, false);
			rect.min.x -= popWidth;
			rect.min.y += 1;
			RichGuage::drawBareGuage(fb, rect, prodBarTuiColours, prodBarRatios, kFakeBarMax);
			fb.blit(rect, prodText, static_cast<int>(prodText.size()) <= rect.size().x ? EAlign::Center : EAlign::Right, EAlign::Bottom, false);

			fb.setScissorRect(oldStencilRect);
		}
	};

	class WorldViewUIComponent : public Element
	{
	public:
		explicit WorldViewUIComponent(CvInterface& interface, WorldView& view) : interface(interface), worldView(view)
		{
			mCanFocus = false;
			mIsMouseInteractable = true;
			// Ensure our FB is updated.
			worldView.invalidateAll();

			setLayout(std::make_unique<WorldViewUIInnerLayout>(*this));
		}

		virtual void drawThis(ivec2 offset, hecktui::Framebuffer& fb) override
		{
			const hecktui::ivec2 position = offset + getRect().min;
			const hecktui::ivec2 size = getRect().size();
			const int w = size.x;
			const int h = size.y;
			worldView.setWindowSize({ w, h });

			// Update displayed path.
			CvInterface::getInstance().onBeforeWorldViewRender();

			if (worldView.isDirty())
			{
				worldViewFB = worldView.draw();
				worldView.clearDirty();
			}

			for (int y = 0; y < h; ++y)
			{
				for (int x = 0; x < w; ++x)
				{
					const WorldViewFramebuffer::Cell cell = worldViewFB.getCells()[{ x, y }];
					
					fb.getPixel(position + hecktui::ivec2{ x, y }) = {
						.c = cell.c,
						.colour{
							.text = cell.textColour,
							.back = cell.backColour,
							},
						.underline = cell.underline,
						};
				}
			}
		}

		virtual bool onEvent(const ConsoleEvent& e) override
		{
			struct Handler
			{
				WorldViewUIComponent& self;

				bool operator()(const MouseButtonEvent& e) const
				{
					if (CvPlot* const plot = self.worldView.getPlotAtWindowCoord({ e.coord.x, e.coord.y }))
					{
						if (e.button == EMouseButton::Left)
						{
							if (e.type == EConsoleEventType::MouseButtonDown)
							{
								self.interface.onWorldViewClickPlot(plot, e.shift);

								if (self.mClickedPlot)
								{
									if (self.mClickedPlotReleased)
									{
										//const auto dt = std::chrono::steady_clock::now() - self.mClickedPlotTime;
										//if (dt <= std::chrono::milliseconds(getDoubleClickMilliseconds()))
										//	self.interface.onWorldViewDoubleClickPlot(plot);

										// FTXUI oddity. We get another mouse "click" event immediately afterwards, so can't let mClickedPlot end up as null.
										//self.mClickedPlot = plot;
										//self.mClickedPlotTime = std::chrono::steady_clock::now();
										//self.mClickedPlotReleased = false;
									}
								}
								else
								{
									//self.mClickedPlot = plot;
									//self.mClickedPlotTime = std::chrono::steady_clock::now();
									//self.mClickedPlotReleased = false;
								}

								//e.screen_->PostEvent(kUIEvent_UnitSelectionChanged);
							}
							else if (e.type == EConsoleEventType::MouseButtonDoubleClick)
							{
								self.interface.onWorldViewDoubleClickPlot(plot);
							}
							else
							{
								self.interface.onWorldViewButtonUpOnPlot(plot, e);
								self.mClickedPlotReleased = true;
							}
						}
						else if (e.button == EMouseButton::Right)
						{
							self.interface.cancelInterfaceMode();
						}
						else if (e.button == EMouseButton::Middle)
						{
							if (e.type == EConsoleEventType::MouseButtonDown)
								self.interface.onWorldViewMiddleButtonOnPlot(plot);
						}
					}

					return true;
				}

				bool operator()(const MouseMoveEvent& e) const
				{
					CvPlot* const plot = self.worldView.getPlotAtWindowCoord({ e.coord.x, e.coord.y });
					self.interface.onWorldViewMouseOverPlot(plot, self.getUISceneState().capture);
					return true;
				}

				bool operator()(const MouseScrollEvent& e) const
				{
					self.worldView.zoom(-e.scroll);
					return true;
				}

				bool operator()(const ConsoleEvent&) const
				{
					return false;
				}
			};

			return dispatch(e, Handler(*this));
		}

		virtual void onEndMouseHover() override
		{
			interface.onWorldViewMouseLeave();
		}

		void updateCityBillboards()
		{
			const ivec2 space = getRect().size();

			worldView.setWindowSize(space);
			
			const iaabb2 visiblePlotsRect = worldView.getDisplayedPlotsAABB().shrunk({ -1, -1 });

			const CvMap& map = gGlobals.getMap();

			const bool areBillboardsVisible = !interface.isInCityScreen();

			const TeamTypes activeTeam = gGlobals.getGame().getActiveTeam();

			auto existingCityBillboards = std::move(cityBillboards);

			if (areBillboardsVisible)
			{
				for (const int plotY : range(visiblePlotsRect.min.y, visiblePlotsRect.max.y))
				{
					for (const int plotX : range(visiblePlotsRect.min.x, visiblePlotsRect.max.x))
					{
	 					if (const CvPlot* const plot = map.plot(plotX, plotY))
						{
							if (CvCity* const city = plot->getPlotCity(); city && city->isRevealed(activeTeam, false))
							{
								const std::pair key{ plotY, plotX };
								std::shared_ptr<CityBillboard> ctrl;
								if (const auto existingIt = existingCityBillboards.find(key); existingIt != existingCityBillboards.end())
									ctrl = cityBillboards.insert(existingCityBillboards.extract(existingIt)).position->second;
								else
								{
									ctrl = std::make_shared<CityBillboard>(city->getIDInfo());
									cityBillboards[key] = ctrl;
									addChild(ctrl);
								}

								ctrl->updateCityBillboard(worldView.getPlotInnerRect({ plotX, plotY }), *city);
							}
						}
					}
				}
			}

			// Remove unused city billboards.
			for (auto& ctrl : existingCityBillboards | std::views::values)
				ctrl->orphan();
		}

		void checkCameraPosition()
		{
			const auto checkAxis = [](int position, int min, int exclMax, bool wrap, int dim) {
				assert((0 <= min && min < exclMax && min < dim) || (min == 0 && exclMax == 0 && dim == 0));
				if (wrap)
				{
					if (position < 0)
						position += dim;
					if (position >= dim)
						position -= dim;

					if (min <= position)
					{
						if (position < exclMax)
							return position;
						else
						{
							const int inclMax = exclMax - 1;
							const int dist1 = position - inclMax;
							const int dist2 = min + dim - position;
							return dist1 < dist2 ? inclMax : min;
						}
					}
					else
					{
						if (position < exclMax - dim)
							return position;
						else
						{
							const int inclMax = exclMax - 1 - dim;
							const int dist1 = position - inclMax;
							const int dist2 = min - position;
							return dist1 < dist2 ? inclMax : min;
						}
					}
				}
				else
					return std::clamp(position, min, exclMax - 1);
			};

			// Check that we're within minimap revealed bounds.
			const iaabb2 rect = interface.updateAndGetMinimapSectionRect();
			const ivec2 position = worldView.getLookAtPlotCoord();
			CvMap& map = gGlobals.getMap();
			const ivec2 clampedPosition{
				checkAxis(position.x, rect.min.x, rect.max.x, map.isWrapX(), map.getGridWidth()),
				checkAxis(position.y, rect.min.y, rect.max.y, map.isWrapY(), map.getGridHeight()),
			};
			worldView.setCenterPlot(clampedPosition);
		}

		CvInterface& interface;
		WorldView& worldView;
		WorldViewFramebuffer worldViewFB{};
		
		const CvPlot* mClickedPlot = nullptr;
		std::chrono::steady_clock::time_point mClickedPlotTime{};
		bool mClickedPlotReleased = false;

		std::map<std::pair<int, int>, std::shared_ptr<CityBillboard>> cityBillboards;
	};

	// We will abuse the layout system to update city billboard panels.
	// This is called after the WorldViewUIComponent is positioned, every frame.
	void WorldViewUIInnerLayout::layoutPhase2Apply(ivec2, ChildList) const
	{
		container.updateCityBillboards();
		container.checkCameraPosition();
	}
}

std::shared_ptr<hecktui::Element> (::buildMainInterfaceWorldViewComponent)(WorldView& worldView)
{
	return std::make_shared<WorldViewUIComponent>(CvInterface::getInstance(), worldView);
}