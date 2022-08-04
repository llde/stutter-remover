#define USE_SCRIPT_EXTENDER
#ifndef USE_SCRIPT_EXTENDER
	#define FORCE_INITIALIZATION
	typedef unsigned char  UInt8;
	typedef unsigned short UInt16;
	typedef unsigned long  UInt32;
	typedef unsigned long long UInt64;
	typedef signed char  SInt8;
	typedef signed short SInt16;
	typedef signed long  SInt32;
	typedef signed long long SInt64;
	void SafeWrite8(UInt32 addr, UInt32 data);
	void SafeWrite16(UInt32 addr, UInt32 data);
	void SafeWrite32(UInt32 addr, UInt32 data);
	void SafeWriteBuf(UInt32 addr, void * data, UInt32 len);
	void WriteRelJump(UInt32 jumpSrc, UInt32 jumpTgt);
	void WriteRelCall(UInt32 jumpSrc, UInt32 jumpTgt);
	//#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
#elif defined OBLIVION
	#include <vector>
	#define WIN32_LEAN_AND_MEAN
	#include "obse_common/obse_version.h"
	#include "common/iPrefix.h"
	#include "obse/PluginAPI.h"
	#include "obse/CommandTable.h"
	#include "obse/GameAPI.h"
	#include "obse/ParamInfos.h"
	#include "obse_common/SafeWrite.h"
#elif defined FALLOUT
	#define WIN32_LEAN_AND_MEAN
	#include "common/iPrefix.h"
	#include "fose/PluginAPI.h"
	#include "fose/CommandTable.h"
	#include "fose/GameAPI.h"
	#include "fose/ParamInfos.h"
	#include "fose_common/SafeWrite.h"
#elif defined NEW_VEGAS
	#define WIN32_LEAN_AND_MEAN
	#include "common/iPrefix.h"
	#include "nvse/PluginAPI.h"
	#include "nvse/CommandTable.h"
	#include "nvse/GameAPI.h"
	#include "nvse/ParamInfos.h"
	#include "nvse/SafeWrite.h"
	#include "nvse/nvse_version.h"
#endif

//settings.cpp
void load_settings();

//main.cpp
void message( const char *format, ... );
void error( const char *format, ... );
std::string make_string( const char *format, ... );
//bool ConsoleReady();

bool is_address_static(void*);
extern DWORD main_thread_ID;
//extern bool system_is_multiprocessor;//true on multicore and multicpu systems, false otherwise

UInt32 get_time_ms();//tGT
//double get_time_s();//tGT
//UInt32 get_game_timer();//whatever the game is currently using
UInt64 get_time_ticks();// RDTSC atm ; QPC was too slow last I checked (a long time ago), and tGT lacks sufficient resolution
double get_tick_period();//multiply this by get_time_ticks() to get the current time in seconds


class TextSection;
extern TextSection *config;
class RNG_jsf32;
extern RNG_jsf32 global_rng;

//initialization:
bool initialize0();//called at OBSE query, initializes core internals
bool initialize1();//called at OBSE query, loads or creates ini file
//after initialize1 and before initialize2, it figures out what version of Oblivion/Fallout/NewVegas is running
bool initialize2();//called at OBSE load, processes ini settings, applies most hooks
void initialize3();//called when the game reaches the main menu, does anything that can't be done till then

//for use by things that use TLS
void register_thread_destruction_callback(void (*)());

//hook addresses:
#if (defined OBLIVION && defined FALLOUT) || (defined OBLIVION && defined NEW_VEGAS) || (defined FALLOUT && defined NEW_VEGAS)
#error multiple targets of (Oblivion, Fallout, New Vegas) defined?!?
#endif
#if defined OBLIVION || defined FALLOUT || defined NEW_VEGAS
#else
#error Neither Oblivion NOR Fallout NOR New Vegas?!?
#endif

