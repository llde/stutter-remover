
#include "random.h"


struct PerThread {
	static int num_threads;
	static int tls_slot;
	static bool initialized;
	static CRITICAL_SECTION internal_cs;
//-------------------------------

	int cs_switch;
	int thread_number;

	DWORD thread_id;//from GetCurrentThreadId()
	void *cs_perfdata3_buffer;
	//UInt32 ht_perf3_accesses; UInt32 ht_perf3_wastage;

	RNG_fast32_16 internal_rng;//for use internal to the stutter remover
	RNG_jsf32 internal_rng_hq;

	RNG_fast32_16 oblivion_rng;//for use by RNG replacement code
//	RNG_jsf32 oblivion_rng_hq;

//	void *delayed_free_pt;
//	int rng_usage;

//-------------------------------
	static void initialize() ;
	PerThread() {
		cs_perfdata3_buffer = NULL;
		cs_switch = 0;
		//delayed_free_pt = NULL;
		internal_rng.seed_64(global_rng.raw64());
		internal_rng_hq.seed_64(global_rng.raw64());
		oblivion_rng.seed_64(global_rng.raw64());
		//ht_perf3_accesses = ht_perf3_wastage  = 0;
//		oblivion_rng_hq.seed_64(global_rng.raw64());
//		rng_usage = 0;
	}
	static PerThread *get_PT();
	static int thread_id_to_number(DWORD id);
	~PerThread();
};

struct CSF {
	void (WINAPI * EnterCriticalSection)     ( CRITICAL_SECTION *cs );
	BOOL (WINAPI * TryEnterCriticalSection)  ( CRITICAL_SECTION *cs );
	void (WINAPI * LeaveCriticalSection)     ( CRITICAL_SECTION *cs );
	void (WINAPI * InitializeCriticalSection)( CRITICAL_SECTION *cs );
	void (WINAPI * DeleteCriticalSection)    ( CRITICAL_SECTION *cs );
	BOOL (WINAPI * InitializeCriticalSectionAndSpinCount)( CRITICAL_SECTION *cs, DWORD spin );
	DWORD (WINAPI * SetSpinCount)( CRITICAL_SECTION *cs, DWORD spin );
};


extern CSF CSF_vanilla;
extern CSF CSF_active;

//extern const CSF CSF_defaultspin1;
extern const CSF CSF_defaultspin2;
//extern const CSF CSF_fair;
//extern const CSF CSF_optimized;
//extern const CSF CSF_test;
//extern const CSF CSF_standard1;
//extern const CSF CSF_standard2;
//void get_active_CSFs1 ( CSF *csf );
//void get_active_CSFs2 ( CSF *csf );
//void set_active_CSFs1 ( const CSF *csf );
//void set_active_CSFs2 ( const CSF *csf );
void initialize_CS_and_LCS_hooks ();

class LightCS {
public:
	volatile long thread_id;
	volatile long recursion_count;
};
typedef void (__fastcall * LCSf)  ( LightCS *, int dummy_parameter, char *name );
/*void __fastcall LCS_basic ( LightCS *, int dummy_parameter, char *name );
void __fastcall LCS_spin ( LightCS *, int dummy_parameter, char *name );
void __fastcall LCS_basic_profile ( LightCS *, int dummy_parameter, char *name );
void __fastcall LCS_spin_profile ( LightCS *, int dummy_parameter, char *name );
void __fastcall LCS_stagger_profile ( LightCS *, int dummy_parameter, char *name );
void __fastcall LCS_priority_profile ( LightCS *, int dummy_parameter, char *name );
void __fastcall LCS_fair_profile ( LightCS *, int dummy_parameter, char *name );
//void __fastcall LCS_fast ( LightCS *, int dummy_parameter, char *name );
//void __fastcall LCS_profiling ( LightCS *, int dummy_parameter, char *name );
void __fastcall LCS_test ( LightCS *, int dummy_parameter, char *name );

void set_active_LCS ( LCS lcs );*/