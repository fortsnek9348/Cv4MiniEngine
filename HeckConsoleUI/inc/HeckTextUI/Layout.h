#pragma once

#include "Core.h"

#include <span>
#include <memory>
#include <optional>

namespace hecktui
{
	class Element;

	struct [[nodiscard]] LayoutSizeInfo
	{
		// minimum <= { preferred, all possible layouts }, always, even for wrapping layouts.
		// Wrapping layouts may significantly underestimate minimum, and preferred may come from a heurestic.
		// https://stackoverflow.com/questions/25337235/swing-flowlayout-preferredsize-does-not-change-automatically/25337448#25337448
		// Really, a full-on solver would be needed. But even then, you can't have a "minimum size" for flows.
		// But you can have "minimum area", "minimum width", and "minimum height". And you'd optimise for min-max side length, or minimum deviations from desired size.
		// But I'd rather avoid complex solvers because I'd worry about non-trivial behaviour like multiple solutions with the same objective value.
		// Would be nice if layout solving could be implemented in a predictable deterministic way.
		// It would probably be iterative. Maybe you'd start with all rows "all on one line", then add rows and columns greedily to reach an objective.
		//

		// https://lisyarus.github.io/blog/posts/how-not-to-ui.html
		// Has a "heightForWidth" function.
		// Although, for vlayouts, you'd need "widthForHeight". And for a wrapping textbox, that's not so trivial.

		// UPDATE
		// Wrapping text is needed for the diplo screen.
		// So let's have full support for wrapped layouts.
		// `minimum` will continue to be the true minimum width and minimum height, which would allow resizable windows to force a minimum size
		// `preferred` will now be the minimum size according to a given constraint. Always greater than or equal to `minimum`.
		// 
		// The overall layout algorithm will be iterative at the Window level. Iterative just like in FTXUI.
		// Will it terminate? It will if the width or height of a control is monotonic over iterations.
		// For a simple case, think about HFlow only.
		// The first iteration will produce a layout that fits on the screen, but would be too short on Y.
		// The second iteration will take the new widths into account in the measure pass and produce a height that fits text.
		// Will the second iteration increase or even change the width of any control?
		// Maybe. Wrapping text may create small amounts of spare width that could be given to another control, if a layout allows it.
		// Alternatively, could width be frozen on the second iteration if a control is HFlow only? Maybe.
		// What if a control has both HFlow and VFlow?
		//     If its minimum size is violated on both dimensions, use the minimum size as preferred size.
		//     If only its minimum width is violated, freeze the height.
		//     If only its minimum height is violated, freeze the width.
		// Maybe.
		// Flutter say they have a fully linear layout algorithm. No idea how. Just "constraints down, sizes up" they say.
		// But that should be impossible. Even for simple text. You can't specify height and minimise width in linear time. Or maybe you can with some algorithmic trick.

		// NOTE: VFlow can be treated as HFlow. HFlow is minimise height and stay within width constraint. You can do that with VFlow too.
		//       The only potential difference between HFlow and VFlow is that VFlow would consume all available height instead of width, if you don't make it behave like HFlow.
		//       HVScrollPanels would need to special-case though.
		// Also note: Distinguishing between HFlow and VFlow may be pointless as a HFlow element should behave the same as a fill layout with the HFlow and an empty VFlow element.

		// The layout algorithm:
		// Clear all LayoutSizeInfo (set to nullopt).
		// do:
		//     Measure root(window size).
		//         If a child's LayoutSizeInfo is nullopt: child->measure(layout space).
		//         Else, child->measure(child's preferred size).
		//         All layouts shall enforce constraint width down to minimum size. VWrap shall behave like HWrap for now (in the future, this could be a layout setting).
		// while first iteration and the window contains flow, or, a flow control was reduced in width
		// 
		// Iteration could instead be localised to layouts that need it, but this risks even more explosion of runtime.

		// If all elements were hwrap, this could be solved without iteration as long as measureX and measureY can be separated.
		// https://github.com/ArthurSonzogni/FTXUI/issues/247#issuecomment-951575473

		// 3-pass layout:
		//     Measure X/wrap dimension
		//     Measure Y/cross dimension
		//     Arrange
		// Would it work for things like vwrap nested in hwrap? Who knows.

		// Thinking about FTXUI's method some more... https://github.com/ArthurSonzogni/FTXUI/issues/247#issuecomment-966995509
		// Maybe only wrapping layouts need to track their wrapping dimension length.
		// Initialise all wrapping lengths to infinity.
		// Measure. Use current wrapping lengths.
		// Arrange. If any wrapped length was reduced, iterate. Don't allow wrapped lengths to increase.

		ivec2 minimum{};
		ivec2 preferred{};

		LayoutSizeInfo flipped() const
		{
			return { minimum.flipped(), preferred.flipped() };
		}

		friend LayoutSizeInfo vmax(LayoutSizeInfo a, LayoutSizeInfo b)
		{
			return {
				vmax(a.minimum, b.minimum),
				vmax(a.preferred, b.preferred),
			};
		}
	};

	enum class EJustilign : uint8_t
	{
		Start,
		Center,
		End,
		Stretch,
		SpaceBetween,
	};

	struct RectJustilign
	{
		EJustilign halign{};
		EJustilign valign{};
	};

	int alignShift(int remaining, EAlign align);
	std::pair<int, int> justilignSingle(std::pair<int, int> space, int min, EJustilign align);
	HECKCONSOLEUI_EXPORT iaabb2 justilignRect(const iaabb2& space, ivec2 minSize, RectJustilign align);

