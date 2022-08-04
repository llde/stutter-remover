
#include "main.h"
#include "game_time.h"
#include <mmsystem.h>
#include "settings.h"
#include "locks.h"

static UInt32 current_time;
static UInt32 bias_tgt;
static UInt32 bias_gtc;
static CRITICAL_SECTION timelock;

//static UInt64 current_time_qpc;
//static double current_qpc_period;
//static UInt32 current_time_gtc;
//static UInt32 current_time_tgt;
static bool reset = true;
void update_GetTickCount() {//called at the start of each frame
	reset = true;
}

UInt32 GetTickCount_replacement_unsynced() {
	using namespace Settings::GetTickCount;
	if (bForceResolution) {
		UInt32 t = timeGetTime() - bias_tgt;
		if (!bPreserveHighFreqComponents) return t;
		UInt32 t2 = GetTickCount() - bias_gtc;
		int diff = t - t2;
		if (diff >= 0 && diff < 15) return t;
		if (diff == 16) return t2 + 15;
		EnterCriticalSection(&timelock);
		message("GetTickCount - bPreserveHighFreqComponents - diff %d", diff);
		if (diff < 0) {bias_tgt += diff; t -= diff;}
		else {bias_tgt += diff - 16; t -= diff - 16;}
		LeaveCriticalSection(&timelock);
		return t;
	}
	else return GetTickCount() - bias_gtc;
}
UInt32 GetTickCount_replacement_synced() {//bForceSync is true
	using namespace Settings::GetTickCount;
	UInt32 t = GetTickCount_replacement_unsynced();

	if (reset) {
		PerThread *pt = PerThread::get_PT();
		if (pt->thread_number == 1) {
			current_time = t;
			reset = false;
			return t;
		}
	}
	SInt32 delta = t-current_time;
	if (delta < 0) {
		message("GetTickCount_replacement_synced - negative delta %d", delta);
		return current_time;
	}
	if (delta < iSyncLimitMilliseconds) return current_time;
	else return t - iSyncLimitMilliseconds;//or should it be just "return t;" ?
}

void hook_GetTickCount() {
	using namespace Settings::GetTickCount;
	message("hook_GetTickCount bPreserveDC_Bias=%d Sync=%dms bForceResolution=%d bPreserveHighFreqComponents=%d bPreserveHighFreqComponents=%d", 
		bPreserveDC_Bias, bForceSync ? iSyncLimitMilliseconds : 0, bForceResolution, bPreserveHighFreqComponents, bPreserveHighFreqComponents);
	if (bPreserveDC_Bias) {
		bias_gtc = 0;
		UInt32 t1 = GetTickCount(), t2;
		while (t1 == (t2 = GetTickCount())) ;
		bias_tgt = timeGetTime() - t2;
	}
	else {
		bias_gtc = PerThread::get_PT()->internal_rng_hq.raw32();
		bias_tgt = timeGetTime() - (GetTickCount() - bias_gtc);
	}
	SafeWrite32( UInt32(Hook_GetTickCount_Indirect), 
		UInt32(bForceSync ? &GetTickCount_replacement_synced : &GetTickCount_replacement_unsynced)
	);
}












/*
UInt32 special_get_millisecond_time(double &fraction) {
	//stupid method of combining tgt & rdtsc, for use by FPS management code only
	static UInt64 last_rdtsc=0;
	static UInt32 last_tgt=0;
	static UInt32 last_ratio_update=0;
	static bool initialized = false;
	static double last_fraction;
	static double ratio = 0;
	UInt32 tgt = timeGetTime();
	UInt64 rdtsc = get_RDTSC_time();
	double delta_rdtsc = (rdtsc - last_rdtsc) * ratio;
	UInt32 delta_tgt = tgt - last_tgt;
	if (delta_tgt > 100000) error("special time - went berzerk");
	last_rdtsc = rdtsc;
	last_tgt = tgt;
	if (!initialized) {
		initialized = true;
		ratio = get_RDTSC_period() * 1000.;
		fraction = last_fraction = 0;
		last_ratio_update = tgt - 999;
		return tgt;
	}
	if (tgt - last_ratio_update >= 1000) {
		ratio = get_RDTSC_period();
		last_ratio_update = tgt;
	}
	double f = last_fraction + delta_rdtsc;
	UInt32 i = UInt32(f);
	f -= i;
	if (i < delta_tgt) f = 0;
	else if (i > delta_tgt) f = 0.99;
	else if (f > 0.99) f = 0.99;
	if (!delta_tgt && f > last_fraction)
		error("special time - this shouldn't be able to happen");
	fraction = f;
	last_fraction = f;
	return tgt;
}*/

/*
DWORD WINAPI FakeGetTickCount1() {
	manage_main_hook_basics();
	if (Settings::Master::bManageFPS) {
		manage_framerate();
		//if (Settings::FPS_Management::bOverrideGameLogicTiming && Hook_FixedTimePerFrame_iFPSClamp)
		//	*Hook_FixedTimePerFrame_iFPSClamp = float(delta);
	}
	return (*Hook_GetTickCount_Indirect)();
}
DWORD WINAPI FakeGetTickCount3() {
	static DWORD last_returned_time = GetTickCount();
	DWORD time2 = (*Hook_GetTickCount_Indirect)();
	static DWORD base_offset = time2 - timeGetTime();
	manage_main_hook_basics();
	last_returned_time = timeGetTime() + base_offset;
	return last_returned_time;
}

	if (Settings::Master::bFix64Hertz) {
		message("replacing GetTickCount");
		SafeWrite32(UInt32(Hook_GetTickCount_Indirect), UInt32(Improved_GetTickCount));
		if (Settings::Master::bExperimentalStuff && Settings::Experimental::bAlternate64HertzFix) {
			_oblivion_time = timeGetTime();
			SafeWrite32(UInt32(Hook_GetTickCount_Indirect), UInt32(Alternate_GetTickCount));
			message("using experimental GetTickCount");
		}
	}

*/


