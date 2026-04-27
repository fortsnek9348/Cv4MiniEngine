#include "MyCvDLLFAStar.h"
#include <FAStar.h>

#include <CvGameCoreDLL/FAStarNode.h>

#include <cstdlib>

FAStar* MyCvDLLFAStar::create()
{
	return new FAStar();
}

void MyCvDLLFAStar::destroy(FAStar*& ptr, bool bSafeDelete)
{
	delete ptr;
	if (bSafeDelete)
		ptr = nullptr;
}

bool MyCvDLLFAStar::GeneratePath(FAStar* astar, int iXstart, int iYstart, int iXdest, int iYdest, bool bCardinalOnly, int iInfo, bool bReuse)
{
	return astar->generatePath({ iXstart, iYstart }, { iXdest, iYdest }, bCardinalOnly, iInfo, bReuse);
}

void MyCvDLLFAStar::Initialize(FAStar* astar, int iColumns, int iRows, bool bWrapX, bool bWrapY,
	FAPointFunc DestValidFunc, FAHeuristic HeuristicFunc, FAStarFunc CostFunc, FAStarFunc ValidFunc,
	FAStarFunc NotifyChildFunc, FAStarFunc NotifyListFunc, void* pData)
{
	*astar = FAStar(iColumns, iRows, bWrapX, bWrapY, DestValidFunc, HeuristicFunc, CostFunc, ValidFunc, NotifyChildFunc, NotifyListFunc,
		pData);
}

void MyCvDLLFAStar::SetData(FAStar* ctx, const void* pData)
{
	ctx->setData(pData);
}

FAStarNode* MyCvDLLFAStar::GetLastNode(FAStar* ctx)
{
	return ctx->getGoalNode();
}

bool MyCvDLLFAStar::IsPathStart(FAStar*, [[maybe_unused]] int iX, [[maybe_unused]] int iY)
{
	abort();
}

bool MyCvDLLFAStar::IsPathDest(FAStar* ctx, int iX, int iY)
{
	return ctx->getGoal() == heck::ivec2{ iX, iY };
}

int MyCvDLLFAStar::GetStartX(FAStar* ctx)
{
	return ctx->getStart().x;
}

int MyCvDLLFAStar::GetStartY(FAStar* ctx)
{
	return ctx->getStart().y;
}

int MyCvDLLFAStar::GetDestX(FAStar*)
{
	abort();
}

int MyCvDLLFAStar::GetDestY(FAStar*)
{
	abort();
}

int MyCvDLLFAStar::GetInfo(FAStar* astar)
{
	return astar->getInfo();
}

void MyCvDLLFAStar::ForceReset(FAStar* astar)
{
	astar->forceReset();
}

bool MyCvDLLFAStar::startGeneratePath(FAStar* astar, int iXstart, int iYstart, int iXdest, int iYdest, bool bCardinalOnly, int iInfo, bool bReuse)
{
	return astar->startGeneratePath({ iXstart, iYstart }, { iXdest, iYdest }, bCardinalOnly, iInfo, bReuse);
}
bool MyCvDLLFAStar::isReset(FAStar* astar, int iXstart, int iYstart, bool bCardinalOnly, int iInfo, bool bReuse)
{
	return astar->isReset({ iXstart, iYstart }, bCardinalOnly, iInfo, bReuse);
}
FAStarNode* MyCvDLLFAStar::getNode(FAStar* astar, int x, int y)
{
	return &(*astar)(x, y);
}
void MyCvDLLFAStar::setPath(FAStar* astar, FAStarNode* node)
{
	astar->setPath(node);
}
void MyCvDLLFAStar::enableVerificationFixes()
{
	FAStar::sEnableVerificationFixes = true;
}
void MyCvDLLFAStar::disableVerificationFixes()
{
	FAStar::sEnableVerificationFixes = false;
}

MyCvDLLFAStar::DeleterFunc MyCvDLLFAStar::getFAStarDeleter() const
{
	return [](FAStar* ptr) { delete ptr; };
}