#ifndef DECLARE_SETTING_I
#	define TEMP_DECLARATIONS
#	define DECLARE_SETTING_SECTION(section)
#	define DECLARE_SETTING_C(section,name,default_value)
#	define DECLARE_SETTING_I(section,name,default_value,comment) namespace Settings {namespace section {extern int name;}}
#	define DECLARE_SETTING_F(section,name,default_value,comment) namespace Settings {namespace section {extern float name;}}
#	define DECLARE_SETTING_B(section,name,default_value,comment) namespace Settings {namespace section {extern bool name;}}
#endif

DECLARE_SETTING_SECTION(Master) 
DECLARE_SETTING_C(Master,_comment,                " You can turn on or off each distinct feature from here.")
DECLARE_SETTING_B(Master,bManageFPS,            1, "regulates flow of gametime, monitors & limits FPS")
DECLARE_SETTING_B(Master,bHookCriticalSections, 1, "tweaks some thread-safety stuff that uses standard Microsofts objects")
#if defined FALLOUT || defined NEW_VEGAS
DECLARE_SETTING_B(Master,bHookLightCriticalSections,1, "tweaks some thread-safety stuff that uses Bethesdas custom objects")
#endif
DECLARE_SETTING_B(Master,bHookHashtables,   1, "tweaks some hashtables that Bethesdas looks things up in")
//
DECLARE_SETTING_B(Master,bReplaceHeap,      0, "replaces Bethesdas custom memory manager with a different one (produces instability)")
DECLARE_SETTING_B(Master,bReplaceGetTickCount, 1, "replaces the games normal measurement of time")
#ifndef FALLOUT
DECLARE_SETTING_B(Master,bLogToConsole,     0, "causes this dll to echo everything it says to the in-game console in addition to the log file")
#endif
DECLARE_SETTING_B(Master,bFastExit,         1, "makes quiting faster")
#ifdef OBLIVION
DECLARE_SETTING_B(Master,bExtraProfiling, 0, "")
#endif
DECLARE_SETTING_B(Master,bFlushLog,            1, "do not buffer writes to log file")
DECLARE_SETTING_I(Master,iSchedulingResolution,1, "makes windows schedule at higher resolutions (0 disables)")
DECLARE_SETTING_B(Master,bReplaceRandom,       1, "")
//
DECLARE_SETTING_B(Master,bExperimentalStuff,  0, "")
DECLARE_SETTING_I(Master,iMainHookPoint,      1, "")

DECLARE_SETTING_SECTION(Experimental)
//DECLARE_SETTING_C(Experimental,_comment, "This section is disabled by default - see Master/bExperimentalStuff")
DECLARE_SETTING_C(Experimental,_comment, "bReduceSleep and iThreadsFixedToCPUs can probably reasonably be used at 1.  > 1 is a bad idea atm.")
DECLARE_SETTING_C(Experimental,_comment, "other settings here you're probably better off not touching")
DECLARE_SETTING_I(Experimental,bReduceSleep,          0, "")
DECLARE_SETTING_I(Experimental,iThreadsFixedToCPUs,   1, "")
DECLARE_SETTING_B(Experimental,bSuppressRandomSeeding,0, "")
#ifdef OBLIVION
DECLARE_SETTING_I(Experimental,bMonitorBSShaderAccumulator,0, "")
DECLARE_SETTING_I(Experimental,iPrintSceneGraphDepth,0, "")
DECLARE_SETTING_B(Experimental,bReplaceRandomWrappers,  1, "")
#endif
DECLARE_SETTING_B(Experimental,bBenchmarkHeap,        0, "")
DECLARE_SETTING_B(Experimental,bAlternateHeapHooks,   0, "")
DECLARE_SETTING_I(Experimental,iHeapMainBlockAddress, 0, "")
DECLARE_SETTING_I(Experimental,iCullingTimeLimit,0, "")

DECLARE_SETTING_SECTION(FPS_Management)
DECLARE_SETTING_C(FPS_Management,_comment, "Absent a good reason otherwise, bInject_iFPSClamp=1, fMaximumFPS= 30 to 85 (or 0), fMinimumFPS= 10 to 20, iFPS_Report_Period = 2000 to 60000, fExtraSleepPercent = 0.0 to 0.2")
#ifdef FALLOUT
DECLARE_SETTING_B(FPS_Management,bInject_iFPSClamp,     0, "")
#else
DECLARE_SETTING_B(FPS_Management,bInject_iFPSClamp,     1, "")
#endif
DECLARE_SETTING_F(FPS_Management,fMaximumFPS,        "  0", "")
DECLARE_SETTING_F(FPS_Management,fMinimumFPS,        " 15", "")
DECLARE_SETTING_I(FPS_Management,iFPS_Report_Period, 15000, "")
DECLARE_SETTING_F(FPS_Management,fExtraSleepPercent,"0.05", "")

DECLARE_SETTING_SECTION(GetTickCount)
DECLARE_SETTING_C(GetTickCount,_comment, "This section is disabled by default - see Master/bReplaceGetTickCount")
DECLARE_SETTING_B(GetTickCount,bForceResolution,       1, "")
DECLARE_SETTING_B(GetTickCount,bPreserveDC_Bias,       1, "")
DECLARE_SETTING_B(GetTickCount,bPreserveHighFreqComponents, 0, "")
DECLARE_SETTING_B(GetTickCount,bForceSync,             0, "")
DECLARE_SETTING_I(GetTickCount,iSyncLimitMilliseconds,50, "")
//DECLARE_SETTING_B(GetTickCount,bLockWhileSynching, 0, "")

