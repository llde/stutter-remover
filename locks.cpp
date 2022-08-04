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


void initialize_CS_and_LCS_hooks () {
	CS_spin_stagger_level = (1 << Settings::CriticalSections::iStaggerLevel) - 1;
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

	if (Settings::Master::bHookCriticalSections && Settings::CriticalSections::bUseOverrides) {
		using namespace Settings::CriticalSections;
		TextSection *ts = config->get_section("OverrideList");
		if (ts) for (ts = ts->get_first_section(); ts; ts = ts->get_next_section()) {
			if (ts->get_name() == "CriticalSection") {
				TextSection *ts_object_addr = ts->get_section("ObjectAddress");
				TextSection *ts_caller_addr = ts->get_section("CallerAddress");
				TextSection *ts_mode = ts->get_section("Mode");
				TextSection *ts_spin = ts->get_section("Spin");
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

}
