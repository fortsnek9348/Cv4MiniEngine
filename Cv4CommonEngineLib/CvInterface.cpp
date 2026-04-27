#include "inc/Cv4CommonEngineLib/CvInterface.h"

#include <CvEventReporter.h>
#include <CvGameCoreUtils.h>
#include <CvMessageControl.h>
#include <CvGlobals.h>
#include <CvGameAI.h>

#include <algorithm>

//using namespace cvengine;

CvInterface::CvInterface()
{
	uninit();
}

void CvInterface::reset()
{
	mIsHasMovedUnit = false;
	mCachedPlotList = {};
}
void CvInterface::uninit()
{
	mIsHasMovedUnit = false;
	mCachedPlotList = {};
}

bool CvInterface::removeFromSelectionList(CvUnit* pUnit)
{
	CvSelectionGroup& selectionList = *getSelectionList();
	for (auto* node = selectionList.headUnitNode(); node; node = selectionList.nextUnitNode(node))
	{
		if (getUnit(node->m_data) == pUnit)
		{
			selectionList.deleteUnitNode(node);
			setInterfaceMode(INTERFACEMODE_SELECTION);
			return true;
		}
	}

	return false;
}
bool CvInterface::mirrorsSelectionGroup() const
{
	// Returns true iff the interface's selection group matches the unit's DLL-side selection group.
	const CvSelectionGroup& uiGroup = *getSelectionList();
	CvUnit* unit = uiGroup.getHeadUnit();
	if (!unit)
		return false;

	const CvSelectionGroup* const dllGroup = unit->getGroup();

	if (!dllGroup || uiGroup.getNumUnits() != dllGroup->getNumUnits())
		return false;

	std::vector<IDInfo> uiUnits;
	std::vector<IDInfo> dllUnits;
	uiUnits.reserve(uiGroup.getNumUnits());
	dllUnits.reserve(uiGroup.getNumUnits());

	for (auto* node = uiGroup.headUnitNode(); node; node = uiGroup.nextUnitNode(node))
		uiUnits.push_back(node->m_data);
	for (auto* node = dllGroup->headUnitNode(); node; node = dllGroup->nextUnitNode(node))
		dllUnits.push_back(node->m_data);

	const auto proj = [](IDInfo a) {
		return std::pair(a.eOwner, a.iID);
		};

	std::ranges::sort(uiUnits, std::less<>(), proj);
	std::ranges::sort(dllUnits, std::less<>(), proj);

	return uiUnits == dllUnits;
}

void CvInterface::clearSelectionList()
{
	getSelectionList()->clearUnits();
}

bool CvInterface::tryRemoveUnitFromSelectionGroup(CvUnit* pUnit)
{
	CvSelectionGroup& selectionList = *getSelectionList();
	for (auto node = selectionList.headUnitNode(); node; node = selectionList.nextUnitNode(node))
	{
		if (getUnit(node->m_data) == pUnit)
		{
			selectionList.deleteUnitNode(node);
			makeSelectionListDirty();
			return true;
		}
	}

	return false;
}

void CvInterface::insertIntoSelectionList(CvUnit* pUnit, bool bClear, bool bToggle, bool bGroup, bool bSound, bool bMinimalChange)
{
	// Bunch of stuff goes on in here...

	if (!pUnit || (pUnit->getGroup() && pUnit->getGroup()->isBusy()))
		return;

	if (bClear)
		clearSelectionList();

	bool added = false;

	CvSelectionGroup& selectionList = *getSelectionList();

	if (bToggle)
	{
		if (!tryRemoveUnitFromSelectionGroup(pUnit))
			added = selectionList.addUnit(pUnit, bMinimalChange);
	}
	else
		added = selectionList.addUnit(pUnit, bMinimalChange);

	if (added)
	{
		if (bSound)
			playUnitSelectionSound(*pUnit);

		resetInterfaceMode();
		makeSelectionListDirty();

		if (bGroup)
		{
			// NOTE: If bGroup, then mirrorsSelectionGroup should be true as well, before adding, it seems.
			//       So, if bGroup, the new unit should be the only one not in the same group as the others.
			for (auto* node = selectionList.headUnitNode(); node; node = selectionList.nextUnitNode(node))
			{
				const CvUnit* const otherUnit = getUnit(node->m_data);

				if (otherUnit->getGroup() != pUnit->getGroup())
				{
					CvMessageControl::getInstance().sendJoinGroup(pUnit->getID(), otherUnit->getID());
					break;
				}
			}
		}

		CvEventReporter::getInstance().unitSelected(pUnit);
	}
}

