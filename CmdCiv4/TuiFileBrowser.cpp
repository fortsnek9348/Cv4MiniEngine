#include "TuiFileBrowser.h"
#include "CvApp.h"

#include <Cv4CommonEngineLib/CvTranslator.h>

#include <HeckTextUI/BasicControls.h>
#include <HeckTextUI/UIScene.h>
#include <HeckTextUI/Window.h>
#include <HeckTextUI/Layout.h>
#include <HeckTextUI/Textbox.h>

#include <ranges>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN 1
#define NOMINMAX 1
#include <Windows.h>
#endif

using namespace hecktui;
using cvengine::TuiFileBrowserConfig;
namespace fs = std::filesystem;

namespace
{
	struct Root
	{
		std::wstring name;
		fs::path path;
	};

	std::vector<Root> gatherRoots()
	{
		std::vector<Root> roots;

		roots.emplace_back(L"User Data", cvengine::CvApp::getUserDataDir());

#ifdef _WIN32
		// C:\, D:\, etc.
		std::vector<wchar_t> buf;
		DWORD ret{};
		while (ret = GetLogicalDriveStringsW(static_cast<DWORD>(buf.size()), buf.data()), ret > buf.size())
			buf.resize(ret);
		// Get rid of the nuls.
		while (!buf.empty() && buf.back() == L'\0')
			buf.pop_back();
		for (const auto& drive : buf | std::views::split(L'\0'))
			roots.emplace_back(std::wstring(std::wstring_view(drive)), std::wstring_view(drive));
		return roots;
#else
		roots.emplace_back(L"/", L"/");
#endif
		return roots;
	}

	class ListEntryRadioButton;

	struct IMainWindowSelectionHandler
	{
		virtual void onSelectListEntry(ListEntryRadioButton*) = 0;
		virtual void onDoubleClickListEntry(ListEntryRadioButton*) = 0;
		virtual ~IMainWindowSelectionHandler() = default;
	};

	std::wstring makeDirEntryLabel(const fs::directory_entry& dirEntry)
	{
		const fs::path& dirEntryPath = dirEntry.path();
		std::wstring name = dirEntryPath.filename().wstring();
		if (dirEntry.is_directory())
			name = L'[' + std::move(name) + L']';
		return name;
	}

	class ListEntryRadioButton : public RadioButton
	{
	public:
		explicit ListEntryRadioButton(fs::directory_entry dirEntry, IMainWindowSelectionHandler* eventHandler)
			: RadioButton(ECheckStyle::None, makeDirEntryLabel(dirEntry))
			, dirEntry(std::move(dirEntry))
			, mEventHandler(eventHandler)
		{
		}

		fs::directory_entry dirEntry;

		virtual bool onEvent(const ConsoleEvent& e) override
		{
			if (e.type == EConsoleEventType::MouseButtonDoubleClick)
			{
				if (static_cast<const MouseButtonEvent&>(e).button == EMouseButton::Left)
				{
					mEventHandler->onDoubleClickListEntry(this);
					return true;
				}
			}
			
			return RadioButton::onEvent(e);
		}

		virtual void onCheckChanged() override
		{
			RadioButton::onCheckChanged();
			if (value)
				mEventHandler->onSelectListEntry(this);
		}

	protected:
		virtual void drawThis(ivec2 offset, Framebuffer& fb) override
		{
			PixelColouring colouring{};
			if (getUISceneState().hover)
				colouring.text = EColour::White;
			if (value)
				std::swap(colouring.text, colouring.back);

			fb.drawText(getLabel(), offset + getRect(), EAlign::Left, EAlign::Top, colouring, false);
		}

	private:
		IMainWindowSelectionHandler* mEventHandler = nullptr;
	};

