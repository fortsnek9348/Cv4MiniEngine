#pragma once

#include "Layout.h"

namespace hecktui
{
	class Element;
}

namespace hecktui::LayoutBuilder
{
	// Things that can be put inside a layout.
	
	struct Fixed
	{
		ivec2 size{};
	};

	struct Flex
	{
		ivec2 weight{};
	};

	struct Merge
	{
	};

	struct Place
	{
		std::shared_ptr<Element> ctrl;
	};

	struct Rule
	{
		EAxis axis{};
	};

	constexpr Fixed hfixed(int w)
	{
		return { { w, 0 } };
	}

	constexpr Fixed vfixed(int h)
	{
		return { { 0, h } };
	}

	constexpr Flex hflex(int w)
	{
		return { { w, 0 } };
	}

	constexpr Flex vflex(int h)
	{
		return { { 0, h } };
	}

	// The actual layouts.

	struct Linear
	{
		EAxis axis{};


	};

	struct Flow
	{
		EAxis axis{};
	};

	struct Split
	{
		EAxis axis{};
	};


	/**
	* 
	* hsplit{
	*	table{
	*		row{ flex(1), flex(2, Place(ResearchGuage)), flex(1, Place(GameProgress)) },
	*	},
	*	hsplit{
	*		vsplit{
	*			Place(PlotList),
	*			Place(WorldView),
	*		},
	*		vsplit{
	*			Place(UnitView),
	*			Place(ActionList),
	*			Place(Minimap),
	*		},
	*	}
	* }
	* 
	* table({
	*	{ NumLock, Div   , Mul , Sub              },
	*	{ n7 | span(2, 2), n9  , Add | vspan(2)   },
	*	{ hspan(2)       , n6  , {}               },
	*	{ n1     , n2    , n3  , Enter | vspan(2) },
	*	{ n0 | hspan(2)  , Dot , {}               },
	* })
	* 
	*/

	struct TableBuilderCell
	{
		std::shared_ptr<Element> element;
		ivec2 fixed{};
		ivec2 flexWeight{};
		ivec2 span{ 1, 1 };
		RectJustilign justilign{
			EJustilign::Stretch,
			EJustilign::Stretch,
		};

		TableBuilderCell() = default;
		template<std::derived_from<Element> E>
		TableBuilderCell(std::shared_ptr<E> element) noexcept : element(std::move(element))
		{
		}
	};

	struct Span
	{
		ivec2 dim{ 1, 1 };

		

		friend constexpr TableBuilderCell&& operator|(TableBuilderCell&& cell, Span span)
		{
			cell.span = span.dim;
			return std::move(cell);
		}

		template<std::derived_from<Element> E>
		friend TableBuilderCell operator|(std::shared_ptr<E> element, Span span)
		{
			return TableBuilderCell(std::move(element)) | span;
		}

		operator TableBuilderCell() const
		{
			return TableBuilderCell() | *this;
		}
	};

	constexpr Span span(int x, int y)
	{
		return { { x, y } };
	}

	constexpr Span hspan(int x)
	{
		return span(x, 1);
	}

	constexpr Span vspan(int y)
	{
		return span(1, y);
	}

	constexpr TableBuilderCell&& operator|(TableBuilderCell&& cell, RectJustilign justilign)
	{
		cell.justilign = justilign;
		return std::move(cell);
	}

	constexpr TableBuilderCell&& operator|(TableBuilderCell&& cell, Flex flex)
	{
		cell.flexWeight = flex.weight;
		return std::move(cell);
	}

	constexpr TableBuilderCell&& operator|(TableBuilderCell&& cell, Fixed fixed)
	{
		cell.fixed = fixed.size;
		return std::move(cell);
	}

	
	HECKCONSOLEUI_EXPORT std::shared_ptr<Element> table(std::initializer_list<std::initializer_list<TableBuilderCell>> rows);
}