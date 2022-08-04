#include <string>
#include <sstream>
#include <list>
#include <set>
#include <map>
#include <cmath>
#include <queue>

#include "main.h"

static CRITICAL_SECTION thread_destruction_callbacks_CS;
static std::set< void (*)() > thread_destruction_callbacks;
void register_thread_destruction_callback(void (*callback)()) {
	EnterCriticalSection(&thread_destruction_callbacks_CS);
	thread_destruction_callbacks.insert(callback);
	LeaveCriticalSection(&thread_destruction_callbacks_CS);
}

extern "C" {
	bool __stdcall DllMain(
			void *  hDllHandle,
			unsigned long   dwReason,
			void *  lpreserved
	)
	{
		if (dwReason == DLL_PROCESS_ATTACH) {
			InitializeCriticalSectionAndSpinCount(&thread_destruction_callbacks_CS, 4000);
		}
		//else if (dwReason == DLL_THREAD_ATTACH) {
			//EnterCriticalSection(&thread_destruction_callbacks_CS);
			//LeaveCriticalSection(&thread_destruction_callbacks_CS);
		//}
		else if (dwReason == DLL_THREAD_DETACH) {
			//message("DLL_THREAD_DETACH");
			EnterCriticalSection(&thread_destruction_callbacks_CS);
			std::set< void(*)() >::iterator it;
			for (it = thread_destruction_callbacks.begin(); it != thread_destruction_callbacks.end(); it++) {
				(*it)();
			}
			LeaveCriticalSection(&thread_destruction_callbacks_CS);
		}
		return true;
	}
}

