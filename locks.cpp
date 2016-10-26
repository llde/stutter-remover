#include <string>
#include <set>
#include <map>
#include <list>
#include <queue>

#include "main.h"
#include <mmsystem.h>
#include "locks.h"
#include "struct_text.h"
#include "settings.h"

#include <signal.h>

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
//#include "Hooks_DirectInput8Create.h"
//#include "obse_common/SafeWrite.h"

//#include "GameAPI.h"
#include <shlobj.h>
#include <shlwapi.h>


CSF CSF_vanilla;
CSF CSF_active;


struct CS_override_data {
	CRITICAL_SECTION *cs;//NULL if override is not by object address
	const void *caller;//NULL if override is not by initialization caller address
	int mode;//-1 to use the default mode
	int spin;//-1 is spin is not overriden
	CS_override_data(CRITICAL_SECTION *cs_, const void *caller_, int mode_, int spin_)
		: cs(cs_), caller(caller_), mode(mode_), spin(spin_) {}
};
struct LCS_override_data {
	LightCS *lcs;//NULL if override is not by object address
	const void *caller;//NULL if override is not by initialization caller address
	int mode;//-1 to use the default mode
	int spin;//-1 is spin is not overriden
	LCS_override_data(LightCS *lcs_, const void *caller_, int mode_, int spin_)
		: lcs(lcs_), caller(caller_), mode(mode_), spin(spin_) {}
};
std::map<CRITICAL_SECTION*, CS_override_data > CS_override_by_object;
std::map<      const void*, CS_override_data > CS_override_by_caller;
std::map<         LightCS*, LCS_override_data> LCS_override_by_object;
std::map<      const void*, LCS_override_data> LCS_override_by_caller;

void CS_explicit_override ( CRITICAL_SECTION *cs, int mode, int spin );
//int CS_default_mode;
//int CS_default_spin;
int CS_spin_stagger_level;//must be 1 less than a power of 2
int LCS_spin_stagger_level;//must be 1 less than a power of 2



bool PerThread::initialized = false;
int PerThread::tls_slot = TLS_OUT_OF_INDEXES;
int PerThread::num_threads = 0;
static std::map<int, PerThread*> pt_map;
CRITICAL_SECTION PerThread::internal_cs;
static void thread_destruction_callback() {
	PerThread *pt = (PerThread *) TlsGetValue(PerThread::tls_slot);
	if (!pt) return;
	message("thread %d dieing", pt->thread_number);
	::EnterCriticalSection(&PerThread::internal_cs);
//	message("PerThread - detaching from thread #%d", pt->thread_number);
//	PerThread::PT_array[pt->thread_number] = NULL;
	pt_map.erase(pt->thread_number);
	delete pt;
	::LeaveCriticalSection(&PerThread::internal_cs);
}
PerThread::~PerThread() {
	if (cs_perfdata3_buffer) {
		free(cs_perfdata3_buffer);
	}
}
void PerThread::initialize() {
	if (initialized) error("PerThread - already initialized");
	::InitializeCriticalSectionAndSpinCount( &internal_cs, 1500 );
	initialized = true;
	tls_slot = TlsAlloc();
	if (tls_slot == TLS_OUT_OF_INDEXES) {error("ERROR: ::PerThread: failed to allocated TLS slot");}
	register_thread_destruction_callback(thread_destruction_callback);
}
PerThread *PerThread::get_PT() {
	PerThread *rv = (PerThread *) TlsGetValue(tls_slot);
	if (rv) return rv;
	if (!initialized) error("PerThread - not initialized yet");
	::EnterCriticalSection(&internal_cs);
	DWORD tid = GetCurrentThreadId();
	rv = new PerThread();
	rv->thread_number = ++num_threads;
	rv->thread_id = GetCurrentThreadId();
	if (Settings::Master::bExperimentalStuff && Settings::Experimental::iThreadsFixedToCPUs) {
		int n = Settings::Experimental::iThreadsFixedToCPUs;
		if (rv->thread_number <= n) {
			HANDLE process = GetCurrentProcess();
			//DWORD_PTR pam;
			//DWORD_PTR sam;
			//GetProcessAffinityMask(process, &pam, &sam);
			SetThreadAffinityMask(GetCurrentThread(), 1 << (rv->thread_number - 1));
		}
		else SetThreadAffinityMask(GetCurrentThread(), ((1 << n) - 1) ^ 0xffffff);
	}
	else {
		Settings::Experimental::iThreadsFixedToCPUs = 0;
	}
	//PT_array[rv->thread_number] = rv;
	pt_map[rv->thread_number] = rv;
	if (!TlsSetValue(tls_slot, rv)) {
		error("ERROR: ::PerThread: failed to set TLS");
	}
	else {
		message("thread %04X assigned PT %08X, serial# %d", tid, rv, rv->thread_number);
	}
	::LeaveCriticalSection(&internal_cs);
	return (PerThread*) rv;
}
int PerThread::thread_id_to_number(DWORD id) {
	::EnterCriticalSection(&internal_cs);
	int rv = 0;
	std::map<int,PerThread*>::iterator it = pt_map.begin();
	for (; !rv && it != pt_map.end(); it++) {
		if (it->second->thread_id == id) rv = it->first;
	}
	::LeaveCriticalSection(&internal_cs);
	return rv;
}

/*
void WINAPI EnterCriticalSection     ( CRITICAL_SECTION *cs );
BOOL WINAPI TryEnterCriticalSection  ( CRITICAL_SECTION *cs );
void WINAPI LeaveCriticalSection     ( CRITICAL_SECTION *cs );
void WINAPI InitializeCriticalSection( CRITICAL_SECTION *cs );
void WINAPI DeleteCriticalSection    ( CRITICAL_SECTION *cs );
*/

#define SPIN_FLAG_BITS 3
#define SPIN_MASK ((1 << SPIN_FLAG_BITS) - 1)
#define SPIN_UNINITIALIZED_MASK   3 //all multiples of 4 are considered uninitialized
#define CS_MODE_UNINITIALIZED  0
#define CS_MODE_UNINITIALIZED2 4
//#define CS_MODE_UNINITIALIZED3 8
#define CS_MODE_NORMAL         1
#define CS_MODE_FAIR           2
#define CS_MODE_STAGGER        3
#define CS_MODE_DEAD           5
#define CS_MODE_PRIORITY       6
#define CS_MODE_UNPRIORITY     7
//#define CS_MODE_OPTIMIZED      9

#define get_spin_mode(spin) ((spin) & SPIN_MASK)
#define calc_spin_fom_mode(spin,mode) (((spin) & (SPIN_MASK ^ 0xFFffFFff)) + (mode))

DWORD GetCurrentProcessorNumberXP(void) {//possibly by Jeremy Jones?
    _asm {mov eax, 1}
    _asm {cpuid}
    _asm {shr ebx, 24}
    _asm {mov eax, ebx}
}
inline void do_staggering_wait( CRITICAL_SECTION *cs ) {
	if (cs->RecursionCount || cs->LockCount == -1) return;

	PerThread *pt = PerThread::get_PT();

	if (pt->cs_switch-- != 0) return;
	pt->cs_switch = pt->internal_rng.raw16() & CS_spin_stagger_level;

	Sleep(0);
	if (cs->LockCount == -1) return;
	Sleep(1);
}
inline void do_fair_wait ( CRITICAL_SECTION *cs ) {
	if (cs->RecursionCount || cs->LockCount == -1) return;
	Sleep(0);
	if (cs->RecursionCount || cs->LockCount == -1) return;
	Sleep(1);
}
#define DECLARE_SPINFUNCS(varname, prefix) const CSF varname = { prefix ## EnterCriticalSection, prefix ## TryEnterCriticalSection, prefix ## LeaveCriticalSection, prefix ## InitializeCriticalSection, prefix ## DeleteCriticalSection, prefix ## InitializeCriticalSectionAndSpinCount, prefix ## SetSpinCount };





#if 0
	#define CS_TIME_TYPE UInt32
	#define CS_GET_TIME() get_time_ms()
	#define CS_TIME_PERIOD() (0.001)
	#define CS_TIME_SIGSHIFT 0
	#define CS_TIME_SIGUNIT (1<<CS_TIME_SIGSHIFT)
#endif
#if 0
	#define CS_TIME_TYPE UInt32
	#define CS_GET_TIME() (UInt32(get_time_ticks() >> 8))
	#define CS_TIME_PERIOD() (get_tick_period() * 256)
	#define CS_TIME_SIGSHIFT 13
	#define CS_TIME_SIGUNIT (1<<CS_TIME_SIGSHIFT)
#endif
#if 1
	#define CS_TIME_TYPE UInt64
	#define CS_GET_TIME() (get_time_ticks())
	#define CS_TIME_PERIOD() (get_tick_period())
	#define CS_TIME_SIGSHIFT 21
	#define CS_TIME_SIGUNIT (1<<CS_TIME_SIGSHIFT)
#endif

#define SPIN_MASK2 SPIN_MASK
//#define SPIN_MASK2 (SPIN_MASK*2+1)
//#define SPIN_FLAG_TEST1 (SPIN_MASK+1)
/*struct PerfData {
	static CRITICAL_SECTION internal_cs;
	static bool initialized;
	struct SourceData {
		CRITICAL_SECTION *cs;
		UInt32 initializer;
		SourceData(CRITICAL_SECTION *cs_, UInt32 initializer_) : cs(cs_), initializer(initializer_) {}
	};
	std::vector<SourceData> sources;
	UInt64 count;
	const char *name;
	CS_TIME_TYPE time;
	CS_TIME_TYPE worst;
	int times_displayed;
	PerfData(const char *_name) : name(_name), count(0), time(0), worst(0), times_displayed(0) {}
	PerfData() : name(""), count(0), time(0), worst(0), times_displayed(0) {
		if (!initialized) {//hopefully called by global constructor, before things have a chance to get multithreaded
			::InitializeCriticalSectionAndSpinCount(&internal_cs, 1000);
			initialized = true;
		}
	}
	void add_source(CRITICAL_SECTION *cs, void *initializer, int index = -1) {
		::EnterCriticalSection(&internal_cs);
		cs->SpinCount |= 1 << SPIN_FLAG_BITS;
		sources.push_back(SourceData(cs, UInt32(initializer)));
		if (*name == 0 && index >= 0) {
			char buffy[128];
			sprintf(buffy, "CS_Group%03d", index);
			name = strdup(buffy);
		}
		::LeaveCriticalSection(&internal_cs);
	}
	void list_sources() {
		::EnterCriticalSection(&internal_cs);
		for (int i = 0; i < sources.size(); i+=2) {
			if (i < sources.size() + 1) message("%s:   %2d: %08X/%08X    %2d: %08X/%08X", name, i, sources[i].cs, sources[i].initializer, i+1, sources[i+1].cs, sources[i+1].initializer);
			else message("%s:   %2d: %08X/%08X", name, i, sources[i].cs, sources[i].initializer);
		}
		::LeaveCriticalSection(&internal_cs);
	}
	void update(CRITICAL_SECTION *cs, CS_TIME_TYPE delta, int waiters) {
		count++;
		if (!delta) return;
		time += delta;
		if (worst < delta) {
			worst = delta;
			print_summary();
		}
		else if ((time - delta)>>(CS_TIME_SIGSHIFT+6) != time>>(CS_TIME_SIGSHIFT+6)) print_summary();
		if (delta >= 2) {
			DWORD t = timeGetTime();
			DWORD threadID = GetCurrentThreadId();
			DWORD cpu = GetCurrentProcessorNumberXP();
			::message("time%8.3f thread %04x:%d waited on CS %08x (spin%4d) for%5d ms (init %d)", (t-base_time)*0.001, threadID, cpu, cs, cs->SpinCount, delta, waiters);
		}
	}
	void print_summary() {
		if (count == 0) return;
		::EnterCriticalSection(&internal_cs);
		double t = time * CS_TIME_PERIOD();
		if (0) ;
		else if (count < 10000)
			::message("%s%6d %7d%7.3fms %dms", name, t, int(count), t / (count + .000001), worst);
		else if (count < 10000000)
			::message("%s%6d %6dk%7.3fms %dms", name, t, int(count/1000), t / (count + .000001), worst);
		else
			::message("%s%6d %6dM%7.3fms %dms", name, t, int(count/1000000), t / (count + .000001), worst);
		if (((++times_displayed) & 15) == 15) list_sources();
		::LeaveCriticalSection(&internal_cs);
	}
};
CRITICAL_SECTION PerfData::internal_cs;
bool PerfData::initialized = false;
*/
/*namespace PerfData2 {//per-CS, so no locking needed
	struct PerCS {
		CRITICAL_SECTION *cs;
		void *caller;
		UInt64 total_time;
		UInt64 total_callings;
		int flags;
	};
	enum {
		MAX_CRITICAL_SECTIONS = 1024,
		ADDRESS_SHIFT_BITS = 5,
		ADDRESS_INDEX_BITS = 6,
		SPINCOUNT_INDEX_BITS = 6
	};
	PerCS CS_block[MAX_CRITICAL_SECTIONS];
	std::map<CRITICAL_SECTION *, PerCS*> CS_map;
	PerCS *CS_lookup[ 1 << (ADDRESS_INDEX_BITS + SPINCOUNT_INDEX_BITS) ];
};*/

namespace PerfData3 {//per-thread, little locking needed
	static CRITICAL_SECTION pd3_cs;

