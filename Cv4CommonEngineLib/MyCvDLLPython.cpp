#include "inc/Cv4CommonEngineLib/MyCvDLLPython.h"
#include "inc/Cv4CommonEngineLib/CvVFS.h"
#include "Common.h"
#include "CommonEngineGlobal.h"

#include <CvGameCoreDLL/CvEventReporter.h>
#include <CvGameCoreDLL/CvGlobals.h>
#include <CvGameCoreDLL/CvInitCore.h>

#include <CommonStuff/StringConversion.h>

#include <pybind11/embed.h>
#include <pybind11/stl.h>
#include <pybind11/iostream.h>

#include <iostream>
#include <ranges>

namespace
{
	struct CvPythonMgr
	{
		bool initialised = false;

		std::optional<pybind11::scoped_interpreter> scopedInterpreter;


		bool usingDefaultImpl = false;
		//int mapSizeOverrideMultiplier = 1;

		std::unordered_map<std::string, pybind11::module, heck::StringHasher, std::equal_to<>> modules;

		uint64_t serial = 0;

		void initialise() try
		{
			++serial;

			// Needed for CvUnVictoryScreen
			// UPDATE: Maybe not anymore.
//#ifdef _WIN32
//			_putenv("PYTHONCASEOK=1");
//#else
//			putenv(std::string("PYTHONCASEOK=1").data());
//#endif
				
			//_putenv("PYTHONIOENCODING=cp1252");
			

			//PyImport_AppendInittab("CvPythonExtensions", &setupCvPythonExtensions);
			scopedInterpreter.emplace(true, 0, nullptr, false);
			
			

			const auto sysModule = pybind11::module_::import("sys");

			auto sysPathsList = sysModule.attr("path").cast<pybind11::list>();
			using PathStr = std::filesystem::path::string_type;
			auto sysPaths = sysPathsList.cast<std::vector<PathStr>>();
			//pybind11::exec("import sys\nsys.path=[]");

			// Using sys.path would have been enough, except that some of BUG's files need an encoding override...
			// https://peps.python.org/pep-0302/
			pybind11::exec(R"(
import sys
import imp
import CvPythonExtensions

class Civ4ModuleImporter:
	def __init__(self):
		self.codeDict = dict()

	def find_module(self, fullname, path):
		#if fullname in sys.builtin_module_names:
		#	return None
		code = CvPythonExtensions.loadImportModuleSourceCode(fullname)
		if len(code) <= 0:
			return None
		# Pass the code over to the loader so that we don't need to read the file again.
		self.codeDict[fullname] = (code, CvPythonExtensions.getLastImportModuleVfsPath())
		return self

	def load_module(self, fullname):
		if fullname in sys.modules:
			return sys.modules[fullname]
		code, vfsPath = self.codeDict[fullname]
		del self.codeDict[fullname]
		mod = imp.new_module(fullname)
		sys.modules[fullname] = mod
		mod.__file__ = vfsPath # "<%s>" % self.__class__.__name__
		mod.__loader__ = self
		mod.__package__ = fullname.rpartition('.')[0]
		exec(compile(code, vfsPath, 'exec'), mod.__dict__)
		return mod

sys.meta_path.append(Civ4ModuleImporter())
)");
			
			initialised = true;

			const auto convertPathToString = std::views::transform([](const std::filesystem::path& p) {
				return p.native();
			});
			
			sysPaths.append_range(cvengine::gVFS->enumeratePhysicalDirsContainingExt(L".pyc") | convertPathToString);
			for (const auto& p : sysPaths)
				std::wclog << L"Python compiled module (pyc) search path: " << std::filesystem::path(p) << std::endl;

			sysModule.attr("path") = pybind11::list(pybind11::cast(sysPaths));

			CvEventReporter::getInstance().init();

			(void)pybind11::module::import("CvAppInterface").attr("init")();
			(void)pybind11::module::import("Cv4MiniEngineEntryPoint").attr("init")();
		}
		catch (const pybind11::error_already_set& ex)
		{
			std::clog << "Python init error: " << ex.what() << std::endl;
			throw;
		}

		std::optional<pybind11::object> tryCall(const char* moduleName, const char* funcName, PyObject* argsTuple)
		{
			auto it = modules.find(moduleName);
			if (it == modules.end())
			{
				try
				{
					it = modules.emplace(moduleName, pybind11::module::import(moduleName)).first;
					//break;
				}
				catch (const pybind11::error_already_set& ex)
				{
					// Failed to load module.
					std::clog << "Python module load error.\n" << ex.what() << std::endl;
					throw;
					//return std::nullopt;

					//CvApp::getInstance().getUI().modalLoopPythonReloadPrompt(L"Failed to load module.", L"Reload Module");
				}
			}

			if (pybind11::hasattr(it->second, funcName))
			{

				try
				{
					usingDefaultImpl = false;
					if (argsTuple)
						return it->second.attr(funcName)(pybind11::reinterpret_steal<pybind11::object>(argsTuple));
					else
						return it->second.attr(funcName)();
				}
				catch (const pybind11::error_already_set& ex)
				{
					// Error in the function somewhere.
					//static std::unordered_set<std::string> messages;
					//if (messages.insert(ex.what()).second)
					std::clog << ex.what() << std::endl;
					throw;
					//CvApp::getInstance().getUI().modalLoopPythonReloadPrompt(L"Failed to evaluate function.", L"Reload All Python");
					//purge();
				}
			}
			else // Function doesn't exist.
			{
				return std::nullopt;
			}
		}

