#include <Cv4CommonEngineLib/CvPythonExtensionsSetup.h>

#include <pybind11/embed.h>

// This has to be done here and not in the common lib so that the linker doesn't throw away the object file.
PYBIND11_EMBEDDED_MODULE(CvPythonExtensions, m)
{
	cvengine::setupCvPythonExtensionsModule(m);
}