#include "GameRunScope.h"
#include "CommandLine.h"

#include <Cv4CommonEngineLib/CommonEngine.h>
#include <Cv4CommonEngineLib/CvAppStates.h>
#include <Cv4CommonEngineLib/CvEngine.h>
#include <Cv4CommonEngineLib/CvInterface.h>
#include <Cv4CommonEngineLib/CvPopup.h>
#include <Cv4CommonEngineLib/CvTranslator.h>
#include <Cv4CommonEngineLib/DLLMessageQueue.h>
#include <Cv4CommonEngineLib/IniReader.h>

#include <CvGameCoreDLL/CvDLLEntityIFaceBase.h>
#include <CvGameCoreDLL/CvDLLFlagEntityIFaceBase.h>
#include <CvGameCoreDLL/CvDLLPlotBuilderIFaceBase.h>
#include <CvGameCoreDLL/CvDLLSymbolIFaceBase.h>
#include <CvGameCoreDLL/CvGameAI.h>
#include <CvGameCoreDLL/CvGlobals.h>
#include <CvGameCoreDLL/CvMap.h>
#include <CvGameCoreDLL/CvPlayerAI.h>
#include <CvGameCoreDLL/CvPopupInfo.h>
#include <CvGameCoreDLL/CvTeamAI.h>

#include <CommonStuff/DynamicLib.h>
#include <CommonStuff/System.h>
#include <CommonStuff/range.h>

#include <pybind11/pybind11.h>

#include <chrono>
#include <iostream>
#include <iomanip>

// Remove symbol characters from message so that iostream doesn't go into fail state.
static std::wstring sanitiseMessageString(std::wstring str)
{
	std::ranges::replace_if(str, [&translator = CvTranslator::getInstance()](wchar_t c) {
		return c >= translator.firstSymbolCode;
		}, L'?');
	return str;
}