	struct CS_Event {
		void *cs;
		void *caller;
		CS_TIME_TYPE delta;
		int thread;
		char waiters;
		char padding[3];
	};
	struct PerThreadBuffer {
		enum {BUFFER_SIZE=32};
		int event_index;
		CS_Event event_list[BUFFER_SIZE];
		PerThreadBuffer() : event_index(0) {}
	};

	enum {THRESHOLD_TIME = CS_TIME_SIGUNIT * 2};

	enum {SHARED_BUFFER_SIZE=768};//must be a multiple of PerThread::BUFFER_SIZE
	int shared_event_index = 0;
	CS_Event shared_events[SHARED_BUFFER_SIZE];

	void print_summary ();
	void flush_shared_buffer ( );
	void flush_thread_buffer(PerThreadBuffer *pt);
	bool inited = false;
	void init() {
		if (inited) return;
		inited = true;
		::InitializeCriticalSectionAndSpinCount(&pd3_cs, 6000);
		::EnterCriticalSection(&pd3_cs);
		::LeaveCriticalSection(&pd3_cs);
	}
	void notify ( void *cs, void *caller, CS_TIME_TYPE delta, int waiters ) {
		PerThread *_pt = PerThread::get_PT();
		PerThreadBuffer *pt = (PerThreadBuffer*) _pt->cs_perfdata3_buffer;
		if (!pt) {
			//pt = new PerThreadBuffer();
			pt = (PerThreadBuffer*)malloc(sizeof(PerThreadBuffer)); new (pt) PerThreadBuffer();
			_pt->cs_perfdata3_buffer = pt;
		}
		int i = pt->event_index++;
		CS_Event &e = pt->event_list[i];
		e.cs = cs;
		e.caller = caller;
		e.delta = delta;
		e.thread = _pt->thread_number;
		e.waiters = waiters;
		if (delta >= THRESHOLD_TIME) {
			DWORD t = get_time_ms();
			DWORD cpu = GetCurrentProcessorNumberXP();
			message("time%8.2f:%2d:%d:%08X waited on CS %08x for%6.1f ms (init %d)", t * 0.001, _pt->thread_number, cpu, caller, cs, (delta * get_tick_period()) * 1000, waiters);
			if (TryEnterCriticalSection(&pd3_cs)) {
				flush_thread_buffer(pt);
				flush_shared_buffer();
				LeaveCriticalSection(&pd3_cs);
				i = 0;
			}
		}
		if (i == PerThreadBuffer::BUFFER_SIZE-1) {
			::EnterCriticalSection(&pd3_cs);
			flush_thread_buffer(pt);
			if (shared_event_index > SHARED_BUFFER_SIZE - PerThreadBuffer::BUFFER_SIZE) flush_shared_buffer();
			::LeaveCriticalSection(&pd3_cs);
		}
	}
	void flush_thread_buffer(PerThreadBuffer *pt) {
		memcpy(&shared_events[shared_event_index], &pt->event_list[0], pt->event_index * sizeof(CS_Event));
		shared_event_index += pt->event_index;
		pt->event_index = 0;
	}

	const char *CS_TYPE  = "CS ";
	const char *LCS_TYPE = "LCS";
	struct PerCS {
		void *cs;
		std::set<const void *> callers;
		std::vector<const void *> callers2;
		const void * first_caller;
		UInt64 time;
		UInt64 blockings;
		UInt64 worst_time;
		int worst_waiters;
		const char *description;
		const char *type;
		PerCS() : cs(NULL), time(0), blockings(0), worst_time(0), worst_waiters(-999), description(NULL), type(CS_TYPE)  {}
	};
	std::map<void *, PerCS> CS_map;
	UInt32 last_print_summary_time = 0;
	inline bool COMPARE_INDECES(int a, int b) {
		return shared_events[a].cs < shared_events[b].cs;
	}
	void maybe_print_summary() {
		UInt32 current_time = get_time_ms();
		if (!last_print_summary_time || UInt32(current_time - last_print_summary_time) > 10000) {
			::EnterCriticalSection(&pd3_cs);
			if (!last_print_summary_time || UInt32(current_time - last_print_summary_time) > 10000) {
				last_print_summary_time = current_time;
				print_summary();
			}
			::LeaveCriticalSection(&pd3_cs);
		}
	}
	void flush_shared_buffer () {
		DWORD t = timeGetTime();
		if (true) {
			int shared_event_indicies[SHARED_BUFFER_SIZE];
			for (int i = 0; i < shared_event_index; i++) shared_event_indicies[i] = i;
			if (shared_event_index >= 100) {
				std::sort( &shared_event_indicies[0], &shared_event_indicies[shared_event_index], COMPARE_INDECES );
			}
			void *old_cs = NULL;
			PerCS *old_pcs = NULL;
			for (int i = 0; i < shared_event_index; i++) {
				CS_Event &e = shared_events[shared_event_indicies[i]];
				void *cs = e.cs;
				PerCS *pcs = (cs == old_cs) ? old_pcs : &CS_map[cs];
				if (!pcs->cs) pcs->cs = cs;
				if (true) {
					if (pcs->callers.insert(e.caller).second) {
						pcs->callers2.push_back(e.caller);
					}
				}
				if (e.delta > pcs->worst_time) pcs->worst_time = e.delta;
				if (e.waiters > pcs->worst_waiters) pcs->worst_waiters = e.waiters;
				pcs->time += e.delta;
				pcs->blockings += 1;
	//			print_summary_counter += e.delta;
			}
		}
		else for (int i = 0; i < shared_event_index; i++) {
			CS_Event &e = shared_events[i];
			PerCS &cs = CS_map[e.cs];
			if (!cs.cs) cs.cs = e.cs;
			if (true) {
				if (cs.callers.insert(e.caller).second) {
					cs.callers2.push_back(e.caller);
				}
			}
			if (e.delta > cs.worst_time) cs.worst_time = e.delta;
			if (e.waiters > cs.worst_waiters) cs.worst_waiters = e.waiters;
			cs.time += e.delta;
			cs.blockings += 1;
//			print_summary_counter += e.delta;
		}
		shared_event_index = 0;
		maybe_print_summary();
		DWORD delta = timeGetTime() - t;
		if (delta > 1) message("PerfData3::flush_shared_buffer took %d ms", delta);
	}
	static UInt64 priority ( PerCS &pcs ) {return pcs.time + (pcs.worst_time >> 1) + (pcs.blockings << 2);}
	void print_summary () {
		::EnterCriticalSection(&pd3_cs);
		std::map<void *, PerCS>::iterator it;
		UInt64 total_priority = 0;
		UInt64 total_time = 0;
		UInt64 total_count = 0;
		it = CS_map.begin();
		for (it = CS_map.begin();it != CS_map.end(); it++) {
			PerCS &pcs = it->second;
			total_priority += priority(pcs);
			total_time += pcs.time;
			total_count += pcs.blockings;
		}
		if (total_count < 1000000) 
			message ("Critical Sections summary(PD3): time%5d   tt:%5.1fs   tc:%.0fk", get_time_ms()/1000, (total_time * CS_TIME_PERIOD()), double(total_count * 0.001));
		else message("Critical Sections summary(PD3): time%5d   tt:%5.1fs   tc:%.0fM", get_time_ms()/1000, (total_time * CS_TIME_PERIOD()), double(total_count * 0.000001));
		UInt64 threshold = total_priority >> 10;
		enum {MAX_OUT = 50};
		PerCS *outcs[MAX_OUT];
		int numoutcs = 0;
		for (it = CS_map.begin();it != CS_map.end(); it++) {
			bool changed = false;
			PerCS &pcs = it->second;
			UInt64 p = priority(pcs);
			if (numoutcs < MAX_OUT) {
				outcs[numoutcs++] = &pcs;
				changed = true;
			}
			else {
				if (priority(*outcs[numoutcs-1]) < p) {
					outcs[numoutcs-1] = &pcs;
					changed = true;
				}
			}
			if (changed) {
				for (int i = numoutcs-2; i >= 0; i--) {
					UInt64 t1 = priority(*outcs[i]);
					int j = i+1;
					UInt64 t2 = priority(*outcs[j]);
					if (t2 > t1) {
						PerCS *tmp=outcs[i];  outcs[i]=outcs[j];  outcs[j]=tmp;
					}
					else i = -1;
				}
			}
		}
		for (int i = 0; i < numoutcs; i++) {
			PerCS &pcs = *outcs[i];
			UInt64 p = priority(pcs);
			if (p > threshold) {
				double time = pcs.time * CS_TIME_PERIOD();
				double worst = pcs.worst_time * CS_TIME_PERIOD();
				double count = pcs.blockings;
				char buffy[512];
				int p = 0;
				for (std::vector<const void*>::iterator it = pcs.callers2.begin(); it != pcs.callers2.end(); it++)  {
					p += sprintf(&buffy[p], "%8X ", (int)*it);
					if (p >= 30) {
						p += sprintf(&buffy[p], "...");
						break;
					}
				}
				buffy[p] = 0;
				if (0) ;
				else if (count < 10000)
					::message("  %s %s %08X %9.0fms %6.0f %8.0fus%5.0fms %3d %s", is_address_static(pcs.cs)?" static":"dynamic", pcs.type, pcs.cs, time*1000, count,         time * 1000000 / (count + .1), worst*1000, int(pcs.worst_waiters), buffy);
				else if (count < 10000000)
					::message("  %s %s %08X %9.0fms %6.0fk%8.0fus%5.0fms %3d %s", is_address_static(pcs.cs)?" static":"dynamic", pcs.type, pcs.cs, time*1000, count/1000,    time * 1000000 / (count + .1), worst*1000, int(pcs.worst_waiters), buffy);
				else
					::message("  %s %s %08X %9.0fms %6.0fM%8.0fus%5.0fms %3d %s", is_address_static(pcs.cs)?" static":"dynamic", pcs.type, pcs.cs, time*1000, count/1000000, time * 1000000 / (count + .1), worst*1000, int(pcs.worst_waiters), buffy);
			}
		}
		::LeaveCriticalSection(&pd3_cs);
	}
};

#define CS_EVENT_CREATION(cs,caller,funcnum) {PerfData3::notify(cs, (void*)caller, 0, 0);}
#define CS_EVENT_ENTRY(cs,t1,t2,waiters,caller) {PerfData3::notify(cs, (void*)caller, t2-t1, waiters);}

//#define CS_EVENT_CREATION(cs,caller,funcnum) {int pdi = get_perfdata_index(cs); perf_generic[pdi].add_source(cs,caller,pdi);}
//#define CS_EVENT_ENTRY(cs,delta,waiters,caller) {int pdi = get_perfdata_index(cs); perf_generic[pdi].add_source(cs,caller,pdi);}
//PerfData perf_generic[1];
//int get_perfdata_index(CRITICAL_SECTION *cs) {return (UInt32(cs) >> 5) & 0;}





#define CS_CODEPATH_INITIALIZE_WITHOUT_SPIN  0
#define CS_CODEPATH_INITIALIZE_WITH_SPIN     1
#define CS_CODEPATH_MISSED_INITIALIZATION    2
static void _standard_initialize_critical_section ( CRITICAL_SECTION *cs, const void *caller, int codepath, int spin ) {
	int mode = Settings::CriticalSections::iDefaultMode;

	std::map<CRITICAL_SECTION*,CS_override_data>::iterator by_object = CS_override_by_object.find(cs);
	std::map<const void*,CS_override_data>::iterator by_caller = CS_override_by_caller.find(caller);

	int oldspin = spin;
	if (Settings::CriticalSections::bEnableMessages) {
//		message("critical section found at 0x%08x (via 0x%08X) (codepath:%d,spin:%d)", cs, caller, codepath, spin);
	}
	if (by_object != CS_override_by_object.end()) {
		if (by_object->second.mode != -1) mode = by_object->second.mode;
		if (by_object->second.spin != -1) spin = by_object->second.spin;
	}
	if (by_caller != CS_override_by_caller.end()) {
		if (by_caller->second.mode != -1) mode = by_caller->second.mode;
		if (by_caller->second.spin != -1) spin = by_caller->second.spin;
	}
	if (Settings::CriticalSections::bEnableMessages) {
		if (by_object != CS_override_by_object.end()) {
			if (by_object->second.mode != -1)
				message("CS 0x%08X (caller 0x%X, path %d, spin%d), by-object, set to mode %d", cs, caller, codepath, oldspin, by_object->second.mode );
			if (by_object->second.spin != -1)
				message("CS 0x%08X (caller 0x%X, path %d, spin%d), by-object, set to spin %d", cs, caller, codepath, oldspin, by_object->second.spin );
		}
		if (by_caller != CS_override_by_caller.end()) {
			if (by_caller->second.mode != -1)
				message("CS 0x%08X (caller 0x%X, path %d, spin%d), by-caller, set to mode %d", cs, caller, codepath, oldspin, by_caller->second.mode );
			if (by_caller->second.spin != -1)
				message("CS 0x%08X (caller 0x%X, path %d, spin%d), by-caller, set to spin %d", cs, caller, codepath, oldspin, by_caller->second.spin );
		}
		if (by_object != CS_override_by_object.end() && by_caller != CS_override_by_caller.end()) {
			if ((by_object->second.mode != by_caller->second.mode) || (by_object->second.spin != by_caller->second.spin))
				message("  WARNING - overrides disagree");
		}
//		message("CS 0x%08X (caller 0x%X,path%d,oldspin%d), set to mode%d / spin%d", cs, caller, codepath, oldspin, mode, spin );
	}
	if (spin < 0) spin = 0;
	switch (mode) {
		case CS_MODE_NORMAL:
		case CS_MODE_FAIR:
		case CS_MODE_STAGGER:
		case CS_MODE_DEAD:
		case CS_MODE_PRIORITY:
		case CS_MODE_UNPRIORITY:
			break;
		default:
			error("invalid critical section mode %d in initialize_critical_section", mode);
	}
	if ((codepath == CS_CODEPATH_INITIALIZE_WITHOUT_SPIN) || (codepath == CS_CODEPATH_INITIALIZE_WITH_SPIN)) {
		::InitializeCriticalSectionAndSpinCount(cs, (spin & ~SPIN_MASK) + mode);
	}
	else {
		CS_explicit_override ( cs, mode, spin );
	}
}