	class ErrorDialog : public Window
	{
	public:
		explicit ErrorDialog(std::wstring text, bool useText)
			: Window(useText ? CvTranslator::getInstance().getText(L"TXT_KEY_MAIN_MENU_ERROR") : L"ERROR", WindowConfig{
			.isDefaultFocus = true,
			.isFullscreen = false,
			.isModal = true,
			.canClose = false,
			.allowSceneToCloseWindow = true,
			.borderStyle = EBorderStyle::Rounded,
				})
		{
			const auto client = getClientArea();
			client->setLayout(std::make_unique<FlowLayout>(FlowConfig{
				.axis = EAxis::Vertical,
				}));
			client->addChild(std::make_shared<Label>(std::move(text)));
			client->addChild(std::make_shared<Button>(L"OK", [this] {
				setWantClose();
				}));
		}
	};

	struct MainWindow : Window, IMainWindowSelectionHandler
	{
		UIScene& scene;
		const std::vector<Root> roots = gatherRoots();
		const TuiFileBrowserConfig& config;
		fs::path viewPath;
		std::optional<std::filesystem::path> optSelectedPath;

		std::shared_ptr<Textbox> txtViewPath;

		std::shared_ptr<Element> lstDirEntries;
		ListEntryRadioButton* currentSelectedListEntry = nullptr;

		std::shared_ptr<Textbox> txtSavePath;

		explicit MainWindow(UIScene& scene, const TuiFileBrowserConfig& config, std::wstring title)
			: Window(std::move(title) , WindowConfig{
			.isDefaultFocus = true,
			.isFullscreen = true,
			.isModal = true,
			.canClose = true,
			.allowSceneToCloseWindow = true,
			.borderStyle = EBorderStyle::Rounded,
			})
			, scene(scene)
			, config(config)
			, viewPath(config.defPath)
		{
			fs::path defFilename;
			if (!viewPath.is_absolute())
				viewPath = fs::absolute(viewPath);
			if (!fs::is_directory(viewPath))
			{
				defFilename = config.defPath.filename();
				viewPath = std::move(viewPath).parent_path();
			}
			if (roots.empty())
				throw std::runtime_error("Uh... file browser could not find filesystem roots (drives, etc).");
			if (!fs::is_directory(viewPath))
			{
				defFilename.clear();
				viewPath = roots[0].path;
			}

			const auto client = getClientArea();
			client->setLayout(std::make_unique<TableLayout>(TableLayoutConfig{
				.cols{ TableLayoutConfig::kAutoSize, TableLayoutConfig::kFlex },
				.rows{ TableLayoutConfig::kAutoSize, TableLayoutConfig::kFlex, TableLayoutConfig::kAutoSize },
				.cells{
					TableLayoutConfig::Cell{.coord{ 0, 0 }, .span{ 2, 1 } },
					TableLayoutConfig::Cell{.coord{ 0, 1 }},
					TableLayoutConfig::Cell{.coord{ 1, 1 }},
					TableLayoutConfig::Cell{.coord{ 0, 2 }, .span{ 2, 1 } },
				},
				}));

			//
			const auto navbar = std::make_shared<Element>();
			navbar->setLayout(std::make_unique<TableLayout>(TableLayoutConfig{
				.cols{ TableLayoutConfig::kAutoSize, TableLayoutConfig::kFlex/*, TableLayoutConfig::kAutoSize*/ },
				.rows{ TableLayoutConfig::kAutoSize },
				.cells{
					TableLayoutConfig::Cell{.coord{ 0, 0 }},
					TableLayoutConfig::Cell{.coord{ 1, 0 }},
					//TableLayoutConfig::Cell{.coord{ 2, 0 }},
				},
				}));
			navbar->addChild(std::make_shared<Button>(L"↑", [this] { navigateUp(); }));
			
			txtViewPath = std::make_shared<Textbox>(
				config.useText ? CvTranslator::getInstance().getText(L"TXT_KEY_MAIN_MENU_LOADSAVE_GAME_FOLDER") : L"Folder",
				L"",
				PixelColouring().back,
				EColour::Gray,
				PixelColouring().text
			);
			navbar->addChild(txtViewPath);
			client->addChild(navbar);

			//
			const auto lstRoots = std::make_shared<Element>();
			lstRoots->setLayout(std::make_unique<FlowLayout>(FlowConfig{
				.axis = EAxis::Vertical,
				}));
			for (const auto& root : roots)
			{
				lstRoots->addChild(std::make_shared<Button>(root.name, [&root, this] {
					changeRoot(root.path);
					}));
			}
			client->addChild(lstRoots);

			auto dirEntriesScrollPanel = std::make_shared<ScrollBarPanel>(EAxis::Vertical);
			lstDirEntries = std::make_shared<Element>();
			lstDirEntries->setLayout(std::make_unique<FlowLayout>(FlowConfig{
				.axis = EAxis::Vertical,
				.itemsCrosswiseJustilign = EJustilign::Start,
				}));
			dirEntriesScrollPanel->getPanel()->setLayout(std::make_unique<FillLayout>());
			dirEntriesScrollPanel->getPanel()->addChild(lstDirEntries);
			client->addChild(dirEntriesScrollPanel);

			//
			const auto bottomBar = std::make_shared<Element>();
			bottomBar->setLayout(std::make_unique<TableLayout>(TableLayoutConfig{
				.cols{ TableLayoutConfig::kFlex, TableLayoutConfig::kAutoSize },
				.rows{ TableLayoutConfig::kAutoSize },
				.cells{
					TableLayoutConfig::Cell{.coord{ 0, 0 }},
					TableLayoutConfig::Cell{.coord{ 1, 0 }},
				},
				}));

			if (config.toSave)
			{
				txtSavePath = std::make_shared<Textbox>(
					config.useText ? CvTranslator::getInstance().getText(L"TXT_KEY_MAIN_MENU_LOADSAVE_FILE_NAME") : L"Filename",
					defFilename.wstring(),
					PixelColouring().back,
					EColour::Gray,
					PixelColouring().text
				);
				bottomBar->addChild(txtSavePath);
			}
			else
				bottomBar->addChild(std::make_shared<Element>());

			bottomBar->addChild(std::make_shared<Button>(config.useText ? CvTranslator::getInstance().getText(L"TXT_KEY_MAIN_MENU_OK") : L"OK", [this] { onOK(); }));
			client->addChild(bottomBar);
			

			if (!tryNavigateTo(viewPath))
			{
				bool navSuccess = false;
				for (const Root& root : roots)
				{
					if (tryNavigateTo(root.path))
					{
						navSuccess = true;
						break;
					}
				}
				if (!navSuccess)
				{
					setWantClose();
					scene.pushWindow(std::make_shared<ErrorDialog>(L"File browser failed to enumerate directory entries in any filesystem root.", config.useText));
				}
			}
		}