class CvHeadlessInterface : public CvInterface
{
public:
	// Inherited via CvInterface
	bool isInMainMenu() const override
	{
		return false;
	}
	bool isLeftMouseDown() const override
	{
		return false;
	}
	bool isRightMouseDown() const override
	{
		return false;
	}
	void startMainInterface() override
	{
	}
	void lookAtPlot(ivec2) override
	{
	}
	void lookAtCityBuilding(int, BuildingTypes) override
	{
	}
	void lookAtSelectionPlot() override
	{
	}
	void lookAtCityOffset(int) override
	{
	}
	CvPlot* getLookAtPlot() const override
	{
		return nullptr;
	}
	CvPlot* getMouseOverPlot() const override
	{
		return nullptr;
	}
	CvPlot* getSelectedCityOrUnitPlot() const override
	{
		return nullptr;
	}
	const CvSelectionGroup* getSelectionList() const override
	{
		return nullptr;
	}
	CvSelectionGroup* getSelectionList() override
	{
		return nullptr;
	}
	void makeSelectionListDirty() override
	{
	}
	void selectionListPostChange() override
	{
	}
	void selectionListPreChange() override
	{
	}
	void clearQueuedPopups() override
	{
	}
	bool isDiploOrPopupWaiting() const override
	{
		return false;
	}
	void launchPopup([[maybe_unused]] std::unique_ptr<CvPopup> pPopup, [[maybe_unused]] bool bCreateOkButton, [[maybe_unused]] PopupStates bState, [[maybe_unused]] int iNumPixelScroll) override
	{
	}
	CvUnit* getLastSelectedUnit() const override
	{
		return nullptr;
	}
	void setLastSelectedUnit(CvUnit*) override
	{
	}
	CvPlot* getGotoPlot() const override
	{
		return nullptr;
	}
	CvPlot* getSingleMoveGotoPlot() const override
	{
		return nullptr;
	}
	CvPlot* getOriginalUnitSelectionPlot() const override
	{
		return nullptr;
	}
	int getOriginalUnitSelectionIndex() const override
	{
		return 0;
	}
	void doSoundtrack(int) override
	{
	}
	void play2DSound(int) override
	{
	}
	void play3DSound(int, const NiPoint3&) override
	{
	}
	void playUnitSelectionSound(const CvUnit&) override
	{
	}
	void setSoundSelectionReady(bool) override
	{
	}
	void selectCity(CvCity*, bool) override
	{
	}
	void selectLookAtCity(bool) override
	{
	}
	void addSelectedCity(CvCity*, bool) override
	{
	}
	void clearSelectedCities() override
	{
	}
	bool isCitySelected(CvCity*) const override
	{
		return false;
	}
	CvCity* getHeadSelectedCity() const override
	{
		return nullptr;
	}
	bool isCitySelection() const override
	{
		return false;
	}
	CLLNode<IDInfo>* nextSelectedCitiesNode(CLLNode<IDInfo>*) override
	{
		return nullptr;
	}
	CLLNode<IDInfo>* headSelectedCitiesNode() override
	{
		return nullptr;
	}
	bool isCityScreenUp() const override
	{
		return false;
	}
	void addTurnMessageDisplayEntry(CvTalkingHeadMessage msg) override
	{
		std::wclog << sanitiseMessageString(msg.getDescription()) << L'\n';
	}
	void addMessage(PlayerTypes ePlayer, bool, int, CvWString msg, const TCHAR*, InterfaceMessageTypes type, const char*, ColorTypes, int, int, bool, bool) override
	{
		if (ePlayer == gGlobals.getGame().getActivePlayer())
		{
			// TODO: Check this...
			if (type != MESSAGE_TYPE_COMBAT_MESSAGE && type != MESSAGE_TYPE_QUEST)
				std::wclog << sanitiseMessageString(std::move(msg)) << L'\n';
		}
	}
	void addQuestMessage(PlayerTypes ePlayer, CvWString msg, int) override
	{
		if (ePlayer == gGlobals.getGame().getActivePlayer())
			std::wclog << sanitiseMessageString(std::move(msg)) << L'\n';
	}
	void flushTalkingHeadMessages() override
	{
	}
	void getDisplayedButtonPopups(CvPopupQueue&) const override
	{
	}
	int getCycleSelectionCounter() const override
	{
		return 0;
	}
	void setCycleSelectionCounter(int) override
	{
	}
	void changeCycleSelectionCounter(int) override
	{
	}
	bool isCombatFocus() const override
	{
		return false;
	}
	void setCombatFocus(bool) override
	{
	}
	bool getInterfaceDirtyBit(InterfaceDirtyBits) const override
	{
		return false;
	}
	void setInterfaceDirtyBit(InterfaceDirtyBits, bool) override
	{
	}
	void updateCursorType() override
	{
	}
	void updatePythonScreens() override
	{
	}
	void lookAt(NiPoint3, CameraLookAtTypes, NiPoint3) override
	{
	}
	void centerCamera(CvUnit*) override
	{
	}
	void releaseLockedCamera() override
	{
	}
	bool isFocusedWidget() const override
	{
		return false;
	}
	bool isFocused() const override
	{
		return false;
	}
	bool isBareMapMode() const override
	{
		return false;
	}
	void toggleBareMapMode() override
	{
	}
	bool isShowYields() const override
	{
		return false;
	}
	void toggleYieldVisibleMode() override
	{
	}
	bool isScoresVisible() const override
	{
		return false;
	}
	void toggleScoresVisible() override
	{
	}
	bool isScoresMinimized() const override
	{
		return false;
	}
	void toggleScoresMinimized() override
	{
	}
	void setInterfaceMode(InterfaceModeTypes) override
	{
	}
	InterfaceModeTypes getInterfaceMode() const override
	{
		return InterfaceModeTypes();
	}
	InterfaceVisibility getShowInterface() const override
	{
		return InterfaceVisibility();
	}
	void setShowInterface(InterfaceVisibility) override
	{
	}
	void setMinimapColor(MinimapModeTypes, ivec2, ColorTypes, float) override
	{
	}
	unsigned char* getMinimapBaseTextureDXT1() const override
	{
		return nullptr;
	}
	int getEndTurnCounter() const override
	{
		return 0;
	}
	void setEndTurnCounter(int) override
	{
	}
	void changeEndTurnCounter(int) override
	{
	}
	void setEndTurnMessage(bool) override
	{
	}
	bool isEndTurnMessage() const override
	{
		return false;
	}
	bool shouldDisplayEndTurn() const override
	{
		return false;
	}
	bool shouldDisplayFlag() const override
	{
		return false;
	}
	bool shouldDisplayReturn() const override
	{
		return false;
	}
	bool shouldDisplayUnitModel() const override
	{
		return false;
	}
	bool shouldShowResearchButtons() const override
	{
		return false;
	}
	bool isForcePopup() const override
	{
		return false;
	}
	void setForcePopup(bool) override
	{
	}
	void toggleTurnLog() override
	{
	}
	void dirtyTurnLog(PlayerTypes) override
	{
	}
	void showDetails(bool) override
	{
	}
	void showAdminDetails() override
	{
	}
	void toggleClockAlarm(bool, int, int) override
	{
	}
	bool isClockAlarmOn() const override
	{
		return false;
	}
	bool isExitingToMainMenu() const override
	{
		return false;
	}
	void exitToMainMenu() override
	{
	}
	void setWorldBuilder(bool) override
	{
	}
	int determineWidth(const std::wstring&) const override
	{
		return 1;
	}
	void setCityTabSelectionRow(CityTabTypes) override
	{
	}
	CityTabTypes getCityTabSelectionRow() const override
	{
		return NO_CITYTAB;
	}
	bool noTechSplash() const override
	{
		return false;
	}
	bool isInAdvancedStart() const override
	{
		return false;
	}
	void setInAdvancedStart(bool) override
	{
	}
	bool isSpaceshipScreenUp() const override
	{
		return false;
	}
	void setBusy(bool) override
	{
	}
};

