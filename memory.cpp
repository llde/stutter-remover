#include <string>
#include <set>
#include <map>

#include "main.h"

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
//#if defined USE_SCRIPT_EXTENDER
//#include "Hooks_DirectInput8Create.h"
#include <queue>

#include "locks.h"
#include "memory.h"
#include "simpleheap.h"
#include "settings.h"

#include <shlobj.h>
#include <shlwapi.h>

void message( const char *format, ... );

class MemoryHeap;

SR_HEAP heap_SimpleHeap1 = {
	SimpleHeap1::allocate,
	SimpleHeap1::deallocate,
	SimpleHeap1::reallocate,
	SimpleHeap1::get_size,
	SimpleHeap1::is_in_heap_fast,
	SimpleHeap1::is_in_heap,
	SimpleHeap1::init,
	NULL,
	NULL,
	NULL,
	NULL,
};
SR_HEAP heap_ThreadHeap2 = {
	ThreadHeap2::allocate,
	ThreadHeap2::deallocate,
	ThreadHeap2::reallocate,
	ThreadHeap2::get_size,
	ThreadHeap2::is_in_heap_fast,
	NULL,//ThreadHeap2::is_in_heap,
	ThreadHeap2::init,
	NULL,
	NULL,
	NULL,
	NULL,
};
SR_HEAP heap_ThreadHeap3 = {
	ThreadHeap3::allocate,
	ThreadHeap3::deallocate,
	ThreadHeap3::reallocate,
	ThreadHeap3::get_size,
	ThreadHeap3::is_in_heap_fast,
	NULL,//ThreadHeap3::is_in_heap,
	ThreadHeap3::init,
	ThreadHeap3::get_bytes_used,
	NULL,
	NULL,
	NULL,
};
SR_HEAP heap_Windows = {
	malloc,
	free,
	realloc,
	_msize,
	NULL,//is_in_heap_fast,
	NULL,//is_in_heap,
	NULL,//init,
	NULL,
	NULL,
	NULL,
	NULL,
};
SR_HEAP heap_Oblivion = {
	allocate_from_memoryheap,
	free_to_memoryheap,
	realloc_from_memoryheap,
	NULL,//get_size
	NULL,//is_in_heap_fast
	NULL,//is_in_heap,
	NULL,//init
	NULL,
	NULL,
	NULL,
	NULL,
};
SR_HEAP heap = heap_Oblivion;

#if 0
class MemoryHeap
{
public:
/*	MemoryHeap(UInt32 mainSize, UInt32 unk1);
	
	void *	Allocate(UInt32 size, UInt32 unk);
	void	Free(void * ptr);

	void	CreatePool(UInt32 entrySize, UInt32 totalSize);

	bool	IsMainHeapAllocation(void * ptr);*/
	
	virtual void	Unk_0(UInt32 unk0);
	virtual void *	AllocatePage(UInt32 size);
	virtual void *	RawAlloc(UInt32 unk0);
	virtual void *	RawAlloc2(UInt32 unk0);
	virtual void	FreeMemory(void * buf);
	virtual void	RawFree(void * buf);
	virtual void	RawFree2(void * buf);
	virtual UInt32	RawSize(void * buf);
	
	// memory panic callback
	typedef void (* Unk164Callback)(UInt32 unk0, UInt32 unk1, UInt32 unk2);
	
//	void	** _vtbl;	// 000
	UInt32	field_004;	// 004 - alignment
	UInt32	field_008;	// 008
	UInt32	field_00C;	// 00C - size of main memory block
	UInt32	field_010;	// 010
	UInt32	field_014;	// 014
	void	* field_018;	// 018 - main memory block
	UInt32	field_01C;	// 01C
	UInt32	field_020;	// 020
	UInt32	field_024;	// 024
	UInt32	field_028;	// 028
	UInt32	field_02C;	// 02C
	UInt32	field_030;	// 030 - size of field_034 / 8
	void	* field_034;	// 034 - 0x2000 byte buffer
	void	* field_038;	// 038 - end of field_034
	UInt32	field_03C;	// 03C
	UInt32	field_040;	// 040
	void	* field_044;	// 044 - 0x80 byte buffer?
	UInt32	field_048;	// 048
	UInt32	field_04C;	// 04C
	UInt32	field_050;	// 050
	UInt32	field_054;	// 054 - available memory at startup
	UInt32	field_058;	// 058
	UInt32	field_05C;	// 05C
	UInt32	field_060;	// 060
	UInt32	field_064;	// 064
	UInt32	unk_068[(0x164 - 0x068) >> 2];	// 068
	Unk164Callback	field_164;	// 164
	UInt32	field_168;	// 168 - used memory at startup
	UInt8	field_16C;	// 16C
	UInt8	field_16D;	// 16D
	// 16E
};

STATIC_ASSERT(offsetof(MemoryHeap, field_16D) == 0x16D);

class MemoryPool
{
public:
	MemoryPool(UInt32 entrySize, UInt32 totalSize, const char * name);
	~MemoryPool();
	
/*	void *	Allocate(void);
	bool	IsMember(void * buf)
	{
		if(!field_040) return false;
		if(buf < field_040) return false;
		if(buf >= ((UInt8 *)field_040) + field_110) return false;

		return true;
	}*/

	struct FreeEntry
	{
		FreeEntry	* prev;
		FreeEntry	* next;
	};
	
