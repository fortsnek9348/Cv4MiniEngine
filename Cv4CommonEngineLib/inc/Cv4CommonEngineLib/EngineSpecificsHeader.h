#pragma once

#include <PlayerBotEnablement.h>

#include <filesystem>
#include <optional>

class CvDLLEntityIFaceBase;
class CvDLLInterfaceIFaceBase;
class CvDLLEngineIFaceBase;
class CvDLLSymbolIFaceBase;
class CvDLLFeatureIFaceBase;
class CvDLLRouteIFaceBase;
class CvDLLPlotBuilderIFaceBase;
class CvDLLRiverIFaceBase;
class CvDLLFlagEntityIFaceBase;
class CvInterface;
class NiPoint3;
class CvEngine;

namespace pybind11
{
	class module_;
}

#if ENABLE_PLAYER_BOT
namespace cvbot
{
	class IPlayerBotPlugin;
}
#endif

// Functions that need to implemented by the engine.
namespace cvengine::engine_specific
{
	// Implementation of whatever displays CvPopups.
	class IPopupUIWindow
	{
	public:
		virtual void setWantClose() = 0;
		virtual bool isWantClose() const = 0;
		virtual ~IPopupUIWindow() = default;
	};

	void registerEngineSpecificPythonBindings(pybind11::module_ m);

	// CvDLLUtilityIFaceBase stuff that depends on UI/graphics implementation.
	CvDLLEntityIFaceBase& getEntityIFace();
	CvDLLSymbolIFaceBase& getSymbolIFace();
	CvDLLFeatureIFaceBase& getFeatureIFace();
	CvDLLRouteIFaceBase& getRouteIFace();
	CvDLLPlotBuilderIFaceBase& getPlotBuilderIFace();
	CvDLLRiverIFaceBase& getRiverIFace();
	CvDLLFlagEntityIFaceBase& getFlagEntityIFace();

	CvInterface& getCvInterface();
	//void resetCvInterface(); // CvInterface::getInstance().reset();
	//void uninitCvInterface(); // CvInterface::getInstance().uninit();
	//void startMainInterface();

#if ENABLE_PLAYER_BOT
	const cvbot::IPlayerBotPlugin* getPlayerBotPlugin();
#endif

	CvEngine& getCvEngine();

	//void playSound3D(int iScriptId, NiPoint3 vPosition);

	bool isShiftDown();
	bool isAltDown();
	bool isCtrlDown();

	std::optional<std::filesystem::path> promptSaveGamePath(const std::filesystem::path& defPath);
	std::optional<std::filesystem::path> promptLoadGamePath(const std::filesystem::path& defDir);
	
	void deferLoadGame(const std::filesystem::path&);

	//void serialiseCvEngine(FFile<StdRawBinaryStream>& file);
	//void deserialiseCvEngine(FFile<StdRawBinaryStream>& file);

	bool isAutorun();

	void exitApp();
	bool isAppExiting();
}