#include "../../CommonConfig.h"

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS

#include "FoundValueSystem.h"

#include "../../CvPlayerAI.h"
#include "../../CvInfos.h"

#include <fstream>

using namespace GiganticMapsOptimisationsLib::FoundValueSystem;

// Whole-map verification. EXPENSIVE!
static constexpr bool kEnableVerify = false;

System::System()
	: mMapGeom(GC.getMapINLINE())
	, mAreaMap(GC.getMapINLINE())
	, mDeadlockedBonusesTracker(mMapGeom)
	, mCommonPlotProps(mMapGeom, mAreaMap)
	, mPlayerPlotProps(MAX_PLAYERS)
{
	for (const PlayerTypes playerI : range<PlayerTypes>(MAX_PLAYERS))
		if (GET_PLAYER(playerI).isAlive())
			mPlayerPlotProps[playerI].emplace(mMapGeom, mAreaMap, playerI, mDeadlockedBonusesTracker);

	// Gather up plot bonuses.
	mBonusLocations.resize(GC.getNumBonusInfos());
	CvMap& map = GC.getMapINLINE();
	for (const int y : range(mMapGeom.dim.y))
		for (const int x : range(mMapGeom.dim.x))
			if (const CvPlot& plot = *map.plot(x, y); plot.getBonusType() != NO_BONUS)
				mBonusLocations[plot.getBonusType()].push_back(ivec2(x, y));

	// If you want a list of cities for TestExperimentCityDistanceField.
	//std::ofstream cityDump("Found value system city list.txt");
	//for (const PlayerTypes playerI : range<PlayerTypes>(MAX_PLAYERS))
	//{
	//	const CvPlayer& player = GET_PLAYER(playerI);
	//	int itIndex{};
	//	for (auto* city = player.firstCity(&itIndex); city; city = player.nextCity(&itIndex))
	//		cityDump << (int)playerI << ' ' << city->getX_INLINE() << ' ' << city->getY_INLINE() << '\n';
	//}
}

void System::onPlotTerrainOrFeatureChanged(const CvPlot& plot)
{
	mCommonPlotProps.onPlotTerrainOrFeatureChanged(plot);
}
void System::onPlotGetYieldChanged(const CvPlot& plot)
{
	mCommonPlotProps.onPlotGetYieldChanged(plot);
}
void System::onCityRadiusCountChanged(const CvPlot& plot)
{
	mCommonPlotProps.onCityRadiusCountChanged(plot);
}
void System::onUnitMoved(const CvUnit& unit, ivec2 from, ivec2 to) // Thread-safe.
{
	mCommonPlotProps.onUnitMoved(unit, from, to);
}
void System::onGlobalAreaCityCountChanged(const CvArea& area, int from, int to)
{
	mCommonPlotProps.onAreaCityCountChanged(area, from, to);
}
void System::onPlotBonusChanged(const CvPlot& plot, BonusTypes from, BonusTypes to)
{
	if (from != to)
	{
		mDeadlockedBonusesTracker.onVisibleHasBonusChanged(getPlotCoord(plot));
		mCommonPlotProps.onPlotBonusChanged(plot);
		for (auto& props : mPlayerPlotProps)
			if (props)
				props->onRevealedBonusChanged(plot);

		if (from != NO_BONUS)
		{
			// An existing bonus was changed or removed. This doesn't happen particularly often...
			std::erase(mBonusLocations[from], getPlotCoord(plot));
		}

		if (to != NO_BONUS)
		{
			// Yes, this can happen. /My/ mod removes bonuses.
			mBonusLocations[to].push_back(getPlotCoord(plot));
		}
	}
}
void System::onPlotOwnerChanged(const CvPlot& plot, PlayerTypes from, PlayerTypes to)
{
	mDeadlockedBonusesTracker.onPlotOwnerChanged(getPlotCoord(plot));
	mCommonPlotProps.onPlotOwnerChanged(plot, from, to);
	for (auto& props : mPlayerPlotProps)
		if (props)
			props->onPlotOwnerChanged(plot);
}
void System::onAfterCityCreated(const CvPlot& plot, PlayerTypes owner)
{
	mDeadlockedBonusesTracker.onAfterCityCreatedOrDestroyed(getPlotCoord(plot));
	mCommonPlotProps.onAfterCityCreated(plot);
	for (auto& props : mPlayerPlotProps)
		if (props)
			props->onAfterCityCreated(plot, owner);
}
void System::onAfterCityDestroyed(const CvPlot& plot, PlayerTypes owner)
{
	mDeadlockedBonusesTracker.onAfterCityCreatedOrDestroyed(getPlotCoord(plot));
	mCommonPlotProps.onAfterCityDestroyed(plot);
	for (auto& props : mPlayerPlotProps)
		if (props)
			props->onAfterCityDestroyed(plot, owner);
}
void System::onPlayerCapitalChanged(PlayerTypes playerI, const CvCity* to)
{
	mPlayerPlotProps[playerI]->onCapitalChanged(to);
}
void System::onPlayerAICitySitesChanged(PlayerTypes playerI)
{
	mPlayerPlotProps[playerI]->onPlayerAICitySitesChanged();
}
void System::onPlayerPlotCultureChanged(const CvPlot& plot, PlayerTypes playerI)
{
	if (playerI != plot.getOwnerINLINE())
	{
		// CvPlayer::splitEmpire will set culture before reviving player.
		if (mPlayerPlotProps[playerI])
			mPlayerPlotProps[playerI]->onCultureChanged(plot);
	}
	else
	{
		// TODO: This may be quadratic time. May be worth filtering down to multiple-culture plots only.
		for (auto& props : mPlayerPlotProps)
			if (props)
				props->onCultureChanged(plot);
	}
}
void System::onPlayerPlotRevealedChanged(const CvPlot& plot, PlayerTypes playerI)
{
	mPlayerPlotProps[playerI]->onRevealedChanged(plot);
}
void System::onPlayerAreaCityCountChanged([[maybe_unused]] const CvArea& area, PlayerTypes playerI, int from, int to)
{
	for (const auto loopPlayerI : range<PlayerTypes>(MAX_PLAYERS))
	{
		if (mPlayerPlotProps[loopPlayerI])
		{
			if (loopPlayerI == playerI)
				mPlayerPlotProps[loopPlayerI]->onAreaCityCountChanged(from, to);
			else
				// Have to notify other players too because the area total city count changed.
				// Note that CvArea::getNumCities is updated by CvArea::changeCitiesPerPlayer.
				mPlayerPlotProps[loopPlayerI]->onAreaCityCountChanged(area.getCitiesPerPlayer(loopPlayerI), area.getCitiesPerPlayer(loopPlayerI));
		}
	}
}
void System::onTeamTech(TeamTypes teamI, TechTypes techI)
{
	CvMap& map = GC.getMapINLINE();
	for (const auto playerI : range<PlayerTypes>(MAX_PLAYERS))
	{
		if (GET_PLAYER(playerI).getTeam() == teamI && mPlayerPlotProps[playerI])
		{
			for (const auto bonusTypeI : range<BonusTypes>(GC.getNumBonusInfos()))
			{
				if (GC.getBonusInfo(bonusTypeI).getTechReveal() == techI)
				{
					for (const ivec2 coord : mBonusLocations[bonusTypeI])
					{
						mDeadlockedBonusesTracker.onVisibleHasBonusChanged(coord);
						mPlayerPlotProps[playerI]->onRevealedBonusChanged(*map.plot(coord.x, coord.y));
					}
				}
			}
		}
	}
}