		void changeRoot(const fs::path& root)
		{
			tryNavigateTo(root);
		}

		void navigateUp()
		{
			tryNavigateTo(viewPath.parent_path());
		}

		struct CIStringView
		{
			std::wstring_view chars;

			bool operator<(CIStringView b) const
			{
				const int cmp =
#ifdef _WIN32
					_wcsnicmp
#else
					wcsncasecmp
#endif
				(chars.data(), b.chars.data(), std::min(chars.size(), b.chars.size()));
				return cmp < 0 || (cmp == 0 && chars.size() < b.chars.size());
			}
		};

		bool tryNavigateTo(const fs::path& dirPath)
		{
			std::vector<fs::directory_entry> localDirEntries;

			try
			{
				std::wstring checkExt = config.fileExt;
				if (!checkExt.empty() && checkExt[0] != L'.') // We need a dot in front of it.
					checkExt.insert(checkExt.begin(), L'.');

				localDirEntries.assign_range(fs::directory_iterator(dirPath, fs::directory_options::follow_directory_symlink | fs::directory_options::skip_permission_denied)
					| std::views::filter([&, this](const fs::directory_entry& dirEntry) {
						return config.fileExt.empty() || dirEntry.is_directory() || dirEntry.path().extension() == checkExt;
						}));
				std::ranges::stable_sort(localDirEntries, [](const fs::directory_entry& a, const fs::directory_entry& b) {
					const bool isDirA = a.is_directory();
					const bool isDirB = b.is_directory();
					if (isDirA != isDirB)
						return !isDirA < !isDirB;
					return CIStringView(a.path().filename().wstring()) < CIStringView(b.path().filename().wstring());
					});
			}
			catch (const fs::filesystem_error&)
			{
				return false;
			}

			viewPath = dirPath;
			txtViewPath->setText(dirPath.wstring());

			currentSelectedListEntry = nullptr;
			lstDirEntries->removeAllChildren();

			for (const fs::directory_entry& dirEntry : localDirEntries)
				lstDirEntries->addChild(std::make_shared<ListEntryRadioButton>(dirEntry, this));

			return true;
		}