#define SR_MAKE_VERSION(SR_CODE, NUM1, NUM2, NUM3, NUM4) ((UInt64(SR_CODE) << 56) | (UInt64(NUM1)<<40) | (UInt64(NUM2)<<32) | (UInt64(NUM3)<<24) | (UInt64(NUM4)<<0) )
/*SR_CODE: 0=Oblivion, 1=Fallout3, 2=Fallout:NewVegas, ???*/
#if defined OBLIVION && (OBLIVION_VERSION == OBLIVION_VERSION_1_2_416)
	static const UInt64 Hook_target = SR_MAKE_VERSION( 0, 1, 2, 0, 416);
	static const char *Hook_target_s = "Oblivion 1.2.0.416";

//	void * (WINAPI const * Hook_MemoryHeap_Allocate)(int size) = (void * (WINAPI *)(int  )) 0x00401AA0;
//	void * (WINAPI const * Hook_MemoryHeap_Free    )(void *p ) = (void * (WINAPI *)(void*)) 0x00401D40;
//	void * (WINAPI const * Hook_MemoryPool_Allocate)(        ) = (void * (WINAPI *)(     )) 0x00402480;
//	void * (WINAPI const * Hook_MemoryPool_Free    )(void *p ) = (void * (WINAPI *)(void*)) 0x00402600;
	static float * const Hook_FixedTimePerFrame_iFPSClamp = (float*) 0x00b33e94;

//	static CRITICAL_SECTION * const Hook_MemoryLock    = (CRITICAL_SECTION*)0xB32B80;//short duration locks, large numbers of threads, high contention
//	static CRITICAL_SECTION * const Hook_ExtraDataLock = (CRITICAL_SECTION*)0xB33800;
//	static CRITICAL_SECTION * const Hook_UnknownLock1  = (CRITICAL_SECTION*)0xB35D00;
//	static CRITICAL_SECTION * const Hook_UnknownLock2  = (CRITICAL_SECTION*)0xB3BE80;
//	static CRITICAL_SECTION * const Hook_UnknownLock3  = (CRITICAL_SECTION*)0xB3C000;
//	static CRITICAL_SECTION * const Hook_UnknownLock4  = (CRITICAL_SECTION*)0xB3FA00;//short duration locks, large numbers of threads, medium activity
//	static CRITICAL_SECTION * const Hook_UnknownLock5  = (CRITICAL_SECTION*)0xB39C00;
//	static CRITICAL_SECTION * const Hook_UnknownLock6  = (CRITICAL_SECTION*)0xB3FC00;//rare but severe stutter; common when traveling at very high speeds with lots of background LOD load tasks queued up
//	static CRITICAL_SECTION * const Hook_UnknownLock7  = (CRITICAL_SECTION*)0xB3F600;
//	static CRITICAL_SECTION * const Hook_UnknownLock8  = (CRITICAL_SECTION*)0xB35D00;

	static void * const Hook_MemPoolInit  = (void*)0x4022B6;
	static DWORD (WINAPI *** Hook_GetTickCount)() = (DWORD (WINAPI ***)()) 0x0040D8A1;
	static void ** const Hook_NiDX9RendererPtr = (void**) 0xB3F928;
	enum {
//		_Oblivion_FixedTimePerFrame_iFPSClamp = 0x00b33e94,
//		ManageTimeStartAddress = 0x0047D170,
//		ManageTimeReenterAddress = 0x0047D176
	};
	
//	typedef HANDLE (WINAPI *CreateThread_t)(LPSECURITY_ATTRIBUTES, SIZE_T, LPTHREAD_START_ROUTINE, LPVOID, DWORD, LPDWORD);
//	CreateThread_t *Oblivion_CreateThread = (CreateThread_t*) 0xA280F8;
	typedef DWORD (WINAPI *GetTickCount_t)();
	static GetTickCount_t * const Hook_GetTickCount_Indirect = (GetTickCount_t*) 0xA280D0;

	typedef bool (*IsMenuModeFunc_t)();
	static const IsMenuModeFunc_t Hook_IsMenuModeFunc = (bool (*)())0x578f60;

	static void ** const Hook_DirectInput8Create = (void**)0x00A2802C;