	char	m_name[0x40];	// 000
	void	* field_040;	// 040 - base buffer
	FreeEntry	* freeList;	// 044

	UInt32	unk_048[(0x080 - 0x048) >> 2];	// 048

	CRITICAL_SECTION	critSection;	// 080

	UInt32	unk_098[(0x100 - 0x098) >> 2];	// 098

	UInt32	field_100;	// 100 - entry size
	UInt32	field_104;	// 104
	UInt16	* field_108;	// 108 - page allocation count (FFFF - unallocated)
	UInt32	field_10C;	// 10C - size of field_108 (in UInt16s)
	UInt32	field_110;	// 110 - total size
	UInt32	field_114;	// 114 - free elements
	UInt32	field_118;	// 118
	// 11C
};
STATIC_ASSERT(offsetof(MemoryPool, field_118) == 0x118);
#endif




#if OBLIVION_VERSION == OBLIVION_VERSION_1_2_416
//	void * (WINAPI * const Oblivion_MemoryHeap_Allocate)(int size) = (void * (WINAPI *)(int  )) 0x00401AA0;
//	void * const Oblivion_MemoryHeap_AllocateB = (void *) 0x00401ABA;
//	void * const Oblivion_MemoryHeap_AllocateC = (void *) 0x00401AA9;
//	void * (WINAPI * const Oblivion_MemoryHeap_Free    )(void *p ) = (void * (WINAPI *)(void*)) 0x00401D40;
//	void * const Oblivion_MemoryHeap_FreeB = (void *) 0x00401D50;
//	void * (WINAPI * const Oblivion_MemoryPool_Allocate)(        ) = (void * (WINAPI *)(     )) 0x00402480;
//	void * (WINAPI * const Oblivion_MemoryPool_Free    )(void *p ) = (void * (WINAPI *)(void*)) 0x00402600;
//	void * (WINAPI * const Oblivion_MemoryHeap_Unk     )(int size) = (void * (WINAPI *)(int  )) 0x009816F9;
//	void * (WINAPI * const Oblivion_MemoryHeap_RawFree )(int size) = (void * (WINAPI *)(int  )) ;
//	const UInt32 Oblivion_MemoryHeap_Stats_Hook1 = 0x00402012;
//	const UInt32 Oblivion_MemoryHeap_Stats_Hook2 = 0x004020CC;
//	const UInt32 Oblivion_MemoryPool_FreeUnusedPagesStart_Hook = 0x00402740;
//	const UInt32 Oblivion_MemoryPool_FreeUnusedPages_Hook1 = 0x00402651;
//	const UInt32 Oblivion_MemoryPool_FreeUnusedPages_Hook2 = 0x0040272E;
//	const UInt32 Oblivion_MemoryPool_FreeUnusedPages_Hook2B = 0x00402724;
//	const UInt32 Oblivion_MemoryHeap_SizeOf_Hook1      =            0x00401580;
//	void (* const Oblivion_MemoryHeap_SizeOf_Hook1B)() = (void(*)())0x004015BD;
//	void (* const Oblivion_MemoryHeap_SizeOf_Hook1C)() = (void(*)())0x0040158C;
//	MemoryPool ** const g_memoryHeap_poolsBySize = (MemoryPool **)0x00B33080;		// size = 0x81  (size / 4 Bytes)
//	MemoryPool ** const g_memoryHeap_poolsByAddress = (MemoryPool **)0x00B32C80;	// size = 0x100 (address / 16 MB)
//	MemoryHeap * const oblivion_heap = (MemoryHeap*)0x00B02020;
//	extern CRITICAL_SECTION * const Oblivion_MemoryLock    = (CRITICAL_SECTION*)0xB32B80;

	//not the real definitions, just close enough to call from asm
//	static void (*oblivion_EnterCriticalSection2)()				= (void (*)())0x00401020;
//	static void (*oblivion_TryEnterCriticalSection2)()			= (void (*)())0x00401030;
//	static void (*oblivion_LeaveCriticalSection2)()				= (void (*)())0x00401040;
//	static void (*oblivion_InitializeCriticalSection2)()		= (void (*)())0x00401000;
//	static void (*oblivion_DeleteCriticalSection2)()			= (void (*)())0x00401010;
#endif

/*void * optimized_MemoryHeap_Allocate_Helper(int size) {
	__asm {
		mov esi, oblivion_heap
		push esi
		jmp Oblivion_MemoryHeap_AllocateB
	}
}*/
bool get_component_dll_path ( char *buffer, int bufsize, const char *fname ) {
	if (!GetModuleFileName(GetModuleHandle(NULL), &buffer[0], bufsize - 64)) {
		strcpy(buffer, fname);
		return false;
	}
	char *slash = strrchr(buffer, '\\');
	if (!slash) return false;
	slash[1] = 0;
	strcat(buffer, COMPONENT_DLL_PATH);
	if (!PathFileExistsA(buffer)) {
		strcat(buffer, fname);
		return false;
	}
	strcat(buffer, fname);
	return true;
}



