//#include "Commands_Inventory.h"
//#include "GameObjects.h"
//#include "GameExtraData.h"
//#include "GameAPI.h"
//#include "ParamInfos.h"
//#include "Hooks_Gameplay.h"
#include <string>
#include <sstream>
#include <list>
#include <set>
#include <map>
#include <cmath>
#include <queue>
#include <ctime>

#include "main.h"
#include <mmsystem.h>

#include "game_time.h"


//#include <shlobj.h>
//#include <shlwapi.h>
//#pragma comment (lib, "shlwapi.lib")
//#pragma comment (lib, "winmm.lib")

//#include <signal.h>

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#if defined OBLIVION
	#define RELATIVE_INI_FILE_PATH "Data\\obse\\plugins\\"
	#define PLUGIN_NAME   "sr_Oblivion_Stutter_Remover"
	#define LOG_FILE_NAME PLUGIN_NAME ## ".log"
	#define INI_FILE_NAME PLUGIN_NAME ## ".ini"
	#define EXE_FILE_NAME "Oblivion.exe";
#elif defined FALLOUT
	#define RELATIVE_INI_FILE_PATH "Data\\fose\\plugins\\"
	#define PLUGIN_NAME "sr_Fallout_Stutter_Remover"
	#define LOG_FILE_NAME PLUGIN_NAME ## ".log"
	#define INI_FILE_NAME PLUGIN_NAME ## ".ini"
	#define EXE_FILE_NAME "Fallout3.exe";
#elif defined NEW_VEGAS
	#define RELATIVE_INI_FILE_PATH "Data\\nvse\\plugins\\"
	#define PLUGIN_NAME "sr_New_Vegas_Stutter_Remover"
	#define LOG_FILE_NAME PLUGIN_NAME ## ".log"
	#define INI_FILE_NAME PLUGIN_NAME ## ".ini"
	#define EXE_FILE_NAME "FalloutNV.exe";
#else
	#error Oblivion or Fallout 3 or Fallout New Vegas?
#endif
#if defined USE_SCRIPT_EXTENDER
	#if defined OBLIVION
		#include "obse/GameForms.h"
		#include "obse/GameMenus.h"
		#include "obse/Hooks_DirectInput8Create.h"
		#include "obse/PluginAPI.h"
		#include "obse/CommandTable.h"
		#include "obse/GameAPI.h"
		#include "obse/ParamInfos.h"
		#include "obse_common/SafeWrite.h"
		#include "obse_common/SafeWrite.cpp"//it's not linked in with the common stuff anymore for some reason
		OBSEConsoleInterface *g_con = NULL;
	#elif defined FALLOUT
		//#include "GameForms.h"
		//#include "GameMenus.h"
		//#include "Hooks_DirectInput8Create.h"
		#include "fose/PluginAPI.h"
		#include "fose/CommandTable.h"
		#include "fose/GameAPI.h"
		#include "fose/ParamInfos.h"
		#include "fose_common/SafeWrite.h"
		#include "fose_common/SafeWrite.cpp"//it's not linked in with the common stuff anymore for some reason
		FOSEConsoleInterface *g_con = NULL;
		namespace Settings {namespace Master {const bool bLogToConsole = false;}}
		void Console_Print(const char * fmt, ...)
		{
			ConsoleManager * mgr = (ConsoleManager*) ConsoleManager_GetSingleton(false);
			if (get_time_ms() < 15000) return;
			if(mgr)
			{
				va_list	args;

				va_start(args, fmt);

				CALL_MEMBER_FN(mgr, Print)(fmt, args);

				va_end(args);
			}
		}
	#elif defined NEW_VEGAS
		#define RUNTIME_VERSION 0x040020D0 //bad?  the headers barf absent something like this
		//#include "nvse/GameForms.h"
		//#include "nvse/GameMenus.h"
		//#include "nvse/Hooks_DirectInput8Create.h"
		#include "nvse/PluginAPI.h"
		#include "nvse/CommandTable.h"
		//#include "nvse/GameAPI.h"
		#include "nvse/ParamInfos.h"
		#include "nvse/SafeWrite.h"
		#include "nvse/SafeWrite.cpp"//it's not linked in with the common stuff anymore for some reason
		//from GameAPI.cpp, tweaked slightly
		typedef void * (*_GetSingleton)(bool canCreateNew);
		const _GetSingleton ConsoleManager_GetSingleton = (_GetSingleton)0x0071B160;
		ConsoleManager * ConsoleManager::GetSingleton(void)
		{
			if (Hook_target != SR_MAKE_VERSION(2, 1, 4, 0, 525)) return NULL;
			return (ConsoleManager *)ConsoleManager_GetSingleton(true);
		}
		void Console_Print(const char * fmt, ...)
		{
			ConsoleManager	* mgr = ConsoleManager::GetSingleton();
			if(mgr)
			{
				va_list	args;

				va_start(args, fmt);

				CALL_MEMBER_FN(mgr, Print)(fmt, args);

				va_end(args);
			}
		}
		NVSEConsoleInterface *g_con = NULL;
	#else
		#error Oblivion or Fallout?
	#endif
	PluginHandle g_pluginHandle = kPluginHandle_Invalid;
	IDebugLog g_log(LOG_FILE_NAME);
