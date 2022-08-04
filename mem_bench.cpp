
#include "main.h"
#include <mmsystem.h>
#include "memory.h"
#include "locks.h"
#include "mem_bench.h"

enum {MEMBENCH_SIZE_MASK = 255};

namespace MemoryBenchmarkStuff {
	struct Set {enum {SET_SIZE=64};
		void *pointers[SET_SIZE];
		size_t size;
	};
	inline void use_node( void *mem, size_t size = 1 ) {
		(*(char*)mem)++;
	}
	inline void use_set( Set *set ) {
		for (int i = 0; i < Set::SET_SIZE; i++) (*(char*)set->pointers[i])++;
	}
	struct FIFO {enum {FIFO_SIZE=16};
		Set *data[FIFO_SIZE];
		volatile long index_in, index_out;
		int padding[32-2];
		FIFO() {index_in = 0; index_out = -1 & (FIFO_SIZE-1);}
		bool push(Set*ptr) {
			int i = index_in;
			int next = (i+1) & (FIFO_SIZE-1);
			if (i == index_out) return false;
			data[i] = ptr;
			MemoryBarrier();
			index_in = next;
			return true;
		}
		Set *pop() {
			int i = index_out;
			int next = (i+1) & (FIFO_SIZE-1);
			if (next == index_in) return NULL;
			Set *rv = data[next];
			MemoryBarrier();
			index_out = next;
			return rv;
		}
	};
	struct GenericBenchmarkThreadParam {
		UInt64 seed;
		SR_HEAP *heap;
		UInt32 end_time;
		long other;
		volatile long results;
		enum {RESULT_SHIFT=4};
	};
	struct DirectionalBenchmarkThreadParam : public GenericBenchmarkThreadParam {
		FIFO *input;
		FIFO *output;
	};
	static void dump_fifo( FIFO *fifo, SR_HEAP *heap ) {
		Set *set;
		while (set=fifo->pop()) {
			for (int i = 0; i < Set::SET_SIZE; i++) {
				heap->free(set->pointers[i]);
			}
			heap->free(set);
		}
	}
	DWORD WINAPI benchmark_heap_thread_simple( void *param ) {
	//no inter-thread traffic, very low complexity
		GenericBenchmarkThreadParam *tp = (GenericBenchmarkThreadParam*)param;
		SR_HEAP heap = *tp->heap;
		int sum = 0;
		enum {SET_SIZE=1<<10};
		RNG_fast32_16 rng;
		rng.seed_64( tp->seed );
		UInt32 units = 0;
		do {
			int size = 8 + (rng.raw16() & MEMBENCH_SIZE_MASK);
			for (int j = 0; j < SET_SIZE; j++) {
				void *mem = heap.malloc(size);
				use_node(mem, size);
				heap.free(mem);
			}
			units++;
		}
		while (SInt32(tp->end_time - timeGetTime()) > 0);
		tp->results = (UInt64(units) * SET_SIZE) >> tp->RESULT_SHIFT;
		return 0;
	}
	DWORD WINAPI benchmark_heap_thread_isolated( void *param ) {
	//no inter-thread traffic, but mallocs/frees are scrambled
		GenericBenchmarkThreadParam *tp = (GenericBenchmarkThreadParam*)param;
		SR_HEAP heap = *tp->heap;
		RNG_jsf32 rng;
		rng.seed_64( tp->seed );
		enum {BUFF_SIZE=1<<12};
		int units = 0;
		void *buff[BUFF_SIZE];
		for (int i = 0; i < BUFF_SIZE; i++) buff[i] = NULL;
		do {
			for (int i = 0; i < BUFF_SIZE/32; i++) {
				int size = 8 + (rng.raw32() & MEMBENCH_SIZE_MASK);
				int bits = rng.raw32();
				int index = 0;
				for (int b = 0; b < 32; b++) {
					bits >>= 1;
					//buff[index] = heap.realloc(buff[index], size);
					if (buff[index]) {
						use_node(buff[index]);
						heap.free(buff[index]);
					}
					buff[index] = heap.malloc(size);
					use_node(buff[index]);
					index = (index + 1 + (bits & 1)) & (BUFF_SIZE-1);
				}
			}
			units++;
		}
		while (SInt32(tp->end_time - timeGetTime()) > 0);
		for (int i = 0; i < BUFF_SIZE; i++) heap.free(buff[i]);
		tp->results = (UInt64(units) * BUFF_SIZE) >> tp->RESULT_SHIFT;
		return 0;
	}
	DWORD WINAPI benchmark_heap_thread_directional( void *param ) {
	//inter-thread traffic
		DirectionalBenchmarkThreadParam *tp = (DirectionalBenchmarkThreadParam*)param;
		SR_HEAP heap = *tp->heap;
		RNG_fast32_16 rng;
		rng.seed_64( tp->seed );
		int units = 0;
		Set *set = NULL;
		Set *in;
		do {
			if (!set) {
				set = new (heap.malloc(sizeof(Set))) Set;
				int size = 8 + (rng.raw16() & MEMBENCH_SIZE_MASK);
				set->size = size;
				for (int i = 0; i < Set::SET_SIZE; i++) {
					set->pointers[i] = heap.malloc(size);
				}
				use_set(set);
				units++;
			}
			if (tp->output && tp->output->push(set)) {
				set = NULL;
			}
			if (tp->input && (in=tp->input->pop())) {
				use_set(in);
				for (int i = 0; i < Set::SET_SIZE; i++) {
					heap.free(in->pointers[i]);
				}
				heap.free(in);
			}
		}
		while (SInt32(tp->end_time - timeGetTime()) > 0);
		if (set) {
			for (int i = 0; i < Set::SET_SIZE; i++) {
				heap.free(set->pointers[i]);
			}
			heap.free(set);
		}
		tp->results = (UInt64(units) * (Set::SET_SIZE+1)) >> tp->RESULT_SHIFT;
		return 0;
	}
	DWORD WINAPI benchmark_heap_thread_chained( void *param ) {
	//focus on thread startup/shutdown delays
		GenericBenchmarkThreadParam *tp = (GenericBenchmarkThreadParam*)param;
		SR_HEAP heap = *tp->heap;
		RNG_jsf32 rng;
		rng.seed_64( tp->seed );
		enum {BUFF_SIZE=1<<10};
		int units = 0;
		void *buff[BUFF_SIZE];
		for (int i = 0; i < BUFF_SIZE; i++) buff[i] = NULL;
		do {
			for (int i = 0; i < BUFF_SIZE/16; i++) {
				int size = 8 + (rng.raw32() & MEMBENCH_SIZE_MASK);
				int bits = rng.raw32();
				int index = 0;
				for (int b = 0; b < 16; b++) {
					index = (index + 1 + (bits & 1)) & (BUFF_SIZE-1);
					bits >>= 1;
					//buff[index] = heap.realloc(buff[index], size);
					if (buff[index]) {
						use_node(buff[index]);
						heap.free(buff[index]);
					}
					buff[index] = heap.malloc(size);
					use_node(buff[index]);
				}
			}
			tp->other++;
			//::InterlockedIncrement(&tp->other);
		}
		while (rng.raw32() & 1);
		for (int i = 0; i < BUFF_SIZE; i++) heap.free(buff[i]);
		if (SInt32(tp->end_time - timeGetTime()) > 0) {
			if (!CreateThread( NULL, 1<<15, &benchmark_heap_thread_chained, param, 0, NULL))
				error("benchmark_heap_thread_chained - failed to start next thread");
		}
		else {
//			message("tp->other at end is %d", int(tp->other));
			tp->results = (UInt64(tp->other) * BUFF_SIZE) >> tp->RESULT_SHIFT;
		}
		return 0;
	}