class CvHeadlessEngine : public CvEngine
{
public:
	// Inherited via CvEngine
	void doTurn() override
	{
	}

	void toggleGlobeview() override
	{
	}

	bool isGlobeviewUp() override
	{
		return false;
	}

	void toggleResourceLayer() override
	{
	}

	void toggleUnitLayer() override
	{
	}

	void setResourceLayer(bool) override
	{
	}

	void moveBaseTurnRight(float) override
	{
	}

	void moveBaseTurnLeft(float) override
	{
	}

	void setFlying(bool) override
	{
	}

	void cycleFlyingMode(int) override
	{
	}

	void setMouseFlying(bool) override
	{
	}

	void setSatelliteMode(bool) override
	{
	}

	void setOrthoCamera(bool) override
	{
	}

	bool getFlying() override
	{
		return false;
	}

	bool getMouseFlying() override
	{
		return false;
	}

	bool getSatelliteMode() override
	{
		return false;
	}

	bool getOrthoCamera() override
	{
		return false;
	}

	bool getCityBillboardVisibility() const override
	{
		return true;
	}

	void setCityBillboardVisibility(bool) override
	{
	}

	bool getCultureVisibility() const override
	{
		return false;
	}

	void setCultureVisibility(bool) const override
	{
	}

	bool getSelectionCursorVisibility() const override
	{
		return false;
	}

	void setSelectionCursorVisibility(bool) override
	{
	}

	bool getUnitFlagVisibility() const override
	{
		return false;
	}

	void setUnitFlagVisibility(bool) override
	{
	}

	void getLandscapeGameDimensions(float& fWidth, float& fHeight) override
	{
		const CvMap& map = gGlobals.getMap();
		fWidth = map.getGridWidth() * gGlobals.getPLOT_SIZE();
		fHeight = map.getGridHeight() * gGlobals.getPLOT_SIZE();
	}

	float getHeightmapZ(const NiPoint3&, bool) override
	{
		return 0.0f;
	}

	void lightenVisibility(ivec2) override
	{
	}

	void darkenVisibility(ivec2) override
	{
	}

	void blackenVisibility(ivec2) override
	{
	}

	void rebuildAllPlots() override
	{
	}

	void rebuildPlot(ivec2, bool, bool) override
	{
	}

	void rebuildRiverPlotTile(ivec2, bool, bool) override
	{
	}

	void rebuildTileArt(ivec2) override
	{
	}

	void forceTreeOffsets(ivec2) override
	{
	}

	bool getGridMode() override
	{
		return false;
	}

	void setGridMode(bool) override
	{
	}

	void addColoredPlot(ivec2, const NiColorA&, PlotStyles, PlotLandscapeLayers) override
	{
	}

	void clearColoredPlots(PlotLandscapeLayers) override
	{
	}