void System::onTeamForceRevealBonusChanged(TeamTypes teamI, BonusTypes bonusTypeI)
{
	CvMap& map = GC.getMapINLINE();
	for (const auto playerI : range<PlayerTypes>(MAX_PLAYERS))
	{
		if (GET_PLAYER(playerI).getTeam() == teamI && mPlayerPlotProps[playerI])
		{
			for (const ivec2 coord : mBonusLocations[bonusTypeI])
			{
				mDeadlockedBonusesTracker.onVisibleHasBonusChanged(coord);
				mPlayerPlotProps[playerI]->onRevealedBonusChanged(*map.plot(coord.x, coord.y));
			}
		}
	}
}

void System::onPlayerAliveChanged(PlayerTypes playerI, bool alive)
{
	if (alive)
		mPlayerPlotProps[playerI].emplace(mMapGeom, mAreaMap, playerI, mDeadlockedBonusesTracker);
	else
		mPlayerPlotProps[playerI].reset();
}

void System::update(PlayerTypes playerI)
{
	for (auto& props : mPlayerPlotProps)
	{
		if (props)
		{
			for (const ivec2 coord : mDeadlockedBonusesTracker.getCanFoundDirtyList())
				props->onCanFoundChanged(mDeadlockedBonusesTracker, coord);
			for (const ivec2 coord : mDeadlockedBonusesTracker.getDeadlockedBonusCountDirtyList())
				props->onDeadlockedBonusesChanged(mDeadlockedBonusesTracker, coord);
		}
	}
	mDeadlockedBonusesTracker.clearDirtyLists();

	// EXPENSIVE
	//if constexpr (kEnableVerify)
	//	mDeadlockedBonusesTracker.verify();

	mCommonPlotProps.update(playerI);
	if (mPlayerPlotProps[playerI])
		mPlayerPlotProps[playerI]->update(); // This calls AI_bonusVal, which are lazily calculated values.

	if constexpr (kEnableVerify)
		mCommonPlotProps.verify(mAreaMap);
}

void System::verify(PlayerTypes playerI) const
{
	mDeadlockedBonusesTracker.verify();
	mCommonPlotProps.verify(mAreaMap);
	if (playerI == NO_PLAYER)
	{
		for (auto& props : mPlayerPlotProps)
			if (props)
				props->verify(mAreaMap, mDeadlockedBonusesTracker);
	}
	else if (mPlayerPlotProps[playerI])
		mPlayerPlotProps[playerI]->verify(mAreaMap, mDeadlockedBonusesTracker);
}

