#pragma once

#include <PlayerBotGameBinding/Common.h>
#include <PlayerBotGameBinding/DLLDefs.h>
#include <PlayerBotGameBinding/EnumFwd.h>

namespace cvbot
{
	class IGame;
	struct Unit;
	struct City;
	struct Plot;
	struct TechState;
	struct GlobalInfoData;
	struct GlobalInfo;
	struct UnitInfo;
	struct MapGeometry;
	struct CivState;
	struct TechInfo;
	struct Interval;
	struct Player;
}

namespace mybot
{
	using cvbot::City;
	using cvbot::EAutomation;
	using cvbot::EBuildingType;
	using cvbot::ECommerce;
	using cvbot::EDomain;
	using cvbot::EMission;
	using cvbot::EUnitClass;
	using cvbot::EUnitId;
	using cvbot::EUnitType;
	using cvbot::GlobalInfo;
	using cvbot::GlobalInfoData;
	using cvbot::i16vec2;
	using cvbot::IGame;
	using cvbot::ivec2;
	using cvbot::kBarbarianPlayer;
	using cvbot::kMoveDenominator;
	using cvbot::kNoPlayer;
	using cvbot::MapGeometry;
	using cvbot::Plot;
	using cvbot::Span2D;
	using cvbot::TechState;
	using cvbot::Unit;
	using cvbot::UnitInfo;
	using cvbot::BotFailure;
	using cvbot::ETech;
	using cvbot::Interval;
	using cvbot::CivState;
	using cvbot::ETeam;
	using cvbot::Player;
	using cvbot::TechInfo;
	using cvbot::cdiv;
	using cvbot::ELeaderhead;
	using cvbot::EBonus;
	using cvbot::EBuildingClass;
	using cvbot::EProject;
	using cvbot::ELeaderTrait;
	using cvbot::EPlotType;
	using cvbot::EFeature;
	using cvbot::EImprovement;
	using cvbot::EYield;
	using cvbot::kCityWorkPlotCoords;
}