#else
	class DummyConsoleInterface {
	public:
		void RunScriptLine(const char *) {}
	} *g_con = NULL;
	class DummyDebugLog {
		FILE *f;
		bool autoflush;
	public:
		DummyDebugLog(const char *filename) {
			f = fopen(filename, "wt");
			autoflush = true;
		}
		void SetAutoFlush(bool _v) {autoflush = _v;}
		void Message(const char *msg) {
			fwrite(msg, 1, strlen(msg), f);
			const char *endline = "\n";
			fwrite(endline, 1, strlen(endline), f);
			if (autoflush) fflush(f);
		}
	} g_log(LOG_FILE_NAME);
	void Console_Print(const char *) {}
#endif

#if defined NEW_VEGAS
	void init_FO3_1_7_0_3();
	void init_FNV_1_0_0_240();
	void init_FNV_1_1_1_271();
	void init_FNV_1_2_0_285();
	void init_FNV_1_2_0_314();
	void init_FNV_1_2_0_352();
	void init_FNV_1_3_0_452();
	void init_FNV_1_4_0_525();
#endif


#include "locks.h"
#include "memory.h"
#include "mem_bench.h"
#include "hashtable.h"
#include "struct_text.h"
#include "explore.h"
#include "version.h"

//bool system_is_multiprocessor = false;

DWORD main_thread_ID = 0;
static UInt32 base_time_ms;//timeGetTime at startup
static UInt64 base_time_ticks;//RDTSC at startup
UInt32 get_time_ms() {return timeGetTime() - base_time_ms;}
static UInt64 __declspec(naked) _get_time_ticks() {
	__asm {
		RDTSC
		ret
	}
}
UInt64 get_time_ticks() {
	return _get_time_ticks() - base_time_ticks;
}
double get_tick_period() {
	//fails after 49 days or so
	while (1) {
		UInt64 dr = get_time_ticks();
		UInt32 dt = get_time_ms();
		if (dt < 10) {Sleep(10 - dt); continue;}
		//if (dt < 100) return (dt * 0.001 + 1) / double(dr + 3000000000.);
		return dt * 0.001 / double(dr);
	}
}
void init_time() {
	base_time_ticks = _get_time_ticks();
	base_time_ms = timeGetTime();
}


#include "settings.h"

bool is_address_static(void* addr) {
#if defined OBLIVION
	UInt32 segmentlist[] = {
		0x00401000, 0x00A27E00,//code?
		0x00A28000, 0x00A283D0,//linkage?
		0x00A283D0, 0x00B01A00,//???
		0x00B02000, 0x00BABC1C,//vtables, strings, constants?
		0x00BAC000, 0x00BAC200 //???
	};
	for (int i = 0; i < sizeof(segmentlist) / sizeof(UInt32); i+=2) {
		if (UInt32(addr) >= segmentlist[i] && UInt32(addr) < segmentlist[i+1]) return true;
	}
	return false;
#elif defined FALLOUT
	if (addr < (void*)0x00401000) return false;
	if (addr > (void*)0x0117D200) return false;
	if (addr > (void*)0x00D9A600 && addr < (void*)0x00D9B000) return false;
	if (addr > (void*)0x00F45800 && addr < (void*)0x00F46000) return false;
	if (addr > (void*)0x0117B79C && addr < (void*)0x0117C000) return false;
	if (addr > (void*)0x0117C400 && addr < (void*)0x0117D000) return false;
	return true;
#elif defined NEW_VEGAS
	if (addr < (void*)0x001273200) return true;//only aproximate
	return false;
#endif
}