	void fillAreaBorderPlot(ivec2, const NiColorA&, AreaBorderLayers) override
	{
	}

	void clearAreaBorderPlots(AreaBorderLayers) override
	{
	}

	void updateFoundingBorder() override
	{
	}

	void triggerEffect(int, NiPoint3, float) override
	{
	}

	CvPlot* pickPlot(ivec2, NiPoint3&) override
	{
		return nullptr;
	}

	void setDirty(EngineDirtyBits, bool) override
	{
	}

	bool isDirty(EngineDirtyBits) override
	{
		return false;
	}

	void pushFogOfWar(FogOfWarModeTypes) override
	{
	}

	FogOfWarModeTypes popFogOfWar() override
	{
		return FOGOFWARMODE_UNEXPLORED;
	}

	void setFogOfWarFromStack() override
	{
	}

	void markBridgesDirty() override
	{
	}

	void addLaunch(PlayerTypes) override
	{
	}

	void addGreatWall(CvCity*) override
	{
	}

	void removeGreatWall(CvCity*) override
	{
	}

	void markPlotTextureAsDirty(ivec2) override
	{
	}
};

class CvEntity
{
public:
	enum class EType
	{
		PlotBuilder,
		Unit,
	};

	virtual ~CvEntity() = default;
	virtual EType getType() const noexcept = 0;
};

class CvUnitEntity : public CvEntity
{
public:
	explicit CvUnitEntity(CvUnit* unit) : unit(unit)
	{
	}

	virtual EType getType() const noexcept override
	{
		return EType::Unit;
	}

	CvUnit* unit = nullptr;
};

class MyCvDLLEntityIFace : public CvDLLEntityIFaceBase
{
public:
	virtual void removeEntity(CvEntity*) override {}
	virtual void addEntity(CvEntity*, unsigned int) override {}
	virtual void setup(CvEntity*) override {}
	virtual void setVisible(CvEntity*, bool) override {}
	virtual void createCityEntity(CvCity*) override {}
	virtual void createUnitEntity(CvUnit*) override {}
	virtual void destroyEntity(CvEntity*&, bool) override {}
	virtual void updatePosition(CvEntity*) override {}
	virtual void setupFloodPlains(CvRiver*) override {}
	virtual bool IsSelected(const CvEntity*) const override
	{
		return false;
	}
	virtual void PlayAnimation(CvEntity*, AnimationTypes, float, bool, int, float, float) override {}
	virtual void StopAnimation(CvEntity*, AnimationTypes) override {}
	virtual void StopAnimation(CvEntity*) override {}
	virtual void NotifyEntity(CvUnitEntity*, MissionTypes) override {}
	virtual void MoveTo(CvUnitEntity*, const CvPlot*) override {}
	virtual void QueueMove(CvUnitEntity*, const CvPlot*) override {}
	virtual void ExecuteMove(CvUnitEntity*, float, bool) override {}
	virtual void SetPosition(CvUnitEntity*, const CvPlot*) override {}
	virtual void AddMission(const CvMissionDefinition*) override {}
	virtual void RemoveUnitFromBattle(CvUnit*) override {}
	virtual void showPromotionGlow(CvUnitEntity*, bool) override {}
	virtual void updateEnemyGlow(CvUnitEntity*) override {}
	virtual void updatePromotionLayers(CvUnitEntity*) override {}
	virtual void updateGraphicEra(CvUnitEntity*, EraTypes) override {}
	virtual void SetSiegeTower(CvUnitEntity*, bool) override {}
	virtual bool GetSiegeTower(CvUnitEntity*) override { return false; }
};

class CvFeature
{
public:
	FeatureTypes type = NO_FEATURE;
};

class MyCvDLLFeature : public CvDLLFeatureIFaceBase
{
public:
	// Inherited via CvDLLFeatureIFaceBase
	virtual CvFeature* createFeature() override
	{
		static constinit CvFeature s;
		return &s;
	}
	virtual void init(CvFeature*, int, int, int, CvPlot*) override {}
	virtual FeatureTypes getFeature(CvFeature*) override { return NO_FEATURE; }
	virtual void setDummyVisibility(CvFeature*, const char*, bool) override {}
	virtual void addDummyModel(CvFeature*, const char*, const char*) override {}
	virtual void setDummyTexture(CvFeature*, const char*, const char*) override {}
	virtual CvString pickDummyTag(CvFeature*, int, int) override { return {}; }
	virtual void resetModel(CvFeature*) override {}
};

