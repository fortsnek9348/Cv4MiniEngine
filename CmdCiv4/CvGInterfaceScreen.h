#pragma once

#include <Cv4CommonEngineLib/CvEngineEnums.h>

#include <string>
#include <memory>
#include <map>

namespace hecktui
{
	class Element;
	class Window;
}

class CvPlot;

namespace cvengine
{
	class CvGInterfaceScreen
	{
	public:
		enum class EAutoSizeBehaviour
		{
			Minimum,
			GrowOnly,
			Maximise,
		};

		CvGInterfaceScreen() = default;
		explicit CvGInterfaceScreen(std::string name, cvengine::ECvScreen kind);

		cvengine::ECvScreen getKind() const noexcept;

		bool isActive() const;

		// Not sure of the correct behaviour of this flag. For now, let's just assume it's some bool you can get and set.
		bool isPersistent() const;
		void setPersistent(bool);

		std::shared_ptr<hecktui::Element> getTuiRoot() const noexcept;

		void clear() noexcept;

		void set(std::string name, std::shared_ptr<hecktui::Element> ctrl);
		void delByPrefix(std::string_view prefix);

		std::shared_ptr<hecktui::Element> at(const std::string& name) const;

		void setPlotHelpTarget(std::string name);
		void updatePlotHelp(CvPlot* plot);

		void setSoundName(std::string name);

		void setInitialTitle(std::wstring title);
		void setAutoSizeBehaviour(EAutoSizeBehaviour);
		void setModal(bool);
		void setWantClose(bool) noexcept;

		virtual void rebuildPythonScreen() = 0;
		virtual void updateFromGameState(hecktui::Window& window) = 0;
		virtual std::shared_ptr<hecktui::Window> createTuiWindow(bool passInput) const;
		virtual bool wantClose() const;
		virtual ~CvGInterfaceScreen() = default;

	private:
		cvengine::ECvScreen mScreenKind{};
		std::string mName;
		std::wstring mTitle;
		EAutoSizeBehaviour mAutoSizeBehaviour = EAutoSizeBehaviour::GrowOnly;
		bool mIsModal = true;
		bool mWantClose = false;
		bool mIsPersistent = false;
		std::shared_ptr<hecktui::Element> mRoot;
		std::map<std::string, std::shared_ptr<hecktui::Element>> mElementDict;
		std::string mSoundName;

		std::string mPlotHelpTargetName;
	};
}