#	define MEMORYHEAP_ALLOCATE_PARAMETERLIST ( MemoryHeap *obheap, int nothing, int size, int flag )
	static const UInt32 Hook_MemoryHeap        = 0xB02020;
	static const UInt32 Hook_Memory_Allocate   = 0x401AA0;
	static const UInt32 Hook_Memory_Free       = 0x401D40;
	static const UInt32 Hook_Memory_Reallocate = 0x401E60;
	static const UInt32 Hook_Memory_CreatePool = 0x401F40;
	static const UInt32 Hook_Memory_Initialize = 0x401260;
	static const UInt32 Hook_Memory_Stats      = 0x401FF0;
	static const UInt32 Hook_Memory_Cleanup    = 0x405D60;
	static const UInt32 Hook_Memory_Cleanup2   = 0x40E626;
	static const UInt32 Hook_Memory_Dtor       = 0x401750;
	static const UInt32 Hook_Memory_FreeUnusedPagesStart = 0x00402740;
	static const UInt32 Hook_Memory_GetHeapish = 0x0;
//alternate stuff:
	static const UInt32 Hook_HeapCreate  = 0xA281E4;
	static const UInt32 Hook_HeapDestroy = 0xA281E0;
	static const UInt32 Hook_HeapAlloc   = 0xA28194;
	static const UInt32 Hook_HeapReAlloc = 0xA2819C;
	static const UInt32 Hook_HeapFree    = 0xA28198;
	static const UInt32 Hook_HeapSize    = 0xA281A0;

	static const UInt32 Hook_ExitPath = 0x0040F48C;

	static void ** const Hook_EnterCriticalSection      = (void**)0xA2806C;
	static void ** const Hook_TryEnterCriticalSection   = (void**)0xA28070;
	static void ** const Hook_LeaveCriticalSection      = (void**)0xA28074;
	static void ** const Hook_InitializeCriticalSection = (void**)0xA28064;
	static void ** const Hook_DeleteCriticalSection     = (void**)0xA28068;
	static void ** const Hook_InitializeCriticalSectionAndSpinCount = (void**)0xA2818C;

	static void * const Hook_EnterLightCS                    = (void*)0x0;
	static void * const Hook_InterlockedCompareExchange      = (void*)0x00A2813C;

	static void ***Hook_EnterCriticalSection_B      = (void***)0x00401026;
	static void ***Hook_TryEnterCriticalSection_B   = (void***)0x00401033;
	static void ***Hook_LeaveCriticalSection_B      = (void***)0x00401043;
	static void ***Hook_InitializeCriticalSection_B = (void***)0x00401006;
	static void ***Hook_DeleteCriticalSection_B     = (void***)0x00401013;

	typedef void * (* _GetSingleton)(bool canCreateNew);
//	static const _GetSingleton ConsoleManager_GetSingleton = (_GetSingleton)0x0;

//	static const UInt32 Hook_LoadingAreaMessage_FuncPoint1  = 0x00440C40;

	static const UInt32 Hook_Hashtable_IntToIndex1   = 0x006A9060;
	static const UInt32 Hook_Hashtable_IntToIndex2   = 0x0;
	static const UInt32 Hook_Hashtable_IntToIndex3   = 0x0;
	static const UInt32 Hook_Hashtable_IntToIndex4   = 0x0;
	static const UInt32 Hook_Hashtable_StringToIndex1= 0x007DAED0;
	static const UInt32 Hook_Hashtable_StringToIndex2= 0x0;
	static const UInt32 Hook_Hashtable_StringToIndex3= 0x0;
	static const UInt32 Hook_Hashtable_StringToIndex4= 0x0;
	static const UInt32 Hook_Hashtable_CharToIndex1  = 0x00452870;
	static const UInt32 Hook_Hashtable_CharToIndex2  = 0x0;

	static const UInt32 Hook_rand   = 0x009859dd;
	static const UInt32 Hook_srand  = 0x009859d0;
	static const UInt32 Hook_rand1  = 0x0047df80;
	static const UInt32 Hook_rand2  = 0x0047dfd0;
	static const UInt32 Hook_rand3  = 0x0047e020;
	static const UInt32 Hook_rand4  = 0x0047e060;
	static const UInt32 Hook_rand5  = 0x0047e0b0;
	static const UInt32 Hook_rand6  = 0x0047e0f0;
	static const UInt32 Hook_rand7  = 0x0047e140;
	static const UInt32 Hook_rand8  = 0x0047fb70;

	static const UInt32 Hook_memset1 = 0x00981630;
	static const UInt32 Hook_memset2 = 0x0;
	static const UInt32 Hook_Sleep   = 0x00A280DC;

	static const UInt32 Hook_MainGameFunctionCaller = 0x40F270;

