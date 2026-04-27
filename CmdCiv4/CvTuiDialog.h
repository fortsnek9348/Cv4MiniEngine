#pragma once

#include <HeckTextUI/Window.h>

#include <map>

// This is a barebones dialog for the TUI, accessible from python.
// Used by options dialog.

// TODO: This is currently a big fat copy of CvGInterfaceScreen, without the screen info. Refactor.

namespace cvengine
{
	class CvTuiDialog
	{
	public:
		enum class EAutoSizeBehaviour
		{
			Minimum,
			GrowOnly,
			Maximise,
		};

		CvTuiDialog() = default;

		std::shared_ptr<hecktui::Element> getTuiRoot() const noexcept;

		void clear() noexcept;

		void set(std::string name, std::shared_ptr<hecktui::Element> ctrl);
		void delByPrefix(std::string_view prefix);

		std::shared_ptr<hecktui::Element> at(const std::string& name) const;

		void setSoundName(std::string name);

		void setInitialTitle(std::wstring title);
		void setAutoSizeBehaviour(EAutoSizeBehaviour);
		//void setModal(bool);
		void setWantClose(bool) noexcept;
		//void setWantFullscreen(bool);

		std::shared_ptr<hecktui::Window> createTuiWindow(bool passInputToMainInterface, hecktui::WindowConfig windowConfig) const;
		bool wantClose() const;

	private:
		//bool mWantFullscreen = false;
		//std::string mName;
		std::wstring mTitle;
		EAutoSizeBehaviour mAutoSizeBehaviour = EAutoSizeBehaviour::GrowOnly;
		//bool mIsModal = true;
		bool mWantClose = false;
		std::shared_ptr<hecktui::Element> mRoot = std::make_shared<hecktui::Element>();
		std::map<std::string, std::shared_ptr<hecktui::Element>> mElementDict;
		std::string mSoundName;
	};
}