void System::dumpEvaluationAtAndAbort(PlayerTypes playerI, ivec2 coord, int iMinRivalRange) const
{
	std::ostringstream basicLog;
	const int liveLoggingValue = computeFoundValue_Logging(playerI, coord, iMinRivalRange, basicLog);
	const int liveValue = GET_PLAYER(playerI).AI_foundValue(coord.x, coord.y, iMinRivalRange, false);
	if (liveLoggingValue != liveValue)
		std::abort(); // computeFoundValue_Logging broken

	const ivec2 rowStartCoord{ 0, coord.y };
	Evaluator evaluator(*this, playerI, rowStartCoord);

	std::array<int32_t, kVectorWidth> vec;

	std::string vectorisedLog;
	const int evalX = std::min(coord.x / kVectorWidth * kVectorWidth, mMapGeom.dim.x - kVectorWidth);
	evaluator.eval(evalX, iMinRivalRange, EFilter::None, vec, &vectorisedLog);

	const int vecI = coord.x - evalX;

	std::ofstream("FoundValueSystem computeFoundValuesRow live log.txt") << basicLog.str();
	std::ofstream("FoundValueSystem computeFoundValuesRow vectorised log.txt") << "Vector element X: " << vecI << '\n' << vectorisedLog;

	std::abort();
}

System::Evaluator::Evaluator(const System& sys, PlayerTypes playerI, ivec2 rowStartCoord)
	: commonGlobal(sys.mCommonPlotProps.createCommonGlobalDataInterface())
	, commonLocal(sys.mCommonPlotProps.createCommonMapInterface(rowStartCoord))
	, playerGlobal(sys.mPlayerPlotProps[playerI]->createPlayerGlobalsInterface())
	, playerLocal(sys.mPlayerPlotProps[playerI]->createPlayerMapInterface(rowStartCoord))
	, commonLocalsInterfaceInitialAddressingCoord(commonLocal.addressingCoord)
	, playerLocalsInterfaceInitialAddressingCoord(playerLocal.addressingCoord)
{
}

void System::Evaluator::eval(int x, int iMinRivalRange, EFilter filter, std::span<int, kVectorWidth> out, std::string* logOutput)
{
	commonLocal.addressingCoord.x = commonLocalsInterfaceInitialAddressingCoord.x + x;
	playerLocal.addressingCoord.x = playerLocalsInterfaceInitialAddressingCoord.x + x;

	const I32Vector v = computeFoundValue_Vectorised(
		commonGlobal,
		playerGlobal,
		commonLocal,
		playerLocal,
		iMinRivalRange,
		filter == EFilter::Revealed ? static_cast<I16Mask>(playerLocal.getRevealedMask(CITY_HOME_PLOT)) : I16Mask::kAll,
		logOutput
	);

	std::ranges::copy(v.toArray(), out.begin());
}

std::vector<int> System::computeFoundValuesRow(PlayerTypes playerI, EFilter filter, int y, int iMinRivalRange) const
{
	std::vector<int> row(mMapGeom.dim.x);

	CvMap& map = GC.getMapINLINE();
	const CvPlayerAI& player = GET_PLAYER(playerI);
	const TeamTypes teamI = player.getTeam();

	const auto filterValue = [filter, teamI, &map, y](int x, int value) {
		if (filter == EFilter::None || map.plotINLINE(x, y)->isRevealed(teamI, false))
			return value;
		else
			return 0;
		};

	const auto calcResultAt = [&player, iMinRivalRange, y, filterValue](int x) {
		return filterValue(x, player.AI_foundValue(x, y, iMinRivalRange, false));
		};

	if (mMapGeom.dim.x < static_cast<int>(kVectorWidth))
	{
		// Only use verification for maps bigger than this.
		if constexpr (kEnableVerify)
			std::abort();
		else
		{
			// Non-gigantic map. Fallback to AI_foundValue to avoid out of bounds.
			// Note that this may be called on multiple threads, so `update` will make sure that lazily calculated values are set before-hand.
			for (const int x : range(mMapGeom.dim.x))
				row[x] = calcResultAt(x);
		}
	}
	else
	{
		const ivec2 rowStartCoord{ 0, y };
		Evaluator evaluator(*this, playerI, rowStartCoord);

		

		for (int x = 0; x + static_cast<int>(kVectorWidth) < mMapGeom.dim.x; x += kVectorWidth)
			evaluator.eval(x, iMinRivalRange, filter, std::span(row).subspan(x).subspan<0, kVectorWidth>());

		// Now do the last vector. To avoid out of bounds, it must end at the map border.
		evaluator.eval(mMapGeom.dim.x - kVectorWidth, iMinRivalRange, filter,
			std::span(row).subspan(mMapGeom.dim.x - kVectorWidth).subspan<0, kVectorWidth>());

		if constexpr (kEnableVerify)
		{
			for (const int x : range(mMapGeom.dim.x))
				if (row[x] != calcResultAt(x))
				{
					mDeadlockedBonusesTracker.verify();
					mCommonPlotProps.verify(mAreaMap);
					mPlayerPlotProps[playerI]->verify(mAreaMap, mDeadlockedBonusesTracker);

					dumpEvaluationAtAndAbort(playerI, { x, y }, iMinRivalRange);
				}
		}
	}

	return row;
}

#endif