CRITICAL_SECTION memory_performance_critical_section;
static volatile long partial_memory_time = 0;
//static volatile long partial_memory_time2 = 0;
static volatile long partial_memory_ops = -1;
static volatile long memory_worst = 0;
void show_memory_perf() {
	static UInt64 memory_time = 0;
//	static UInt64 memory_time2 = 0;
	static UInt64 memory_ops = 0;
	static UInt32 init_time_ms = 0;
	static UInt64 init_time_ticks = 0;
	UInt32 tmp1, tmp2 /*, tmp3*/;
	do {tmp1 = partial_memory_time; } while (::InterlockedCompareExchange(&partial_memory_time , 0, tmp1) != tmp1);
	do {tmp2 = partial_memory_ops;  } while (::InterlockedCompareExchange(&partial_memory_ops  , 0, tmp2) != tmp2);
//	do {tmp3 = partial_memory_time2;} while (::InterlockedCompareExchange(&partial_memory_time2, 0, tmp3) != tmp3);

	::EnterCriticalSection(&memory_performance_critical_section);
	if (!init_time_ms) {
		tmp2++;
		init_time_ms = get_time_ms();
		init_time_ticks = get_time_ticks();
	}
	UInt64 oldmt = memory_time;
	UInt64 newmt = memory_time += tmp1;
	UInt64 newops = memory_ops += tmp2;
//	UInt64 newmt2 = memory_time2 += tmp3;
	::LeaveCriticalSection(&memory_performance_critical_section);
	enum {SHIFT=24};
	if ((newmt >> SHIFT) != (oldmt >> SHIFT) && newops) {
		newmt <<= 4;
//		newmt2 <<= 4;
		int ops_div;
		char ops_sym;
		if (0) ;
		else if (newops < 1000)       {ops_sym=' '; ops_div = 1;}
		else if (newops < 1000000)    {ops_sym='k'; ops_div = 1000;}
		else if (newops < 1000000000) {ops_sym='M'; ops_div = 1000000;}
		else {ops_sym='B'; ops_div = 1000000000;}

		UInt64 time_div;
		char time_sym;
		if (0) ;
		else if (newmt < 1000)       {time_sym=' '; time_div = 1;}
		else if (newmt < 1000000)    {time_sym='k'; time_div = 1000;}
		else if (newmt < 1000000000) {time_sym='M'; time_div = 1000000;}
		else if (newmt < 1000000000000ull) {time_sym='B'; time_div = 1000000000;}
		else {time_sym='T'; time_div = 1000000000000ull;}

		double total_ticks = (get_time_ticks() - init_time_ticks);
		double total_ms = (get_time_ms() - init_time_ms + 0.5);
		double time_ratio = total_ticks / total_ms / 1000000.;
//		message("time_ratio: %.1g   newmt: %.1g   newops: %.1g", time_ratio, double(newmt), double(newops));
//		double worst = double(memory_worst) * 16.0 / time_ratio;
//		double avg = double(newmt)  / double(newops) / time_ratio;
//		double avg2 = double(newmt2)  / double(newops) / time_ratio;
//		message("avg1: %.1g   avg2: %.1g   worst: %.1g", avg, avg2, worst);
//		message("Memory Performance: %d %c ops, %d %c ticks, %.0f worst ms, %.0f avg ns, %.1f avg xs ns", 
		message("Memory Performance: %d %c ops, %d %c ticks, %.0f worst ms, %.0f avg ns", 
			int(newops / ops_div), ops_sym, 
			int(newmt / time_div), time_sym, 
			double(memory_worst) * 16.0 / time_ratio / 1000000, 
			double(newmt)  / double(newops) / time_ratio
//			,double(newmt2) / double(newops) / time_ratio
		);
	}
}
static void end_memory_measurement ( UInt32 start_time ) {
	UInt32 end_time = UInt32(get_time_ticks()>>4);
	UInt32 delta = end_time - start_time;
	if (delta > 0x10000000) delta = 0;//remove?
//	enum {THRESH=(3000 * 20)>>4};
//	UInt32 delta2 = 0; if (delta > THRESH) delta2 = delta - THRESH;
	enum {MAXSTEP=(1<<16)-1};
	int tmp;
//	do {tmp = partial_memory_time2;} while (::InterlockedCompareExchange(&partial_memory_time2, tmp+delta2, tmp) != tmp);
	do {tmp = partial_memory_time;} while (::InterlockedCompareExchange(&partial_memory_time, tmp+delta, tmp) != tmp);
	int tmp2;
	while(1) {
		tmp2 = memory_worst;
		if (delta < tmp2) break;
		int tmp3 = ::InterlockedCompareExchange(&memory_worst, delta, tmp2);
		if (tmp3 == tmp2) break;
		if (tmp3 >= delta) break;
	}
	if (!(::InterlockedIncrement(&partial_memory_ops) & ((1<<17)-1)) || ((tmp+delta) & ~((1<<25)-1))) show_memory_perf();
}
#define STARTTIME UInt32 start_time = UInt32(get_time_ticks()>>4);
#define ENDTIME   end_memory_measurement(start_time);
//#define STARTTIME __asm {push eax} __asm {push ebx} __asm {push ecx} __asm {push edx} __asm {push ebp} __asm {push esi} __asm {push edi}
//#define ENDTIME   __asm {pop  edi} __asm {pop  esi} __asm {pop  ebp} __asm {pop  edx} __asm {pop  ecx} __asm {pop  ebx} __asm {pop  eax}
//#define STARTTIME 
//#define ENDTIME   