class MyCvDLLSymbol : public CvDLLSymbolIFaceBase
{
public:
	// Inherited via CvDLLSymbolIFaceBase
	virtual void init(CvSymbol*, int, int, int, CvPlot*) override {}
	virtual CvSymbol* createSymbol() override { return nullptr; }
	virtual void destroy(CvSymbol*&, bool) override {}
	virtual void setAlpha(CvSymbol*, float) override {}
	virtual void setScale(CvSymbol*, float) override {}
	virtual void Hide(CvSymbol*, bool) override {}
	virtual bool IsHidden(CvSymbol*) override { return false; }
	virtual void updatePosition(CvSymbol*) override {}
	virtual int getID(CvSymbol*) override { return -1; }
	virtual SymbolTypes getSymbol(CvSymbol*) override { return NO_SYMBOL; }
	virtual void setTypeYield(CvSymbol*, int, int) override {}
};

class CvRoute
{
public:
	RouteTypes type = NO_ROUTE;
};

class MyCvDLLRoute : public CvDLLRouteIFaceBase
{
public:
	// Inherited via CvDLLRouteIFaceBase
	virtual CvRoute* createRoute() override
	{
		static constinit CvRoute s;
		return &s;
	}
	virtual void init(CvRoute*, int, int, int, CvPlot*) override {}
	virtual RouteTypes getRoute(CvRoute*) override { return NO_ROUTE; }
	virtual int getConnectionMask(CvRoute*) override { return 0; }
	virtual void updateGraphicEra(CvRoute*) override {}
};

class CvRiver
{
public:
};

class MyCvDLLRiver : public CvDLLRiverIFaceBase
{
public:
	// Inherited via CvDLLRiverIFaceBase
	virtual CvRiver* createRiver() override
	{
		static constinit CvRiver s;
		return &s;
	}
	virtual void init(CvRiver*, int, int, int, CvPlot*) override {}
};

class MyCvDLLPlotBuilder : public CvDLLPlotBuilderIFaceBase
{
public:
	virtual void init(CvPlotBuilder*, CvPlot*) override {}
	virtual CvPlotBuilder* create() override { return nullptr; }
};

class MyCvDLLFlagEntity : public CvDLLFlagEntityIFaceBase
{
public:
	// Inherited via CvDLLFlagEntityIFaceBase
	virtual CvFlagEntity* create(PlayerTypes) override { return nullptr; }
	virtual PlayerTypes getPlayer(CvFlagEntity*) const override { return NO_PLAYER; }
	virtual CvPlot* getPlot(CvFlagEntity*) const override { return nullptr; }
	virtual void setPlot(CvFlagEntity*, CvPlot*, bool) override {}
	virtual void updateUnitInfo(CvFlagEntity*, const CvPlot*, bool) override {}
	virtual void updateGraphicEra(CvFlagEntity*) override {}
	virtual void destroy(CvFlagEntity*&, bool) override {}
};

struct HeadlessApp : cvengine::ICommonEngineCallbackHandler
{
	// Inherited via ICommonEngineCallbackHandler
	void registerPythonExtensions(const pybind11::module_&) override
	{
	}
	bool isShiftDown() const override
	{
		return false;
	}
	bool isAltDown() const override
	{
		return false;
	}
	bool isCtrlDown() const override
	{
		return false;
	}
	std::optional<std::filesystem::path> promptSaveGamePath(const std::filesystem::path&) override
	{
		throw std::runtime_error(std::string("Common Engine called ") + __func__ + ".");
	}
	std::optional<std::filesystem::path> promptLoadGamePath(const std::filesystem::path&) override
	{
		throw std::runtime_error(std::string("Common Engine called ") + __func__ + ".");
	}
	void deferLoadGame(const std::filesystem::path&) override
	{
		throw std::runtime_error(std::string("Common Engine called ") + __func__ + ".");
	}
	bool isAutorun() const override
	{
		// AI autoplay
		return gGlobals.getGame().getAIAutoPlay() > 0;
	}
	void exitApp() override
	{
		throw std::runtime_error(std::string("Common Engine called ") + __func__ + ".");
	}
	bool isAppExiting() const override
	{
		return false;
	}
};