void WINAPI standard_EnterCriticalSection     ( CRITICAL_SECTION *cs ) {
	switch (cs->SpinCount & SPIN_MASK) {
		case CS_MODE_FAIR: {
			do_fair_wait(cs);
		}
		break;
		case CS_MODE_STAGGER: {
			do_staggering_wait(cs);
		}
		break;
		case CS_MODE_DEAD: {
			return;
		}
		break;
		case CS_MODE_NORMAL: {
		}
		break;
		case CS_MODE_PRIORITY: {
			if (GetCurrentThreadId() != main_thread_ID) do_fair_wait(cs);
		}
		break;
		case CS_MODE_UNPRIORITY: {
			if (GetCurrentThreadId() == main_thread_ID) do_fair_wait(cs);
		}
		break;
		default: {
			if ((cs->SpinCount & SPIN_UNINITIALIZED_MASK) == CS_MODE_UNINITIALIZED) {
				::EnterCriticalSection(cs);
				if ((cs->SpinCount & SPIN_UNINITIALIZED_MASK) == CS_MODE_UNINITIALIZED) {
					_standard_initialize_critical_section (cs, (void*) ((&cs)[-1]), CS_CODEPATH_MISSED_INITIALIZATION, cs->SpinCount ? cs->SpinCount : Settings::CriticalSections::iDefaultSpin );
				}
				::LeaveCriticalSection(cs);
				standard_EnterCriticalSection(cs);
				return;
			}
		}
	}
	if (Settings::CriticalSections::bEnableMessages) {
		UInt32 t = get_time_ms();
		::EnterCriticalSection(cs);
		UInt32 delta = get_time_ms() - t;
		if (delta >= 2) {
			DWORD cpu = GetCurrentProcessorNumberXP();
			void * caller = ((&cs)[-1]);
			message("time%8.2f:%2d:%d:%08X waited on CS %08x for%5d ms", t * 0.001, PerThread::get_PT()->thread_number, cpu, caller, cs, delta);
		}
	}
	else {
		::EnterCriticalSection(cs);
	}
}
BOOL WINAPI standard_TryEnterCriticalSection  ( CRITICAL_SECTION *cs ) {
	if ((cs->SpinCount & SPIN_MASK) == CS_MODE_DEAD) return TRUE;
	return ::TryEnterCriticalSection(cs);
}
void WINAPI standard_LeaveCriticalSection     ( CRITICAL_SECTION *cs ) {
	if ((cs->SpinCount & SPIN_MASK) == CS_MODE_DEAD) return;
	::LeaveCriticalSection(cs);
}
void WINAPI standard_InitializeCriticalSection( CRITICAL_SECTION *cs ) {
	_standard_initialize_critical_section ( cs, (void*) ((&cs)[-1]), CS_CODEPATH_INITIALIZE_WITHOUT_SPIN, Settings::CriticalSections::iDefaultSpin );
}
void WINAPI standard_DeleteCriticalSection    ( CRITICAL_SECTION *cs ) {
	::DeleteCriticalSection(cs);
}
BOOL WINAPI standard_InitializeCriticalSectionAndSpinCount( CRITICAL_SECTION *cs, DWORD spin ) {
	_standard_initialize_critical_section ( cs, (void*) ((&cs)[-1]), CS_CODEPATH_INITIALIZE_WITH_SPIN, spin );
	return true;
}
DWORD WINAPI standard_SetSpinCount( CRITICAL_SECTION *cs, DWORD spin ) {
	return ::SetCriticalSectionSpinCount(cs, (spin & ~SPIN_MASK) + (cs->SpinCount & SPIN_MASK));
}
DECLARE_SPINFUNCS(CSF_standard, standard_)





static void _profiling_initialize_critical_section ( CRITICAL_SECTION *cs, const void *caller, int codepath, int spin ) {
	int mode = Settings::CriticalSections::iDefaultMode;

	std::map<CRITICAL_SECTION*,CS_override_data>::iterator by_object = CS_override_by_object.find(cs);
	std::map<const void*,CS_override_data>::iterator by_caller = CS_override_by_caller.find(caller);

	int oldspin = spin;
	if (Settings::CriticalSections::bEnableMessages) {
//		message("critical section found at 0x%08x (via 0x%08X) (codepath:%d,spin:%d)", cs, caller, codepath, spin);
	}
	if (by_object != CS_override_by_object.end()) {
		if (by_object->second.mode != -1) mode = by_object->second.mode;
		if (by_object->second.spin != -1) spin = by_object->second.spin;
	}
	if (by_caller != CS_override_by_caller.end()) {
		if (by_caller->second.mode != -1) mode = by_caller->second.mode;
		if (by_caller->second.spin != -1) spin = by_caller->second.spin;
	}
	if (Settings::CriticalSections::bEnableMessages) {
		if (by_object != CS_override_by_object.end()) {
			if (by_object->second.mode != -1)
				message("CS 0x%08X (caller 0x%X, path %d, spin%d), by-object, set to mode %d", cs, caller, codepath, oldspin, by_object->second.mode );
			if (by_object->second.spin != -1)
				message("CS 0x%08X (caller 0x%X, path %d, spin%d), by-object, set to spin %d", cs, caller, codepath, oldspin, by_object->second.spin );
		}
		if (by_caller != CS_override_by_caller.end()) {
			if (by_caller->second.mode != -1)
				message("CS 0x%08X (caller 0x%X, path %d, spin%d), by-caller, set to mode %d", cs, caller, codepath, oldspin, by_caller->second.mode );
			if (by_caller->second.spin != -1)
				message("CS 0x%08X (caller 0x%X, path %d, spin%d), by-caller, set to spin %d", cs, caller, codepath, oldspin, by_caller->second.spin );
		}
		if (by_object != CS_override_by_object.end() && by_caller != CS_override_by_caller.end()) {
			if ((by_object->second.mode != by_caller->second.mode) || (by_object->second.spin != by_caller->second.spin))
				message("  WARNING - overrides disagree");
		}
//		message("CS 0x%08X (caller 0x%X,path%d,oldspin%d), set to mode%d / spin%d", cs, caller, codepath, oldspin, mode, spin );
	}
	if (spin < 0) spin = 0;
	switch (mode) {
		case CS_MODE_NORMAL:
		case CS_MODE_FAIR:
		case CS_MODE_STAGGER:
		case CS_MODE_DEAD:
		case CS_MODE_PRIORITY:
		case CS_MODE_UNPRIORITY:
			break;
		default:
			error("invalid critical section mode %d in initialize_critical_section", mode);
	}
	if ((codepath == CS_CODEPATH_INITIALIZE_WITHOUT_SPIN) || (codepath == CS_CODEPATH_INITIALIZE_WITH_SPIN)) {
		::InitializeCriticalSectionAndSpinCount(cs, (spin & ~SPIN_MASK) + mode);
	}
	else {
		CS_explicit_override ( cs, mode, spin );
	}
	{CS_EVENT_CREATION(cs,caller, codepath)}
}
/*static void _profiling_final_enter_critical_section ( CRITICAL_SECTION *cs, void *caller ) {
	if (TryEnterCriticalSection(cs)) return;
//	InterlockedIncrement(&cs->LockCount);
	for (int left = cs->SpinCount; left >= 0; left--) {
		if (TryEnterCriticalSection(cs)) {
//			InterlockedDecrement(&cs->LockCount);
			return;
		}
	}
	UInt32 t = timeGetTime();
	for (int tries=0;true;tries++) {
		Sleep(tries >> 4);
		if (tries >= 256 && !(tries & 63)) {
			message("_profiling_final_enter_critical_section - stuck in loop? (CS:%08X, caller:%08X, thread:%2d, other:%2d, tt:%dms)", cs, caller, PerThread::get_PT()->thread_number, PerThread::thread_id_to_number(DWORD(cs->OwningThread)), timeGetTime()-t);
		}
		if (TryEnterCriticalSection(cs)) {
//			InterlockedDecrement(&cs->LockCount);
			return;
		}
	}
}*/
void raw_profiling_EnterCriticalSection ( CRITICAL_SECTION *cs, void *caller ) {
	int waiters = cs->LockCount + 1 - cs->RecursionCount;
	if (!waiters && ::TryEnterCriticalSection(cs)) return;
	CS_TIME_TYPE t = CS_GET_TIME();

	switch (cs->SpinCount & SPIN_MASK) {
		case CS_MODE_FAIR: {
			do_fair_wait(cs);
		}
		break;
		case CS_MODE_STAGGER: {
			do_staggering_wait(cs);
		}
		break;
		case CS_MODE_DEAD: {
			return;
		}
		break;
		case CS_MODE_NORMAL: {
		}
		break;
		case CS_MODE_PRIORITY: {
			if (GetCurrentThreadId() != main_thread_ID) do_fair_wait(cs);
		}
		break;
		case CS_MODE_UNPRIORITY: {
			if (GetCurrentThreadId() == main_thread_ID) do_fair_wait(cs);
		}
		break;
		default: {
			if ((cs->SpinCount & SPIN_UNINITIALIZED_MASK) == CS_MODE_UNINITIALIZED) {
				//::EnterCriticalSection(cs);
				//::EnterCriticalSection(&PerfData3::internal_cs);
				_profiling_initialize_critical_section (cs, caller, CS_CODEPATH_MISSED_INITIALIZATION, cs->SpinCount ? cs->SpinCount : Settings::CriticalSections::iDefaultSpin );
				//::LeaveCriticalSection(&PerfData3::internal_cs);
				//::LeaveCriticalSection(cs);
				raw_profiling_EnterCriticalSection(cs, caller);
				return;
			}
		}
	}
	::EnterCriticalSection(cs);
//	_profiling_final_enter_critical_section(cs, ((&cs)[-1]));
	CS_TIME_TYPE t2 = CS_GET_TIME();
	{CS_EVENT_ENTRY(cs, t, t2, waiters, caller);}
}
void WINAPI profiling_EnterCriticalSection     ( CRITICAL_SECTION *cs ) {
	raw_profiling_EnterCriticalSection( cs, ((&cs)[-1]) );
}
BOOL WINAPI profiling_TryEnterCriticalSection  ( CRITICAL_SECTION *cs ) {
	if ((cs->SpinCount & SPIN_MASK) == CS_MODE_DEAD) return TRUE;
	return ::TryEnterCriticalSection(cs);
}
void WINAPI profiling_LeaveCriticalSection     ( CRITICAL_SECTION *cs ) {
	if ((cs->SpinCount & SPIN_MASK) == CS_MODE_DEAD) return;
	::LeaveCriticalSection(cs);
}
void WINAPI profiling_InitializeCriticalSection( CRITICAL_SECTION *cs ) {
	_profiling_initialize_critical_section ( cs, (void*) ((&cs)[-1]), CS_CODEPATH_INITIALIZE_WITHOUT_SPIN, Settings::CriticalSections::iDefaultSpin );
}
void WINAPI profiling_DeleteCriticalSection    ( CRITICAL_SECTION *cs ) {
	::DeleteCriticalSection(cs);
}
BOOL WINAPI profiling_InitializeCriticalSectionAndSpinCount( CRITICAL_SECTION *cs, DWORD spin ) {
	_profiling_initialize_critical_section ( cs, (void*) ((&cs)[-1]), CS_CODEPATH_INITIALIZE_WITH_SPIN, spin );
	return true;
}
DWORD WINAPI profiling_SetSpinCount( CRITICAL_SECTION *cs, DWORD spin ) {
	return ::SetCriticalSectionSpinCount(cs, (spin & ~SPIN_MASK) + (cs->SpinCount & SPIN_MASK));
}
DECLARE_SPINFUNCS(CSF_profiling, profiling_)