void * __fastcall replaced_Allocate MEMORYHEAP_ALLOCATE_PARAMETERLIST {//401AA0
//	return ((char*)heap.malloc(size + 64))+32;
	if (!size) {
		//if (Settings::Heap::bEnableMessages) message("Allocating zero bytes as 1 byte");
		size = 1;
	}
	if (Settings::Heap::bZeroAllocations) {
		void *rv = heap.malloc(size);
		memset(rv, 0, size);
		return rv;
	}
	return heap.malloc(size);
}
void __fastcall replaced_Free( MemoryHeap *obheap, int nothing, void *mem ) {//401D40
//	heap.free(((char*)mem) - 32);
	if (mem) heap.free(mem);
}
void *__fastcall replaced_Reallocate( MemoryHeap *obheap, int nothing, void *mem, int size ) {//401E60
//	message("Reallocate (%X, %d)", mem, size);
//	void *rv = ((char*)heap.realloc(((char*)mem) - 32, size+64)) + 32;
//	void *rv = heap.realloc(mem, size);
	if (size <= 0 || !mem) {
		if (Settings::Heap::bEnableMessages) message("Reallocating 0x%X to %d bytes", mem, size);
	}
	if (!size) {if (mem) heap.free(mem); return NULL;}
	else if (!mem) return heap.malloc(size);
	else return heap.realloc(mem, size);
}
void __fastcall replaced_CreatePool( MemoryHeap *obheap, int nothing, int size, void *location, const char *name ) {//401F40
//	message("CreatePool %3d / %8X / %s", size, location, name);
//	if (heap.CreatePool) TerminateProcess(GetCurrentProcess(), 0);
//	if (heap.CreatePool) heap.CreatePool(size);
}
void __fastcall replaced_MemInitialize( MemoryHeap *heap, int nothing, int size, int unk ) {//401260
	message("Heap Initialization (%d, %d)", size, unk);
}
//MemProb2: called with (ecx, edi, 0), (0,edx,1), (ecx, 0x4000, 0), (esi,0,0)
//first parameter is "pass_number", often incremented when called inside a loop
//if it gets too high, Oblivion quits abruptly due to out-of-memory?
//second parameter may be a size of memory?, and appears to be ignored
//third parameter may be a flag?, does very little if that flag is true?
void __fastcall replaced_MemStats( MemoryHeap *obheap, int nothing, int *buffer, int unk ) {//401FF0
	MEMORYSTATUS mem_status;
	GlobalMemoryStatus ( & mem_status );
#if defined OBLIVION
	int &heap_field_054 = *reinterpret_cast<int*>(((UInt8*)obheap) + 0x054);
	if (heap_field_054 > mem_status.dwAvailPhys) heap_field_054 = mem_status.dwAvailPhys;
#endif
	memset(buffer, 0, 0x54);
//	buffer[0] = 1 << 27; //*total_size = 1 << 27;
//	if (heap.MemStats) TerminateProcess(GetCurrentProcess(), 0);
//	if (heap.MemStats) *total_size = heap.MemStats();
	return;
}
void __fastcall replaced_FreeUnusedPages( MemoryHeap *obheap, int nothing ) {//402740
//	message("MemoryPool::FreeUnusedPages");
	if (heap.FreePages) heap.FreePages();
	return;
}
void __cdecl replaced_MemoryCleanup( int a, int b, int c ) {//405D60 / B02184 / 40E626
	message("MemoryHeap CleanupProc called (%08X, %d, %d)", a, b, c);
//	TerminateProcess(GetCurrentProcess(), 0);
	return;
}
void replaced_MemoryDtor() {//401750
	message("MemoryHeap Dtor called");
	return;
}



void * __fastcall replaced_Allocate_profiling MEMORYHEAP_ALLOCATE_PARAMETERLIST {//401AA0
	STARTTIME
//	message("Allocate (%d)", size);
//	if (size < 8) size = 8;
	if (!size) size = 1;
	void *rv = heap.malloc(size);
//	if (size <= 0 && Settings::Heap::bEnableMessages) message("Allocating %d bytes (small): %X", size, rv);
	if (Settings::Heap::bZeroAllocations) memset(rv, 0, size);
//	message(" = %X", rv);
	ENDTIME
	return rv;
}
void __fastcall replaced_Free_profiling( MemoryHeap *obheap, int nothing, void *mem ) {//401D40
	STARTTIME
//	message("Deallocate (%X)", mem);
	if (mem) heap.free(mem);
	ENDTIME
//	return;
}
void *__fastcall replaced_Reallocate_profiling( MemoryHeap *obheap, int nothing, void *mem, int size ) {//401E60
	STARTTIME
//	message("Reallocate (%X, %d)", mem, size);
	void *rv;//rv = heap.realloc(mem, size);
	if (!size) {rv = NULL; if (mem) heap.free(mem);}
	else if (!mem) rv = heap.malloc(size);
	else rv = heap.realloc(mem, size);
	if (size <= 0 || !mem) {
		if (Settings::Heap::bEnableMessages)
			message("Reallocating to %d bytes (%X -> %X)", size, mem, rv);
	}
	ENDTIME
	return rv;
}

