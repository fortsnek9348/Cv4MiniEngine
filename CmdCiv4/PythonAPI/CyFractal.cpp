#include "CyFractal.h"

#include <CvFractal.h>
#include <CvRandom.h>

#include <pybind11/operators.h>
#include <pybind11/stl.h>

template<class T>
static auto cyenum(const pybind11::object& m, const char* name)
{
	pybind11::enum_<T> e(m, name, pybind11::arithmetic());
	e.def(pybind11::self + int());
	e.def(int() + pybind11::self);
	pybind11::implicitly_convertible<int, T>();
	return e;
}

void CyFractal::registerWithPython(const pybind11::module& m)
{
#define R(x) def(#x, &CyFractal::x)
#define E(x) value(#x, CvFractal::x)

	pybind11::class_<CyFractal> klass(m, "CyFractal");
	klass
		.def(pybind11::init())
		.R(fracInit)
		.R(fracInitHints)
		.R(fracInitRifts)
		.R(getHeight)
		.R(getHeightFromPercent)
		;

	auto fracVals = cyenum<CvFractal::FracVals>(klass, "FracVals");
	// These are not enum values.
	fracVals.attr("DEFAULT_FRAC_X_EXP") = pybind11::cast(std::to_underlying(CvFractal::DEFAULT_FRAC_X_EXP));
	fracVals.attr("DEFAULT_FRAC_Y_EXP") = pybind11::cast(std::to_underlying(CvFractal::DEFAULT_FRAC_Y_EXP));
	fracVals
		.E(FRAC_WRAP_X)
		.E(FRAC_WRAP_Y)
		.E(FRAC_PERCENT)
		.E(FRAC_POLAR)
		.E(FRAC_CENTER_RIFT)
		.E(FRAC_INVERT_HEIGHTS)
		//.E(DEFAULT_FRAC_X_EXP)
	   // .E(DEFAULT_FRAC_Y_EXP)

		;
}

CyFractal::CyFractal() : mFractal(std::make_unique<CvFractal>())
{
}

void CyFractal::fracInit(int iNewXs, int iNewYs, int iGrain, CvRandom& random, int iFlags, int iFracXExp, int iFracYExp)
{
	mFractal->fracInit(iNewXs, iNewYs, iGrain, random, iFlags, nullptr, iFracXExp, iFracYExp);
}

void CyFractal::fracInitHints(int iNewXs, int iNewYs, int iGrain, CvRandom& random, int iFlags, const std::vector<uint8_t>& hints, int iFracXExp, int iFracYExp)
{
	mFractal->fracInitHinted(iNewXs, iNewYs, iGrain, random, hints.data(), (int)hints.size(), iFlags, nullptr, iFracXExp, iFracYExp);
}

void CyFractal::fracInitRifts(int iNewXs, int iNewYs, int iGrain, CvRandom& random, int iFlags, const CyFractal& rifts, int iFracXExp, int iFracYExp)
{
	mFractal->fracInit(iNewXs, iNewYs, iGrain, random, iFlags, rifts.mFractal.get(), iFracXExp, iFracYExp);
}

int CyFractal::getHeight(int x, int y)
{
	return mFractal->getHeight(x, y);
}

int CyFractal::getHeightFromPercent(int iPercent)
{
	return mFractal->getHeightFromPercent(iPercent);
}