	class HECKCONSOLEUI_EXPORT Layout
	{
	public:
		using ChildList = std::span<const std::shared_ptr<Element>>;

		bool debug = false;

		// Set by Element during layout. Wrapping layouts should take this into account according to `getWrappingAxis`.
		//ivec2 measureSpace{};

		virtual std::optional<EAxis> getWrappingAxis() const noexcept;

		virtual bool layoutPrepareIteration(bool initial, ivec2 space) noexcept;

		virtual LayoutSizeInfo layoutPhase1Measure(ChildList children) const = 0;

		// A parent control can change its child list in this. If it does, `children` will be invalidated.
		// WorldView UI does this.
		virtual void layoutPhase2Apply(ivec2 space, ChildList children) const = 0;

		virtual ~Layout() = default;
	};

	class HECKCONSOLEUI_EXPORT FillLayout : public Layout
	{
	public:
		std::vector<RectJustilign> childAlignments;
		RectJustilign defaultAlign{ EJustilign::Stretch, EJustilign::Stretch };
		ivec2 marginTopLeft{};
		ivec2 marginBottomRight{};

		virtual LayoutSizeInfo layoutPhase1Measure(ChildList children) const override;
		virtual void layoutPhase2Apply(ivec2 space, ChildList children) const override;
	};

	class HECKCONSOLEUI_EXPORT SizeInfoOverrideLayout : public Layout
	{
	public:
		std::unique_ptr<Layout> overriddenLayout;
		std::optional<int> overriddenMinWidth{};
		std::optional<int> overriddenMinHeight{};

		explicit SizeInfoOverrideLayout(std::unique_ptr<Layout> overriddenLayout, std::optional<int> overriddenMinWidth, std::optional<int> overriddenMinHeight);

		virtual LayoutSizeInfo layoutPhase1Measure(ChildList children) const override;
		virtual void layoutPhase2Apply(ivec2 space, ChildList children) const override;
	};

	struct FlowConfig
	{
		// Flow-wise direction. "Cross-wise" is the other direction.
		EAxis axis = EAxis::Horizontal;
		bool wrap = false;
		EJustilign itemsFlowwiseJustilign = EJustilign::Start;
		EJustilign itemsCrosswiseJustilign = EJustilign::Stretch; // SpaceBetween == Center, as there's only one item per "column".
		EJustilign linesCrosswiseJustilign = EJustilign::Start;
	};

	class HECKCONSOLEUI_EXPORT FlowLayout : public Layout
	{
	public:
		FlowConfig config{};

		explicit FlowLayout(FlowConfig config);

		virtual std::optional<EAxis> getWrappingAxis() const noexcept override;
		virtual bool layoutPrepareIteration(bool initial, ivec2 space) noexcept override;
		virtual LayoutSizeInfo layoutPhase1Measure(ChildList children) const override;
		virtual void layoutPhase2Apply(ivec2 space, ChildList children) const override;

	private:
		// Layout solving element state
		int mLayoutWrappingSpace = 0;
	};

	struct TableLayoutConfig
	{
		// == Layout Procedure (columns) ==
		// Like https://learn.microsoft.com/en-us/dotnet/api/system.windows.forms.tablelayoutpanel.rowstyles.
		// Columns are sized based on priority according to sizing mode.
		// Here, AutoSize columns will be sized before Proportional columns.
		// Proportional colums get the *remaining* space.
		//
		// So, it goes like this:
		// - Determine minimum and preferred widths of all columns (taking span into account).
		// - If container width is less than or equal to table minimum width, then simply squash the columns equally from minimum width.
		// - Else, we have more than enough space for the minimum widths of all columns.
		//     - Let min proportional total width = sum(min widths of all proportional columns)
		//     - Let max autosize total width allowed = container width - min proportional total width
		//     - Size the AutoSize columns by Lerp-ing between minimum and preferred widths in order to not exceed max autosize total width allowed.
		//     - Assign proportional columns their initial widths using the remaining space.
		//     - Enforce minimum widths of proportional columns by stealing from neighbouring columns. There will be enough width to steal.

		

		struct RowColDesc
		{
			// Fixed minimum size.
			int min = 0;
			// AutoSize if <= 0, else, Proportional.
			int weight = 0;
		};

		struct Cell
		{
			ivec2 coord{};
			// NOTE: Span with proportional columns or gap does not have correct layout. It will function most of the time though.
			ivec2 span{ 1, 1 };
			RectJustilign align{ EJustilign::Stretch, EJustilign::Stretch };
		};

		// clang needs the defaulted members too, for some reason.
		static constexpr RowColDesc kFlex{ .min = 0, .weight = 1 };
		static constexpr RowColDesc kAutoSize{ .min = 0, .weight = 0 };

		std::vector<RowColDesc> cols;
		std::vector<RowColDesc> rows;

		// NOTE: Cells may be specified in any order and they may overlap.
		std::vector<Cell> cells;

		EAxis repeatAxis = EAxis::Vertical;
		ivec2 gap{};
	};

	class HECKCONSOLEUI_EXPORT TableLayout : public Layout
	{
	public:
		TableLayoutConfig config{};

		explicit TableLayout(TableLayoutConfig config);

		virtual LayoutSizeInfo layoutPhase1Measure(ChildList children) const override;
		virtual void layoutPhase2Apply(ivec2 space, ChildList children) const override;
	};
}