//	static long *Hook_NumericID_MapSize1 = (long*) 0x0045ABE6;//normally 0x25, badhair suggests changing this from 0x25 to 0x13AF
//	static long *Hook_NumericID_MapSize2 = (long*) 0x0045AA86;//normally 0x25
//	static long *Hook_NumericID_MapSize3 = (long*) 0x0045A946;//normally 0x25
//	static long *Hook_NumericID_MapSize4 = (long*) 0x0045A866;//normally 0x13AF

#	define COMPONENT_DLL_PATH "Data\\obse\\plugins\\ComponentDLLs\\"

#elif defined FALLOUT && (FALLOUT_VERSION == 0x01070030) //FALLOUT_VERSION_1_7_30
	static const UInt64 Hook_target = SR_MAKE_VERSION( 1, 1, 7, 0, 3);
	static const char *Hook_target_s = "Fallout3 1.2.0.416";

	static float * const Hook_FixedTimePerFrame_iFPSClamp = (float*)(0x01090BA0 + 4);

	static void ** const Hook_NiDX9RendererPtr = (void**) 0x0108f048;

	static DWORD (WINAPI *** const Hook_GetTickCount)() = (DWORD (WINAPI ***)()) 0x0;

	typedef DWORD (WINAPI *GetTickCount_t)();
	static GetTickCount_t * const Hook_GetTickCount_Indirect = (GetTickCount_t*) 0xD9B090;

	static void ** const Hook_DirectInput8Create = (void**)0x00D9B02C;

#	define MEMORYHEAP_ALLOCATE_PARAMETERLIST ( MemoryHeap *obheap, int nothing, int size )
	static const UInt32 Hook_MemoryHeap = 0x1090A78;
	/*
		Heap methods
			see 0x1090A78 for the heap itself (global variable)
			Heap has NO vtable
			???:			0x6A0250
			???:			0x6E2080
			allocate:		0x86B930
			free:			0x86BA60
			reallocate:		0x86BAE0
			stupid_realloc:	0x86BB50 (takes 3 extra unused parameters)
			create_pool:	0x86BF10
			get_size:		0x86B8C0
			???:			0x86B7D0 (returns either NULL or some sort of alternative heap-like object)
			???:			0x86BCB0
			???:			0x86BA50
			???:			0x86B8B0
			get_stats:		0x86C000
			constructor?:	
			destructor?:	
			static init:	0x86D670
		Heap static members
			Pool *pool_from_address[16384]:		0x1090BE0
			Pool *pool_from_size[?]:			0x1090FE0
		Pool methods
			constructor:	0x86D690 ?
			destructor:		0x86D770 ?
			allocate:		0x86D7D0
			free:			0x86D930
			is_member:		0x86B870
			weird:			0x86BD50 ?
		HeapWrapper methods
			//and see 0x1091CF0, which seems to be a pointer at a pointer at something Heap-like
			//  with vtable=0xE2C578, possibly an NiStandardAllocator wrapper for their custom heap
		HeapWrapperWrapper functions
			//and see 0x89E810, a wrapper for the NiStandardAllocator?
			//and maybe also 0x1091D88 ???
	*/
	static const UInt32 Hook_Memory_Allocate   = 0x86B930;
	static const UInt32 Hook_Memory_Free       = 0x86BA60;
	static const UInt32 Hook_Memory_Reallocate = 0x86BAE0;
	static const UInt32 Hook_Memory_DumbRealloc= 0x86BB50;
	static const UInt32 Hook_Memory_CreatePool = 0x86BF10;
	static const UInt32 Hook_Memory_GetHeapish = 0;//0x86B7D0;
	static const UInt32 Hook_Memory_Initialize = 0x0;
	static const UInt32 Hook_Memory_Stats      = 0x0;
	static const UInt32 Hook_Memory_Cleanup    = 0x0;
	static const UInt32 Hook_Memory_Cleanup2   = 0x0;
	static const UInt32 Hook_Memory_Dtor       = 0x0;
	static const UInt32 Hook_Memory_FreeUnusedPagesStart = 0x0;