int CvInterface::getNumSelectedUnits() const
{
	if (auto* const list = getSelectionList())
		return list->getNumUnits();
	else
		return 0;
}

CvUnit* CvInterface::getSelectionUnit(int iIndex) const
{
	const CvSelectionGroup& list = *getSelectionList();
	if (static_cast<size_t>(iIndex) < static_cast<size_t>(list.getNumUnits()))
		return list.getUnitAt(iIndex);
	else
		return nullptr;
}

bool CvInterface::isUnitSelected(const CvUnit& unit) const
{
	return getSelectionList() && getSelectionList()->getUnitIndex(&unit) >= 0;
}

void CvInterface::resetInterfaceMode()
{
	setInterfaceMode(INTERFACEMODE_SELECTION);
}

bool CvInterface::isHasMovedUnit() const
{
	return mIsHasMovedUnit;
}
void CvInterface::setHasMovedUnit(bool bNewValue)
{
	mIsHasMovedUnit = bNewValue;
}

void CvInterface::changePlotListColumn(int iChange)
{
	mCachedPlotList.column += iChange;
	verifyPlotListColumn();
}
int CvInterface::getPlotListColumn() const
{
	return mCachedPlotList.column;
}
void CvInterface::verifyPlotListColumn()
{
	mCachedPlotList.column = std::clamp(mCachedPlotList.column, 0, static_cast<int>(std::max<size_t>(1, mCachedPlotList.unitIds.size()) - 1));
}
int CvInterface::getPlotListOffset() const
{
	// ???
	return 0;
}
void CvInterface::cacheInterfacePlotUnits(CvPlot& plot)
{
	mCachedPlotList.unitIds.clear();
	for (auto* node = plot.headUnitNode(); node; node = plot.nextUnitNode(node))
	{
		CvUnit* const unit = ::getUnit(node->m_data);
		if (unit && isActiveVisibleUnit(*unit))
			mCachedPlotList.unitIds.push_back(node->m_data);
	}
}
int CvInterface::countEntities(UnitTypes unitType)
{
	int count = 0;
	if (CvUnit* const selectedUnit = getSelectionUnit(0))
	{
		CvPlot& plot = *selectedUnit->plot();
		for (auto* node = plot.headUnitNode(); node; node = plot.nextUnitNode(node))
		{
			CvUnit* const unit = ::getUnit(node->m_data);
			if (unit && unit->getUnitType() == unitType && isActiveVisibleUnit(*unit))
				++count;
		}
	}
	return count;
}
CvUnit* CvInterface::getCachedInterfacePlotUnit(int iIndex) const
{
	if (static_cast<size_t>(iIndex) >= mCachedPlotList.unitIds.size())
		return nullptr;
	return ::getUnit(mCachedPlotList.unitIds[iIndex]);
}
int CvInterface::getNumCachedInterfacePlotUnits() const
{
	return static_cast<int>(mCachedPlotList.unitIds.size());
}
int CvInterface::getNumVisibleUnits() const
{
	int count = 0;
	if (CvUnit* const selectedUnit = getSelectionUnit(0))
	{
		CvPlot& plot = *selectedUnit->plot();
		for (auto* node = plot.headUnitNode(); node; node = plot.nextUnitNode(node))
		{
			CvUnit* const unit = ::getUnit(node->m_data);
			if (unit && isActiveVisibleUnit(*unit))
				++count;
		}
	}
	return count;
}

bool CvInterface::isActiveVisibleUnit(const CvUnit& unit)
{
	return unit.isInvisible(gGlobals.getGame().getActiveTeam(), false) && unit.plot()->isActiveVisible(false);
}