DECLARE_SETTING_SECTION(CriticalSections)
DECLARE_SETTING_C(CriticalSections,_comment," CS stuff helps Oblivion, Fallout, and New Vegas significantly")
DECLARE_SETTING_C(CriticalSections,_comment," much of the benefit comes from the Renderer+0x180 suppression (see overrides below)")
DECLARE_SETTING_C(CriticalSections,_comment," modes: 1=vanilla, 2=fair, 3=staggering(hybrid of 1 & 2), 5=suppressed")
DECLARE_SETTING_B(CriticalSections,bUseOverrides,   1, "")
DECLARE_SETTING_I(CriticalSections,iDefaultMode,    3, "")
DECLARE_SETTING_I(CriticalSections,iDefaultSpin, 1000, "")
DECLARE_SETTING_I(CriticalSections,iStaggerLevel,   5, "")
DECLARE_SETTING_B(CriticalSections,bEnableMessages, 1, "")
DECLARE_SETTING_B(CriticalSections,bEnableProfiling,0, "")//0

//LCS stuff is for FO3 & FNV, not Oblivion
//however, it is much more limited on FO3 than on FNV
#if defined FALLOUT || defined NEW_VEGAS
DECLARE_SETTING_SECTION(LightCriticalSections)
DECLARE_SETTING_C(LightCriticalSections,_comment,"LCS stuff is like CS stuff, but with a Bethesda implementation.  And inlined sometimes, so difficult for me to work with")
DECLARE_SETTING_B(LightCriticalSections,bEnableProfiling,0, "")//0
DECLARE_SETTING_B(LightCriticalSections,bEnableMessages, 1, "")
DECLARE_SETTING_I(LightCriticalSections,iDefaultMode,    3, "")
DECLARE_SETTING_I(LightCriticalSections,iDefaultSpin, 1000, "")
DECLARE_SETTING_I(LightCriticalSections,iStaggerLevel,   5, "")
DECLARE_SETTING_B(LightCriticalSections,bFullHooks,      0, "")//0
DECLARE_SETTING_B(LightCriticalSections,bUseOverrides,   0, "")
#endif

//Heap stuff is seriously buggy on Fallout, mildly buggy on Oblivion, not sure about New Vegas
DECLARE_SETTING_SECTION(Heap)
DECLARE_SETTING_C(Heap,_comment, "This section is disabled by default - see Master/bReplaceHeap")
DECLARE_SETTING_C(Heap,_comment, "I recommend enabling it however.  ")
DECLARE_SETTING_C(Heap,_comment," Heap replacement can produce MAJOR improvements in performance on Oblivion at a significant cost in stability")
DECLARE_SETTING_C(Heap,_comment," It crashes instantly on Fallout3 last I remember checking")
DECLARE_SETTING_C(Heap,_comment," It seems to work on Fallout: New Vegas ?")
DECLARE_SETTING_C(Heap,_comment," Algorithms: 1=FastMM4, 2=Microsoft (slow on XP), 3=SimpleHeap1, 4=TBBMalloc, 5=ThreadHeap2, 6=ThreadHeap3, 8=tcmalloc")
DECLARE_SETTING_C(Heap,_comment," Algorithms numbers 1, 4, and 8 require external DLL files in the Data/OBSE/Plugins/ComponentDLLs folder")
DECLARE_SETTING_C(Heap,_comment," Size is in units of megabytes, and only effects algorithms 3, 5, and 6 (other algorithms dynamically determine their own size)")
DECLARE_SETTING_I(Heap,iHeapAlgorithm,  6, "")
DECLARE_SETTING_B(Heap,bEnableProfiling,0, "")
#if defined OBLIVION
	DECLARE_SETTING_I(Heap,iHeapSize,  450, "")
#elif defined FALLOUT
	DECLARE_SETTING_I(Heap,iHeapSize,  250, "")
#elif defined NEW_VEGAS
	DECLARE_SETTING_I(Heap,iHeapSize,  250, "")//250
#endif
DECLARE_SETTING_B(Heap,bEnableMessages, 0, "")//0
DECLARE_SETTING_I(Heap,bZeroAllocations,0, "")

DECLARE_SETTING_SECTION(Hashtables)
DECLARE_SETTING_B(Hashtables,bAllowDynamicResizing, 0, "")
DECLARE_SETTING_I(Hashtables,iHashtableResizeScale1, 2, "")
DECLARE_SETTING_I(Hashtables,iHashtableResizeScale2, 4, "")
DECLARE_SETTING_I(Hashtables,iHashtableResizeDelay, 40, "")
DECLARE_SETTING_B(Hashtables,bUseOverrides,    1, "")
DECLARE_SETTING_B(Hashtables,bEnableMessages,  0, "")//0
DECLARE_SETTING_B(Hashtables,bEnableExtraMessages,0, "")//0
DECLARE_SETTING_B(Hashtables,bEnableProfiling, 0, "")//0


#ifdef TEMP_DECLARATIONS
#	undef TEMP_DECLARATIONS
#	undef DECLARE_SETTING_SECTION
#	undef DECLARE_SETTING_C
#	undef DECLARE_SETTING_I
#	undef DECLARE_SETTING_F
#	undef DECLARE_SETTING_B
#endif