//alternate stuff:
	static const UInt32 Hook_HeapCreate  = 0xD9B214;
	static const UInt32 Hook_HeapDestroy = 0xD9B210;
	static const UInt32 Hook_HeapAlloc   = 0xD9B048;
	static const UInt32 Hook_HeapReAlloc = 0xD9B1F8;
	static const UInt32 Hook_HeapFree    = 0xD9B04C;
	static const UInt32 Hook_HeapSize    = 0xD9B154;

	static const UInt32 Hook_ExitPath = 0x006EED57;

	static void ** const Hook_EnterCriticalSection      = (void**)0xD9B09C;
	static void ** const Hook_TryEnterCriticalSection   = (void**)0xD9B108;
	static void ** const Hook_LeaveCriticalSection      = (void**)0xD9B0A0;
	static void ** const Hook_InitializeCriticalSection = (void**)0xD9B094;
	static void ** const Hook_DeleteCriticalSection     = (void**)0xD9B098;
	static void ** const Hook_InitializeCriticalSectionAndSpinCount = (void**)0xD9B1AC;

	static void * const Hook_EnterLightCS                    = (void*) 0x00409A80;
	static void * const Hook_TryEnterLightCS                 = (void*) 0x0;
	static void * const Hook_LeaveLightCS                    = (void*) 0x0;
	//static void * const Hook_InterlockedCompareExchange      = (void**)0x00D9B06C;

	static void ***Hook_EnterCriticalSection_B      = (void***)0x0;
	static void ***Hook_TryEnterCriticalSection_B   = (void***)0x0;
	static void ***Hook_LeaveCriticalSection_B      = (void***)0x0;
	static void ***Hook_InitializeCriticalSection_B = (void***)0x0;
	static void ***Hook_DeleteCriticalSection_B     = (void***)0x0;

	typedef void * (* _GetSingleton)(bool canCreateNew);
	static const _GetSingleton ConsoleManager_GetSingleton = (_GetSingleton)0x0062B5D0;

	static UInt32 Hook_Hashtable_IntToIndex1   = 0x004589E0;
	static UInt32 Hook_Hashtable_IntToIndex2   = 0x0;
	static UInt32 Hook_Hashtable_IntToIndex3   = 0x0;
	static UInt32 Hook_Hashtable_IntToIndex4   = 0x0;
	static UInt32 Hook_Hashtable_StringToIndex1= 0x00553FA0;
	static UInt32 Hook_Hashtable_StringToIndex2= 0x0;
	static UInt32 Hook_Hashtable_StringToIndex3= 0x0;
	static UInt32 Hook_Hashtable_StringToIndex4= 0x0;
	static UInt32 Hook_Hashtable_CharToIndex1  = 0x0;
	static UInt32 Hook_Hashtable_CharToIndex2  = 0x0;

	static const UInt32 Hook_memset1 = 0x00C00610;
	static const UInt32 Hook_memset2 = 0x0;
	static const UInt32 Hook_rand    = 0xC04D9A;
	static const UInt32 Hook_srand   = 0xC04D8D;
	static const UInt32 Hook_Sleep   = 0x00D9B074;

	static const UInt32 Hook_MainGameFunctionCaller = 0x006EEC86;


#	define COMPONENT_DLL_PATH "Data\\fose\\plugins\\ComponentDLLs\\"