class Heapish {
	//unknown heap-like object responsible for deallocating some types of memory
	//needed for FO3 & FNV
public:
	virtual void vtable_0x00() = 0;
	virtual void vtable_0x04() = 0;
	virtual void vtable_0x08() = 0;
	virtual void vtable_0x0C(void *mem, int zero) = 0;//free
	virtual void vtable_0x10() = 0;
	virtual void vtable_0x14() = 0;
	virtual void vtable_0x18() = 0;
	virtual void vtable_0x1C() = 0;
	virtual void vtable_0x20() = 0;
	virtual int  vtable_0x24(void *mem, int zero) = 0;//get_size
};
typedef Heapish *(__fastcall  *GetHeapish_t) ( void *, int, void * );
/*int __fastcall replaced_GetSize2( MemoryHeap *obheap, int nothing, void *mem ) {
	if (heap.is_in_heap_fast && heap.is_in_heap_fast(mem)) {
		if (heap.get_size) return heap.get_size(mem);
	}
	FO3_unknown_heapish_object *tmp = func_86B7D0(obheap, 0, mem);
	if (!tmp) tmp = func_86B820(obheap, 0, mem);
	if (!tmp) {
		return _msize(mem);
	}
	return tmp->vtable_0x24(mem, 0);
}*/
void __fastcall replaced_Free2( MemoryHeap *bethheap, int nothing, void *mem ) {
	if (!mem) return;
	if (heap.is_in_heap_fast && heap.is_in_heap_fast(mem)) {
/*		static bool already = false;
		if (!already) {
			already = true;
			message("replaced Bethesda free in use");
		}*/
		heap.free(mem);
		return;
	}
#if defined FALLOUT
	if (!((char*)bethheap)[0] || !((int*)bethheap)[0x100/4]) {
#elif defined NEW_VEGAS
	if (!((char*)bethheap)[0] || !((int*)bethheap)[0x110/4]) {
#else
	if (true) {
#endif
		message("free prior to Bethesda heap intialization?");
		::free(mem);
		return;
	}
	Heapish *heapish = ((GetHeapish_t)Hook_Memory_GetHeapish)(bethheap, 0, mem);
	if (heapish) {
		static bool already = false;
		if (!already) {
			already = true;
			message("Unknown Heapish free in use");
		}
		message("Heapish vtable: %x", *(void**)heapish);
		heapish->vtable_0x0C(mem, 0);
		return;
	}
	::free(mem);
}

const HANDLE dummy_heap_handle = HANDLE(-1);
int num_heaps_created = 0;
LPVOID __stdcall replaced_HeapAlloc(HANDLE hHeap, DWORD dwFlags, DWORD dwBytes) {
//	if (hHeap != dummy_heap_handle) error("replaced_HeapAlloc - wrong heap");
	//return ::malloc(dwBytes);
	return heap.malloc(dwBytes);
}
HANDLE __stdcall replaced_HeapCreate(DWORD flOptions, DWORD dwInitialSize, DWORD dwMaximumSize) {
	::EnterCriticalSection(&memory_performance_critical_section);
	message("replaced_HeapCreate");
	if (++num_heaps_created != 1) {
		error("replaced_HeapCreate - already");
	}
	::LeaveCriticalSection(&memory_performance_critical_section);
	return dummy_heap_handle;
}
DWORD __stdcall replaced_HeapDestroy(HANDLE hHeap) {
	::EnterCriticalSection(&memory_performance_critical_section);
//	if (hHeap != dummy_heap_handle) error("replaced_HeapDestroy - wrong heap");
//	if (--num_heaps_created != 0) {
//		error("replaced_HeapDestroy - already");
//	}
	::LeaveCriticalSection(&memory_performance_critical_section);
	return TRUE;
}
BOOL __stdcall replaced_HeapFree(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem) {
//	if (hHeap != dummy_heap_handle) error("replaced_HeapFree - wrong heap");
	//::free(lpMem);
	heap.free(lpMem);
	return TRUE;
}
LPVOID __stdcall replaced_HeapReAlloc(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem, DWORD dwBytes) {
//	if (hHeap != dummy_heap_handle) error("replaced_HeapReAlloc - wrong heap");
//	return ::realloc(lpMem, dwBytes);
	return heap.realloc(lpMem, dwBytes);
}
DWORD __stdcall replaced_HeapSize(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem) {
//	if (hHeap != dummy_heap_handle) error("replaced_HeapSize - wrong heap");
//	return _msize(lpMem);
	return heap.get_size(lpMem);
}

void hook_heap() {
	::InitializeCriticalSectionAndSpinCount(&memory_performance_critical_section, 8000);
	UInt32 addr;

	if (!Settings::Master::bExperimentalStuff || !Settings::Experimental::bAlternateHeapHooks) {
		addr = Hook_Memory_Initialize;if (addr) WriteRelJump( addr, UInt32(replaced_MemInitialize) );
		addr = Hook_Memory_FreeUnusedPagesStart;if (addr) WriteRelJump(addr, UInt32(replaced_FreeUnusedPages));
		addr = Hook_Memory_Allocate;  if (addr) WriteRelJump( addr, UInt32(replaced_Allocate) );
		if (Hook_Memory_GetHeapish && heap.is_in_heap_fast) {
			message("using GetHeapish in heap hooks");
			addr = Hook_Memory_Free;  if (addr) WriteRelJump( addr, UInt32(replaced_Free2) );
		}
		else {
			if (Hook_Memory_GetHeapish && !heap.is_in_heap_fast)
				message("GetHeapish is not supported on this heap algorithm");
			addr = Hook_Memory_Free;  if (addr) WriteRelJump( addr, UInt32(replaced_Free) );
		}
		addr = Hook_Memory_Reallocate;if (addr) WriteRelJump( addr, UInt32(replaced_Reallocate) );
		addr = Hook_Memory_CreatePool;if (addr) WriteRelJump( addr, UInt32(replaced_CreatePool) );
		addr = Hook_Memory_Stats;     if (addr) WriteRelJump( addr, UInt32(replaced_MemStats) );
		addr = Hook_Memory_Cleanup;   if (addr) WriteRelJump( addr, UInt32(replaced_MemoryCleanup) );
		addr = Hook_Memory_Dtor;      if (addr) WriteRelJump( addr, UInt32(replaced_MemoryDtor) );
	}
	else if (heap.get_size) {
		message("using alternate hooks for heap stuff");
		addr = Hook_Memory_Allocate;  if (addr) WriteRelJump( addr, UInt32(replaced_Allocate) );
	//	addr = Hook_Memory_Free;      if (addr) WriteRelJump( addr, UInt32(replaced_FO3_Free) );
	//	addr = 0x86B8C0;              if (addr) WriteRelJump( addr, UInt32(replaced_FO3_GetSize) );
		addr = Hook_Memory_CreatePool;if (addr) WriteRelJump( addr, UInt32(replaced_CreatePool) );

		//replace win32 calls:
		addr = Hook_HeapDestroy;if (addr) SafeWrite32(addr, UInt32(replaced_HeapDestroy));
		addr = Hook_HeapCreate; if (addr) SafeWrite32(addr, UInt32(replaced_HeapCreate));
		addr = Hook_HeapAlloc;  if (addr) SafeWrite32(addr, UInt32(replaced_HeapAlloc));
		addr = Hook_HeapFree;   if (addr) SafeWrite32(addr, UInt32(replaced_HeapFree));
		addr = Hook_HeapReAlloc;if (addr) SafeWrite32(addr, UInt32(replaced_HeapReAlloc));
		addr = Hook_HeapSize;   if (addr) SafeWrite32(addr, UInt32(replaced_HeapSize));
	}
	else {
		message("alternate heap hooks not supported for this heap algorithm atm");
	}
}
void hook_heap_profiling() {
	::InitializeCriticalSectionAndSpinCount(&memory_performance_critical_section, 8000);
	UInt32 addr;
	addr = UInt32(Hook_Memory_FreeUnusedPagesStart);
	if (addr) WriteRelJump(addr, UInt32(replaced_FreeUnusedPages));

	addr = Hook_Memory_Allocate;  if (addr) WriteRelJump( addr, UInt32(replaced_Allocate_profiling) );
	if (Hook_Memory_GetHeapish) {
		message("using GetHeapish in heap hooks");
		addr = Hook_Memory_Free;  if (addr) WriteRelJump( addr, UInt32(replaced_Free2) );
	}
	else {
		addr = Hook_Memory_Free;  if (addr) WriteRelJump( addr, UInt32(replaced_Free) );
	}
	addr = Hook_Memory_Reallocate;if (addr) WriteRelJump( addr, UInt32(replaced_Reallocate_profiling) );
	addr = Hook_Memory_CreatePool;if (addr) WriteRelJump( addr, UInt32(replaced_CreatePool) );
	addr = Hook_Memory_Initialize;if (addr) WriteRelJump( addr, UInt32(replaced_MemInitialize) );
	addr = Hook_Memory_Stats;     if (addr) WriteRelJump( addr, UInt32(replaced_MemStats) );
	addr = Hook_Memory_Cleanup;   if (addr) WriteRelJump( addr, UInt32(replaced_MemoryCleanup) );
	addr = Hook_Memory_Dtor;      if (addr) WriteRelJump( addr, UInt32(replaced_MemoryDtor) );
}
static bool is_in_heap_wrap(void *mem) {
	return bool(heap.get_size(mem));
}
bool load_external_heap(const char *dll_name, const char *malloc_name, const char *free_name, const char *realloc_name, const char *msize_name, const char *is_in_heap_name) {
	char buffy[4096];
	HMODULE hm = NULL;
	if (!get_component_dll_path(&buffy[0], 4000, dll_name)) {
		message("Failed to find %s", buffy);
		return false;
	}
	hm = LoadLibrary( buffy );
	if (!hm) {
		message("Failed to load %s", buffy);
		return false;
	}
	heap.malloc = (void*(*)(size_t)) GetProcAddress(hm, malloc_name);
	heap.free = (void(*)(void*))GetProcAddress(hm, free_name);
	heap.realloc = (void*(*)(void*,size_t))GetProcAddress(hm, realloc_name);
	heap.get_size = (size_t(*)(void*))GetProcAddress(hm, msize_name);
	heap.is_in_heap_fast = (bool(*)(void*))GetProcAddress(hm, is_in_heap_name);
	if (!heap.is_in_heap_fast) heap.is_in_heap_fast = is_in_heap_wrap;
	if (!heap.is_in_heap) heap.is_in_heap = is_in_heap_wrap;
	if (!heap.malloc || !heap.free || !heap.realloc || !heap.get_size) {
		message("Invalid %s ?", buffy);
		return false;
	}
	return true;
}
bool optimize_memoryheap(int mode, bool do_profiling) {
	heap.malloc = malloc;
	heap.free = free;
	heap.realloc = realloc;
	heap.get_size = _msize;
	heap.is_in_heap_fast = NULL;
	heap.is_in_heap = NULL;
	heap.CreatePool = NULL;
	heap.FreePages = NULL;
	heap.MemStats = NULL;
	bool hook_needed = false;
	if (mode == 0) {
		message("Heap Replacement using algorithm #0: Not optimizing MemoryHeap");
	}
	else if (mode == 3) {
		message("Heap Replacement using algorithm #3: attempting to replace their custom heap manager with my SimpleHeap1");
		SimpleHeap1::init();
		heap.malloc  = SimpleHeap1::allocate;
		heap.free    = SimpleHeap1::deallocate;
		heap.realloc = SimpleHeap1::reallocate;
		heap.is_in_heap_fast = SimpleHeap1::is_in_heap_fast;
		heap.is_in_heap   = SimpleHeap1::is_in_heap;
		heap.get_size = SimpleHeap1::get_size;
		hook_needed = true;
	}
/*	else if (mode == 6) {
		message("Heap Replacement using algorithm #6: attempting to replace the Oblivion heap manager with my SimpleHeap2");
		;            ; SimpleHeap2::init();
		heap.malloc  = SimpleHeap2::allocate;
		heap.free    = SimpleHeap2::deallocate;
		heap.realloc = SimpleHeap2::reallocate;
		hook_needed = true;
	}
	else if (mode == 7) {
		message("Heap Replacement using algorithm #7: attempting to replace the Oblivion heap manager with my ThreadHeap1");
		;            ; ThreadHeap1::init();
		heap.malloc  = ThreadHeap1::allocate;
		heap.free    = ThreadHeap1::deallocate;
		heap.realloc = ThreadHeap1::reallocate;
		hook_needed = true;
	}*/
	else if (mode == 5) {
		message("Heap Replacement using algorithm #5: attempting to completely replace their custom heap manager with my ThreadHeap2");
		;            ; ThreadHeap2::init();
		heap.malloc  = ThreadHeap2::allocate;
		heap.free    = ThreadHeap2::deallocate;
		heap.realloc = ThreadHeap2::reallocate;
		heap.is_in_heap_fast = ThreadHeap2::is_in_heap_fast;
		heap.get_size = ThreadHeap2::get_size;
		hook_needed = true;
	}
	else if (mode == 6) {
		message("Heap Replacement using algorithm #6: attempting to completely replace the Oblivion heap manager with my ThreadHeap3");
		ThreadHeap3::init();
		heap = heap_ThreadHeap3;
		/*heap.malloc  = ThreadHeap3::allocate;
		heap.free    = ThreadHeap3::deallocate;
		heap.realloc = ThreadHeap3::reallocate;
		heap.is_in_heap_fast = ThreadHeap3::is_in_heap_fast;
		heap.get_size = ThreadHeap3::get_size;*/
		hook_needed = true;
	}
	else if (mode == 2) {
		message("Heap Replacement using algorithm #2: attempting to replace their custom heap manager with libc malloc/free");
		hook_needed = true;
	}
	else if (mode == 1) {
		message("Heap Replacement using algorithm #1: attempting to replace their custom heap manager with FastMM4 / BorlndMM.dll");
		if (!load_external_heap("BorlndMM.dll", "GetMemory", "FreeMemory", "ReallocMemory", "GetAllocMemSize", ""))
			return false;
		else hook_needed = true;
		/*if (true) {
			char buffy[4096];
			HMODULE hm = NULL;
			//"C:\\Program Files\\Oblivion\\Data\\obse\\plugins\\ComponentDLLs\\BorlndMM.dll"
			//if (!get_component_dll_path(&buffy[0], 4000, "debugMM.dll")) {
			if (!get_component_dll_path(&buffy[0], 4000, "BorlndMM.dll")) {
				message("Failed to find %s", buffy);
				return false;
			}
			hm = LoadLibrary( buffy );
			if (!hm) {
				message("Failed to load %s", buffy);
				return false;
			}
			heap.malloc = (void*(*)(size_t)) GetProcAddress(hm, "GetMemory");
			heap.free = (void(*)(void*))GetProcAddress(hm, "FreeMemory");
			heap.realloc = (void*(*)(void*,size_t))GetProcAddress(hm, "ReallocMemory");
			heap.get_size = (size_t(*)(void*))GetProcAddress(hm, "GetAllocMemSize");
			if (!heap.malloc || !heap.free || !heap.realloc) {
				message("Invalid %s ?", buffy);
				return false;
			}
		}
		hook_needed = true;*/
	}
	else if (mode == 11) {
		if (!load_external_heap("debugMM.dll", "GetMemory", "FreeMemory", "ReallocMemory", "GetAllocMemSize", ""))
			return false;
		else hook_needed = true;
	}
	else if (mode == 4) {
		message("Heap Replacement using algorithm #4: attempting to replace the Oblivion heap manager with TBBMM (tbbmalloc)");
		if (!load_external_heap("tbbmalloc.dll", "scalable_malloc", "scalable_free", "scalable_realloc", "scalable_msize", ""))
			return false;
		else hook_needed = true;
	}
	else if (mode == 9) {
		message("Heap Replacement using algorithm #9: attempting to replace the Oblivion heap manager with nedmalloc");
		if (true) {
			char buffy[4096];
			HMODULE hm = NULL;
			if (!get_component_dll_path(&buffy[0], 4000, "nedmalloc.dll")) {
				message("Failed to find %s", buffy);
				return false;
			}
			hm = LoadLibrary( buffy );
			if (!hm) {
				message("Failed to load %s", buffy);
				return false;
			}
			heap.malloc = (void*(*)(size_t)) GetProcAddress(hm, "nedmalloc");
			heap.free = (void(*)(void*))GetProcAddress(hm, "nedfree");
			heap.realloc = (void*(*)(void*, size_t))GetProcAddress(hm, "nedrealloc");
			//heap.get_size = ?
			if (!heap.malloc || !heap.free || !heap.realloc) {
				message("Invalid %s ?", buffy);
				return false;
			}
		}
		hook_needed = true;
	}
	else if (mode == 8) {
		message("Heap Replacement using algorithm #8: attempting to replace the Oblivion heap manager with tcmalloc (Google's heap)");
		if (!load_external_heap("libtcmalloc_minimal.dll", "tc_malloc", "tc_free", "tc_realloc", "tc_malloc_size", ""))
			return false;
		else hook_needed = true;
	}
	else if(mode = 10){
		message("Heap Replacement using algorithm #9: attempting to replace the Oblivion heap manager with ROBAlloc");
		if (true) {
			char buffy[4096];
			HMODULE hm = NULL;
			if (!get_component_dll_path(&buffy[0], 4000, "roballoc.dll")) {
				message("Failed to find %s", buffy);
				return false;
			}
			hm = LoadLibrary( buffy );
			if (!hm) {
				message("Failed to load %s", buffy);
				return false;
			}
			heap.malloc = (void*(*)(size_t)) GetProcAddress(hm, "obAlloc");
			heap.free = (void(*)(void*))GetProcAddress(hm, "obFree");
			heap.realloc = (void*(*)(void*, size_t))GetProcAddress(hm, "obRealloc");
			heap.get_size =  (size_t(*)(void*))GetProcAddress(hm, "obSizePage");
			heap.is_in_heap = (bool(*)(void*))GetProcAddress(hm, "obIsInHeap");
			//heap.get_size = ?
			if (!heap.malloc || !heap.free || !heap.realloc) {
				message("Invalid %s ?", buffy);
				return false;
			}
		}
		hook_needed = true;
	}
	else {
		message("Unrecognized Heap replacement algorithm #: Not replacing heap");
	}
	if (hook_needed) {
		if (!Hook_Memory_Allocate) {
			message(" Hooks for memory heap replacement are missing; aborting memory heap replacement.");
		}
		else if (!do_profiling) hook_heap();
		else {
			hook_heap_profiling();
			message("MemoryHeap profiling enabled");
		}
	}

	if (false && hook_needed) {
		message("beginning single-threaded heap test");
		message("allocated some: %X", heap.malloc(14));
		message("allocated some: %X", heap.malloc(14));
		heap.free(heap.malloc(14));
		message("freed some");
		std::vector<void*> ptrs;
		ptrs.resize(16183);
		for (int i = 0; i < ptrs.size(); i++) ptrs[i] = NULL;
		for (int i = 0; i < 10000; i++) ptrs[rand() % ptrs.size()] = heap.malloc(12);
		message("allocated a bunch");
		for (int i = 0; i < 1000000; i++) {
			int index = rand() % ptrs.size();
			if (ptrs[index] == NULL) {
				ptrs[index] = heap.malloc(12);
			}
			else if (!(rand() % 3)) {
				ptrs[index] = heap.realloc(ptrs[index], rand() & 255);
			}
			else {
				heap.free(ptrs[index]);
				ptrs[index] = NULL;
			}
//			if (!(i&0)) message("%d", i);
		}
		message("allocated and freed and realloced a bunch");
		message("beginning memory hook test");
		void *blah = allocate_from_memoryheap ( 17 );
		message("allocated a block");
		free_to_memoryheap(blah);
		message("freed a block");
		message("done with memory hook test");
	}

	return true;
}

void *allocate_from_memoryheap ( size_t size ) {
#if defined OBLIVION
	return ((void *(__fastcall *) MEMORYHEAP_ALLOCATE_PARAMETERLIST)Hook_Memory_Allocate)((MemoryHeap*)Hook_MemoryHeap, 0, size, 0 );
#else
	return ((void *(__fastcall *) MEMORYHEAP_ALLOCATE_PARAMETERLIST)Hook_Memory_Allocate)((MemoryHeap*)Hook_MemoryHeap, 0, size );
#endif
}
void  free_to_memoryheap       ( void *ptr   ) {
	((void (__fastcall *) (MemoryHeap *heap, int nothing, void *ptr))Hook_Memory_Free)((MemoryHeap*)Hook_MemoryHeap, 0, ptr );
}
void *realloc_from_memoryheap  ( void *ptr, size_t new_size ) {
	return ((void *(__fastcall *) (MemoryHeap *heap, int nothing, void *ptr, int size))Hook_Memory_Reallocate)((MemoryHeap*)Hook_MemoryHeap, 0, ptr, new_size );
}
