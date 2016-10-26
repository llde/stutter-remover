#include "main.h"

#if defined NEW_VEGAS

UInt64 Hook_target;
const char *Hook_target_s;

float * Hook_FixedTimePerFrame_iFPSClamp = (float*)0x0;
void ** Hook_NiDX9RendererPtr = (void**) 0x0;
DWORD (WINAPI *** const Hook_GetTickCount)() = (DWORD (WINAPI ***)()) 0x0;

GetTickCount_t * Hook_GetTickCount_Indirect = (GetTickCount_t*) 0x0;//0x00FD5060;
void ** Hook_EnterCriticalSection      = (void**)0x0;//0x00FD505C;
void ** Hook_TryEnterCriticalSection   = (void**)0x0;//0x00FD51B0;
void ** Hook_LeaveCriticalSection      = (void**)0x0;//0x00FD5100;
void ** Hook_InitializeCriticalSection = (void**)0x0;//0x00FD5054;
void ** Hook_DeleteCriticalSection     = (void**)0x0;//0x00FD5058;
void ** Hook_InitializeCriticalSectionAndSpinCount = (void**)0x0;//0x00FD5130;

void ** Hook_DirectInput8Create = (void**)0x0;
UInt32 Hook_MemoryHeap = 0x0;
UInt32 Hook_Memory_Allocate   = 0x0;
UInt32 Hook_Memory_Free       = 0x0;
UInt32 Hook_Memory_Reallocate = 0x0;
UInt32 Hook_Memory_CreatePool = 0x0;
UInt32 Hook_Memory_Initialize = 0x0;
UInt32 Hook_Memory_Stats      = 0x0;
UInt32 Hook_Memory_Cleanup    = 0x0;
UInt32 Hook_Memory_Cleanup2   = 0x0;
UInt32 Hook_Memory_Dtor       = 0x0;
UInt32 Hook_Memory_FreeUnusedPagesStart = 0x0;
UInt32 Hook_Memory_GetHeapish = 0x0;
UInt32 Hook_HeapCreate  = 0x0;
UInt32 Hook_HeapDestroy = 0x0;
UInt32 Hook_HeapAlloc   = 0x0;
UInt32 Hook_HeapReAlloc = 0x0;
UInt32 Hook_HeapFree    = 0x0;
UInt32 Hook_HeapSize    = 0x0;

UInt32 Hook_ExitPath = 0x0;
void * Hook_EnterLightCS                    = (void*) 0x0;
void * Hook_TryEnterLightCS                 = (void*) 0x0;
void * Hook_LeaveLightCS                    = (void*) 0x0;
void * Hook_InterlockedCompareExchange      = (void**)0x0;
void ***Hook_EnterCriticalSection_B      = (void***)0x0;
void ***Hook_TryEnterCriticalSection_B   = (void***)0x0;
void ***Hook_LeaveCriticalSection_B      = (void***)0x0;
void ***Hook_InitializeCriticalSection_B = (void***)0x0;
void ***Hook_DeleteCriticalSection_B     = (void***)0x0;
UInt32 Hook_Hashtable_IntToIndex1   = 0x0;
UInt32 Hook_Hashtable_IntToIndex2   = 0x0;
UInt32 Hook_Hashtable_IntToIndex3   = 0x0;
UInt32 Hook_Hashtable_IntToIndex4   = 0x0;
UInt32 Hook_Hashtable_StringToIndex1= 0x0;
UInt32 Hook_Hashtable_StringToIndex2= 0x0;
UInt32 Hook_Hashtable_StringToIndex3= 0x0;
UInt32 Hook_Hashtable_StringToIndex4= 0x0;
UInt32 Hook_Hashtable_CharToIndex1  = 0x0;
UInt32 Hook_Hashtable_CharToIndex2  = 0x0;
UInt32 Hook_memset1                 = 0x0;
UInt32 Hook_memset2                 = 0x0;
UInt32 Hook_rand                    = 0x0;
UInt32 Hook_srand                   = 0x0;
UInt32 Hook_Sleep                   = 0x0;
UInt32 Hook_MainGameFunction        = 0x0;
UInt32 Hook_MainGameFunctionCaller  = 0x0;