		virtual void onSelectListEntry(ListEntryRadioButton* btn) override
		{
			if (currentSelectedListEntry)
				currentSelectedListEntry->value = false;
			currentSelectedListEntry = btn;
			if (config.toSave && !btn->dirEntry.is_directory())
				txtSavePath->setText(btn->dirEntry.path().filename().wstring());
		}
		virtual void onDoubleClickListEntry(ListEntryRadioButton* btn) override
		{
			if (btn->dirEntry.is_directory())
				tryNavigateTo(btn->dirEntry.path());
			else
			{
				if (!config.wantDirectory)
				{
					optSelectedPath = btn->dirEntry.path();
					setWantClose();
				}
			}
		}

		void onOK()
		{
			if (!config.wantDirectory)
			{
				if (txtSavePath)
				{
					fs::path path = txtSavePath->getText();
					if (!path.has_filename())
					{
						// We have to have a filename.
						scene.pushWindow(std::make_shared<ErrorDialog>(L"Need a filename.", config.useText));
						return;
					}
					else
					{
						if (path.has_parent_path())
						{
							// If there's a parent path (absolute or relative), it has to exist.
							if (!fs::is_directory(path.parent_path()))
							{
								scene.pushWindow(std::make_shared<ErrorDialog>(L"Parent path does not exist.", config.useText));
								return;
							}
						}

						// If there's no extension, add the default.
						if (!config.fileExt.empty() && !path.has_extension())
							path.replace_extension(config.fileExt);
					}

					optSelectedPath = viewPath / path;
					setWantClose();
				}
				else if (currentSelectedListEntry)
				{
					optSelectedPath = currentSelectedListEntry->dirEntry.path();
					setWantClose();
				}
			}
			else
			{
				if (currentSelectedListEntry && currentSelectedListEntry->dirEntry.is_directory())
					optSelectedPath = currentSelectedListEntry->dirEntry.path();
				else
					optSelectedPath = viewPath;

				setWantClose();
			}
		}

		virtual bool onEvent(const ConsoleEvent& e) override
		{
			if (Window::onEvent(e))
				return true;
			
			if (e.type == EConsoleEventType::ActionKeyPressed && static_cast<const ActionKeyEvent&>(e).key == EActionKey::Enter)
			{
				if (txtViewPath->getUISceneState().focus)
					tryNavigateTo(txtViewPath->getText());
				else
					onOK();
				return true;
			}
			return false;
		}
	};
}

std::optional<std::filesystem::path> cvengine::tryPromptTuiFileBrowserPath(const TuiFileBrowserConfig& config)
{
	ConsoleDevice consoleDevice;
	Framebuffer fb(consoleDevice.getDim());
	UIScene scene;
	scene.setDim(fb.getDim());

	const auto mainWindow = std::make_shared<MainWindow>(scene, config, config.toSave ? L"Save" : config.wantDirectory ? L"Pick directory" : L"Load");
	scene.pushWindow(mainWindow);

	for (;;)
	{
		if (consoleDevice.getDim() != fb.getDim())
		{
			fb = hecktui::Framebuffer(consoleDevice.getDim());
			scene.setDim(fb.getDim());
		}

		scene.update();
		if (scene.empty())
			break;

		fb.clear();
		scene.draw(fb);
		fb.mergeBorders();
		consoleDevice.print(fb);

		consoleDevice.wait();

		while (const EventStorage e = consoleDevice.poll())
			(void)scene.injectEvent(*e);
	}

	return std::move(mainWindow->optSelectedPath);
}