void WINAPI defaultspin2_EnterCriticalSection     ( CRITICAL_SECTION *cs ) {
	if (Settings::CriticalSections::bEnableProfiling) {
		raw_profiling_EnterCriticalSection( cs, (void*) ((&cs)[-1]) );
	}
	else {
		standard_EnterCriticalSection(cs);
	}
//	(*(void (WINAPI *)( CRITICAL_SECTION * )) *Hook_EnterCriticalSection)(cs);
}
BOOL WINAPI defaultspin2_TryEnterCriticalSection  ( CRITICAL_SECTION *cs ) {return (*(BOOL (WINAPI *)( CRITICAL_SECTION * )) *Hook_TryEnterCriticalSection)(cs);}
void WINAPI defaultspin2_LeaveCriticalSection     ( CRITICAL_SECTION *cs ) {(*(void (WINAPI *)( CRITICAL_SECTION * )) *Hook_LeaveCriticalSection)(cs);}
void WINAPI defaultspin2_InitializeCriticalSection( CRITICAL_SECTION *cs ) {
//	message("defaultspin2_InitializeCriticalSection: %08X", cs);
//	for (int i = -4; i < 6; i++) message("   %d:%8X", ((&cs)[i]));
	if (Settings::CriticalSections::bEnableProfiling) {
		_profiling_initialize_critical_section ( cs, (void*) ((&cs)[-1]), CS_CODEPATH_INITIALIZE_WITHOUT_SPIN, Settings::CriticalSections::iDefaultSpin );
	}
	else {
		_standard_initialize_critical_section  ( cs, (void*) ((&cs)[-1]), CS_CODEPATH_INITIALIZE_WITHOUT_SPIN, Settings::CriticalSections::iDefaultSpin );
	}
}
void WINAPI defaultspin2_DeleteCriticalSection    ( CRITICAL_SECTION *cs ) {(*(void (WINAPI *)( CRITICAL_SECTION * )) *Hook_DeleteCriticalSection)(cs);}
BOOL WINAPI defaultspin2_InitializeCriticalSectionAndSpinCount( CRITICAL_SECTION *cs, DWORD spin ) {
	//the wrapper this is intended for does not include this function
	//so it should never be used
	if (Settings::CriticalSections::bEnableProfiling) {
		_profiling_initialize_critical_section ( cs, (void*) ((&cs)[-1]), CS_CODEPATH_INITIALIZE_WITH_SPIN, spin );
	}
	else {
		_standard_initialize_critical_section  ( cs, (void*) ((&cs)[-1]), CS_CODEPATH_INITIALIZE_WITH_SPIN, spin );
	}
	return true;
}
DWORD WINAPI defaultspin2_SetSpinCount( CRITICAL_SECTION *cs, DWORD spin ) {return ::SetCriticalSectionSpinCount(cs, (spin & ~SPIN_MASK) + (cs->SpinCount & SPIN_MASK));}
DECLARE_SPINFUNCS(CSF_defaultspin2, defaultspin2_)


//#define TEST_DO_SUPPRESS() 	(int(cs) == suppressed_section_address1 || int(cs) == suppressed_section_address2)
//#define TEST_DO_SUPPRESS() (false)
/*
void WINAPI test_EnterCriticalSection     ( CRITICAL_SECTION *cs ) {
	if (check_suppression_list(cs)) return;

	CS_TIME_TYPE t = CS_GET_TIME();
	int waiters = cs->LockCount + 1 - cs->RecursionCount;
	if (!waiters && ::TryEnterCriticalSection(cs)) return;
	ImproveFairness(cs);
	EnterCriticalSection(cs);
	CS_TIME_TYPE delta = CS_GET_TIME() - t;
	int pdi = get_perfdata_index(cs);
//	if (!(cs->SpinCount & (1 << SPIN_FLAG_BITS))) {CS_EVENT_CREATION(cs,((&cs)[-1]), 2)}
	{CS_EVENT_ENTRY(cs, delta, waiters, ((&cs)[-1]));}
}
BOOL WINAPI test_TryEnterCriticalSection  ( CRITICAL_SECTION *cs ) {
	if (check_suppression_list(cs)) return TRUE;
	BOOL r = ::TryEnterCriticalSection(cs);
	return r;
}
void WINAPI test_LeaveCriticalSection     ( CRITICAL_SECTION *cs ) {
	if (check_suppression_list(cs)) return;
	::LeaveCriticalSection(cs);
}
void WINAPI test_InitializeCriticalSection( CRITICAL_SECTION *cs ) {
	int spin = spin_default2;
	check_spins(cs, (void*) ((&cs)[-1]), spin);
	spin &= ~SPIN_FLAG_TEST1;
	::InitializeCriticalSectionAndSpinCount(cs, spin);
	{CS_EVENT_CREATION(cs,((&cs)[-1]), 0)}
}
void WINAPI test_DeleteCriticalSection    ( CRITICAL_SECTION *cs ) {
	::DeleteCriticalSection(cs);
}
void WINAPI test_InitializeCriticalSectionAndSpinCount( CRITICAL_SECTION *cs, int spin ) {
	DWORD threadid = GetCurrentThreadId();
	UInt32 caller = UInt32((&cs)[-1]);
	int spin2 = spin;
	if (spin_special_index < 3 && spin_special[spin_special_index] >= 0)
		spin2 = spin_special[spin_special_index];
	message("Special crit. sec. %d (@ %X, thread %X, inited from %X) spin %d -> %d", ++spin_special_index, cs, threadid, caller, spin, spin2);
	::InitializeCriticalSectionAndSpinCount(cs, spin2);
	{CS_EVENT_CREATION(cs,caller, 1)}
}
DWORD WINAPI test_SetSpinCount( CRITICAL_SECTION *cs, DWORD spin ) {return ::SetCriticalSectionSpinCount(cs, (spin & ~SPIN_MASK2) + (cs->SpinCount & SPIN_MASK2));}
DECLARE_SPINFUNCS(CSF_test, test_)
*/

void get_active_CSFs2 ( CSF *csf ) {
	if (!Hook_EnterCriticalSection_B) {
		csf->EnterCriticalSection      = NULL;
		csf->TryEnterCriticalSection   = NULL;
		csf->LeaveCriticalSection      = NULL;
		csf->InitializeCriticalSection = NULL;
		csf->DeleteCriticalSection     = NULL;
		return;
	}
	csf->EnterCriticalSection      = (void (WINAPI *)( CRITICAL_SECTION *cs ))**Hook_EnterCriticalSection_B;
	csf->TryEnterCriticalSection   = (BOOL (WINAPI *)( CRITICAL_SECTION *cs ))**Hook_TryEnterCriticalSection_B;
	csf->LeaveCriticalSection      = (void (WINAPI *)( CRITICAL_SECTION *cs ))**Hook_LeaveCriticalSection_B;
	csf->InitializeCriticalSection = (void (WINAPI *)( CRITICAL_SECTION *cs ))**Hook_InitializeCriticalSection_B;
	csf->DeleteCriticalSection     = (void (WINAPI *)( CRITICAL_SECTION *cs ))**Hook_DeleteCriticalSection_B;//*/
//	csf->InitializeCriticalSectionAndSpinCount
}
void get_active_CSFs1 ( CSF *csf ) {
	csf->EnterCriticalSection      = (void (WINAPI *)( CRITICAL_SECTION * ))*Hook_EnterCriticalSection;
	csf->TryEnterCriticalSection   = (BOOL (WINAPI *)( CRITICAL_SECTION * ))*Hook_TryEnterCriticalSection;
	csf->LeaveCriticalSection      = (void (WINAPI *)( CRITICAL_SECTION * ))*Hook_LeaveCriticalSection;
	csf->InitializeCriticalSection = (void (WINAPI *)( CRITICAL_SECTION * ))*Hook_InitializeCriticalSection;
	csf->DeleteCriticalSection     = (void (WINAPI *)( CRITICAL_SECTION * ))*Hook_DeleteCriticalSection;
	csf->InitializeCriticalSectionAndSpinCount = (BOOL (WINAPI *)( CRITICAL_SECTION *,DWORD ))*Hook_InitializeCriticalSectionAndSpinCount;	
}
#ifdef OBLIVION //replaced by LCS stuff in Fallout3
void set_active_CSFs2 ( const CSF *csf ) {
	if (!Hook_EnterCriticalSection_B) return;
	SafeWrite32( UInt32(Hook_EnterCriticalSection_B),      UInt32(&csf->EnterCriticalSection));
	SafeWrite32( UInt32(Hook_TryEnterCriticalSection_B),   UInt32(&csf->TryEnterCriticalSection));
	SafeWrite32( UInt32(Hook_LeaveCriticalSection_B),      UInt32(&csf->LeaveCriticalSection));
	SafeWrite32( UInt32(Hook_InitializeCriticalSection_B), UInt32(&csf->InitializeCriticalSection));
	SafeWrite32( UInt32(Hook_DeleteCriticalSection_B),     UInt32(&csf->DeleteCriticalSection));//*/
//	csf->InitializeCriticalSectionAndSpinCount
}
#endif
void set_active_CSFs1 ( const CSF *csf ) {
	CSF_active = *csf;
	SafeWrite32( UInt32(Hook_EnterCriticalSection),      UInt32(csf->EnterCriticalSection));
	SafeWrite32( UInt32(Hook_TryEnterCriticalSection),   UInt32(csf->TryEnterCriticalSection));
	SafeWrite32( UInt32(Hook_LeaveCriticalSection),      UInt32(csf->LeaveCriticalSection));
	SafeWrite32( UInt32(Hook_InitializeCriticalSection), UInt32(csf->InitializeCriticalSection));
	SafeWrite32( UInt32(Hook_DeleteCriticalSection),     UInt32(csf->DeleteCriticalSection));
	SafeWrite32( UInt32(Hook_InitializeCriticalSectionAndSpinCount), UInt32(csf->InitializeCriticalSectionAndSpinCount));
}
void CS_explicit_override ( CRITICAL_SECTION *cs, int mode, int spin ) {
	while (1) {
		UInt32 oldvalue = cs->SpinCount;
		UInt32 spin2 = spin;
		UInt32 mode2 = mode;
		if (spin2 == -1) spin2 = oldvalue;
		if (mode2 == -1) mode2 = oldvalue;
		UInt32 newvalue = (spin2 & ~SPIN_MASK) + (mode2 & SPIN_MASK);
		UInt32 check = ::InterlockedCompareExchange((long *)&cs->SpinCount, newvalue, oldvalue);
		if (check == oldvalue) return;
	}
}
//void suppress_critical_section ( CRITICAL_SECTION *cs ) {
//	set_critical_section_mode(cs, SPIN_MODE_DEAD);
//}


#if defined FALLOUT || defined NEW_VEGAS