[[noreturn]] static void invalidArgs()
{
	static constexpr const char* kHelp = R"(Usage: Cv4HeadlessMiniEngine [options] [path to save game]
Options:
	-bot <path to DLL/so>
		Use the specified bot.
)";

	std::cerr << kHelp << std::endl;
	std::exit(EXIT_FAILURE);
}

int main(int argc, char* argv[])
{
	// Don't you just hate it when you can't see error messages because the error output is in fail state?
	std::cout.exceptions(~std::ios::goodbit);
	std::clog.exceptions(~std::ios::goodbit);
	std::cerr.exceptions(~std::ios::goodbit);
	std::wcout.exceptions(~std::ios::goodbit);
	std::wclog.exceptions(~std::ios::goodbit);
	std::wcerr.exceptions(~std::ios::goodbit);
	// So it seems you can't mix std stream encodings on Linux. Well... that's just great isn't it.
	// Oh, just had to add this:
	std::ios::sync_with_stdio(false);

	

	const std::filesystem::path userDataDirPath = heck::getUserGamesSpecialDirectory("Beyond the Sword", heck::EUserGamesSpecialDirectory::Data);
	const std::filesystem::path userConfigDirPath = heck::getUserGamesSpecialDirectory("Beyond the Sword", heck::EUserGamesSpecialDirectory::Config);
	const std::filesystem::path cacheDirPath = heck::getUserGamesSpecialDirectory("Cv4MiniEngine", heck::EUserGamesSpecialDirectory::Cache);
	const std::filesystem::path replaysDirName = L"Replays (Cv4HeadlessEngine)";
	const std::filesystem::path savesDirName = L"Saves (Cv4HeadlessEngine)";
	const std::filesystem::path iniFilename = L"Cv4MiniEngine.ini";
	const std::filesystem::path profileFilename = L"Cv4MiniEngine Profile.txt";

	const std::filesystem::path iniPath = userConfigDirPath / iniFilename;
	cvengine::IniData::createIfNew(iniPath);
	const cvengine::IniData ini = cvengine::loadINI(iniPath);

	constexpr cvengine::IniDocKey kCivilizationIVIniSection_CV4ENGINE{ "CV4MINIENGINE" };
	constexpr cvengine::IniDocKey kCivilizationIVIniProp_VanillaCiv4RootDir{ "VanillaCiv4RootDir", "Path to root directory of your Civilization 4 installation." };
	const std::filesystem::path vanillaCiv4RootDir = ini.get(kCivilizationIVIniSection_CV4ENGINE, kCivilizationIVIniProp_VanillaCiv4RootDir, L"");

	if (vanillaCiv4RootDir.empty())
		throw std::runtime_error("CV4ENGINE VanillaCiv4RootDir not specified in INI.");

	const std::filesystem::path engineAssetsOverrideDir = heck::findEnvironmentVariable(L"CV4MINIENGINE_DATADIR").value_or(L".");

	const CommandLine cmdLine = parseCommandLine(argc, argv);

	const cvbot::IPlayerBotPlugin* botPlugin = nullptr;

	// Load player bot DLL.
	if (!cmdLine.botPath.empty())
	{
		if (!hasCvGameCoreDLLPlayerBotSupport())
			throw std::runtime_error("Bot specified, but current CvGameCoreDLL not compiled with bot support.");
	
		heck::DynamicLibrary lib(cmdLine.botPath);
	
		botPlugin = reinterpret_cast<const cvbot::IPlayerBotPlugin*(*)()>(lib.resolve("getPlayerBotPlugin"))();
	}

	CvHeadlessInterface interface;
	CvHeadlessEngine graphicsEngine;
	MyCvDLLEntityIFace entityIFace;
	MyCvDLLSymbol symbolIFace;
	MyCvDLLFeature featureIFace;
	MyCvDLLRoute routeIFace;
	MyCvDLLPlotBuilder plotBuilderIFace;
	MyCvDLLRiver riverIFace;
	MyCvDLLFlagEntity flagIFace;

	HeadlessApp app;

	[[maybe_unused]] const cvengine::CommonEngineInitResult initResult = cvengine::initialiseCommonEngine(std::wcout, cvengine::CommonEngineConfig{
		.civ4InstallationRoot = vanillaCiv4RootDir,
		.optModRelPath = !cmdLine.modRelPath.empty() ? std::optional(cmdLine.modRelPath) : std::nullopt,
		.optEngineAssetsOverrideDir = engineAssetsOverrideDir,
		.userDataDirPath = userDataDirPath,
		.userConfigDirPath = userConfigDirPath,
		.cacheDirPath = cacheDirPath,
		.replaysDirName = replaysDirName,
		.savesDirName = savesDirName,
		.iniFilename = iniFilename,
		.profileFilename = profileFilename,
		.interface = &interface,
		.engine = &graphicsEngine,
		.entityIFace = &entityIFace,
		.symbolIFace = &symbolIFace,
		.featureIFace = &featureIFace,
		.routeIFace = &routeIFace,
		.plotBuilderIFace = &plotBuilderIFace,
		.riverIFace = &riverIFace,
		.flagEntityIFace = &flagIFace,
		.optPlayerBotPlugin = botPlugin,
		.callbackHandler = &app,
		.isPitbossHost = true,
		.enableCustomAssets = false,
		});

	// Patch scripts.
	(void)pybind11::module::import("Cv4MiniEngineEntryPoint").attr("init")();

	const auto progressCallback = [](std::wstring_view s) { std::wcout << s << '\n'; };

	if (cmdLine.saveFile.empty())
	{
		cvengine::app::NewGameStartupState state(cvengine::app::getGameSetupFromIni(), false);
		state.onEnter(progressCallback);
		state.onLeave();
	}
	else if (gGlobals.getDLLIFaceNonInl()->isDescFileName(cmdLine.saveFile.string().c_str()))
	{
		cvengine::app::StartScenarioGameState state(cmdLine.saveFile, {
			.activePlayerI = static_cast<PlayerTypes>(cmdLine.scenarioPreferredPlayer),
			});
		state.onEnter(progressCallback);
		state.onLeave();
	}
	else
	{
		cvengine::app::LoadGameState state(cmdLine.saveFile);
		state.onEnter(progressCallback);
		state.onLeave();
	}

	{
		cvengine::app::InGameState state;
		state.onEnter(progressCallback);
	}

	constexpr int kNumAIAutoplayTurns = 100'000;
	constexpr int kMaxUpdatesStalled = 10'000;

	CvGame& game = gGlobals.getGame();
	if (!botPlugin)
		game.setAIAutoPlay(kNumAIAutoplayTurns);

	CvPlayer& activePlayer = GET_PLAYER(game.getActivePlayer());
	activePlayer.setPlayerBotFinalTurn(kNumAIAutoplayTurns);


	int lastUpdateTurn = 0;
	int lastProgressingUpdateNum = -1;
	int updateNum = 0;

	GameRunScope runScope;

	while (game.getGameState() == GAMESTATE_ON)
	{
		const int turn = game.getGameTurn();

		//if (turn > 100)
		//	break;

		bool progression = false;

		progression |= turn != lastUpdateTurn;
		progression |= DLLMessageQueue::getInstance().execute();

		// Doing the update first will get the bot to handle popups.
		game.update();

		runScope.update();

		if (activePlayer.isTurnActive())
		{
			if ([[maybe_unused]] const CvDiploParameters* const diplo = activePlayer.popFrontDiplomacy())
				throw std::runtime_error("Active player has queued diplo.");

			while (const CvPopupInfo* const popupInfo = activePlayer.popFrontPopup())
			{
				if (popupInfo->getText() == L"showDawnOfMan")
					continue;
				std::wclog << popupInfo->getText() << L'\n'; // This could be a bot failure exception.
				throw std::runtime_error("Active player has a queued popup.");
			}
		}

		lastUpdateTurn = turn;
		++updateNum;

		if (progression)
			lastProgressingUpdateNum = updateNum;
		else if (updateNum - lastProgressingUpdateNum > kMaxUpdatesStalled)
			throw std::runtime_error("Game update loop has stalled.");
	}

	runScope.dumpSummary();


	cvengine::shutdownCommonEngine();

	return 0;
}