	class benchmark_heap_mixed {
	public:
		struct Set {enum {SET_SIZE=256};
			void *pointers[SET_SIZE];
		};
		struct FIFO {enum {FIFO_SIZE=16};
			Set *data[FIFO_SIZE];
			volatile long index_in, index_out;
			int padding[256-2];
			FIFO() {index_in = 0; index_out = -1 & (FIFO_SIZE-1);}
			bool push(Set*ptr) {
				int i = index_in;
				int next = (i+1) & (FIFO_SIZE-1);
				if (i == index_out) return false;
				data[i] = ptr;
				MemoryBarrier();
				index_in = next;
				return true;
			}
			Set *pop() {
				int i = index_out;
				int next = (i+1) & (FIFO_SIZE-1);
				if (next == index_in) return NULL;
				Set *rv = data[next];
				MemoryBarrier();
				index_out = next;
				return rv;
			}
		};
		struct FIFO_safe {enum {FIFO_SIZE=16};
			CRITICAL_SECTION cs;
			Set *data[FIFO_SIZE];
			volatile long index_push, index_pop;
			int padding[256-2];
			FIFO_safe() {
				index_push = 0; index_pop = -1 & (FIFO_SIZE-1);
				InitializeCriticalSection(&cs);
			}
			bool push(Set*ptr) {
				EnterCriticalSection(&cs);
				int i = index_push;
				int next = (i+1) & (FIFO_SIZE-1);
				if (i == index_pop) {
					LeaveCriticalSection(&cs);
					return false;
				}
				data[i] = ptr;
				MemoryBarrier();
				index_push = next;
				LeaveCriticalSection(&cs);
				return true;
			}
			Set *pop() {
				EnterCriticalSection(&cs);
				int i = index_pop;
				int next = (i+1) & (FIFO_SIZE-1);
				if (next == index_push) {
					LeaveCriticalSection(&cs);
					return NULL;
				}
				Set *rv = data[next];
				MemoryBarrier();
				index_pop = next;
				LeaveCriticalSection(&cs);
				return rv;
			}
		};
		enum {MAX_THREADS=16};
		FIFO thread_pairs[MAX_THREADS * MAX_THREADS];
		UInt64 seeds[MAX_THREADS];
		const int num_threads;
		const SR_HEAP heap;
		const int work_units_per_thread;
		int _padding[16];
		long int threads_finished;
		benchmark_heap_mixed(int threads, SR_HEAP *heap_, UInt64  mallocs_per_thread)
		:
			num_threads(threads), 
			heap(*heap_), 
			work_units_per_thread(mallocs_per_thread / Set::SET_SIZE)
		{
			for (int i = 0; i < num_threads; i++) seeds[i] = global_rng.raw64();
		}
		bool purge_input(int ti, int oti) {
			bool rv = false;
			for (int i = 0; i < num_threads; i++) {
				if (++oti >= num_threads) oti = 0;
				Set *set;
				while (set = thread_pairs[ti*MAX_THREADS + oti].pop()) {
					if (ti != oti) rv = true;
					for (int j = 0; j < Set::SET_SIZE; j++) heap.free(set->pointers[j]);
				}
			}
			return rv;
		}
		void do_per_thread_work(int ti) {
			int work_units = work_units_per_thread;
			RNG_jsf32 rng_hq; rng_hq.seed_64(seeds[ti]);
			RNG_fast32_16 rng_fast; rng_fast.seed_64(rng_hq.raw64());
			Set *set = NULL;
			while (work_units) {
				switch (0) {//rng_fast.raw16() & 0) {
					case 0:
					{//send a set of mallocs to a random thread; if none available then purge all input channels
						if (!set) {
							set = new Set;
							for (int i = 0; i < Set::SET_SIZE; i++) {
								set->pointers[i] = heap.malloc(8+(rng_fast.raw16() & MEMBENCH_SIZE_MASK));
							}
							work_units--;
						}
						int oti = (rng_fast.raw16() * num_threads) >> 16;
						for (int i = 0; i < num_threads; i++) {
							if (++oti >= num_threads) oti = 0;
							if (thread_pairs[MAX_THREADS*oti + ti].push(set)) {
								set = NULL;
								break;
							}
						}
						if (set) {
							if (!purge_input(ti, (rng_fast.raw16() * num_threads) >> 16)) {
								//there was no room to send output to any other thread
								//AND there was no input from other threads
								//so sleep a little to give them time
								Sleep(0);
							}
						}
					}
					break;
				}
			}
			purge_input(ti, (rng_fast.raw16() * num_threads) >> 16);
			if (set) {
				for (int i = 0; i < Set::SET_SIZE; i++) heap.free(set->pointers[i]);
				delete set;
			}
			InterlockedIncrement(&threads_finished);
		};
		struct ThreadParam {
			benchmark_heap_mixed *object;
			int thread_index;
		};
		static DWORD __stdcall _do_per_thread_work(void *param) {
			ThreadParam *tp = (ThreadParam*)param;
			tp->object->do_per_thread_work(tp->thread_index);
			return 0;
		}
		UInt64 do_benchmark() {
			if (num_threads < 2) return 0;
			threads_finished = 0;
			ThreadParam tp[MAX_THREADS];
			for (int i = 0; i < num_threads; i++) {
				tp[i].object = this;
				tp[i].thread_index = i;
				seeds[i] = global_rng.raw64();
			}
			MemoryBarrier();
			UInt64 start_time = get_time_ticks();
			for (int i = 0; i < num_threads; i++) {
				::HANDLE h = ::CreateThread( NULL, 65536, &_do_per_thread_work, &tp[i], 0, NULL);
				if (!h) error("failed to create thread");
			}
			int ms_slept = 0;
			int next_message = 1024 * 10;
			int sleep_unit = 2;
			while (threads_finished < num_threads) {
				ms_slept += sleep_unit;
				::Sleep(sleep_unit);
				if (ms_slept > 256 * sleep_unit) sleep_unit*=2;
				if (ms_slept >= next_message) {
					next_message <<= 1;
					message("benchmark_heap_impl::do_benchmark(threads=%d/%d) : %d ms slept so far", threads_finished, num_threads, ms_slept);
				}
				::MemoryBarrier();
			}
			//threads stop processing input as soon as they generate all of their output
			//so output from the later threads to finish will never have been processed:
			for (int ti1 = 0; ti1 < num_threads; ti1++) {
				for (int ti2 = 0; ti2 < num_threads; ti2++) {
					Set *tmp;
					while (tmp = thread_pairs[ti1 * MAX_THREADS + ti2].pop()) {
						for (int i = 0; i < Set::SET_SIZE; i++) heap.free(tmp->pointers[i]);
					}
				}
			}
			return get_time_ticks() - start_time;
		}
	};
}

