#pragma once

#include <CvGameCoreDLL/CvFractal.h>

#include <pybind11/pybind11.h>

#include <vector>
#include <span>

class CvRandom;

class CyFractal
{
public:
	static void registerWithPython(const pybind11::module& m);

	CyFractal();

	void fracInit(int iNewXs, int iNewYs, int iGrain, CvRandom& random, int iFlags, int iFracXExp, int iFracYExp);
	void fracInitHints(int iNewXs, int iNewYs, int iGrain, CvRandom& random, int iFlags, const std::vector<uint8_t>& hints, int iFracXExp, int iFracYExp);
	void fracInitRifts(int iNewXs, int iNewYs, int iGrain, CvRandom& random, int iFlags, const CyFractal& rifts, int iFracXExp, int iFracYExp);
	int getHeight(int x, int y);
	int getHeightFromPercent(int iPercent);

private:
	std::unique_ptr<CvFractal> mFractal;
};