namespace LCS_full {
	enum { LCS_BASE_RECURSION = 0x8000 };
	//static UInt32 recursion_to_thread_number(UInt32 value) {return value >> LCS_THREAD_NUMBER_SHIFT;}
	struct FakeLightCS;
	struct FakeLightCS_ExtraData {
		int mode;//not yet implemented
		int spin_count;//not yet implemented
		volatile unsigned long contention_count;//only updated when profiling is enabled
		unsigned long uncontention_count;//only updated when profiling is enabled
		PerfData3::PerCS *profiling_data;
		const char *message_ptrs[16];
		FakeLightCS *lcs;
		FakeLightCS_ExtraData() : spin_count(0), contention_count(0), uncontention_count(0), profiling_data(NULL) {
			for (int i = 0; i < 16; i++) message_ptrs[i] = NULL;
		}
	};
	std::map<void*,FakeLightCS_ExtraData*> LCS_lookup;
	struct FakeLightCS {
		FakeLightCS_ExtraData *data;
		volatile DWORD recursion_count;

	};
	void LCS_init ( FakeLightCS *f, const void *caller, int codepath ) {
		DWORD id = GetCurrentThreadId();

	/*	UInt32 t = get_time_ms();
		while (InterlockedCompareExchange( &f->owning_thread, id, 0 )) {//prevent race condition on initialization
			UInt32 delta = get_time_ms() - t;
			if (delta > 100) {
				message("LCS_init on %X may be in trouble", f);
			}
		}*/

		EnterCriticalSection(&PerThread::internal_cs);
		if (f->recursion_count >= LCS_BASE_RECURSION) {//already initialized
			if (!f->data) {//..but somehow damaged?
				message("LCS_init found damaged LCS at %X from %X(%d) ?!?", f, caller, codepath);
				std::map<void*,FakeLightCS_ExtraData*>::iterator it = LCS_lookup.find(f);
				if (it == LCS_lookup.end()) {error("LCS_init non-recoverable", f, caller, codepath);}
				f->data = it->second;
			}
			LeaveCriticalSection(&PerThread::internal_cs);
			return;
		}

		int mode = Settings::LightCriticalSections::iDefaultMode;
		int spin = Settings::LightCriticalSections::iDefaultSpin;

		std::map<LightCS*,LCS_override_data>::iterator by_object = LCS_override_by_object.find( (LightCS*)f );
		std::map<const void*,LCS_override_data>::iterator by_caller = LCS_override_by_caller.find(caller);

		if (by_object != LCS_override_by_object.end()) {
			if (by_object->second.mode != -1) mode = by_object->second.mode;
			if (by_object->second.spin != -1) spin = by_object->second.spin;
		}
		if (by_caller != LCS_override_by_caller.end()) {
			if (by_caller->second.mode != -1) mode = by_caller->second.mode;
			if (by_caller->second.spin != -1) spin = by_caller->second.spin;
		}
		if (Settings::LightCriticalSections::bEnableMessages) {
			if (by_object != LCS_override_by_object.end()) {
				if (by_object->second.mode != -1)
					message("LCS 0x%08X (caller 0x%X, spin%d), by-object, set to mode %d", f, caller, codepath, spin, by_object->second.mode );
				if (by_object->second.spin != -1)
					message("LCS 0x%08X (caller 0x%X, spin%d), by-object, set to spin %d", f, caller, codepath, spin, by_object->second.spin );
			}
			if (by_caller != LCS_override_by_caller.end()) {
				if (by_caller->second.mode != -1)
					message("LCS 0x%08X (caller 0x%X, spin%d), by-caller, set to mode %d", f, caller, codepath, spin, by_caller->second.mode );
				if (by_caller->second.spin != -1)
					message("LCS 0x%08X (caller 0x%X, spin%d), by-caller, set to spin %d", f, caller, codepath, spin, by_caller->second.spin );
			}
			if (by_object != LCS_override_by_object.end() && by_caller != LCS_override_by_caller.end()) {
				if ((by_object->second.mode != by_caller->second.mode) || (by_object->second.spin != by_caller->second.spin))
					message("  WARNING - overrides disagree");
			}
	//		message("CS 0x%08X (caller 0x%X,path%d,oldspin%d), set to mode%d / spin%d", cs, caller, codepath, oldspin, mode, spin );
		}
		if (spin < 0) spin = 0;
		switch (mode) {
			case CS_MODE_NORMAL:
			case CS_MODE_FAIR:
			case CS_MODE_STAGGER:
			case CS_MODE_DEAD:
			case CS_MODE_PRIORITY:
			case CS_MODE_UNPRIORITY:
				break;
			default:
				error("invalid critical section mode %d in initialize_critical_section", mode);
		}
		FakeLightCS_ExtraData *data = new FakeLightCS_ExtraData();
		data->mode = mode;
		data->spin_count = spin;
		data->lcs = f;
		LCS_lookup[f] = data;
		f->data = data;

		LeaveCriticalSection(&PerThread::internal_cs);
		f->recursion_count = LCS_BASE_RECURSION;
	}
	inline void do_staggering_wait( FakeLightCS *lcs, PerThread *pt ) {
		if (pt->cs_switch-- != 0) return;
		pt->cs_switch = pt->internal_rng.raw16() & LCS_spin_stagger_level;

		Sleep(0);
		if ((lcs->recursion_count & 0xFFFF) == LCS_BASE_RECURSION) return;
		Sleep(1);
	}
	inline void do_fair_wait ( FakeLightCS *lcs ) {
		Sleep(0);
		if ((lcs->recursion_count & 0xFFFF) == LCS_BASE_RECURSION) return;
		Sleep(1);
	}
	bool __fastcall LCS_internal_TryEnter(FakeLightCS *f, int thread_number) {
		if (f->data->mode == CS_MODE_DEAD) return true;

		UInt32 rc = f->recursion_count;
		if ((rc & 0xFFFF) != LCS_BASE_RECURSION) {//not available, but maybe we own it?
			if (rc >> 16 == thread_number) {
				f->recursion_count++;
				return true;
			}
			return false;
		}
		else {//available... if we're fast enough
			return rc == InterlockedCompareExchange( &f->recursion_count, (thread_number << 16) + (LCS_BASE_RECURSION+1), rc );
		}
	}
	bool __fastcall LCS_internal_ReTryEnter(FakeLightCS *f, int thread_number) {//called only if TryEnter already failed
		UInt32 rc = f->recursion_count;
		if ((rc & 0xFFFF) != LCS_BASE_RECURSION) return false;
		return rc == InterlockedCompareExchange( &f->recursion_count, (thread_number << 16) + (LCS_BASE_RECURSION+1), rc );
	}
	bool __fastcall LCS_standard_TryEnter(LightCS *lcs) {
		FakeLightCS *&f = (FakeLightCS*&)lcs;
		if (!f->data) LCS_init(f, (const void*)0x00000001, 1);//grabbing the callers address off the stack when there are no stack params... I can probably use a local var, but I'm not sure that will stay working across compiler versions/settings

		int thread_number = PerThread::get_PT()->thread_number;
		return LCS_internal_TryEnter(f, thread_number);
	}
	void __fastcall LCS_standard_Leave(LightCS *lcs) {
		FakeLightCS *&f = (FakeLightCS*&)lcs;
		if (f->data->mode == CS_MODE_DEAD) return;

		//InterlockedDecrement(&f->recursion_count);
		f->recursion_count--;
	}
	void __fastcall LCS_standard_Enter(LightCS *lcs, int dummy_parameter, const char *name ) {
		FakeLightCS *&f = (FakeLightCS*&)lcs;
		if (!f->data) LCS_init(f, (const void*)(&name)[-1], 0);

		PerThread *pt = PerThread::get_PT();
		int thread_number = pt->thread_number;
		if (LCS_internal_TryEnter(f, thread_number)) return;

		UInt32 t = get_time_ms();
		UInt32 t2 = t;
		UInt32 last_print = t;
		UInt32 waiters = 0;//1 + f->data->contention_count - f->data->uncontention_count;
		//message("LCS enter %d->%08X, encountering contention (%d, %d ?)", thread_number, f, f->recursion_count >> 16, waiters);
		//InterlockedIncrement(&f->data->contention_count);

		switch (f->data->mode) {
			case CS_MODE_FAIR: {
				do_fair_wait(f);
			}
			break;
			case CS_MODE_STAGGER: {
				do_staggering_wait(f, pt);
			}
			break;
			case CS_MODE_DEAD: {
				return;
			}
			break;
			case CS_MODE_NORMAL: {
			}
			break;
			case CS_MODE_PRIORITY: {
				if (thread_number != 1) do_fair_wait(f);
			}
			break;
			case CS_MODE_UNPRIORITY: {
				if (thread_number == 1) do_fair_wait(f);
			}
			break;
		}

		int times_spun = 0;
		int spin_count = f->data->spin_count;
		while (true) {
			if (LCS_internal_ReTryEnter(f, thread_number)) break;
			t2 = get_time_ms();
			UInt32 delta = t2 - last_print;
			if (delta > 1000) {
				last_print += 1000;
				message("LCS %X is taking a very long time to enter (%d)... owning thread is %X, current is %d", f, t2 - t, f->recursion_count>>16, thread_number);
			}

			if (times_spun < spin_count) ;
			else if (times_spun < spin_count + 10000) Sleep(0);
			else Sleep(1);
			times_spun++;
		}
		//f->data->uncontention_count++;
		//message("LCS enter %d->%08X, resolved contention", thread_number, lcs);
		UInt32 delta = t2 - t;
		if (delta >= 2 && Settings::LightCriticalSections::bEnableMessages) {
			DWORD cpu = GetCurrentProcessorNumberXP();
			const void * caller = (&name)[-1];
			message("time%8.2f:%2X:%d:%08X waited on CS %08x for%5d ms (? waiting threads, name %s)", t2 * 0.001, pt->thread_number, cpu, caller, lcs, delta, name ? name : "NULL");
		}
	}




	bool __fastcall LCS_profiling_TryEnter(LightCS *lcs) {
		FakeLightCS *&f = (FakeLightCS*&)lcs;
		if (!f->data) LCS_init(f, (const void*)0x00000001, 1);//grabbing the callers address off the stack when there are no stack params... I can probably use a local var, but I'm not sure that will stay working across compiler versions/settings
		if (UInt32(f->data) < 1<<16) message("LCS %X has f->data %X (TryEnter)", f, f->data);

		int thread_number = PerThread::get_PT()->thread_number;
		return LCS_internal_TryEnter(f, thread_number);
	}
	void __fastcall LCS_profiling_Leave(LightCS *lcs) {
		FakeLightCS *&f = (FakeLightCS*&)lcs;
		if (UInt32(f->data) < 1<<16) message("LCS %X has f->data %X (Leave)", f, f->data);
		if (f->data->mode == CS_MODE_DEAD) return;

		//InterlockedDecrement(&f->recursion_count);
		f->recursion_count--;
	}
	void __fastcall LCS_profiling_Enter(LightCS *lcs, int dummy_parameter, const char *name ) {
		FakeLightCS *&f = (FakeLightCS*&)lcs;
		if (!f->data) LCS_init(f, (const void*)(&name)[-1], 0);
		if (UInt32(f->data) < 1<<16) message("LCS %X has f->data %X (Enter)", f, f->data);

		PerThread *pt = PerThread::get_PT();
		int thread_number = pt->thread_number;
		if (LCS_internal_TryEnter(f, thread_number)) return;

		int waiters = 1 + f->data->contention_count - f->data->uncontention_count;
		CS_TIME_TYPE t = CS_GET_TIME();
		CS_TIME_TYPE t2 = t;
		CS_TIME_TYPE last_print = t;
		//message("LCS enter %d->%08X, encountering contention (%d, %d ?)", thread_number, f, f->recursion_count >> 16, waiters);
		InterlockedIncrement(&f->data->contention_count);

		switch (f->data->mode) {
			case CS_MODE_FAIR: {
				do_fair_wait(f);
			}
			break;
			case CS_MODE_STAGGER: {
				do_staggering_wait(f, pt);
			}
			break;
			case CS_MODE_NORMAL: {
			}
			break;
			case CS_MODE_PRIORITY: {
				if (thread_number != 1) do_fair_wait(f);
			}
			break;
			case CS_MODE_UNPRIORITY: {
				if (thread_number == 1) do_fair_wait(f);
			}
			break;
		}

		int times_spun = 0;
		int spin_count = f->data->spin_count;
		while (true) {
			t2 = CS_GET_TIME();
			if (LCS_internal_ReTryEnter(f, thread_number)) break;
			CS_TIME_TYPE delta = t2 - last_print;
			if (delta > CS_TIME_SIGUNIT * 1000) {
				last_print += CS_TIME_SIGUNIT * 1000;
				message("LCS %X is taking a very long time to enter (%.2fs)... owning thread is %X, current is %d", f, (t2 - t) * CS_TIME_PERIOD(), f->recursion_count>>16, thread_number);
			}

			if (times_spun < spin_count) ;
			else if (times_spun < spin_count + 10000) Sleep(0);
			else Sleep(1);
			times_spun++;
		}
		f->data->uncontention_count++;
		//message("LCS enter %d->%08X, resolved contention", thread_number, lcs);
		CS_TIME_TYPE delta = t2 - t;
		if (delta >= CS_TIME_SIGUNIT * 1 && Settings::LightCriticalSections::bEnableMessages) {
			DWORD cpu = GetCurrentProcessorNumberXP();
			const void * caller = (&name)[-1];
			double period = CS_TIME_PERIOD();
			message("time%8.2f:%2d:%d:%08X waited on LCS %08x for%5f ms (%d waiting threads, name %s)", t2 * period, pt->thread_number, cpu, caller, lcs, delta*period*1000, waiters, name?name:"[NULL]");
		}
		const void *caller = (const void*)(&name)[-1];
		if (!f->data->profiling_data) {
			EnterCriticalSection(&PerfData3::pd3_cs);
			PerfData3::PerCS &pcs = PerfData3::CS_map[f->data];
			f->data->profiling_data = &pcs;
			LeaveCriticalSection(&PerfData3::pd3_cs);
			pcs.first_caller = caller;
			pcs.cs = f;
			pcs.type = PerfData3::LCS_TYPE;
		}
		PerfData3::PerCS &pcs = *f->data->profiling_data;
		pcs.blockings++;
		pcs.time += delta;
		if (delta > pcs.worst_time) pcs.worst_time = delta;
		if (waiters > pcs.worst_waiters) pcs.worst_waiters = waiters;
		PerfData3::maybe_print_summary();
	}
}
namespace LCS_partial_hooks {
	void __inline LCS_exit ( LightCS *lcs ) {
		if (--lcs->recursion_count) return;
		DWORD thread_id = GetCurrentThreadId();
		InterlockedCompareExchange(&lcs->thread_id, 0, lcs->thread_id);
	}

	inline void do_staggering_wait( LightCS *lcs ) {
		PerThread *pt = PerThread::get_PT();
		if (pt->cs_switch-- != 0) return;
		pt->cs_switch = pt->internal_rng.raw16() & LCS_spin_stagger_level;

		Sleep(0);
		if (!lcs->thread_id) return;
		Sleep(1);
	}
	inline void do_fair_wait ( LightCS *lcs ) {
		Sleep(0);
		if (!lcs->thread_id) return;
		Sleep(1);
	}
	bool __fastcall LCS_TryEnter ( LightCS *lcs ) {
		DWORD thread_id = GetCurrentThreadId();
		DWORD old = lcs->thread_id;
		if (old == thread_id) {
			lcs->recursion_count++;
			return true;
		}
		if (old != 0) return false;
		if (!InterlockedCompareExchange(&lcs->thread_id, thread_id, 0)) {
			lcs->recursion_count++;
			return true;
		}
		return false;
	}
	void __fastcall LCS_standard_Enter ( LightCS *lcs, int dummy_parameter, char *name ) {
		if (LCS_TryEnter(lcs)) return;
		switch (Settings::LightCriticalSections::iDefaultMode) {
			case CS_MODE_NORMAL: 
			break;
			case CS_MODE_FAIR: do_fair_wait(lcs);
			break;
			case CS_MODE_STAGGER: do_staggering_wait(lcs);
			break;
			case CS_MODE_DEAD: return;
			break;
			case CS_MODE_PRIORITY: if (PerThread::get_PT()->thread_number != 1) do_fair_wait(lcs);
			break;
			case CS_MODE_UNPRIORITY: if (PerThread::get_PT()->thread_number == 1) do_fair_wait(lcs);
			break;
		}
		int count = 0;
		while (1) {
			if (LCS_TryEnter(lcs)) break;
			if (count < Settings::LightCriticalSections::iDefaultSpin) ;
			else if (count < 10000) ::Sleep(0);
			else ::Sleep(1);
		}
	}
	void __fastcall LCS_profile_Enter ( LightCS *lcs, int dummy_parameter, char *name ) {
		if (LCS_TryEnter(lcs)) return;
		CS_TIME_TYPE t1 = CS_GET_TIME();
		switch (Settings::LightCriticalSections::iDefaultMode) {
			case CS_MODE_NORMAL: break;
			case CS_MODE_FAIR: do_fair_wait(lcs); break;
			case CS_MODE_STAGGER: do_staggering_wait(lcs); break;
			case CS_MODE_DEAD: return; break;
			case CS_MODE_PRIORITY: if (PerThread::get_PT()->thread_number != 1) do_fair_wait(lcs);
			break;
			case CS_MODE_UNPRIORITY: if (PerThread::get_PT()->thread_number == 1) do_fair_wait(lcs);
			break;
		}
		int count = 0;
		while (1) {
			if (LCS_TryEnter(lcs)) break;
			if (count < Settings::LightCriticalSections::iDefaultSpin) ;
			else if (count < 10000) ::Sleep(0);
			else ::Sleep(1);
		}
		CS_TIME_TYPE t2 = CS_GET_TIME();
		{CS_EVENT_ENTRY(lcs, t1, t2, -99, ((&name)[-1]));}
	}
}
#endif //defined FALLOUT || defined NEW_VEGAS
#if 0
void __inline LCS_exit ( LightCS *lcs ) {
	if (--lcs->recursion_count) return;
	DWORD thread_id = GetCurrentThreadId();
	InterlockedCompareExchange(&lcs->thread_id, 0, lcs->thread_id);
}