#elif defined NEW_VEGAS
	extern UInt64 Hook_target;
	extern const char *Hook_target_s;

	extern float * Hook_FixedTimePerFrame_iFPSClamp;
	extern void ** Hook_NiDX9RendererPtr;
	extern DWORD (WINAPI *** const Hook_GetTickCount)();

	typedef DWORD (WINAPI *GetTickCount_t)();

	extern GetTickCount_t * Hook_GetTickCount_Indirect;

	extern void ** Hook_DirectInput8Create;

#	define MEMORYHEAP_ALLOCATE_PARAMETERLIST ( MemoryHeap *obheap, int nothing, int size )
	extern UInt32 Hook_MemoryHeap;
	extern UInt32 Hook_Memory_Allocate;
	extern UInt32 Hook_Memory_Free    ;
	extern UInt32 Hook_Memory_Reallocate;
	extern UInt32 Hook_Memory_CreatePool;
	extern UInt32 Hook_Memory_Initialize;
	extern UInt32 Hook_Memory_Stats     ;
	extern UInt32 Hook_Memory_Cleanup   ;
	extern UInt32 Hook_Memory_Cleanup2  ;
	extern UInt32 Hook_Memory_Dtor      ;
	extern UInt32 Hook_Memory_FreeUnusedPagesStart;
	extern UInt32 Hook_Memory_GetHeapish;
//alternate stuff:
	extern UInt32 Hook_HeapCreate ;
	extern UInt32 Hook_HeapDestroy;
	extern UInt32 Hook_HeapAlloc  ;
	extern UInt32 Hook_HeapReAlloc;
	extern UInt32 Hook_HeapFree   ;
	extern UInt32 Hook_HeapSize   ;

	extern UInt32 Hook_ExitPath;

	extern void ** Hook_EnterCriticalSection      ;//0x00FD505C;
	extern void ** Hook_TryEnterCriticalSection   ;//0x00FD51B0;
	extern void ** Hook_LeaveCriticalSection      ;//0x00FD5100;
	extern void ** Hook_InitializeCriticalSection ;//0x00FD5054;
	extern void ** Hook_DeleteCriticalSection     ;//0x00FD5058;
	extern void ** Hook_InitializeCriticalSectionAndSpinCount;//0x00FD5130;

	extern void * Hook_EnterLightCS                    ;
	extern void * Hook_TryEnterLightCS                 ;
	extern void * Hook_LeaveLightCS                    ;
	extern void * Hook_InterlockedCompareExchange      ;

	extern void ***Hook_EnterCriticalSection_B      ;
	extern void ***Hook_TryEnterCriticalSection_B   ;
	extern void ***Hook_LeaveCriticalSection_B      ;
	extern void ***Hook_InitializeCriticalSection_B ;
	extern void ***Hook_DeleteCriticalSection_B     ;

//	typedef void * (* _GetSingleton)(bool canCreateNew);
//	extern _GetSingleton ConsoleManager_GetSingleton = (_GetSingleton)0x0;

	extern UInt32 Hook_Hashtable_IntToIndex1   ;
	extern UInt32 Hook_Hashtable_IntToIndex2   ;
	extern UInt32 Hook_Hashtable_IntToIndex3   ;
	extern UInt32 Hook_Hashtable_IntToIndex4   ;
	extern UInt32 Hook_Hashtable_StringToIndex1;
	extern UInt32 Hook_Hashtable_StringToIndex2;
	extern UInt32 Hook_Hashtable_StringToIndex3;
	extern UInt32 Hook_Hashtable_StringToIndex4;
	extern UInt32 Hook_Hashtable_CharToIndex1  ;
	extern UInt32 Hook_Hashtable_CharToIndex2  ;

	extern UInt32 Hook_memset1;
	extern UInt32 Hook_memset2;
	extern UInt32 Hook_rand;
	extern UInt32 Hook_srand;
	extern UInt32 Hook_Sleep;

	extern UInt32 Hook_MainGameFunction       ;//specific to NV for the moment
	extern UInt32 Hook_MainGameFunctionCaller ;//specific to NV for the moment


#	define COMPONENT_DLL_PATH "Data\\nvse\\plugins\\ComponentDLLs\\"
#else
#	error Unrecognized version of Oblivion / Fallout / New Vegas
#endif


