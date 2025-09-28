#pragma once

#include <CvDLLFAStarIFaceBase.h>

class MyCvDLLFAStar : public CvDLLFAStarIFaceBase
{
public:

	// Inherited via CvDLLFAStarIFaceBase
	virtual FAStar* create() override;
	virtual void destroy(FAStar*& ptr, bool bSafeDelete) override;
	virtual bool GeneratePath(FAStar*, int iXstart, int iYstart, int iXdest, int iYdest, bool bCardinalOnly, int iInfo, bool bReuse) override;
	virtual void Initialize(FAStar*, int iColumns, int iRows, bool bWrapX, bool bWrapY, FAPointFunc DestValidFunc, FAHeuristic HeuristicFunc, FAStarFunc CostFunc, FAStarFunc ValidFunc, FAStarFunc NotifyChildFunc, FAStarFunc NotifyListFunc, void* pData) override;
	virtual void SetData(FAStar*, const void* pData) override;
	virtual FAStarNode* GetLastNode(FAStar*) override;
	virtual bool IsPathStart(FAStar*, int iX, int iY) override;
	virtual bool IsPathDest(FAStar*, int iX, int iY) override;
	virtual int GetStartX(FAStar*) override;
	virtual int GetStartY(FAStar*) override;
	virtual int GetDestX(FAStar*) override;
	virtual int GetDestY(FAStar*) override;
	virtual int GetInfo(FAStar*) override;
	virtual void ForceReset(FAStar*) override;

	virtual bool startGeneratePath(FAStar*, int iXstart, int iYstart, int iXdest, int iYdest, bool bCardinalOnly, int iInfo, bool bReuse) override;
	virtual bool isReset(FAStar*, int iXstart, int iYstart, bool bCardinalOnly, int iInfo, bool bReuse) override;
	virtual FAStarNode* getNode(FAStar*, int x, int y) override;
	virtual void setPath(FAStar*, FAStarNode*) override;
	virtual void enableVerificationFixes() override;
	virtual void disableVerificationFixes() override;

	virtual DeleterFunc getFAStarDeleter() const override;
};