void __fastcall LCS_basic ( LightCS *lcs, int dummy_parameter, char *name ) {
	if (lcs->recursion_count >= 4096) {
		if (lcs->recursion_count < 8192) lcs->recursion_count = 1<<20;
		return;
	}
	DWORD thread_id = GetCurrentThreadId();
	if (lcs->thread_id == thread_id) {
		lcs->recursion_count++;
		return;
	}
	int count = 0;
	while (1) {
		if (!InterlockedCompareExchange(&lcs->thread_id, thread_id, 0)) {
			lcs->recursion_count++;
			return;
		}
		if (count < 10000) ::Sleep(0);
		else ::Sleep(1);
	}
}
void __fastcall LCS_basic_profile ( LightCS *lcs, int dummy_parameter, char *name ) {
	if (lcs->recursion_count >= 4096) {
		if (lcs->recursion_count < 8192) lcs->recursion_count = 1<<20;
		return;
	}
	DWORD thread_id = GetCurrentThreadId();
	if (lcs->thread_id == thread_id) {
		lcs->recursion_count++;
		return;
	}
	CS_TIME_TYPE t = CS_GET_TIME();
	int count = 0;
	while (1) {
		if (!InterlockedCompareExchange(&lcs->thread_id, thread_id, 0)) {
			CS_TIME_TYPE t2 = CS_GET_TIME();
			{CS_EVENT_ENTRY(lcs, t, t2, -99, ((&name)[-1]));}
			lcs->recursion_count++;
			return;
		}
		::Sleep(count < 10000 ? 0 : 1);
	}
}
void __fastcall LCS_spin ( LightCS *lcs, int dummy_parameter, char *name ) {
	if (lcs->recursion_count >= 4096) {
		if (lcs->recursion_count < 8192) lcs->recursion_count = 1<<20;
		return;
	}
	DWORD thread_id = GetCurrentThreadId();
	if (lcs->thread_id == thread_id || !InterlockedCompareExchange(&lcs->thread_id, thread_id, 0)) {
		lcs->recursion_count++;
		return;
	}
//	CS_TIME_TYPE t = CS_GET_TIME();
	for (int spin = 0; spin < Settings::LightCriticalSections::iDefaultSpin; spin++) {
		if (!InterlockedCompareExchange(&lcs->thread_id, thread_id, 0)) goto end;
	}
	for (int spin = 0; spin < Settings::LightCriticalSections::iDefaultSpin; spin++) {
		if (!InterlockedCompareExchange(&lcs->thread_id, thread_id, 0)) goto end;
		::Sleep(0);
	}
	while (1) {
		if (!InterlockedCompareExchange(&lcs->thread_id, thread_id, 0)) goto end;
		::Sleep(1);
	}
end:
//	CS_TIME_TYPE t2 = CS_GET_TIME();
//	{CS_EVENT_ENTRY(lcs, t,t2, -99, ((&name)[-1]));}
	lcs->recursion_count++;
	return;
}
void __fastcall LCS_spin_profile ( LightCS *lcs, int dummy_parameter, char *name ) {
	if (lcs->recursion_count >= 4096) {
		if (lcs->recursion_count < 8192) lcs->recursion_count = 1<<20;
		return;
	}
	DWORD thread_id = GetCurrentThreadId();
	if (lcs->thread_id == thread_id || !InterlockedCompareExchange(&lcs->thread_id, thread_id, 0)) {
		lcs->recursion_count++;
		return;
	}
	CS_TIME_TYPE t = CS_GET_TIME();
	for (int spin = 0; spin < Settings::LightCriticalSections::iDefaultSpin; spin++) {
		if (!InterlockedCompareExchange(&lcs->thread_id, thread_id, 0)) goto end;
	}
	for (int spin = 0; spin < Settings::LightCriticalSections::iDefaultSpin; spin++) {
		if (!InterlockedCompareExchange(&lcs->thread_id, thread_id, 0)) goto end;
		::Sleep(0);
	}
	while (1) {
		if (!InterlockedCompareExchange(&lcs->thread_id, thread_id, 0)) goto end;
		::Sleep(1);
	}
end:
	CS_TIME_TYPE t2 = CS_GET_TIME();
	{CS_EVENT_ENTRY(lcs, t,t2, -99, ((&name)[-1]));}
	lcs->recursion_count++;
	return;
}

void __fastcall LCS_partial_helper ( LightCS *lcs, int thread_id, bool passive ) {
	long old_thread = lcs->thread_id;
	if (passive) Sleep(0);
	if (old_thread == thread_id || (!old_thread && !InterlockedCompareExchange(&lcs->thread_id, thread_id, 0))) {
		lcs->recursion_count++;
		return;
	}
//	CS_TIME_TYPE t = CS_GET_TIME();
	while (true) {
		if (!(old_thread = InterlockedCompareExchange(&lcs->thread_id, thread_id, 0))) {
			lcs->recursion_count++;
//			CS_TIME_TYPE t2 = CS_GET_TIME();
//			{CS_EVENT_ENTRY(lcs, t,t2, -99, caller);}
			return;
		}
		Sleep(0);
	}
}
void __fastcall LCS_partial_helper_profile ( LightCS *lcs, int thread_id, bool passive, const void *caller ) {
//	if (lcs->recursion_count >= 4096) {
//		if (lcs->recursion_count < 8192) lcs->recursion_count = 1<<20;
//		return;
//	}
	long old_thread = lcs->thread_id;
//	int tmp;
	if (passive) Sleep(0);
	if (old_thread == thread_id || (!old_thread && !InterlockedCompareExchange(&lcs->thread_id, thread_id, 0))) {
		lcs->recursion_count++;
		return;
	}
	CS_TIME_TYPE t = CS_GET_TIME();
	while (true) {
		if (!(old_thread = InterlockedCompareExchange(&lcs->thread_id, thread_id, 0))) {
			lcs->recursion_count++;
			CS_TIME_TYPE t2 = CS_GET_TIME();
			{CS_EVENT_ENTRY(lcs, t,t2, -99, caller);}
			return;
		}
		Sleep(0);
	}

/*	enum { FLAG = 1<<12 };
	static volatile long thread_interest_mask = 0;
//	static volatile long thread_last_entrance_caller[32] = {NULL};
//	static volatile long thread_last_entrance_LCS[32] = {NULL};
//	static long last_106E5A0_entrance = NULL;
//	static long last_116FCA0_entrance = NULL;
//	if ((thread_interest_mask >> thread_id) & 1) {
//		message("thread %d active (of %d)", thread_id, thread_interest_mask);
//	}
	if (thread_id == (old_thread & (FLAG-1))) {
		lcs->recursion_count += 1;
		return;
	}
	else if (!old_thread) {
		old_thread = InterlockedCompareExchange(&lcs->thread_id, thread_id, 0);
		if (!old_thread) {
//			message("(self=%d, LCS=%X) readily available", thread_id, lcs);
			//not locked or reserved by any other thread
			lcs->recursion_count += 1;
//			if (int(lcs) == 0x106E5A0) InterlockedExchange(&last_106E5A0_entrance, int(caller) | ((thread_id == 1)?0x80000000:0)); else if (int(lcs) == 0x116FCA0) InterlockedExchange(&last_116FCA0_entrance, int(caller) | ((thread_id == 1)?0x80000000:0));
			return;
		}
	}
	CS_TIME_TYPE t = CS_GET_TIME();
	bool have_priority = false;
	int tries = 0;
	UInt32 _t = timeGetTime();
#define MICROWAIT() MemoryBarrier()
//#define MICROWAIT() 
	while (1) {
		tries++;
		UInt32 _delta = timeGetTime() - _t;
		if (_delta >= 4000) {
			message("(self=%2d,LCS=%X,other:%2d,flag:%d,raw:%d:%d) %s %d %X", thread_id,lcs,old_thread&(FLAG-1),(old_thread&FLAG)?1:0,*(volatile long*)&lcs->thread_id,lcs->recursion_count, "stuck in loop?", _delta, caller);
//			int other = old_thread&(FLAG-1);
//			if (other < 32) {
//				other = 1 << other;
//				UInt32 a = thread_interest_mask;
//				if (!(a & other)) {
//					while (true) {
//						a = InterlockedCompareExchange(&thread_interest_mask, a|other, a);
//						if (a & other) break;
//					}
//				}
//			}
			Sleep((_delta - 2000) / 10);
			old_thread = lcs->thread_id;
//			message("(self=%2d,LCS=%X,other:%2d,flag:%d) %s %d %X (%X,%X)", thread_id,lcs,old_thread&(FLAG-1),(old_thread&FLAG)?1:0, "stuck in loop?", _delta, caller, last_106E5A0_entrance, last_116FCA0_entrance);
		}
		if (old_thread & FLAG) {
			if (have_priority) {//unoccupied and reserved by self
				if (old_thread == FLAG) {
					message("(self=%2d,LCS=%X,other:%2d) %s", thread_id,lcs,old_thread, "unoccupied and reserved by self");
					old_thread = InterlockedCompareExchange(&lcs->thread_id, thread_id, FLAG);
					if (old_thread == FLAG)
						goto end;
					else have_priority = false;//error("reserved LCS was taken by someone else?");
				}
				else {//occupied but reserved by self
//					message("(self=%2d,LCS=%X,other:%2d) %s", thread_id,lcs,old_thread, "occupied but reserved by self");
					if (system_is_multiprocessor && (tries < Settings::CriticalSections::iDefaultSpin))
						MICROWAIT();
					else Sleep(0);
					old_thread = lcs->thread_id;
				}
			}
			else {
				if (old_thread == FLAG) {
					//unoccupied but reserved by another thread
					message("(self=%2d,LCS=%X,other:%2d) %s", thread_id,lcs,old_thread, "unoccupied but reserved by another thread");
					if (system_is_multiprocessor && !passive && (tries < Settings::CriticalSections::iDefaultSpin)) {
						MICROWAIT();
					}
					else {
						Sleep(0);
					}
				}
				else {//occupied and reserved by another thread
//					message("(self=%2d,LCS=%X,other:%2d) %s", thread_id,lcs,old_thread, "occupied and reserved by another thread");
					Sleep(0);
				}
				old_thread = lcs->thread_id;
			}
		}
		else {
			if (old_thread == 0) {//unoccupied and unreserved
//				message("(self=%2d,LCS=%X,other:%2d) %s", thread_id,lcs,old_thread, "unoccupied and unreserved");
				old_thread = InterlockedCompareExchange(&lcs->thread_id, thread_id, 0);
				if (!old_thread) goto end;
			}
			else {//occupied but not reserved
//				message("(self=%2d,LCS=%X,other:%2d) %s", thread_id,lcs,old_thread, "occupied but not reserved");
				if (passive) {
					if (system_is_multiprocessor && (tries < Settings::CriticalSections::iDefaultSpin))
						MICROWAIT();
					else Sleep(0);
					old_thread = lcs->thread_id;
				}
				else {
					tmp = old_thread | FLAG;
					if ( old_thread == InterlockedCompareExchange(&lcs->thread_id, tmp, old_thread) ) {
						old_thread = tmp;
						have_priority = true;
						old_thread |= FLAG;
					}
				}
			}
		}
	}
end:
#undef MICROWAIT
	lcs->recursion_count += 1;
//	if (int(lcs) == 0x106E5A0) InterlockedExchange(&last_106E5A0_entrance, int(caller) | ((thread_id == 1)?0x80000000:0)); else if (int(lcs) == 0x116FCA0) InterlockedExchange(&last_116FCA0_entrance, int(caller) | ((thread_id == 1)?0x80000000:0));
	CS_TIME_TYPE t2 = CS_GET_TIME();
	{CS_EVENT_ENTRY(lcs, t,t2, -99, caller);}
	return;*/
}