class HandleTimeResolution {
	int resolution;
public:
	HandleTimeResolution() : resolution(0) {}
	void reset() {
		if (resolution) {
			timeEndPeriod(resolution);
			message("timeEndPeriod: %d -> 0", resolution);
		}
		resolution = 0;
	}
	void set_resolution(int res) {
		reset();
		if (res <= 0) return;
		timeBeginPeriod(res);
		message("timeBeginPeriod: %d -> %d", resolution, res);
		resolution = res;
	}
	~HandleTimeResolution() {reset();}
};
static HandleTimeResolution HandleTimeResolution_instance;
static bool ConsoleReady() {
#ifdef OBLIVION
	return (*(void**)0xB3A6FC) != NULL;
#elif defined FALLOUT
	return ConsoleManager_GetSingleton(false) != NULL;
#elif defined NEW_VEGAS
	return ConsoleManager_GetSingleton(false) != NULL;
#endif
}
static bool isGameMode() {
//	InterfaceManager *im = InterfaceManager::GetSingleton();
//	return im->IsGameMode();
//	return !(im && im->cursor && ((char)(im->unk008)));
//	return !((bool (*)())0x578f60)();
#if defined OBLIVION
	return !Hook_IsMenuModeFunc();
#elif defined FALLOUT
	return true;
#elif defined NEW_VEGAS
	return true;
#endif
}
static int GetMenuMode() {
#if defined OBLIVION
	InterfaceManager* intfc = InterfaceManager::GetSingleton();
	if (intfc && intfc->activeMenu)
		return intfc->activeMenu->id;
	return 0;
#elif defined FALLOUT
	return 0;
#elif defined NEW_VEGAS
	return 0;
#endif
}
void FastExit(void)
{
#if defined NEW_VEGAS
	if (g_con) g_con->RunScriptLine("con_SaveINI", NULL);
#else
	if (g_con) g_con->RunScriptLine("con_SaveINI");
#endif
	HandleTimeResolution_instance.reset();
	g_log.SetAutoFlush(true);
	g_log.Message("exiting via Stutter Remover FastExit");
	g_log.Message(make_string("... at time %.2fs", get_time_ms() * 0.001).c_str());
	TerminateProcess(GetCurrentProcess(),0);
}
CRITICAL_SECTION message_buffer_cs;
std::vector<std::string> message_buffer;
enum {MAX_MESSAGE_COUNT = 100};
void my_console_print ( const char *msg ) {
	::EnterCriticalSection(&message_buffer_cs);
	if (message_buffer.size() < MAX_MESSAGE_COUNT) message_buffer.push_back(std::string(msg));
	::LeaveCriticalSection(&message_buffer_cs);
}
void flush_message_buffer() {
	std::vector<std::string> message_buffer2;
	::EnterCriticalSection(&message_buffer_cs);
	message_buffer2.swap(message_buffer);
	::LeaveCriticalSection(&message_buffer_cs);
	if (message_buffer2.size() != 0) {
		if (Settings::Master::bLogToConsole && ConsoleReady()) {
			for (int i = 0; i < message_buffer2.size(); i++)
				Console_Print(message_buffer2[i].c_str());
		}
		message_buffer2.clear();
	}
}
void message(const char *format, ...) {
	char buffy[4096];
	static bool truncated = false;

	va_list those_dots;
	va_start(those_dots, format);
	_vsnprintf_s(buffy, 4000, format, those_dots);
	//vsprintf(error_string, format, those_dots);
	va_end(those_dots);

	::EnterCriticalSection(&message_buffer_cs);
	g_log.Message(buffy);
//	if (message_buffer.size() < MAX_MESSAGE_COUNT) g_log.Message(buffy);
//	else if (!truncated) {truncated = true; g_log.Message("some messages failed to print in this log file");}
	if (Settings::Master::bLogToConsole && message_buffer.size() < MAX_MESSAGE_COUNT) my_console_print(buffy);
	::LeaveCriticalSection(&message_buffer_cs);
}
std::string make_string( const char *format, ... ) {
	char buffy[8192];

	va_list those_dots;
	va_start(those_dots, format);
	_vsnprintf_s(buffy, 8190, format, those_dots);
	//vsprintf(error_string, format, those_dots);
	va_end(those_dots);

	return std::string(buffy);
}
void error( const char *format, ... ) {
	char buffy[4096];

	va_list those_dots;
	va_start(those_dots, format);
	_vsnprintf_s(buffy, 4000, format, those_dots);
	//vsprintf(error_string, format, those_dots);
	va_end(those_dots);

	g_log.SetAutoFlush(true);
	g_log.Message("ERROR:");
	g_log.Message(buffy);
	TerminateProcess(GetCurrentProcess(), 0);
}
void message_console(const char *format, ...) {
	char buffy[4096];

	va_list those_dots;
	va_start(those_dots, format);
	_vsnprintf_s(buffy, 4000, format, those_dots);
	//vsprintf(error_string, format, those_dots);
	va_end(those_dots);

	my_console_print(buffy);
}
void message_log(const char *format, ...) {
	char buffy[4096];

	va_list those_dots;
	va_start(those_dots, format);
	_vsnprintf_s(buffy, 4000, format, those_dots);
	//vsprintf(error_string, format, those_dots);
	va_end(those_dots);

	g_log.Message(buffy);
}

