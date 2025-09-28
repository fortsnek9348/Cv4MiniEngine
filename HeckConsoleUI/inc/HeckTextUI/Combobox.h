#pragma once

#include "BasicControls.h"

namespace hecktui
{
	enum class EComboboxStyle
	{
		Bulky,
		Compact,
	};

	class HECKCONSOLEUI_EXPORT Combobox : public EmptyButton
	{
	public:

		explicit Combobox(EComboboxStyle style = EComboboxStyle::Bulky);

		virtual void onClick(ModifierKeyState) override;

		void setListItems(std::vector<std::wstring> items);
		size_t getSelectionIndex() const;
		void setSelectionIndex(size_t i);

		virtual void onSelectionChanged();
		

	protected:
		virtual LayoutSizeInfo measureThis() const override;
		virtual void drawThis(ivec2 offset, Framebuffer& fb) override;

	private:
		struct Internals;

		EComboboxStyle mStyle{};

		std::vector<std::wstring> mItems;
		size_t mSelectionI = 0;
		int mMaxWidth = 0;
		bool mIsDropDownActive = false;

		void launchDropDown();
	};
}