void __fastcall LCS_fair ( LightCS *lcs, int dummy_parameter, char *name ) {
	LCS_partial_helper(lcs, PerThread::get_PT()->thread_number, false);
}
void __fastcall LCS_fair_profile ( LightCS *lcs, int dummy_parameter, char *name ) {
	LCS_partial_helper_profile(lcs, PerThread::get_PT()->thread_number, false,((&name)[-1]));
}
void __fastcall LCS_stagger ( LightCS *lcs, int dummy_parameter, char *name ) {
	PerThread *pt = PerThread::get_PT();
	LCS_partial_helper(lcs, pt->thread_number, ((pt->cs_switch++) & LCS_spin_stagger_level) ? false : true );
}
void __fastcall LCS_stagger_profile ( LightCS *lcs, int dummy_parameter, char *name ) {
	PerThread *pt = PerThread::get_PT();
	LCS_partial_helper_profile(lcs, pt->thread_number, ((pt->cs_switch++) & LCS_spin_stagger_level) ? false : true,((&name)[-1]) );
}
void __fastcall LCS_priority ( LightCS *lcs, int dummy_parameter, char *name ) {
	int tn = PerThread::get_PT()->thread_number;
	LCS_partial_helper(lcs, tn, tn!=1);
}
void __fastcall LCS_priority_profile ( LightCS *lcs, int dummy_parameter, char *name ) {
	int tn = PerThread::get_PT()->thread_number;
	LCS_partial_helper_profile(lcs, tn, tn!=1, ((&name)[-1]));
}
void __fastcall LCS_unpriority ( LightCS *lcs, int dummy_parameter, char *name ) {
	int tn = PerThread::get_PT()->thread_number;
	LCS_partial_helper(lcs, tn, tn==1);
}
void __fastcall LCS_unpriority_profile ( LightCS *lcs, int dummy_parameter, char *name ) {
	int tn = PerThread::get_PT()->thread_number;
	LCS_partial_helper_profile(lcs, tn, tn==1, ((&name)[-1]));
}
void __fastcall LCS_test ( LightCS *lcs, int dummy_parameter, char *name ) {
	if (true) {
		UInt64 t = get_time_ticks();
		for (int i = 0; i < 1000; i++) {
			CS_TIME_TYPE t2 = CS_GET_TIME();
			LCS_spin(lcs, 0, NULL);
			LCS_exit (lcs);
			{CS_EVENT_ENTRY(NULL, t2,CS_GET_TIME(), -98, ((&name)[-1]));}
		}
		UInt64 delta = get_time_ticks() - t;
		message("LCSF test: %fns", delta * get_tick_period() * (1000000000. / 1000));
	}
	DWORD thread_id = GetCurrentThreadId();
	if (lcs->thread_id == thread_id) {
		lcs->recursion_count++;
		return;
	}
	int count = 0;
	while (1) {
		if (!InterlockedCompareExchange(&lcs->thread_id, thread_id, 0)) {
			lcs->recursion_count++;
			return;
		}
		if (count < 10000) ::Sleep(0);
		else ::Sleep(1);
	}
}
namespace switchable_LCS_stuff {
	CRITICAL_SECTION internal_cs;
	struct PerLCS {
		LightCS *lcs;
		volatile long thread;
		volatile long waiters;
//		int recursion_count;
		int spin;
		int mode;
		int padding[2];
		void reset() {
			lcs = NULL;
			thread = 0;
			waiters = 0;
//			recursion_count = 0;
			spin = 0;
			mode = 0;
		}
	};
	PerLCS *PerLCS_array = NULL;
	int num_lcses = 0;
	int count_since_clean = 0;
	enum {MAX_LCSes=1<<15};
	enum {BASE_LCS_INDEX_OFFSET = 0x76421000};
	enum {NUM_BETWEEN_CLEANS = MAX_LCSes / 64};
	enum {LCS_RESERVED_FLAG = (1<<30)};
	int allocate_unused_LCS_slots() {
//		if (count_since_clean++ >= NUM_BETWEEN_CLEANS) clean_unused_LCS_slots();
		count_since_clean = 0;
		int i = num_lcses++;
		if (i >= MAX_LCSes) {
			error("switchable_LCS_stuff::new_LCS_detected - out of LCS slots");
		}
		return i;
	}
	void initialize_LCS( LightCS *lcs, const void *caller ) {
		EnterCriticalSection(&internal_cs);
		if (lcs->thread_id == 0) {
			int i = allocate_unused_LCS_slots();
			PerLCS &plcs = PerLCS_array[i];
//			plcs.reset();

			std::map<LightCS*,LCS_override_data>::iterator by_object = LCS_override_by_object.find(lcs);
			std::map<const void*,LCS_override_data>::iterator by_caller = LCS_override_by_caller.find(caller);

			int mode = Settings::LightCriticalSections::iDefaultMode;
			int oldspin = Settings::LightCriticalSections::iDefaultSpin;
			int spin = oldspin;
			if (Settings::LightCriticalSections::bEnableMessages) {
		//		message("light critical section found at 0x%08x (via 0x%08X) (codepath:%d,spin:%d)", cs, caller, codepath, spin);
			}
			if (by_object != LCS_override_by_object.end()) {
				if (by_object->second.mode != -1) mode = by_object->second.mode;
				if (by_object->second.spin != -1) spin = by_object->second.spin;
			}
			if (by_caller != LCS_override_by_caller.end()) {
				if (by_caller->second.mode != -1) mode = by_caller->second.mode;
				if (by_caller->second.spin != -1) spin = by_caller->second.spin;
			}
			if (Settings::LightCriticalSections::bEnableMessages) {
				if (by_object != LCS_override_by_object.end())
					message("LCS 0x%08X (caller 0x%X, spin%d), by-object, set to mode %d / spin %d", lcs, caller, oldspin, by_object->second.mode, by_object->second.spin );
				if (by_caller != LCS_override_by_caller.end())
					message("LCS 0x%08X (caller 0x%X, spin%d), by-caller, set to mode %d / spin %d", lcs, caller, oldspin, by_caller->second.mode, by_caller->second.spin );
				if (by_object != LCS_override_by_object.end() && by_caller != LCS_override_by_caller.end()) {
					if ((by_object->second.mode != by_caller->second.mode) || (by_object->second.spin != by_caller->second.spin))
						message("  WARNING - overrides disagree");
				}
		//		message("CS 0x%08X (caller 0x%X,path%d,oldspin%d), set to mode%d / spin%d", cs, caller, codepath, oldspin, mode, spin );
			}
			if (spin < 0) spin = 0;
			switch (mode) {
				case CS_MODE_NORMAL:
				case CS_MODE_FAIR:
				case CS_MODE_STAGGER:
				case CS_MODE_DEAD:
				case CS_MODE_PRIORITY:
					break;
				default:
					error("invalid light critical section mode %d in initialize_LCS", mode);
			}

//			if ((codepath == CS_CODEPATH_INITIALIZE_WITHOUT_SPIN) || (codepath == CS_CODEPATH_INITIALIZE_WITH_SPIN)) {
//				::InitializeCriticalSectionAndSpinCount(cs, (spin & ~SPIN_MASK) + mode);
//			}
//			else {
//				CS_explicit_override ( cs, mode, spin );
//			}
			plcs.mode = mode;
			plcs.spin = spin;
			if (plcs.mode == CS_MODE_DEAD) lcs->recursion_count = 1<<20;

			plcs.lcs = lcs;
			MemoryBarrier();
			lcs->thread_id = BASE_LCS_INDEX_OFFSET + i;
		}
		LeaveCriticalSection(&internal_cs);
	}
	void __fastcall LCS_standard_enter ( LightCS *lcs, int dummy_parameter, char *name ) {
		if (lcs->recursion_count >= 4096) {//SPIN_MODE_DEAD
			if (lcs->recursion_count < 8192) lcs->recursion_count = 1<<20;
			return;
		}
		UInt32 index = lcs->thread_id;
		if (!index) {
			initialize_LCS(lcs, (&name)[-1]);
			index = lcs->thread_id;
		}
		index -= BASE_LCS_INDEX_OFFSET;
		if (index >= MAX_LCSes) {
			error("LCS_standard_enter - index out of range");
		}
		PerLCS &plcs = PerLCS_array[index];
		PerThread *pt = PerThread::get_PT();
		int thread = pt->thread_number;
		if (plcs.mode == CS_MODE_DEAD) return;//should only happen the first time it is entered
		int old_thread = plcs.thread;
		//simply recursing
		if ((old_thread & 255) == thread) {
			lcs->recursion_count++;
			return;
		}
		//no contest
		if (old_thread == 0) {
			old_thread = InterlockedCompareExchange(&plcs.thread, thread, 0);
			if (old_thread == 0) {
				lcs->recursion_count++;
				return;
			}
		}
		CS_TIME_TYPE t;
		if (Settings::LightCriticalSections::bEnableProfiling) t = CS_GET_TIME();
		bool passive = false;
		switch (plcs.mode) {
			case CS_MODE_NORMAL:
				passive = true;
				break;
			case CS_MODE_FAIR:
				break;
			case CS_MODE_STAGGER:
				passive = ((pt->cs_switch++) & LCS_spin_stagger_level) ? false : true;
				break;
			case CS_MODE_PRIORITY:
				passive = (thread != 1);
				break;
			case CS_MODE_UNPRIORITY:
				passive = (thread == 1);
				break;
			default:
				error("LCS_standard_enter - no such spin mode");
				break;
		}
//		InterlockedIncrement(&plcs.waiters);
//#define MICROWAIT() MemoryBarrier()
#define MICROWAIT() 
		bool have_priority = false;
		int left = plcs.spin;
		UInt32 start_ms = timeGetTime();
		while (1) {
			left--;
			int delta = UInt32(timeGetTime() - start_ms);
			int other = old_thread & 255;
			bool reserved = (old_thread & LCS_RESERVED_FLAG) ? true : false;
			if (delta >= 20) {
				if (passive) {
					passive = false;
				}
				if (delta >= 3000) {
					message("LCS_standard_enter - thread %d taking too long (%d) to enter LCS %X (state:%d:%d, raw:%x)", thread, delta, lcs, other, reserved?1:0, plcs.thread);
					Sleep((delta - 1000) / 20);
				}
			}
			if (other == 0) {
				if (reserved) {
					if (have_priority) {//unoccupied, we have priority
						int tmp = InterlockedCompareExchange(&plcs.thread, thread, old_thread);
						if (tmp == old_thread) goto end;
						else error("LCS_standard_enter - someone stole our LCS?");
					}
					else {//unoccupied, another thread has priority
						if ( passive || !system_is_multiprocessor || left <= 0 ) Sleep(0);
						else MICROWAIT();
						old_thread = plcs.thread;
					}
				}
				else {
					//unoccupied, not reserved, therefore available
					int tmp = InterlockedCompareExchange(&plcs.thread, thread, old_thread);
					if (old_thread == tmp) goto end;
					old_thread = tmp;
				}
			}
			else {//occupied
				if (reserved) {
					if (have_priority) {//occupied, we have priority
						if ( !system_is_multiprocessor ) Sleep(0);
						old_thread = plcs.thread;
					}
					else {//occupied and reserved by other threads
						if ( passive || !system_is_multiprocessor || left <= 0 ) Sleep(0);
						else MICROWAIT();
						old_thread = plcs.thread;
					}
				}
				else {//occupied but unreserved
					if (!passive) {
						int tmp = InterlockedCompareExchange(&plcs.thread, old_thread | LCS_RESERVED_FLAG, old_thread);
						if (tmp == old_thread) {
							old_thread |= LCS_RESERVED_FLAG;
							have_priority = true;
						}
						else old_thread = tmp;
					}
				}
			}
		}
#undef MICROWAIT
end:
//		InterlockedDecrement(&plcs.waiters);
		lcs->recursion_count++;
		if (Settings::LightCriticalSections::bEnableProfiling) {
			CS_TIME_TYPE t2 = CS_GET_TIME();
			{CS_EVENT_ENTRY(lcs, t,t2, -99, (&name)[-1]);}
		}
	}
	int __stdcall LCS_standard_interlocked_compare_exchange ( volatile long *dest, int new_value, int old_value ) {
		if (new_value != 0) return ::InterlockedCompareExchange(dest, new_value, old_value);
		UInt32 index = UInt32(old_value - BASE_LCS_INDEX_OFFSET);
		if (index >= MAX_LCSes) return ::InterlockedCompareExchange(dest, new_value, old_value);
		//now we know that parameters to the call match the format expected for LCSes exiting
		PerLCS &plcs = PerLCS_array[index];
		if (plcs.lcs != (void*)dest) ::InterlockedCompareExchange(dest, new_value, old_value);
		//now we know that LCS_enter was called on the same address earlier, and caller thinks the current value still matches
		//I think that's a good enough set of criteria
		//though admittedly if an LCS is created, used, destroyed, and then identical values are compare-exchanged at an identical address...
		//that could be bad

		if (plcs.mode == CS_MODE_DEAD) {
			dest[1] = 1<<20;//return value ignored in LightCS::leave()
			return true;
		}
		enum {MASK1=0xFF, MASK2=0xFFffFF00};
		int thread = PerThread::get_PT()->thread_number;
		old_value = plcs.thread;
		new_value = old_value & MASK2;
		if (thread != (old_value & MASK1)) {
			error("LCS_standard_interlocked_compare_exchange - wrong thread for LCS?  (LCS:%X,caller:%X,thread:%d,other:%d)", dest, (&dest)[-1], thread, plcs.thread );
		}
		while (true) {
			int r = InterlockedCompareExchange(&plcs.thread, new_value, old_value);
			if (r == old_value) break;
			if ((r & MASK1) != (old_value & MASK1)) {
				error("LCS_standard_interlocked_compare_exchange - LCS was in invalid state? %X:%X, %X->%X", dest, thread, old_value,r);
			}
			old_value = r;
			new_value = r & MASK2;
		}
		//dest[1] = 1;//not needed because the next entrance will set it to 1?
		return true;//return value ignored in LightCS::leave()
	}
	void initialize_switchable_LCS_stuff( bool profiling ) {
		InitializeCriticalSectionAndSpinCount(&internal_cs, 500);
		PerLCS_array = new PerLCS[MAX_LCSes];
		for (int i = 0; i < MAX_LCSes; i++) PerLCS_array[i].reset();
		if (profiling) 
			WriteRelJump(UInt32(Hook_EnterLightCS), UInt32(LCS_standard_enter));
		else WriteRelJump(UInt32(Hook_EnterLightCS), UInt32(LCS_standard_enter));
//		typedef int (__stdcall *ice_t)( volatile int *, int , int );
//		ice_t indirect_LCS_exit = &LCS_standard_interlocked_compare_exchange;
		SafeWrite32(UInt32(Hook_InterlockedCompareExchange), UInt32(&LCS_standard_interlocked_compare_exchange));
	}
};

