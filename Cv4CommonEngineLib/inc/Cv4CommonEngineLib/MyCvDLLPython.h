#pragma once

#include <CvDLLPythonIFaceBase.h>

class MyCvDLLPython : public CvDLLPythonIFaceBase
{
public:

	static void initialisePython();
	static void shutdownPython();


	// Inherited via CvDLLPythonIFaceBase
	virtual bool isInitialized() override;
	virtual const char* getMapScriptModule() override;
	virtual PyObject* MakeFunctionArgs(PyObject** args, int argc) override;
	virtual bool moduleExists(const char* moduleName, bool bLoadIfNecessary) override;
	virtual bool callFunction(const char* moduleName, const char* fxnName, PyObject* fxnArg) override;
	virtual bool callFunction(const char* moduleName, const char* fxnName, PyObject* fxnArg, long* result) override;
	virtual bool callFunction(const char* moduleName, const char* fxnName, PyObject* fxnArg, CvString* result) override;
	virtual bool callFunction(const char* moduleName, const char* fxnName, PyObject* fxnArg, CvWString* result) override;
	virtual bool callFunction(const char* moduleName, const char* fxnName, PyObject* fxnArg, std::vector<uint8_t>* pList) override;
	virtual bool callFunction(const char* moduleName, const char* fxnName, PyObject* fxnArg, std::vector<int>* pIntList) override;
	virtual bool callFunction(const char* moduleName, const char* fxnName, PyObject* fxnArg, int* pIntList, int* iListSize) override;
	virtual bool callFunction(const char* moduleName, const char* fxnName, PyObject* fxnArg, std::vector<float>* pFloatList) override;
	virtual bool callPythonFunction(const char* szModName, const char* szFxnName, int iArg, long* result) override;
	virtual bool pythonUsingDefaultImpl() override;


	// Incremented every time python is restarted.
	uint64_t getCurrentPythonEnvironmentSerial() const noexcept;

	void restart();

	// No more. We'll do it through CvInitCore now so that startingPlotRange can use the multiplier.
	//void setMapSizeOverrideMultiplier(int k);

	static void setUsingDefaultImpl();
};