/*double benchmark_unidirectional(SR_HEAP *heap, int milliseconds) {
	using namespace MemoryBenchmarkStuff;
	DirectionalBenchmarkThreadParam tp1, tp2;
	FIFO fifo;
	tp1.heap = tp2.heap = heap;
	tp1.end_time = tp2.end_time = timeGetTime() + milliseconds;
	tp1.results = tp2.results = 0;
	tp1.input = tp2.output = &fifo;
	tp1.output = tp2.input = NULL;
	if (!CreateThread( NULL, 65536, &benchmark_heap_thread_directional, &tp1, 0, NULL))
		error("failed to create thread");
	if (!CreateThread( NULL, 65536, &benchmark_heap_thread_directional, &tp2, 0, NULL))
		error("failed to create thread");
	Sleep(milliseconds);
	while (true) {
		long r1 = tp1.results;
		long r2 = tp2.results;
		if (!r1 || !r2) {
			Sleep(0);
			continue;
		}
		dump_fifo(&fifo, heap);
		return (r1+r2) << DirectionalBenchmarkThreadParam::RESULT_SHIFT;
	}
}*/
double benchmark_heap(SR_HEAP *heap, int mode, int threads, int milliseconds) {
//	return benchmark_unidirectional_only(heap, milliseconds);
	using namespace MemoryBenchmarkStuff;
	if (mode == MEMORY_BENCHMARK_SIMPLE || mode == MEMORY_BENCHMARK_ISOLATED || mode == MEMORY_BENCHMARK_UNIDIRECTIONAL || mode == MEMORY_BENCHMARK_BIDIRECTIONAL || mode == MEMORY_BENCHMARK_CHAINED) {
		std::vector<DirectionalBenchmarkThreadParam> tp; tp.resize(threads);
		std::vector<HANDLE> handles; handles.resize(threads);
		UInt32 et = timeGetTime() + milliseconds;
		for (int i = 0; i < threads; i++) {
			tp[i].heap = heap;
			tp[i].seed = PerThread::get_PT()->internal_rng_hq.raw64();
			tp[i].end_time = et;
			tp[i].results = 0;
			tp[i].other = 0;
		}
		std::vector<FIFO> fifos;
		if (mode == MEMORY_BENCHMARK_UNIDIRECTIONAL) {
			if (threads & 1) error("MEMORY_BENCHMARK_UNIDIRECTIONAL with odd thread count");
			fifos.resize(threads>>1);
			for (int i = 0; i < threads; i++) {
				tp[i].input = (i & 1) ? &fifos[i>>1] : NULL;
				tp[i].output = (i & 1) ? NULL : &fifos[i>>1];
			}
		}
		else if (mode == MEMORY_BENCHMARK_BIDIRECTIONAL) {
			if (threads & 1) error("MEMORY_BENCHMARK_BIDIRECTIONAL with odd thread count");
			fifos.resize(threads);
			for (int i = 0; i < threads; i++) {
				tp[i].input = &fifos[i];
				tp[i].output = &fifos[i ^ 1];
			}
		}
		for (int i = 0; i < threads; i++) {
			if (mode == MEMORY_BENCHMARK_SIMPLE) 
				handles[i] = CreateThread( NULL, 1<<14, &benchmark_heap_thread_simple, &tp[i], 0, NULL);
			else if (mode == MEMORY_BENCHMARK_ISOLATED) 
				handles[i] = CreateThread( NULL, 1<<16, &benchmark_heap_thread_isolated, &tp[i], 0, NULL);
			else if (mode == MEMORY_BENCHMARK_UNIDIRECTIONAL || mode == MEMORY_BENCHMARK_BIDIRECTIONAL) 
				handles[i] = CreateThread( NULL, 1<<15, &benchmark_heap_thread_directional, &tp[i], 0, NULL);
			else if (mode == MEMORY_BENCHMARK_CHAINED) {
				handles[i] = CreateThread( NULL, 1<<15, &benchmark_heap_thread_chained, &tp[i], 0, NULL);
			}
			if (!handles[i]) error("failed to create thread");
		}
		Sleep(milliseconds);
		while (true) {
			int r_threads = 0;
			UInt64 r_sum = 0;
			for (int i = 0; i < threads; i++) {
				r_threads += (tp[i].results != 0) ? 1 : 0;
				r_sum += (tp[i].results > 0) ? tp[i].results : 0;
			}
			if (r_threads == threads) {
				if (fifos.size()) {
					for (int i = 0; i < fifos.size(); i++) dump_fifo(&fifos[i], heap);
				}
				return r_sum << DirectionalBenchmarkThreadParam::RESULT_SHIFT;
			}
			Sleep(0);
		}
	}
/*	else if (complexity == 2) {
		benchmark_heap_impl b(threads, heap, mallocs_per_thread);
		return b.do_benchmark();
	}
//	::Sleep(100);
//	message("benchmark_heap_thread_count=%d", benchmark_heap_thread_count);
//	return 0;

	UInt64 start_time = get_RDTSC_time();
	int ms_slept = 0;
	int next_message = 1024 * 10;
	int sleep_unit = 2;
	while (benchmark_heap_thread_count < threads) {
		ms_slept += sleep_unit;
		::Sleep(sleep_unit);
		if (ms_slept > 256 * sleep_unit) sleep_unit*=2;
		if (ms_slept >= next_message) {
			next_message <<= 1;
			message("do_benchmarks(threads=%d/%d) : %d ms slept so far", benchmark_heap_thread_count, threads, ms_slept);
		}
		::MemoryBarrier();
	}
//	message("%d ms", ms_slept);
	return get_RDTSC_time() - start_time;*/
	return 0;
}
void benchmark_heap(SR_HEAP *heap) {
	struct BENCHMARK_SETTINGS {
		const char *name;
		int mode;
		int threads;
	};
	BENCHMARK_SETTINGS benchmark_settings[] = {
		{"simple", MEMORY_BENCHMARK_SIMPLE, 1},
		{"normal", MEMORY_BENCHMARK_ISOLATED, 1},
		{"simple 2-thread", MEMORY_BENCHMARK_SIMPLE, 2},
		{"isolated 2-thread", MEMORY_BENCHMARK_ISOLATED, 2},
		{"unidir. 2-thread", MEMORY_BENCHMARK_UNIDIRECTIONAL, 2},
		{"bidir. 2-thread", MEMORY_BENCHMARK_BIDIRECTIONAL, 2},
		{"chained 8-thread", MEMORY_BENCHMARK_CHAINED, 8},
		{"chained 8-thread", MEMORY_BENCHMARK_CHAINED, 8},
		{"chained 8-thread", MEMORY_BENCHMARK_CHAINED, 8},
		{"chained 8-thread", MEMORY_BENCHMARK_CHAINED, 8},
		{"chained 8-thread", MEMORY_BENCHMARK_CHAINED, 8},//*/
/*
		{"simple 4-thread", MEMORY_BENCHMARK_SIMPLE, 4},
		{"simple 8-thread", MEMORY_BENCHMARK_SIMPLE, 8},
		{"simple 16-thread", MEMORY_BENCHMARK_SIMPLE, 16},
		{"isolated 4-thread", MEMORY_BENCHMARK_ISOLATED, 4},
		{"isolated 8-thread", MEMORY_BENCHMARK_ISOLATED, 8},
		{"isolated 16-thread", MEMORY_BENCHMARK_ISOLATED, 16},
		{"unidir. 4-thread", MEMORY_BENCHMARK_UNIDIRECTIONAL, 4},
		{"unidir. 8-thread", MEMORY_BENCHMARK_UNIDIRECTIONAL, 8},
		{"unidir. 16-thread", MEMORY_BENCHMARK_UNIDIRECTIONAL, 16},
		{"bidir. 4-thread", MEMORY_BENCHMARK_BIDIRECTIONAL, 4},
		{"bidir. 8-thread", MEMORY_BENCHMARK_BIDIRECTIONAL, 8},
		{"bidir. 16-thread", MEMORY_BENCHMARK_BIDIRECTIONAL, 16},//*/
		{NULL, -1, 0},
	};
	message("benchmarking heap (%X %X)", heap, &heap_ThreadHeap3);
	message("times are for all threads combined, in nanoseconds, on a per malloc-or-free basis");
	if (heap->get_bytes_used) message("total bytes used: %d", heap->get_bytes_used());
	enum {MILLISECONDS=360};
	for (int test_index = 0; benchmark_settings[test_index].threads > 0; test_index++) {
		BENCHMARK_SETTINGS &bs = benchmark_settings[test_index];
		double operations = 2 * benchmark_heap(heap, bs.mode, bs.threads, MILLISECONDS);
		double rate = MILLISECONDS * 0.001 / double(operations) * pow(10.0, 9.0);
		char buffy[512];
		char *ptr = &buffy[0];
		int len = sprintf(ptr, "%s", bs.name);
		ptr += len;
		for (int n = 20-len; n >= 0; n--) *(ptr++) = ' ';
		ptr += sprintf(ptr, "%.1f ns/op", rate);
		Sleep(10);
		message("%s", buffy);
//		if (heap->get_bytes_used) message("total bytes used: %d", heap->get_bytes_used());
	}
	if (heap->get_bytes_used) message("total bytes used: %d", heap->get_bytes_used());
}