void set_active_LCSf ( LCSf lcsf ) {
	WriteRelJump(UInt32(Hook_EnterLightCS), UInt32(lcsf));
}
#endif


void initialize_CS_and_LCS_hooks () {
	CS_spin_stagger_level = (1 << Settings::CriticalSections::iStaggerLevel) - 1;
#if defined FALLOUT || defined NEW_VEGAS
	LCS_spin_stagger_level = (1 << Settings::LightCriticalSections::iStaggerLevel) - 1;
#endif

	if (Settings::Master::bHookCriticalSections) {
		using namespace Settings::CriticalSections;
#ifdef OBLIVION
		set_active_CSFs2(&CSF_defaultspin2);
#endif
		if (bEnableProfiling) message("Critical Sections profiling enabled");
		switch (iDefaultMode) {
			case CS_MODE_NORMAL:
				message("Critical Sections mode %d (emulate vanilla)", iDefaultMode);
				break;
			case CS_MODE_FAIR:
				message("Critical Sections mode %d (improve fairness)", iDefaultMode);
				break;
			case CS_MODE_STAGGER:
				message("Critical Sections mode %d (staggered prioritization)", iDefaultMode);
				break;
			case CS_MODE_DEAD:
				message("Critical Sections mode %d (suppress): a very bad idea", iDefaultMode);
				break;
			case CS_MODE_PRIORITY:
				message("Critical Sections mode %d (prioritize main thread)", iDefaultMode);
				break;
			case CS_MODE_UNPRIORITY:
				message("Critical Sections mode %d (deprioritize main thread)", iDefaultMode);
				break;
			default:
				message("invalid default mode %d for Critical Sections; using mode %d instead", iDefaultMode, CS_MODE_NORMAL);
				iDefaultMode = CS_MODE_NORMAL;
				break;
		}
		if ( bEnableProfiling ) {
			PerfData3::init();
			set_active_CSFs1(&CSF_profiling);
		}
		else set_active_CSFs1(&CSF_standard);
	}

#if defined NEW_VEGAS || defined FALLOUT
	if (Settings::Master::bHookLightCriticalSections) {
		if (Settings::LightCriticalSections::bFullHooks && Hook_EnterLightCS) {// && Hook_TryEnterLightCS) {
			if (!Settings::LightCriticalSections::bEnableProfiling) {
				message("hooking LCS functions (full hooks, %d, %d)", 
					Settings::LightCriticalSections::iDefaultMode, Settings::LightCriticalSections::iDefaultSpin);
				WriteRelJump(UInt32(Hook_EnterLightCS), UInt32(LCS_full::LCS_standard_Enter));
				if (Hook_LeaveLightCS) WriteRelJump(UInt32(Hook_LeaveLightCS), UInt32(LCS_full::LCS_standard_Leave));
				if (Hook_TryEnterLightCS) WriteRelJump(UInt32(Hook_TryEnterLightCS), UInt32(LCS_full::LCS_standard_TryEnter));
			}
			else {
				message("hooking LCS functions with profiling (full hooks, %d, %d)", 
					Settings::LightCriticalSections::iDefaultMode, Settings::LightCriticalSections::iDefaultSpin);
				PerfData3::init();
				WriteRelJump(UInt32(Hook_EnterLightCS), UInt32(LCS_full::LCS_profiling_Enter));
				if (Hook_LeaveLightCS) WriteRelJump(UInt32(Hook_LeaveLightCS), UInt32(LCS_full::LCS_profiling_Leave));
				if (Hook_TryEnterLightCS) WriteRelJump(UInt32(Hook_TryEnterLightCS), UInt32(LCS_full::LCS_profiling_TryEnter));
			}
		}
		else if (Hook_EnterLightCS) {
			if (!Settings::LightCriticalSections::bEnableProfiling) {
				message("hooking LCS functions (partial hooks, %d, %d)", 
					Settings::LightCriticalSections::iDefaultMode, Settings::LightCriticalSections::iDefaultSpin);
				WriteRelJump(UInt32(Hook_EnterLightCS), UInt32(LCS_partial_hooks::LCS_standard_Enter));
			}
			else {
				message("hooking LCS functions with profiling (partial hooks, %d, %d)", 
					Settings::LightCriticalSections::iDefaultMode, Settings::LightCriticalSections::iDefaultSpin);
				PerfData3::init();
				WriteRelJump(UInt32(Hook_EnterLightCS), UInt32(LCS_partial_hooks::LCS_profile_Enter));
			}
		}
	}
#endif
#if 0
	if (Settings::Master::bHookLightCriticalSections && Hook_EnterLightCS) {
		using namespace Settings::LightCriticalSections;
		if (Settings::LightCriticalSections::bFullHooks) {
			if (bEnableProfiling) message("Light Critical Sections: full hooks enabled (profiling enabled)");
			else message("Light Critical Sections: full hooks enabled");
					switch (iDefaultMode) {
						case CS_MODE_NORMAL:
							message("Light Critical Sections mode %d (emulate vanilla): ", iDefaultMode);
							break;
						case CS_MODE_FAIR:
							message("Light Critical Sections mode %d (improve fairness): ", iDefaultMode);
							break;
						case CS_MODE_STAGGER:
							message("Light Critical Sections mode %d (add staggering): ", iDefaultMode);
							break;
						case CS_MODE_DEAD:
							message("Light Critical Sections mode %d (suppress): a very bad idea", iDefaultMode);
							break;
						case CS_MODE_PRIORITY:
							message("Light Critical Sections mode %d (priority): ", iDefaultMode);
							break;
						case CS_MODE_UNPRIORITY:
							message("Light Critical Sections mode %d (unpriority): ", iDefaultMode);
							break;
						default:
							message("invalid default mode %d for Light Critical Sections; using mode %d instead", iDefaultMode, CS_MODE_NORMAL);
							iDefaultMode = CS_MODE_NORMAL;
							break;
					}

			switchable_LCS_stuff::initialize_switchable_LCS_stuff(bEnableProfiling);
		}
		else {
			if (bEnableProfiling) message("Light Critical Sections hook: using partial hooks (profiling enabled)");
			else message("Light Critical Sections hook: using partial hooks");
			switch (iDefaultMode) {
//				case 0:
//					message("Light Critical Sections mode %d (vanilla)", iDefaultMode);
//					if (bEnableProfiling) WriteRelJump(UInt32(Hook_EnterLightCS), UInt32(LCS_basic_profile));
//					//else WriteRelJump(UInt32(Hook_EnterLightCS), UInt32(LCS_basic));
//					break;
				case CS_MODE_NORMAL:
					message("Light Critical Sections mode %d (add spin)", iDefaultMode);
					if (bEnableProfiling) WriteRelJump(UInt32(Hook_EnterLightCS), UInt32(LCS_spin_profile));
					else WriteRelJump(UInt32(Hook_EnterLightCS), UInt32(LCS_spin));
					break;
				case CS_MODE_FAIR:
					message("Light Critical Sections mode %d (improve fairness): only partially supported without full hooks", iDefaultMode);
					if (bEnableProfiling) WriteRelJump(UInt32(Hook_EnterLightCS), UInt32(LCS_fair_profile));
					else WriteRelJump(UInt32(Hook_EnterLightCS), UInt32(LCS_fair));
					break;
				case CS_MODE_STAGGER:
					message("Light Critical Sections mode %d (add staggering): only partially supported without full hooks", iDefaultMode);
					if (bEnableProfiling) WriteRelJump(UInt32(Hook_EnterLightCS), UInt32(LCS_stagger_profile));
					else WriteRelJump(UInt32(Hook_EnterLightCS), UInt32(LCS_stagger));
					break;
				case CS_MODE_DEAD:
					message("Light Critical Sections mode %d (suppress): requires full hooks (and a bad idea anyway); ignoring", iDefaultMode);
					break;
				case CS_MODE_PRIORITY:
					message("Light Critical Sections mode %d (priority): only partially supported without full hooks", iDefaultMode);
					if (bEnableProfiling) WriteRelJump(UInt32(Hook_EnterLightCS), UInt32(LCS_priority_profile));
					else WriteRelJump(UInt32(Hook_EnterLightCS), UInt32(LCS_priority));
					break;
				case CS_MODE_UNPRIORITY:
					message("Light Critical Sections mode %d (unpriority): only partially supported without full hooks", iDefaultMode);
					if (bEnableProfiling) WriteRelJump(UInt32(Hook_EnterLightCS), UInt32(LCS_unpriority_profile));
					else WriteRelJump(UInt32(Hook_EnterLightCS), UInt32(LCS_unpriority));
					break;
				default:
					message("invalid default mode %d for Light Critical Sections; not hooking LCS code", iDefaultMode);
					break;
			}
		}
	}
#endif

	if (Settings::Master::bHookCriticalSections && Settings::CriticalSections::bUseOverrides) {
		using namespace Settings::CriticalSections;
		TextSection *ts = config->get_section("OverrideList");
		if (ts) for (ts = ts->get_first_section(); ts; ts = ts->get_next_section()) {
			if (ts->get_name() == "CriticalSection") {
				TextSection *ts_object_addr = ts->get_section("ObjectAddress");
				TextSection *ts_caller_addr = ts->get_section("CallerAddress");
				TextSection *ts_mode = ts->get_section("Mode");
				TextSection *ts_spin = ts->get_section("Spin");
#if defined NEW_VEGAS
				TextSection *ts_version = ts->get_section("Version");
				const char *version = ts_version ? ts_version->get_c_string() : "";
				if (strcmp(version, Hook_target_s)) continue;
#endif
				if (!ts_mode && !ts_spin) continue;
//				std::map<CRITICAL_SECTION*,CS_override_data> CS_override_by_object;
//				std::map<const void*,CS_override_data> CS_override_by_caller;
				CRITICAL_SECTION *object = (CRITICAL_SECTION *) (ts_object_addr ? ts_object_addr->get_int() : NULL);
				void *caller = (void *) (ts_caller_addr ? ts_caller_addr->get_int() : NULL);
				int mode = ts_mode ? ts_mode->get_int() : -1;
				int spin = ts_spin ? ts_spin->get_int() : -1;
				if (!caller && !object) {
//					if (bEnableMessages) 
//						message("Critical Section override entry without usable data");
				}
				if (object) {
					//if (bEnableMessages) message("Critical Section override: address:%X, mode:%d, spin:%d", object, mode, spin);
					CS_override_by_object.insert(std::pair<CRITICAL_SECTION*,CS_override_data>(object,
						CS_override_data(object, NULL, mode, spin)
					));
				}
				if (caller) {
					//if (bEnableMessages) message("Critical Section override: caller:%X, mode:%d, spin:%d", caller, mode, spin);
					CS_override_by_caller.insert(std::pair<void*,CS_override_data>(caller,
						CS_override_data(NULL, caller, mode, spin)
					));
				}
			}
		}
	}


#if defined FALLOUT || defined NEW_VEGAS
	if (Settings::Master::bHookLightCriticalSections && Settings::LightCriticalSections::bFullHooks && Settings::LightCriticalSections::bUseOverrides) {
		using namespace Settings::LightCriticalSections;
		TextSection *ts = config->get_section("OverrideList");
		for (ts = ts->get_first_section(); ts; ts = ts->get_next_section()) {
			if (ts->get_name() == "LightCriticalSection") {
				TextSection *ts_object_addr = ts->get_section("ObjectAddress");
				TextSection *ts_caller_addr = ts->get_section("CallerAddress");
				TextSection *ts_mode = ts->get_section("Mode");
				TextSection *ts_spin = ts->get_section("Spin");
				if (ts_spin || ts_mode) {
//					std::map<CRITICAL_SECTION*,CS_override_data> CS_override_by_object;
//					std::map<const void*,CS_override_data> CS_override_by_caller;
					LightCS *object = (LightCS *) (ts_object_addr ? ts_object_addr->get_int() : NULL);
					void *caller = (void *) (ts_caller_addr ? ts_caller_addr->get_int() : NULL);
					int mode = ts_mode ? ts_mode->get_int() : -1;
					int spin = ts_spin ? ts_spin->get_int() : -1;
					if (!caller && !object) {
//						if (bEnableMessages) 
//							message("Critical Section override entry without usable data");
					}
					if (object) {
						//if (bEnableMessages) message("Light Critical Section override: address:%X, mode:%d, spin:%d", object, mode, spin);
						LCS_override_by_object.insert(std::pair<LightCS*,LCS_override_data>(object,
							LCS_override_data(object, NULL, mode, spin)
						));
					}
					if (caller) {
						//if (bEnableMessages) message("Light Critical Section override: caller:%X, mode:%d, spin:%d", caller, mode, spin);
						LCS_override_by_caller.insert(std::pair<void*,LCS_override_data>(caller,
							LCS_override_data(NULL, caller, mode, spin)
						));
					}
				}
			}
		}
	}
#endif
}