void init_FO3_1_7_0_3() {
	//else if (hash == 0xF4365A9A) {
	message("Fallout3 1.7.0.3 (final version) (minimally supported)");
	Hook_target_s = "Fallout3 1.7.0.3";
	Hook_target = SR_MAKE_VERSION( 1, 1, 7, 0, 3);
	Hook_GetTickCount_Indirect = (GetTickCount_t*) 0xD9B090;
	Hook_EnterCriticalSection      = (void**)0xD9B09C;
	Hook_TryEnterCriticalSection   = (void**)0xD9B108;
	Hook_LeaveCriticalSection      = (void**)0xD9B0A0;
	Hook_InitializeCriticalSection = (void**)0xD9B094;
	Hook_DeleteCriticalSection     = (void**)0xD9B098;
	Hook_InitializeCriticalSectionAndSpinCount = (void**)0xD9B1AC;
}
void init_FNV_1_0_0_240() {
	//else if (hash == 0xF2C8A1E5) {
	message("FalloutNV 1.0.0.240 (original release version) (minimally supported)");
	Hook_target_s = "FalloutNV 1.0.0.240";
	Hook_target = SR_MAKE_VERSION( 2, 1, 0, 0, 240);
	Hook_GetTickCount_Indirect = (GetTickCount_t*) 0x00FD5060;
	Hook_EnterCriticalSection      = (void**)0x00FD505C;
	Hook_TryEnterCriticalSection   = (void**)0x00FD51B0;
	Hook_LeaveCriticalSection      = (void**)0x00FD5100;
	Hook_InitializeCriticalSection = (void**)0x00FD5054;
	Hook_DeleteCriticalSection     = (void**)0x00FD5058;
	Hook_InitializeCriticalSectionAndSpinCount = (void**)0x00FD5130;
}
void init_FNV_1_1_1_271() {
	//else if (hash == 0xEE5B8433) {
	message("FalloutNV 1.1.1.271 (update 2) (minimally supported)");
	Hook_target_s = "FalloutNV 1.1.1.271";
	Hook_target = SR_MAKE_VERSION( 2, 1, 1, 1, 271);
	Hook_GetTickCount_Indirect = (GetTickCount_t*) 0x00FD6060;
	Hook_EnterCriticalSection      = (void**)0x00FD605C;
	Hook_TryEnterCriticalSection   = (void**)0x00FD61B0;
	Hook_LeaveCriticalSection      = (void**)0x00FD6100;
	Hook_InitializeCriticalSection = (void**)0x00FD6054;
	Hook_DeleteCriticalSection     = (void**)0x00FD6058;
	Hook_InitializeCriticalSectionAndSpinCount = (void**)0x00FD6130;
}
void init_FNV_1_2_0_285() {
	//else if (hash == 0x38C42F4E) {
	message("FalloutNV 1.2.0.285 (update 3)");
	Hook_target_s = "FalloutNV 1.2.0.285";
	Hook_target = SR_MAKE_VERSION( 2, 1, 2, 0, 285);
	Hook_GetTickCount_Indirect = (GetTickCount_t*) 0x00FD8060;
	Hook_EnterCriticalSection      = (void**)0x00FD805C;
	Hook_TryEnterCriticalSection   = (void**)0x00FD81B0;
	Hook_LeaveCriticalSection      = (void**)0x00FD8100;
	Hook_InitializeCriticalSection = (void**)0x00FD8054;
	Hook_DeleteCriticalSection     = (void**)0x00FD8058;
	Hook_InitializeCriticalSectionAndSpinCount = (void**)0x00FD8130;
	Hook_ExitPath = 0x0086789E;
	Hook_Hashtable_IntToIndex1 = 0x479380;
	Hook_Hashtable_StringToIndex1= 0x4053D0;
	Hook_Hashtable_StringToIndex2= 0x0;//0x657080;
	Hook_MainGameFunction = 0x86A8A0;
	Hook_MainGameFunctionCaller = 0x867613;
	Hook_DirectInput8Create = (void**)0xEE1A70;
	Hook_memset1 = 0xEBFD20;
	Hook_memset2 = 0x403DC0;//why?
}
void init_FNV_1_2_0_314() {
	//else if (hash == 0x0) {
	message("FalloutNV 1.2.0.314 (update 4)");
	Hook_target = SR_MAKE_VERSION( 2, 1, 2, 0, 314);
	Hook_target_s = "FalloutNV 1.2.0.314";
	Hook_GetTickCount_Indirect = (GetTickCount_t*) 0x00FD9060;
	Hook_EnterCriticalSection      = (void**)0x00FD905C;
	Hook_TryEnterCriticalSection   = (void**)0x00FD91B0;
	Hook_LeaveCriticalSection      = (void**)0x00FD9100;
	Hook_InitializeCriticalSection = (void**)0x00FD9054;
	Hook_DeleteCriticalSection     = (void**)0x00FD9058;
	Hook_InitializeCriticalSectionAndSpinCount = (void**)0x00FD9130;
	Hook_ExitPath = 0x0867cde;
	Hook_Hashtable_IntToIndex1 = 0x558120;//0x558120,*0x848D20*,0xAE3730,0xC70590? and maybe even 0x44C3C0,C39CE0
	Hook_Hashtable_IntToIndex2 = 0xC70590;
	Hook_Hashtable_IntToIndex3 = 0xAE3730;
//	Hook_Hashtable_IntToIndex4 = 0x848D20;//this one seems different
	Hook_Hashtable_StringToIndex1= 0xB5AE20;//0xB5AE20,0xA07E80,0xA080D0,0x6572E0,
//	Hook_Hashtable_StringToIndex2= 0xA07E80;//considers strings as upper case
	//Hook_Hashtable_CharToIndex = 0x68FE20;//68FE20,860AD0
	//Hook_Hashtable_Other = 0x0;//6996F0

	Hook_MainGameFunction = 0x86AD00;
	Hook_MainGameFunctionCaller = 0x867A53;
	Hook_DirectInput8Create = (void**)0xA1D595;
	Hook_memset1 = 0x00EC0570;
	Hook_memset2 = 0x00403E10;//wrapper
	Hook_Sleep   = 0x00FD9104;
	Hook_rand    = 0x00EC5168;
	Hook_srand   = 0x00EC5156;

	Hook_HeapAlloc   = 0xFD91A0;
	Hook_HeapReAlloc = 0xFD919C;
	Hook_HeapFree    = 0xFD91A4;
	Hook_HeapSize    = 0xFD91A8;
	Hook_HeapCreate  = 0xFD9254;
	Hook_HeapDestroy = 0x0;
	Hook_Memory_CreatePool = 0xA9ED50;
}
void init_FNV_1_2_0_352() {
	//else if (hash == 0x0) {
	message("FalloutNV 1.2.0.352 (update 5)");
	Hook_target = SR_MAKE_VERSION( 2, 1, 2, 0, 352);
	Hook_target_s = "FalloutNV 1.2.0.352";
	Hook_GetTickCount_Indirect = (GetTickCount_t*) 0x00FD9060;
	Hook_EnterCriticalSection      = (void**)0x00FD905C;
	Hook_TryEnterCriticalSection   = (void**)0x00FD91B0;
	Hook_LeaveCriticalSection      = (void**)0x00FD9100;
	Hook_InitializeCriticalSection = (void**)0x00FD9054;
	Hook_DeleteCriticalSection     = (void**)0x00FD9058;
	Hook_InitializeCriticalSectionAndSpinCount = (void**)0x00FD9130;
	Hook_ExitPath = 0x0;
	//hash functions:
	// int
	//	0x8417E0	many vtables
	//	0x8489B0	2 vtables, extra  parameter (unused)
	//	0xA19550	18 vtables
	//	0xC702E0	many vtables, optimized 
	//char:
	//	0x860A30	2 vtables, not optimized
	//word: 
	//	0x49CC30	6 vtables, not optimized
	//other: 
	//	0x487100	4  VTs, function calls, loop
	//	0x487410	8  VTs, function call, loop
	//	0x69A4C0	2  VTs, function call, extra (unused?) parameter
	//	0xA07E70	1  VTs, string with case conversion
	//	0xA080C0	6  VTs, string
	//	0xE8D840	20 VTs, string, optimized
	//	0xEBC000	2  VTs, string
	//	0xEBC150	1  VTs, string with case conversion
	Hook_Hashtable_IntToIndex1 = 0xC702E0;
	Hook_Hashtable_IntToIndex2 = 0xA19550;
	Hook_Hashtable_IntToIndex3 = 0x8417E0;
//	Hook_Hashtable_IntToIndex4 = 0x8489B0;//extra (unused) parameter
	Hook_Hashtable_StringToIndex1= 0xE8D840;
//	Hook_Hashtable_StringToIndex2= 0xA080C0;
	//Hook_Hashtable_CharToIndex = 0x860A30;
//	Hook_Hashtable_WordToIndex = 0x49CC30;
	//Hook_Hashtable_Other = 0x0;

	Hook_MainGameFunction = 0x86A7F0;
	Hook_MainGameFunctionCaller = 0x867553;
	Hook_DirectInput8Create = (void**)0xEE2310;
	Hook_memset1 = 0x00EC05C0;
	Hook_memset2 = 0x0;
	Hook_Sleep   = 0x00FD9104;
	Hook_rand    = 0x00EC51A8;
	Hook_srand   = 0x00EC5196;

	Hook_HeapAlloc   = 0xFD91A0;
	Hook_HeapReAlloc = 0xFD919C;
	Hook_HeapFree    = 0xFD91A4;
	Hook_HeapSize    = 0xFD91A8;
	Hook_HeapCreate  = 0xFD9254;
	Hook_HeapDestroy = 0x0;

	Hook_Memory_CreatePool = 0xA9ECC0;
	Hook_Memory_Allocate   = 0xA9EE40;
	Hook_Memory_Free       = 0xA9F050;
	Hook_Memory_Reallocate = 0xA9F140;
//	Hook_Memory_Reallocate2= 0xA9F1F0;//extra (unused) parameters
}
void init_FNV_1_3_0_452() {
	//else if (hash == 0x0) {
	message("FalloutNV 1.3.0.452 (update 6)");
	Hook_target = SR_MAKE_VERSION( 2, 1, 3, 0, 452);
	Hook_target_s = "FalloutNV 1.3.0.452";

	Hook_GetTickCount_Indirect = (GetTickCount_t*) 0x00FDC060;
	Hook_EnterCriticalSection      = (void**)0x00FDC05C;
	Hook_TryEnterCriticalSection   = (void**)0x00FDC1B0;
	Hook_LeaveCriticalSection      = (void**)0x00FDC100;
	Hook_InitializeCriticalSection = (void**)0x00FDC054;
	Hook_DeleteCriticalSection     = (void**)0x00FDC058;
	Hook_InitializeCriticalSectionAndSpinCount = (void**)0x00FDC130;
	Hook_ExitPath = 0x0;

//	Hook_Hashtable_IntToIndex1 = 0x0;
//	Hook_Hashtable_StringToIndex1= 0x0;

	Hook_MainGameFunction = 0x86C8E0;
	Hook_MainGameFunctionCaller = 0x869653;
	Hook_DirectInput8Create = (void**)0xEE5220;
	Hook_memset1 = 0x00EC34C0;
	Hook_memset2 = 0x0;
	Hook_Sleep = 0x00FDC104;
	Hook_rand    = 0x00EC80B8;
	Hook_srand   = 0x00EC80A6;

	Hook_HeapAlloc   = 0x0;
	Hook_HeapReAlloc = 0x0;
	Hook_HeapFree    = 0x0;
	Hook_HeapSize    = 0x0;
	Hook_HeapCreate  = 0x0;
	Hook_HeapDestroy = 0x0;

	Hook_Memory_CreatePool = 0x00AA1100;
	Hook_Memory_Allocate   = 0x00AA1280;
	Hook_Memory_Free       = 0x00AA14A0;
	Hook_Memory_Reallocate = 0x00AA1590;
//	Hook_Memory_Reallocate2= 0x00AA1640;//extra (unused) parameters
	Hook_Memory_GetHeapish = 0x00AA19E0;
}