/*
	double raw_delta = delta;
	enum {HARD_MAX_FRAME_TIME = 160};
	if ( delta > HARD_MAX_FRAME_TIME ) delta = HARD_MAX_FRAME_TIME;
	if ( delta < 0 ) error("manage_framerate - time went backwards");//delta = 0;//should be impossible
	double base_delta = delta;

	if (!console_ready) {
		last_actual_time = actual_time;
		return delta;
	}

//	bool game_mode = isGameMode();
//	if (!game_mode) {
//		last_actual_time = actual_time;
//		return delta;
//	}

	int sleep_time = 0;
	int burn_time = 0;

	if (frametime_min > 0 && delta < frametime_min && game_mode) {
		//maybe remove game_mode requirement?
		int timeleft = frametime_min - delta;
		delta = frametime_min;
		burn_time = timeleft;
		if (burn_time > iSchedulingParanoia && iSleepExtra >= 0) {
			sleep_time += burn_time - iSchedulingParanoia;
			sleep_accumulation = 0;
			burn_time = iSchedulingParanoia;
		}
	}

	if (iSleepExtra > 0) {
		sleep_accumulation += base_delta * iSleepExtra;
		if ( sleep_accumulation > 1023 ) {
			int ms = sleep_accumulation >> 10;
			sleep_accumulation &= 1023;
			if (ms > 10) ms = 10;
			sleep_time += ms;
		}
	}

	if (base_delta == 0 && !burn_time && !sleep_time) burn_time = 1;

	if (sleep_time > 0) {
		Sleep(sleep_time);
	}
	if (burn_time || sleep_time) {
		DWORD now;
		while ((now = timeGetTime()) - actual_time < burn_time + sleep_time)
			;
		actual_time = now;
		delta = actual_time - last_actual_time;
	}
	if (delta > HARD_MAX_FRAME_TIME) delta = HARD_MAX_FRAME_TIME;
	if (delta < 1) delta = 1;//should be impossible

	if ( frametime_max > 0 && delta > frametime_max && game_mode )
		delta = frametime_max;

	if (true) {
		if (frametime_index < 0) {
			for (int i = 0; i < FRAMETIME_ARRAY_LENGTH; i++) frametime_data[i].zero();
			frametime_index = 0;
		}
		FrameTimeElement &ft = frametime_data[frametime_index];
		int actual_delta = unclipped_delta+sleep_time+burn_time;
		ft.count += 1;
		ft.base_time += unclipped_delta;
		ft.actual_time += actual_delta;
	//	ft.subjective_time += delta;
		if (base_delta > ft.worst_time) ft.worst_time = unclipped_delta;
		if (base_delta < ft.best_time) ft.best_time = unclipped_delta;
		frametime_message_accumulation += actual_delta;
		if (iFPS_Frequency && frametime_message_accumulation > iFPS_Frequency) {
			frametime_message_accumulation = 0;
			int worst = 0;
			int best = 99000;
			int count = 0;
			int base_time = 0;
		//	int bound_time = 0;
		//	double subjective_time = 0;
			for (int i = 0; i < FRAMETIME_ARRAY_LENGTH; i++) {
				if (frametime_data[i].worst_time > worst) worst = frametime_data[i].worst_time;
				if (frametime_data[i].best_time < best) best = frametime_data[i].best_time;
				count += frametime_data[i].count;
				base_time += frametime_data[i].base_time;
		//		bound_time += frametime_data[i].bound_time;
		//		subjective_time += frametime_data[i].subjective_time;
			}
			if (!count) count = 1;
			message("FPS data, time:%.0fs, avg:%3d, extremes:%2d-%3d, current:%4d", 
				(actual_time - ::base_time) * 0.001, 
				base_time/count, best, worst, unclipped_delta
			);
		}
		if (ft.actual_time >= FRAMETIME_DURATION) {
			frametime_index++;
			if (frametime_index >= FRAMETIME_ARRAY_LENGTH) frametime_index = 0;
			frametime_data[frametime_index].zero();
		}
	}

	last_actual_time = actual_time;
	return delta;
}*/

//DWORD WINAPI FakeGetTickCount0();
//DWORD WINAPI FakeGetTickCount1();
//DWORD WINAPI FakeGetTickCount2();
//DWORD (WINAPI **GetTickCount_Indirect_Vanilla)() = NULL;
//DWORD (WINAPI *FakeGetTickCount_Indirect)() = NULL;

//DWORD WINAPI FakeGetTickCount0() {
//	manage_main_hook_basics();
//	DWORD time = (*Hook_GetTickCount_Indirect)();
//	return time;
//}
//static DWORD GetTickCount_offset = 0;
//DWORD WINAPI Improved_GetTickCount() {
//	return timeGetTime() + GetTickCount_offset;
//}
//DWORD WINAPI Alternate_GetTickCount() {
//	int actual_time = timeGetTime();
//	if (actual_time - _oblivion_time > 20) _oblivion_time = actual_time;
//	return _oblivion_time + GetTickCount_offset;
//}