UInt32 hash_murmur2(const void * key, unsigned int len)
//  http://sites.google.com/site/murmurhash/
{
#define mmix(h,k) { k *= m; k ^= k >> r; k *= m; h *= m; h ^= k; }
	UInt32 seed = 0;//
	const UInt32 m = 0x5bd1e995;
	const UInt32 r = 24;
	UInt32 l = len;

	const UInt8 * data = (const UInt8 *)key;

	UInt32 h = seed;

	while(len >= 4)
	{
		UInt32 k = *(UInt32*)data;

		mmix(h,k);

		data += 4;
		len -= 4;
	}

	UInt32 t = 0;

	switch(len)
	{
	case 3: t ^= data[2] << 16;
	case 2: t ^= data[1] << 8;
	case 1: t ^= data[0];
	};

	mmix(h,t);
	mmix(h,l);

	h ^= h >> 13;
	h *= m;
	h ^= h >> 15;

#undef mmix
	return h;
}
static UInt32 get_exe_hash(const char * fname)
{
	HANDLE h = CreateFile(fname, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(h == INVALID_HANDLE_VALUE) {
		error("Couldn't find %s, make sure you're running this from the same folder as the executable.", fname);
	}
	__int64 length64; GetFileSizeEx(h, (LARGE_INTEGER*)&length64);
	if (length64 == 0) error("File is zero size (%s)", fname);
	if (length64 < 0 || length64 > (1 << 28)) error("File is too large (%s)", fname);
	UInt32 length = UInt32(length64);

	std::vector<UInt8> buf; buf.resize(length);
	UInt32 bytes_read;
	ReadFile(h, &buf[0], DWORD(length), &bytes_read, NULL);
	if (bytes_read != length) error("failed to read entire file (%d / %d)", bytes_read, UInt32(length));

	// *really* clear 2GB+ address-aware flag
	if(length > 0x40)
	{
		UInt32	headerOffset = *((UInt32 *)(&buf[0] + 0x3C)) + 4;	// +4 to skip 'PE\0\0'
		UInt32	flagsOffset = headerOffset + 0x12;
		UInt32	checksumOffset = headerOffset + 0x14 + 0x40;	// +14 to skip COFF header

		if(length > checksumOffset + 2)
		{
			UInt16	* flagsPtr = (UInt16 *)(&buf[0] + flagsOffset);
			UInt32	* checksumPtr = (UInt32 *)(&buf[0] + checksumOffset);
			if (*flagsPtr & 0x0020) message("the LAA flag was enabled in this executable");
			*flagsPtr &= ~0x0020;
			*checksumPtr = 0;
		}
	}
	return hash_murmur2(&buf[0], length);
}

void initialize3() {//called when the game reaches the main menu
	if (Settings::Master::bFlushLog) g_log.SetAutoFlush(true);
	else g_log.SetAutoFlush(false);
	DWORD threadID = GetCurrentThreadId();
	message("initialize3() running in thread %x, renderer at %08X", threadID, Hook_NiDX9RendererPtr ? *Hook_NiDX9RendererPtr : 0x0);
	if (Settings::Master::bExperimentalStuff && Settings::Experimental::bBenchmarkHeap) {// && !Settings::Master::bReplaceHeap) 
		if (Settings::Master::bReplaceHeap)
			benchmark_heap(&heap);
		else benchmark_heap(&heap_Oblivion);		
	}
}

static bool console_ready = false;
//static DWORD _oblivion_time;
void manage_main_hook_basics() {
	//if (Settings::Master::bExperimentalStuff && Settings::Experimental::bAlternate64HertzFix) {
	//	_oblivion_time = timeGetTime();
	//}
	update_GetTickCount();
	if (!console_ready) {
		if (ConsoleReady()) {
			console_ready = true;
			initialize3();
		}
		//if (Settings::Master::bExperimentalStuff && Settings::Experimental::bAlternate64HertzFix && !Settings::Master::iMainHookPoint) {
		//	_oblivion_time = ::timeGetTime();
		//}
	}
	else flush_message_buffer();
}
//return value = subjective time delta

#define MFPS_TIMEFUNC() get_time_ms()
void manage_framerate( ) {
	using namespace Settings::FPS_Management;

	struct FrameTimeElement {
		int count;
		double best_time;
		double worst_time;
		double total_time;
		double total_necessary_time;
		void zero() {count = 0; best_time = 99000; worst_time = 0; total_time = 0; total_necessary_time = 0;}
	};
	static FrameTimeElement FPS_data;

	static double sleep_accumulation = 0;
	static double last_frame_time;//actual_time2 of the prior frame
	static double min_delta;
	static double max_delta;
	static bool init = false;
	if (!init) {
		init = true;
		min_delta = (Settings::Master::bManageFPS && fMaximumFPS > 0) ? 1000.0 / fMaximumFPS : 0;
		max_delta = (Settings::Master::bManageFPS && fMinimumFPS > 0) ? 1000.0 / fMinimumFPS : 9999;
		if (max_delta > 160) max_delta = 160.0;
		min_delta -= 0.7;
		if (min_delta < 4) min_delta = 4.0;
		last_frame_time = MFPS_TIMEFUNC() - min_delta;
		FPS_data.zero();
	}

	double time1 = MFPS_TIMEFUNC();
	double necessary_delta = time1 - last_frame_time;

	//static double total_sleep_overshoot = 0;
	//static double total_sleep_calls = 0;
	static double expected_sleep_overshoot = 0;
	double time2 = time1;
	if (min_delta && necessary_delta < min_delta) {
		double target_time = last_frame_time + min_delta;
		double time_to_waste = target_time - time1;
		if (time_to_waste > expected_sleep_overshoot * 0.25) {
			UInt32 sleep_amount = UInt32(time_to_waste - expected_sleep_overshoot * 1.1 + 0.5);
			::Sleep(sleep_amount);
			time2 = MFPS_TIMEFUNC();
			UInt32 slept = UInt32(time2 - time1 + 0.5);
			sleep_accumulation -= slept;
			if (slept > sleep_amount) expected_sleep_overshoot = expected_sleep_overshoot * 0.95 + 0.05 * (slept - sleep_amount);
			else expected_sleep_overshoot = expected_sleep_overshoot * 0.95;
		}
		while (time2 < target_time) time2 = MFPS_TIMEFUNC();
	}
	double time_wasted = time2 - time1;

	sleep_accumulation += fExtraSleepPercent * 0.01 * (necessary_delta + time_wasted);
	if (sleep_accumulation < -1) sleep_accumulation = -1;
	else if (sleep_accumulation > 1) {
		::Sleep(1);
		sleep_accumulation -= 1;
		time2 = MFPS_TIMEFUNC();
		time_wasted = time2 - time1;
	}

	double actual_delta = time2 - last_frame_time;
	double logical_delta = actual_delta;
	if (logical_delta > max_delta) logical_delta = max_delta;


	if (necessary_delta < FPS_data.best_time) FPS_data.best_time = necessary_delta;
	if (necessary_delta > FPS_data.worst_time) FPS_data.worst_time = necessary_delta;
	FPS_data.count++;
	FPS_data.total_necessary_time += necessary_delta;
	FPS_data.total_time += actual_delta;
	if (FPS_data.total_time > iFPS_Report_Period && iFPS_Report_Period > 0) {
		message("FPS time:%.1fs-%.1fs,FPS:%4.1f, mean:%4.1f/%4.1fms, min-max:%4.1f-%3.0fms, now:%3.0fms  ", 
			time2 * 0.001, (time2 - FPS_data.total_time) * 0.001, 
			FPS_data.count / FPS_data.total_time * 1000, 
			FPS_data.total_necessary_time / FPS_data.count, 
			FPS_data.count > 1 ? (FPS_data.total_necessary_time - FPS_data.worst_time)/ (FPS_data.count - 1) : -1, 
			FPS_data.best_time, FPS_data.worst_time, necessary_delta
		);
		FPS_data.zero();
	}

	last_frame_time = time2;
	if (bInject_iFPSClamp && Hook_FixedTimePerFrame_iFPSClamp) *Hook_FixedTimePerFrame_iFPSClamp = logical_delta;
}
#undef MFPS_TIMEFUNC

static UInt32 main_hook_wrapped_function;
static UInt32 main_hook_function_wrapper_c() {
	update_GetTickCount();//called whether or not bReplaceGetTickCount is true ; it's safe
	if (!console_ready) {
		console_ready = true;
		initialize3();
	}
	else flush_message_buffer();

	if (Settings::Master::bManageFPS) manage_framerate();
	return main_hook_wrapped_function;
}
static void __declspec(naked) main_hook_function_wrapper_asm() {
	__asm {
		push ecx
		call main_hook_function_wrapper_c
		pop ecx
		mov eax, main_hook_wrapped_function
		jmp eax
	}
}

static const GUID GUID_SysMouse = { 0x6F1D2B60, 0xD5A0, 0x11CF, { 0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00 } };
static const GUID GUID_SysKeyboard = { 0x6F1D2B61, 0xD5A0, 0x11CF, { 0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00} };
class FakeDirectInputDevice : public IDirectInputDevice8 {
private:
    IDirectInputDevice8* real_device;
	ULONG Refs;
public:
    /*** Constructor and misc functions ***/
    FakeDirectInputDevice(IDirectInputDevice8* real_device_ ) {
        real_device=real_device_;
		Refs=1;
    }
    /*** IUnknown methods ***/
    HRESULT _stdcall QueryInterface (REFIID riid, LPVOID * ppvObj) { return real_device->QueryInterface(riid,ppvObj); }
    ULONG _stdcall AddRef(void) { return ++Refs; }
    ULONG _stdcall Release(void) {
		if(--Refs==0) {
			real_device->Release();
			delete this;
			return 0;
		} else { return Refs; }
	}

    /*** IDirectInputDevice8A methods ***/
    HRESULT _stdcall GetCapabilities(LPDIDEVCAPS a) { return real_device->GetCapabilities(a); }
    HRESULT _stdcall EnumObjects(LPDIENUMDEVICEOBJECTSCALLBACKA a,LPVOID b,DWORD c) { return real_device->EnumObjects(a,b,c); }
    HRESULT _stdcall GetProperty(REFGUID a,DIPROPHEADER* b) { return real_device->GetProperty(a,b); }
    HRESULT _stdcall SetProperty(REFGUID a,const DIPROPHEADER* b) { return real_device->SetProperty(a,b); }
    HRESULT _stdcall Acquire(void) { return real_device->Acquire(); }
    HRESULT _stdcall Unacquire(void) { return real_device->Unacquire(); }
    HRESULT _stdcall GetDeviceState(DWORD a,LPVOID b) {
		manage_main_hook_basics();
		if (Settings::Master::bManageFPS) manage_framerate();

		DIMOUSESTATE2* MouseState=(DIMOUSESTATE2*)b;
		HRESULT hr=real_device->GetDeviceState(sizeof(DIMOUSESTATE2),MouseState);
		return hr;
    }
	HRESULT _stdcall GetDeviceData(DWORD a,DIDEVICEOBJECTDATA* b,DWORD* c,DWORD d) {return real_device->GetDeviceData(a,b,c,d);}
    HRESULT _stdcall SetDataFormat(const DIDATAFORMAT* a) { return real_device->SetDataFormat(a); }
    HRESULT _stdcall SetEventNotification(HANDLE a) { return real_device->SetEventNotification(a); }
    HRESULT _stdcall SetCooperativeLevel(HWND a,DWORD b) { return real_device->SetCooperativeLevel(a,b); }
    HRESULT _stdcall GetObjectInfo(LPDIDEVICEOBJECTINSTANCEA a,DWORD b,DWORD c) { return real_device->GetObjectInfo(a,b,c); }
    HRESULT _stdcall GetDeviceInfo(LPDIDEVICEINSTANCEA a) { return real_device->GetDeviceInfo(a); }
    HRESULT _stdcall RunControlPanel(HWND a,DWORD b) { return real_device->RunControlPanel(a,b); }
    HRESULT _stdcall Initialize(HINSTANCE a,DWORD b,REFGUID c) { return real_device->Initialize(a,b,c); }
    HRESULT _stdcall CreateEffect(REFGUID a,LPCDIEFFECT b,LPDIRECTINPUTEFFECT *c,LPUNKNOWN d) { return real_device->CreateEffect(a,b,c,d); }
    HRESULT _stdcall EnumEffects(LPDIENUMEFFECTSCALLBACKA a,LPVOID b,DWORD c) { return real_device->EnumEffects(a,b,c); }
    HRESULT _stdcall GetEffectInfo(LPDIEFFECTINFOA a,REFGUID b) { return real_device->GetEffectInfo(a,b); }
    HRESULT _stdcall GetForceFeedbackState(LPDWORD a) { return real_device->GetForceFeedbackState(a); }
    HRESULT _stdcall SendForceFeedbackCommand(DWORD a) { return real_device->SendForceFeedbackCommand(a); }
    HRESULT _stdcall EnumCreatedEffectObjects(LPDIENUMCREATEDEFFECTOBJECTSCALLBACK a,LPVOID b,DWORD c) { return real_device->EnumCreatedEffectObjects(a,b,c); }
    HRESULT _stdcall Escape(LPDIEFFESCAPE a) { return real_device->Escape(a); }
    HRESULT _stdcall Poll(void) { return real_device->Poll(); }
    HRESULT _stdcall SendDeviceData(DWORD a,LPCDIDEVICEOBJECTDATA b,LPDWORD c,DWORD d) { return real_device->SendDeviceData(a,b,c,d); }
    HRESULT _stdcall EnumEffectsInFile(LPCSTR a,LPDIENUMEFFECTSINFILECALLBACK b,LPVOID c,DWORD d) { return real_device->EnumEffectsInFile(a,b,c,d); }
    HRESULT _stdcall WriteEffectToFile(LPCSTR a,DWORD b,LPDIFILEEFFECT c,DWORD d) { return real_device->WriteEffectToFile(a,b,c,d); }
    HRESULT _stdcall BuildActionMap(LPDIACTIONFORMATA a,LPCSTR b,DWORD c) { return real_device->BuildActionMap(a,b,c); }
    HRESULT _stdcall SetActionMap(LPDIACTIONFORMATA a,LPCSTR b,DWORD c) { return real_device->SetActionMap(a,b,c); }
    HRESULT _stdcall GetImageInfo(LPDIDEVICEIMAGEINFOHEADERA a) { return real_device->GetImageInfo(a); }
};
class FakeDirectInput : public IDirectInput8A {
private:
    IDirectInput8* real_input;
    IDirectInput8* obse_input;
	ULONG Refs;
public:
    /*** Constructor ***/
    FakeDirectInput(IDirectInput8A* Real) { real_input=Real; Refs=1; }
    /*** IUnknown methods ***/
    HRESULT _stdcall QueryInterface (REFIID riid, LPVOID* ppvObj) { return real_input->QueryInterface(riid,ppvObj); }
	ULONG _stdcall AddRef(void) { return ++Refs; }
    ULONG _stdcall Release(void) {
		if(--Refs==0) {
			real_input->Release();
			delete this;
			return 0;
		} else { return Refs; }
    }
    /*** IDirectInput8A methods ***/
    HRESULT _stdcall CreateDevice(REFGUID r, IDirectInputDevice8A** device,IUnknown* unused) {
        if(r!=GUID_SysMouse) { 
			return real_input->CreateDevice(r,device,unused);
		} else {
			IDirectInputDevice8A* real_device;
			HRESULT hr;

            hr=real_input->CreateDevice(r,&real_device,unused);
            if(hr!=DI_OK) return hr;
            *device = new FakeDirectInputDevice(real_device);
            return DI_OK;
        }
    }
    HRESULT _stdcall EnumDevices(DWORD a,LPDIENUMDEVICESCALLBACKA b,void* c,DWORD d) { return real_input->EnumDevices(a,b,c,d); }
    HRESULT _stdcall GetDeviceStatus(REFGUID r) { return real_input->GetDeviceStatus(r); }
    HRESULT _stdcall RunControlPanel(HWND a,DWORD b) { return real_input->RunControlPanel(a,b); }
    HRESULT _stdcall Initialize(HINSTANCE a,DWORD b) { return real_input->Initialize(a,b); }
    HRESULT _stdcall FindDevice(REFGUID a,LPCSTR b,LPGUID c) { return real_input->FindDevice(a,b,c); }
    HRESULT _stdcall EnumDevicesBySemantics(LPCSTR a,LPDIACTIONFORMATA b,LPDIENUMDEVICESBYSEMANTICSCBA c,void* d,DWORD e) { return real_input->EnumDevicesBySemantics(a,b,c,d,e); }
    HRESULT _stdcall ConfigureDevices(LPDICONFIGUREDEVICESCALLBACK a,LPDICONFIGUREDEVICESPARAMSA b,DWORD c,void* d) { return real_input->ConfigureDevices(a,b,c,d); }
};


#if defined USE_SCRIPT_EXTENDER
	static void *DICreate_RealFunc;
	static HRESULT _stdcall Hook_DirectInput8Create_Execute(HINSTANCE a,DWORD b,REFIID c,void* d,IUnknown* e) {
		IDirectInput8A* dinput;
		typedef HRESULT(_stdcall * CreateDInputProc)(HINSTANCE, DWORD, REFIID, LPVOID, LPUNKNOWN);
		HRESULT hr = ((CreateDInputProc)DICreate_RealFunc)(a, b, c, &dinput, e);
		if(hr!=DI_OK) return hr;
		*((IDirectInput8A**)d)=new FakeDirectInput(dinput);

		return DI_OK;
	}
#endif

void initialize_main_hook() {
	int hook_point = Settings::Master::iMainHookPoint;
	if (false) ;
	else if (hook_point == 0) {
		message("hook mode %d: no main hook used; some features will not work", hook_point);
	}
	else if (hook_point == 1 && Hook_MainGameFunctionCaller) {
		message("hook point %d: hooking call to main game function", hook_point);
		UInt32 instruction_address = Hook_MainGameFunctionCaller;
		UInt32 old_rel_target = *(UInt32*)(instruction_address+1);
		UInt32 old_target = old_rel_target + instruction_address + 5;
		main_hook_wrapped_function = old_target;
		WriteRelCall(instruction_address, UInt32(&main_hook_function_wrapper_asm));
	}
	else if (hook_point == 2 && Hook_DirectInput8Create) {
		message("hook mode %d: using DX8 mouse code", hook_point);
		DICreate_RealFunc = *(void**)Hook_DirectInput8Create;
		SafeWrite32(UInt32(Hook_DirectInput8Create),(DWORD)Hook_DirectInput8Create_Execute);
		//ZeroMemory(&DI_data,sizeof(DI_data));
		//for(WORD w=0;w<kMaxMacros;w++) DI_data.DisallowStates[w]=0x80;
	}
#if defined OBLIVION
//	else if (hook_point == 3) {
//		message("hook mode %d: using main GetTickCount call", hook_point);
//		FakeGetTickCount_Indirect = FakeGetTickCount1;
//		SafeWrite32(UInt32(Hook_GetTickCount), UInt32(&FakeGetTickCount_Indirect));
//	}
#elif defined FALLOUT
#elif defined NEW_VEGAS
#endif
	else {
		message("Unsupported hook point %d; no main hook; some features will not work", hook_point);
	}
}
bool initialize0() {//called at startup / OBSE query
	g_log.SetAutoFlush(true);
	char buffy[512];
	sprintf_s(buffy, "initialize0() running in thread %x", GetCurrentThreadId());
	g_log.Message(buffy);
	init_time();
	global_rng.seed_64(base_time_ticks);
	InitializeCriticalSectionAndSpinCount(&message_buffer_cs, 1500);
	PerThread::initialize();

	SYSTEM_INFO stSI;
	GetSystemInfo(&stSI);
	int num_processors = stSI.dwNumberOfProcessors;
//	HANDLE process = GetCurrentProcess();
//	DWORD_PTR pam;
//	DWORD_PTR sam;
//	GetProcessAffinityMask(process, &pam, &sam);
//	int num_processors = 0;
//	for (int i = 0; i < 32; i++) if ((pasm >> i) & 1) num_processors++;
//	if (num_processors > 1) system_is_multiprocessor = true;
//	else system_is_multiprocessor = false;

	return true;
}
bool initialize1() {//called at startup / OBSE query
	char buffy[800];
	sprintf_s(buffy, "initialize1() running in thread %x", GetCurrentThreadId());
	g_log.Message(buffy);

	load_settings();
	return true;
}
bool initialize2() {//called at startup / OBSE load
	char buffy[8192];
	std::time_t current_time = std::time(NULL);
	std::tm *local_tm = localtime(&current_time);
	//Localtime
	sprintf_s(buffy, "initialize2() running in thread %x at %s", GetCurrentThreadId(), std::asctime(local_tm));
	//asctime
	g_log.Message(buffy);

	if (Settings::Master::bReplaceGetTickCount) hook_GetTickCount();
	if (Settings::Master::bReplaceRandom) initialize_random_hooks();

	if (Hook_ExitPath && Settings::Master::bFastExit) {
		WriteRelJump(Hook_ExitPath,(UInt32)&FastExit);
	}
	initialize_CS_and_LCS_hooks();//checking the master settings is handled inside, because one of them doesn't exist on some platforms
	if (Settings::Master::bReplaceHeap) {
		if (!optimize_memoryheap(Settings::Heap::iHeapAlgorithm, Settings::Heap::bEnableProfiling)) {
			error("Failed to optimize heap");
		}
	}
	if (Settings::Master::bHookHashtables) {
		initialize_hashtable_hooks();
	}


	initialize_main_hook();

	if (Settings::Master::iSchedulingResolution > 0 && Settings::Master::iSchedulingResolution <= 20)
		HandleTimeResolution_instance.set_resolution( Settings::Master::iSchedulingResolution );

	initialize_callperf();
	if (Settings::Master::bExperimentalStuff) {
		initialize_experimental_stuff();
	}
	return true;
}

#if defined USE_SCRIPT_EXTENDER
extern "C" {
#if defined OBLIVION
	__declspec(dllexport) bool OBSEPlugin_Query(const OBSEInterface * obse, PluginInfo * info)
	{
		if (!initialize0()) return false;
		if (!initialize1()) return false;
		// fill out the info structure
		info->infoVersion = PluginInfo::kInfoVersion;
		info->name = PLUGIN_NAME;
		info->version = (VERSION_NUM_A << 12) + (VERSION_NUM_B << 8) + (VERSION_NUM_C << 0);

		// version checks
		if(!obse->isEditor)
		{
			if(obse->obseVersion < 15)
			{
				_ERROR("OBSE version too old (got %08X expected at least %08X)", obse->obseVersion, 15);
				return false;
			}

			if(obse->oblivionVersion != OBLIVION_VERSION)
			{
				_ERROR("incorrect Oblivion version (got %08X need %08X)", obse->oblivionVersion, OBLIVION_VERSION);
				return false;
			}

		}
		else
		{
			// no version checks needed for editor
			return false;
		}

		// version checks pass

		return true;
	}
	__declspec(dllexport) bool OBSEPlugin_Load(const OBSEInterface * obse)
	{
		g_pluginHandle = obse->GetPluginHandle();

		g_con = (OBSEConsoleInterface*)obse->QueryInterface(kInterface_Console);
		if(!g_con)
		{
			_ERROR("console interface not found");
			return false;
		}

		if (!initialize2()) return false;

		return true;
	}
#elif defined FALLOUT
	__declspec(dllexport) bool FOSEPlugin_Query(const FOSEInterface * fose, PluginInfo * info)
	{
		if (!initialize0()) return false;
		if (!initialize1()) return false;
		// fill out the info structure
		info->infoVersion = PluginInfo::kInfoVersion;
		info->name = PLUGIN_NAME;
		info->version = (VERSION_NUM_A << 12) + (VERSION_NUM_B << 8) + (VERSION_NUM_C << 0);

		// version checks
		if(!fose->isEditor)
		{
			if(fose->foseVersion < 1)//should be 2 or something
			{
				_ERROR("FOSE version too old (got %08X expected at least %08X)", fose->foseVersion, 15);
				return false;
			}

			if(fose->runtimeVersion != FALLOUT_VERSION)
			{
				_ERROR("incorrect Fallout version (got %08X need %08X)", fose->runtimeVersion, FALLOUT_VERSION);
				return false;
			}
		}
		else
		{
			// no version checks needed for editor
			return false;
		}

		// version checks pass

		return true;
	}
	__declspec(dllexport) bool FOSEPlugin_Load(const FOSEInterface * fose)
	{
		g_pluginHandle = fose->GetPluginHandle();

		g_con = (FOSEConsoleInterface*)fose->QueryInterface(kInterface_Console);
		if(!g_con)
		{
			message("console interface not found");
//			_ERROR("console interface not found");
//			return false;
		}

		if (!initialize2()) return false;

		return true;
	}
#elif defined NEW_VEGAS
	__declspec(dllexport) bool NVSEPlugin_Query(const NVSEInterface * nvse, PluginInfo * info)
	{
		if (!initialize0()) return false;
		if (!initialize1()) return false;
		// fill out the info structure
		info->infoVersion = PluginInfo::kInfoVersion;
		info->name = PLUGIN_NAME;
		info->version = (VERSION_NUM_A << 12) + (VERSION_NUM_B << 8) + (VERSION_NUM_C << 0);

		// version checks
		if(!nvse->isEditor)
		{
			if(nvse->nvseVersion < 0)//should be 2 or something
			{
				_ERROR("NVSE version too old (got %08X expected at least %08X)", nvse->nvseVersion, 15);
				return false;
			}

			switch (nvse->runtimeVersion) {
				//void init_FO3_1_7_0_3();
				//void init_FNV_1_0_0_240();
				//void init_FNV_1_1_1_271();
				//void init_FNV_1_2_0_285();
				case MAKE_NEW_VEGAS_VERSION(0, 0, 240): init_FNV_1_0_0_240();
				break;
			//	case MAKE_NEW_VEGAS_VERSION(1, 0, 268): 
			//	break;
				case MAKE_NEW_VEGAS_VERSION(1, 1, 271): init_FNV_1_1_1_271();
				break;
				case MAKE_NEW_VEGAS_VERSION(2, 0, 285): init_FNV_1_2_0_285();
				break;
				case MAKE_NEW_VEGAS_VERSION(2, 0, 314): init_FNV_1_2_0_314();
				break;
				case MAKE_NEW_VEGAS_VERSION(2, 0, 352): init_FNV_1_2_0_352();
				break;
				case MAKE_NEW_VEGAS_VERSION(3, 0, 452): init_FNV_1_3_0_452();
				break;
				case MAKE_NEW_VEGAS_VERSION(4, 0, 525): init_FNV_1_4_0_525();
				break;
				default: {
					message("This version of New Vegas is not recognized/supported by the Stutter Remover.");
					//_ERROR("This version of New Vegas is not recognized / supported by the Stutter Remover.");
					return false;
				}
				break;
			}
		}
		else
		{
			// no version checks needed for editor
			return false;
		}

		// version checks pass

		return true;
	}
	__declspec(dllexport) bool NVSEPlugin_Load(const NVSEInterface * nvse)
	{
		g_pluginHandle = nvse->GetPluginHandle();

		//g_con = (NVSEConsoleInterface*)nvse->QueryInterface(kInterface_Console);
		if(!g_con)
		{
			message("console interface not found");
//			_ERROR("console interface not found");
//			return false;
		}

		if (!initialize2()) return false;

		return true;
	}
#endif
}//extern "C"
#endif

#if defined FORCE_INITIALIZATION

class AutoInitialize {
public:
	AutoInitialize() {
		if (!initialize0()) return;
		message("preliminary version (does not use NVSE)");
		if (!initialize1()) return;
		UInt32 hash = get_exe_hash(EXE_FILE_NAME);
		message("hash of [%s] = %08X", EXE_FILE_NAME, hash);
#if defined OBLIVION
#error this path not set up for Oblivion yet
#elif defined FALLOUT
#error this path not set up for Fallout yet
#elif defined NEW_VEGAS
		//void init_FO3_1_7_0_3();
		//void init_FNV_1_0_0_240();
		//void init_FNV_1_1_1_271();
		//void init_FNV_1_2_0_285();
		if (0) ;
		else if (hash == 0xF4365A9A) init_FO3_1_7_0_3();
		else if (hash == 0xF2C8A1E5) init_FNV_1_0_0_240();
		else if (hash == 0xEE5B8433) init_FNV_1_1_1_271();
		else if (hash == 0x38C42F4E) init_FNV_1_2_0_285();
		else {
			error("This version of %s is not recognized (it is either encrypted or an unsupported version)", EXE_FILE_NAME);
		}
#else
#error What game is this for?
#endif
		if (!initialize2()) return;
		message("AutoInitialize finished");
	}
} AutoInitialize_instance;
extern "C" {
	void __declspec(dllexport) dummy_export() {
		//message("void __declspec(dllexport) blah(): doing nothing\n");
	}
}
#endif