		template<class T>
		bool tryCall(const char* moduleName, const char* funcName, PyObject* argsTuple, T* result)
		{
			try
			{
				if (const auto optResult = tryCall(moduleName, funcName, argsTuple))
				{
					// If result is None, the output arg is not set? I have ensured that all callers of callFunction have an initialised result value (which might not be zero).
					if (!optResult->is_none())
						*result = optResult->cast<T>();
					return true;
				}
				else
					return false;
			}
			catch (const pybind11::cast_error& ex)
			{
				// Result cast error, probably.
				std::clog << ex.what() << std::endl;
				throw;
				//return std::nullopt;

				//CvApp::getInstance().getUI().modalLoopPythonReloadPrompt(L"Failed to cast function return value.", L"Reload All Python");
				//purge();
			}
		}

		void purge()
		{
			std::clog << "!!! RESTARTING PYTHON INTERPRETER !!!" << std::endl;
			modules.clear();
			scopedInterpreter.reset();
			initialised = false;
			initialise();
		}
	};

	CvPythonMgr& getPythonManager()
	{
		static CvPythonMgr x;
		return x;
	}
}

void MyCvDLLPython::initialisePython()
{
	getPythonManager().initialise();
}

void MyCvDLLPython::shutdownPython()
{
	getPythonManager().scopedInterpreter.reset();
}

bool MyCvDLLPython::isInitialized()
{
	return getPythonManager().initialised;
}

const char* MyCvDLLPython::getMapScriptModule()
{
	static std::string temp;
	temp = heck::convertWideToUtf8(gGlobals.getInitCore().getMapScriptName());
	return temp.c_str();
}

PyObject* MyCvDLLPython::MakeFunctionArgs(PyObject** args, int argc)
{
	pybind11::tuple t(argc);
	for (int i = 0; i < argc; ++i)
	{
		t[i] = args[i];
		Py_DecRef(args[i]);
	}
	return t.release().ptr();
}

bool MyCvDLLPython::moduleExists([[maybe_unused]] const char* moduleName, [[maybe_unused]] bool bLoadIfNecessary)
{
	std::abort();
}

bool MyCvDLLPython::callFunction(const char* moduleName, const char* fxnName, PyObject* fxnArg)
{
	return getPythonManager().tryCall(moduleName, fxnName, fxnArg).has_value();
}

bool MyCvDLLPython::callFunction(const char* moduleName, const char* fxnName, PyObject* fxnArg, long* result)
{
	return getPythonManager().tryCall(moduleName, fxnName, fxnArg, result);
}

bool MyCvDLLPython::callFunction(const char* moduleName, const char* fxnName, PyObject* fxnArg, CvString* result)
{
	return getPythonManager().tryCall(moduleName, fxnName, fxnArg, static_cast<std::string*>(result));
}

bool MyCvDLLPython::callFunction(const char* moduleName, const char* fxnName, PyObject* fxnArg, CvWString* result)
{
	return getPythonManager().tryCall(moduleName, fxnName, fxnArg, static_cast<std::wstring*>(result));
}

bool MyCvDLLPython::callFunction(const char* moduleName, const char* fxnName, PyObject* fxnArg, std::vector<uint8_t>* result)
{
	return getPythonManager().tryCall(moduleName, fxnName, fxnArg, result);
}

bool MyCvDLLPython::callFunction(const char* moduleName, const char* fxnName, PyObject* fxnArg, std::vector<int>* result)
{
	auto& mgr = getPythonManager();
	return mgr.tryCall(moduleName, fxnName, fxnArg, result);
}

bool MyCvDLLPython::callFunction([[maybe_unused]] const char* moduleName, [[maybe_unused]] const char* fxnName, [[maybe_unused]] PyObject* fxnArg, [[maybe_unused]] int* pIntList, [[maybe_unused]] int* iListSize)
{
	std::abort();
}

bool MyCvDLLPython::callFunction(const char* moduleName, const char* fxnName, PyObject* fxnArg, std::vector<float>* result)
{
	return getPythonManager().tryCall(moduleName, fxnName, fxnArg, result);
}

bool MyCvDLLPython::callPythonFunction(const char* szModName, const char* szFxnName, int iArg, long* result)
{
	return callFunction(szModName, szFxnName, MakeFunctionArgs(std::array{ pybind11::int_(iArg).ptr() }.data(), 1), result);
}

uint64_t MyCvDLLPython::getCurrentPythonEnvironmentSerial() const noexcept
{
	return getPythonManager().serial;
}

void MyCvDLLPython::restart()
{
	return getPythonManager().purge();
}

//void MyCvDLLPython::setMapSizeOverrideMultiplier(int k)
//{
//	getPythonManager().mapSizeOverrideMultiplier = k;
//}

bool MyCvDLLPython::pythonUsingDefaultImpl()
{
	return getPythonManager().usingDefaultImpl;
}

void MyCvDLLPython::setUsingDefaultImpl()
{
	getPythonManager().usingDefaultImpl = true;
}

#ifdef _WIN32
//#ifdef _DEBUG
// Call this from immediate window.
// https://stackoverflow.com/questions/1796510/accessing-a-python-traceback-from-the-c-api
__declspec(dllexport) void dumpPythonStackTrace()
{
	PyThreadState* tstate = PyThreadState_GET();
	if (NULL != tstate && NULL != tstate->frame) {
		PyFrameObject* frame = tstate->frame;

		printf("Python stack trace:\n");
		while (NULL != frame) {
			// int line = frame->f_lineno;
			/*
			 frame->f_lineno will not always return the correct line number
			 you need to call PyCode_Addr2Line().
			*/
			int line = PyCode_Addr2Line(frame->f_code, frame->f_lasti);
			const char* filename = PyString_AsString(frame->f_code->co_filename);
			const char* funcname = PyString_AsString(frame->f_code->co_name);
			printf("    %s(%d): %s\n", filename, line, funcname);
			frame = frame->f_back;
		}
	}
}
//#endif
#endif