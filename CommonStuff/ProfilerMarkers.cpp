#include "inc/CommonStuff/ProfilerMarkers.h"

#ifdef _WIN32
#define DIAGHUB_ENABLE_TRACE_SYSTEM
#include "UserMarks.h"
DIAGHUB_DECLARE_TRACE;

// <https://learn.microsoft.com/en-us/visualstudio/profiling/add-timeline-graph-user-marks?view=vs-2022>
static const bool kInit = [] {
	// Start the trace system
	DIAGHUB_START_TRACE_SYSTEM();

	// Initialize user marks
	USERMARKS_INITIALIZE(L"User mark name");

	// Initialize user mark range
	USERMARKRANGE_INITIALIZE(L"Range name");

	return true;
	}();

// Stop the trace system
//DIAGHUB_STOP_TRACE_SYSTEM();

// Emit events
// USERMARKS_EMIT(L"Message to emit with user mark");
// USERMARKRANGE_START(L"Message to emit with range");
// USERMARKRANGE_END();

void heck::emitProfilerMarker(const wchar_t* label)
{
	USERMARKS_EMIT(label);
}
#else
void heck::emitProfilerMarker(const wchar_t*)
{
}
#endif