void init_FNV_1_4_0_525() {
	//else if (hash == 0x0) {
	message("FalloutNV 1.4.0.525 (update 7)");
	Hook_target = SR_MAKE_VERSION( 2, 1, 4, 0, 525);
	Hook_target_s = "FalloutNV 1.4.0.525";

	Hook_NiDX9RendererPtr = (void**)0x11F4748;

	Hook_GetTickCount_Indirect = (GetTickCount_t*) 0x00FDF060;
	Hook_EnterCriticalSection      = (void**)0x00FDF05C;
	Hook_TryEnterCriticalSection   = (void**)0x00FDF1B0;
	Hook_LeaveCriticalSection      = (void**)0x00FDF100;
	Hook_InitializeCriticalSection = (void**)0x00FDF054;
	Hook_DeleteCriticalSection     = (void**)0x00FDF058;
	Hook_InitializeCriticalSectionAndSpinCount = (void**)0x00FDF130;
	Hook_ExitPath = 0x86B66E;

	Hook_EnterLightCS = (void*) 0x0040FBF0;
	Hook_LeaveLightCS = (void*) 0x0040FBA0;//not very hookable - this gets inlined about 50% of the time
	Hook_TryEnterLightCS = (void*) 0x0078D200;

	Hook_FixedTimePerFrame_iFPSClamp = (float*) (0x11F6394 + 4);

	Hook_Hashtable_IntToIndex1 = 0x6C6A60;
	Hook_Hashtable_IntToIndex2 = 0xA0D260;
	Hook_Hashtable_IntToIndex3 = 0xA63840;
	Hook_Hashtable_IntToIndex4 = 0x84b930;
	
	//hash char: 0x863C70
	Hook_Hashtable_StringToIndex1 = 0xA4D6D0;
	Hook_Hashtable_StringToIndex2 = 0xA0D440;
	Hook_Hashtable_StringToIndex3 = 0x486DF0;
	Hook_Hashtable_StringToIndex4 = 0xEC1C30;

	Hook_MainGameFunction = 0x86E650;//86C8E0;
	Hook_MainGameFunctionCaller = 0x86B3E3;//0x869653;
	Hook_DirectInput8Create = (void**)0xEE7F10;//0xEE5220;
	Hook_memset1 = 0xEC61C0u;//0EC34C0;
	Hook_memset2 = 0x403D30u;
	Hook_Sleep   = 0x00FDF104;
	Hook_rand    = 0xECADB8;//0EC80B8;
	Hook_srand   = 0xECADA6;//0EC80A6;

	Hook_HeapAlloc   = 0xFDF1A0;
	Hook_HeapReAlloc = 0xFDF19C;
	Hook_HeapFree    = 0xFDF1A4;
	Hook_HeapSize    = 0xFDF1A8;
	Hook_HeapCreate  = 0xFDF254;
	Hook_HeapDestroy = 0x0;

	Hook_Memory_CreatePool = 0xAA3CC0;
	Hook_Memory_Allocate   = 0xAA3E40;
	Hook_Memory_Free       = 0xAA4060;
	Hook_Memory_Reallocate = 0xAA4150;
//	Hook_Memory_Reallocate2= 0xAA4200;//extra (unused) parameters
	Hook_Memory_GetHeapish = 0x0;
}

#endif //NEW_VEGAS
