#include <stdlib.h>
#include <string>
#include <set>
#include "main.h"
#include "simpleheap.h"
#include "settings.h"

// My first attempt at writing a heap:

//#define DEBUG_SIMPLEHEAP      //turns on extra debugging messages
#define SIMPLEHEAP_USE_MSIZE
#define SIMPLEHEAP_CHECK_ALIGNMENT
//#define SIMPLEHEAP_FASTBLOCKS //all block sizes are powers of 2, alignment checks are done using bitwise logic
//#define SIMPLEHEAP__DISABLE_DEALLOCATION //supresses all deallocate calls; memory is never reused
namespace SimpleHeap1 {
	struct Node {
		Node *next;
	};
	Node const *FROZEN_NODE = (Node*)0x10;
	enum {subblock_size_shift = 21};
	enum {subblock_size = 1 << subblock_size_shift};
//	enum {superblock_size_in_MegaBytes = 250 };
	enum {smart_bypass = 1 };
	char *superblock;
	volatile int used_subblocks;
	int total_subblocks;
	char * volatile subblock_lookup; //block-index to size-index
/*
	vanilla oblivion pool sizes:
		8	12 MB
		12	8 MB
		16	4 MB
		20	4 MB
		24	8 MB
		28	4 MB
		32	8 MB
		36	4 MB
		40	4 MB
		44	4 MB
		48	2 MB
		52	4 MB
		56	32 MB
		64	4 MB
		68	4 MB
		72	8 MB
		80	4 MB
		92	4 MB
		96	8 MB
		100	8 MB
		108	4 MB
		120	4 MB
		200	8 MB
		216	4 MB "NiGeometry Pool"
		244	4 MB "NiNode Pool"
		256	8 MB
		264	4 MB "BSFadeNode Pool"
	total: 8*8+16*4+12+32+2= 174 MB
	pool alignment: 16 MB
	footprint: 8*16+16*16+16+32+2 = 27*16+2 = 434 MB?
	maxsize: 264
	shift_unit_shift: 2
*/
//	enum {max_size = 2048}; const int sizes[] = {8, 16, 24, 32, 48, 64, 96, 128, 192, 256, 384, 512, 768, 1024, 1536, max_size};
	enum {max_size = 512}; const int sizes[] = {8, 16, 32, 64, 128, 256, max_size};
//	enum {max_size = 56}; const int sizes[] = {max_size};
//	enum {max_size = 1<<20}; const int sizes[] = {8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 1<<16,1<<17,1<<18,1<<19,1<<20};
//	enum {max_size = 65536}; const int sizes[] = {8, 16, 24, 32, 48, 64, 96, 128, 192, 256, 384, 512, 768, 1024, 1536, 2048, 3072, 4096, 6144, 8192, 12288, 16384, 24576, 32768, 49152, max_size};
	enum {num_sizes = sizeof(sizes) / sizeof(int)};
	enum {size_unit_shift = 2};
	enum {size_unit = 1 << size_unit_shift};
	int size_lookup[(max_size / size_unit) + 1];
	Node * volatile free_list[num_sizes];
	CRITICAL_SECTION cs;
	int size_to_index(size_t s) {
		if (s > max_size) return -1;
		for (int i = 0; i < num_sizes; i++) if (sizes[i] >= s) return i;
		error("ERROR: SimpleHeap1::size_to_index - this should not happen");
		return -1;
	}
	enum {IS_VALID_ADDRES__MISALIGNED = -1};
	enum {IS_VALID_ADDRES__UNUSED_BLOCK = -2};
	enum {IS_VALID_ADDRES__OUT_OF_BLOCK = -3};
	int is_valid_address(void *ptr) {
		unsigned int offset = int(ptr) - int(superblock);
		unsigned int block = offset >> subblock_size_shift;
		if (block >= total_subblocks) {
			return IS_VALID_ADDRES__OUT_OF_BLOCK;
		}
		int si = subblock_lookup[block];
		if (si < 0 || si >= num_sizes) {
			MemoryBarrier();
			if (block >= used_subblocks) {
				return IS_VALID_ADDRES__UNUSED_BLOCK;
			}
			si = subblock_lookup[block];
			if (si < 0 || si >= num_sizes) {
				error("ERROR: SimpleHeap1 - is_valid_address internal error ???");
				return IS_VALID_ADDRES__OUT_OF_BLOCK;
			}
		}
#ifdef SIMPLEHEAP_FASTBLOCKS
		if ((offset & (subblock_size-1)) & (sizes[si]-1)) return IS_VALID_ADDRES__MISALIGNED;
#else
		if ((offset & (subblock_size-1)) % sizes[si]) return IS_VALID_ADDRES__MISALIGNED;
#endif
		return block;
	}
	bool is_in_heap_fast(void *ptr) {
		unsigned int offset = int(ptr) - int(superblock);
		return offset < ((unsigned int)total_subblocks) << subblock_size_shift;
	}
	bool is_in_heap(void *ptr) {
		return is_valid_address(ptr) >= 0;
	}
	size_t get_size(void *ptr) {
		unsigned int offset = int(ptr) - int(superblock);
		unsigned int block = offset >> subblock_size_shift;
		unsigned char si = subblock_lookup[block];
		if (si >= num_sizes) {
			MemoryBarrier();
			if (block >= used_subblocks) {
				return 0;
			}
			si = subblock_lookup[block];
			if (si >= num_sizes) {
				error("ERROR: SimpleHeap1 - is_valid_address internal error ???");
				return 0;
			}
		}
		return sizes[si];
	}
	void *_allocate(int si) {
#ifdef DEBUG_SIMPLEHEAP
		bool retried = false;
#endif
		Node *ptr = free_list[si];
		if (ptr) {
			while (1) {//return result from freelist, fast
				if (ptr == NULL) {
#ifdef DEBUG_SIMPLEHEAP
					message("free-list was emptied beneath us");
#endif
					break;
				}
				if (ptr == FROZEN_NODE) {
#ifdef DEBUG_SIMPLEHEAP
					message("free-list was frozen, retrying");
					retried = true;
#endif
					if (si < num_sizes - 1) si++;
					ptr = free_list[si];
					if (smart_bypass) if (ptr == NULL || ptr == FROZEN_NODE) return ::malloc(sizes[si]);
//					MemoryBarrier();
					continue;
				}
				Node *ptr2 = (Node*)::InterlockedCompareExchangePointer((void**)&free_list[si], (void*)FROZEN_NODE, ptr);
				if (ptr2 != ptr) {
					ptr = ptr2;
#ifdef DEBUG_SIMPLEHEAP
					message("failed to lock free-list, retrying");
					retried = true;
#endif
					continue;
				}
				Node *next = ptr->next;
#if 1
//def DEBUG_SIMPLEHEAP
				ptr2 = (Node*) InterlockedExchangePointer((void**)&free_list[si], next);
				if (ptr2 != FROZEN_NODE) {
					error("ERROR: free-list was not frozen when it should have been, terminating");
				}
#else
				free_list[si] = next;
//				MemoryBarrier();
#endif
#ifdef DEBUG_SIMPLEHEAP
				if (retried) message("succeeded this time");
				int type = is_valid_address(ptr);
				if (type < 0) message("invalid allocation (addr=%X, size<=%d, type=%d)", ptr, sizes[si], type);
#endif
				return ptr;
			}
		}
#ifdef DEBUG_SIMPLEHEAP
		message("free list empty, attempting to fill it");
#endif
		int old_used_subblocks = used_subblocks;
		if (old_used_subblocks != total_subblocks) {
#ifdef DEBUG_SIMPLEHEAP
			message("entering critical section...");
#endif
			//freelist empty, try to expand freelist
			if (!::TryEnterCriticalSection(&cs)) {
#ifdef DEBUG_SIMPLEHEAP
				message("contention exists, will use malloc instead");
#endif
				return ::malloc(sizes[si]);
			}
//			if (old_used_subblocks != used_subblocks && subblock_lookup[old_used_subblocks] == si) {//another thread expanded the freelist while we were waiting for access, re-try fast path
			if (old_used_subblocks != used_subblocks) {//another thread expanded the freelist while we were waiting for access, re-try fast path
#ifdef DEBUG_SIMPLEHEAP
				message("another thread expanded free list while we were blocking");
#endif
				::LeaveCriticalSection(&cs);
				return _allocate(si);
			}
			if (used_subblocks == total_subblocks) {
#ifdef DEBUG_SIMPLEHEAP
				message("space in main block was exhausted while we were blocking, using malloc instead");
#endif
				::LeaveCriticalSection(&cs);
				return ::malloc(sizes[si]);
			}
			//expand freelist
			if (Settings::Heap::bEnableMessages) message("expanding free list (used_subblocks=%d,  unit size=%d,  time=%d)", used_subblocks, sizes[si], get_time_ms()/1000 );
			int sbi = used_subblocks++;
			subblock_lookup[sbi] = si;
			if (used_subblocks == total_subblocks) {
				message("SimpleHeap1 - exhausted all blocks");
			}
			int base_addr = int(superblock) + subblock_size * sbi;
			Node *last = NULL;
			Node *first = (Node*) (base_addr + sizes[si]);
			for (int i = sizes[si]; i <= subblock_size - sizes[si]; i += sizes[si]) {
#ifdef DEBUG_SIMPLEHEAP
//				message("marking %f", double(i) / sizes[si]);
#endif
				int addr = base_addr + i;
				Node *node = (Node *)addr;
				node->next = (Node *)(addr + sizes[si]);
				last = node;
			}
			Node *rv = (Node*)base_addr;
#ifdef DEBUG_SIMPLEHEAP
			message("committing free list expansion");
#endif
			while (1) {
				Node *tmp = free_list[si];
				if (tmp == FROZEN_NODE) {
#ifdef DEBUG_SIMPLEHEAP
					message("failed to committ free list expansion because it was frozen, retrying");
#endif
//					LoadFence();
					MemoryBarrier();
					continue;
				}
				last->next = tmp;
				Node *tmp2;
				tmp2 = (Node*)::InterlockedCompareExchangePointer( (void**)&free_list[si], first, tmp);
				if (tmp2 != tmp) {
#ifdef DEBUG_SIMPLEHEAP
					message("failed to committ free list expansion, retrying");
#endif
					tmp = tmp2;
					continue;
				}
				break;
			}
#ifdef DEBUG_SIMPLEHEAP
			message("finished free list expansion (size<=%d; block=%d)", sizes[si], sbi);
#endif
			::LeaveCriticalSection(&cs);
#ifdef DEBUG_SIMPLEHEAP
			message("left critical section");
			int type = is_valid_address(rv);
			if (type < 0) message("invalid allocation (addr=%X, size<=%d, type=%d)", rv, sizes[si], type);
#endif
			return (void*)rv;
		}
		//no space to expand freelist, revert to malloc
#ifdef DEBUG_SIMPLEHEAP
		message("no space in main block to expand freelist, reverting to malloc");
#endif
		return malloc(sizes[si]);
	}
	void *allocate(size_t size) {
		if (size > max_size) return ::malloc(size);//too big, revert to malloc
		return _allocate(size_lookup[(size + (size_unit-1)) >> size_unit_shift]);
	}
	void deallocate(void *mem) {
#ifdef SIMPLEHEAP__DISABLE_DEALLOCATION
		return;
#endif
		int block = is_valid_address(mem);
		if (block < 0) {
			if (block == SimpleHeap1::IS_VALID_ADDRES__OUT_OF_BLOCK) {//came from malloc, not our heap
				::free(mem);
			}
			else if (block == SimpleHeap1::IS_VALID_ADDRES__UNUSED_BLOCK) {//INVALID
				error("deallocate - address %X, is_valid_address says it's from an unused block", mem);
			}
			else if (block == SimpleHeap1::IS_VALID_ADDRES__MISALIGNED) {//INVALID
				error("deallocate - address %X, is_valid_address says it's misaligned", mem);
			}
			else {//INTERNAL ERROR
				error("deallocate - address %X, is_valid_address says it's ??? (%d)", mem, block);
			}
			return;
		}
		int si = subblock_lookup[block];
		if (si < 0 || si >= num_sizes) {
			MemoryBarrier();
			si = subblock_lookup[block];
			if (si < 0 || si >= num_sizes) {
				message("SimpleHeap1::deallocate - invalid size index, terminating (block=%d, si=%d, addr=%X, base=%X, total_blocks=%d)", block, si, mem, superblock, total_subblocks);
				return;
			}
		}

		//memset(mem, 0, sizes[si]);//disabled for 4.1.33 - what was this doing enabled?
		Node *node = (Node*)mem;
		bool retried = false;
		while (1) {
			Node *node2 = free_list[si];
			if (node2 == FROZEN_NODE) {
#ifdef DEBUG_SIMPLEHEAP
				message("failed to push deallocated address (free-list frozen), retrying");
				retried = true;
#endif
//				MemoryBarrier();
				continue;
			}
			node->next = node2;
			if (node2 != (Node*)::InterlockedCompareExchangePointer((void**)&free_list[si], node, node2)) {
#ifdef DEBUG_SIMPLEHEAP
				message("failed to push deallocated address, retrying");
				retried = true;
#endif
				continue;
			}
#ifdef DEBUG_SIMPLEHEAP
			if (retried) message("succeeded at pushing deallocated address");
#endif
			return;
		}
	}
	void *reallocate(void *mem, size_t size) {
		size_t offset = int(mem) - int(superblock);
		int block = offset >> subblock_size_shift;
		if (block >= total_subblocks || block < 0) {//came from malloc, not our heap
			return realloc(mem, size);
		}
		int si = subblock_lookup[block];
		int oldsize = sizes[si];
		void *mem2 = allocate(size);
		memcpy( mem2, mem, size > oldsize ? oldsize : size );
		deallocate(mem);
		return mem2;
	}

	void init() {
		::InitializeCriticalSectionAndSpinCount(&cs, 1500);
		total_subblocks = (Settings::Heap::iHeapSize << 20) >> SimpleHeap1::subblock_size_shift;
		used_subblocks = 0;
		if (!Settings::Master::bExperimentalStuff || !Settings::Experimental::iHeapMainBlockAddress) {
			//enum {ALIGN_SUPERBLOCK=4096};
			//superblock = (char*)((int(malloc(total_subblocks * subblock_size + ALIGN_SUPERBLOCK))+ALIGN_SUPERBLOCK-1)&(0xFFffFFfful - (ALIGN_SUPERBLOCK-1)));
			superblock = (char*)VirtualAlloc(NULL, total_subblocks * subblock_size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
		}
		else {
			superblock = (char*)::VirtualAlloc((void*)Settings::Experimental::iHeapMainBlockAddress, total_subblocks * subblock_size, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
			message("SimpleHeap1::init - VirtualAlloc: %08X %08x %X", superblock, Settings::Experimental::iHeapMainBlockAddress, total_subblocks * subblock_size);
		}
		if (!superblock) error("SimpleHeap1::init() - failed to allocate superblock");
		subblock_lookup = (char*)malloc(total_subblocks);
		if (!subblock_lookup) error("SimpleHeap1::init() - failed to allocate subblock_lookup");
		for (int i = 0; i < total_subblocks; i++) subblock_lookup[i] = -1;
		for (int i = 0; i < num_sizes; i++) free_list[i] = NULL;
		for (int i = 0; i < num_sizes; i++) if (sizes[i] % size_unit) {
			error("SimpleHeap1::init() - invalid size %d, terminating", sizes[i]);
		}
		for (int s = 0; s <= (max_size / size_unit); s++) size_lookup[s] = size_to_index(s * size_unit);
#ifdef SIMPLEHEAP_FASTBLOCKS
		//sanity-check fastblocks requirements
		for (int i = 0; i < num_sizes; i++) {
			int x = sizes[i];
			int L2x = int(floor(log((double)x) / log(2.) + 0.5));
			if ((1<<L2x) != x) error("SimpleHeap - bad internal configuration (size %d conflicts with fastblocks)", sizes[i]);
		}
#endif
		//make sure at least 1 block is allocated to each bucket... that might improve startup stutter
		for (int i = 0; i < num_sizes; i++) {
			deallocate(allocate(sizes[i]));
		}
	}
};






//My next attempt at writting a heap.  Trying to make it a simple wrapper.  
namespace ThreadHeap1 {
	namespace BaseHeap {
		using namespace SimpleHeap1;
	};
	enum {num_sizes = BaseHeap::num_sizes};
	enum {BUNCH_SIZE = 2048};
	typedef BaseHeap::Node Node;
	int tls_slot = TLS_OUT_OF_INDEXES;
	int bunch_size[num_sizes];
	struct Bunch {
		BaseHeap::Node *next;
		Bunch *next_bunch;
	};
	CRITICAL_SECTION cs;
	Bunch * volatile free_bunch_list[num_sizes];
	struct PerThread {
		Node * volatile free_list[num_sizes];
		volatile int free_list_count[num_sizes];
		Node * volatile free_list2[num_sizes];
		volatile int free_list2_count[num_sizes];
		//TODO: optimize data structure... the current 2 list thing is silly and sub-optimal
		//a single list + a circular buffer should be better
		PerThread() {
			for (int i = 0; i < num_sizes; i++) {
				free_list[i] = NULL;
				free_list_count[i] = 0;
				free_list2[i] = NULL;
				free_list2_count[i] = 0;
			}
		}
	};
	PerThread *get_PT() {
		void *rv = TlsGetValue(tls_slot);
		if (rv) return (PerThread*) rv;
		::EnterCriticalSection(&cs);
		rv = TlsGetValue(tls_slot);
		if (rv) {
			::LeaveCriticalSection(&cs);
			return (PerThread*) rv;
		}
		rv = new PerThread();
		if (!TlsSetValue(tls_slot, rv)) {
			error("ERROR: ThreadHeap1: failed to set TLS");
		}
		::LeaveCriticalSection(&cs);
		return (PerThread*) rv;
	}
	void *_allocate(int si) {
		PerThread *pt = get_PT();
		Node *rv = pt->free_list[si];
		if (rv) {
			pt->free_list[si] = rv->next;
			pt->free_list_count[si]--;
			return rv;
		}
		//1st list empty, check 2nd list
		rv = pt->free_list2[si];
		if (rv) {
			pt->free_list2[si] = rv->next;
			pt->free_list2_count[si]--;
			return rv;
		}
		//all thread-local lists are empty, try to grab a bunch for this thread
		Bunch *bunch = free_bunch_list[si];
		if (bunch) {
			::EnterCriticalSection(&cs);
			bunch = free_bunch_list[si];
			if (bunch) {
				free_bunch_list[si] = bunch->next_bunch;
				rv = (Node*)bunch;
				pt->free_list2[si] = bunch->next;
				pt->free_list2_count[si] = bunch_size[si] - 1;
				::LeaveCriticalSection(&cs);
				return rv;
			}
			::LeaveCriticalSection(&cs);
		}
		//all per-thread lists are empty, fallback to non-thread-aware system ; buffer several, hopefully that will be faster
/*		rv = NULL;
		for (int i = 0; i < bunch_size[si]; i++) {
			Node *n = (Node*)BaseHeap::_allocate(si);
			n->next = rv;
			rv = n;
		}
		pt->free_list[si] = rv->next;
		pt->free_list_count[si] = bunch_size[si] - 1;*/
//		return rv;
		return BaseHeap::_allocate(si);
	}
	void *allocate(size_t size) {
		if (size > BaseHeap::max_size) return ::malloc(size);//too big, revert to malloc
		return _allocate(BaseHeap::size_lookup[(size + (BaseHeap::size_unit-1)) >> BaseHeap::size_unit_shift]);
	}
	void deallocate(void *ptr) {
		int bi = BaseHeap::is_valid_address(ptr);
		if (bi < 0) {//avoid placing strange allocations in to per-thread free-lists
			BaseHeap::deallocate(ptr);
			return;
		}
		int si = BaseHeap::subblock_lookup[bi];
		PerThread *pt = get_PT();
		Node *mem = (Node*)ptr;
		if (pt->free_list_count[si] < bunch_size[si]) {
			mem->next = pt->free_list[si];
			pt->free_list[si] = mem;
			return;
		}
		if (pt->free_list2_count[si] < bunch_size[si]) {
			mem->next = pt->free_list2[si];
			pt->free_list2[si] = mem;
			return;
		}

		Bunch *bunch = (Bunch*) pt->free_list2;
		pt->free_list2[si] = mem;
		pt->free_list2_count[si] = 1;

		::EnterCriticalSection(&cs);
		bunch->next_bunch = free_bunch_list[si];
		free_bunch_list[si] = bunch;
		::LeaveCriticalSection(&cs);
	}
	void *reallocate(void *mem, size_t size) {
		int bi = BaseHeap::is_valid_address(mem);
		if (bi == BaseHeap::IS_VALID_ADDRES__OUT_OF_BLOCK) {//came from malloc, not our heap
			return realloc(mem, size);
		}
		int si = BaseHeap::subblock_lookup[bi];
		int oldsize = BaseHeap::sizes[si];
		void *mem2 = allocate(size);
		memcpy( mem2, mem, size > oldsize ? oldsize : size );
		deallocate(mem);
		return mem2;
	}
	void init() {
		BaseHeap::init();

		tls_slot = TlsAlloc();
		if (tls_slot == TLS_OUT_OF_INDEXES) error("ERROR: ThreadHeap1 - out of TLS slots?");
		::InitializeCriticalSectionAndSpinCount(&cs, 2500);
		for (int i = 0; i < num_sizes; i++) {
			bunch_size[i] = 1 + ((BUNCH_SIZE-1) / BaseHeap::sizes[i]);
			free_bunch_list[i] = NULL;
		}
	}
};





//aiming for maximum correctness this time
namespace SimpleHeap2 {
	struct Node {
		Node *next;
	};
//	Node const *FROZEN_NODE = (Node*)0x10;
	enum {subblock_size_shift = 20};
	enum {subblock_size = 1 << subblock_size_shift};
//	enum {superblock_size_in_MegaBytes = 250 };
	char *superblock;
	volatile int used_subblocks;
	int total_subblocks;
	char * volatile subblock_lookup; //block-index to size-index
	enum {max_size = 512};
	const int sizes[] = {8, 16, 32, 64, 128, 256, max_size};
//	enum {max_size = 56};
//	const int sizes[] = {max_size};
	enum {num_sizes = sizeof(sizes) / sizeof(int)};
	enum {size_unit_shift = 2};
	enum {size_unit = 1 << size_unit_shift};
	int size_lookup[(max_size / size_unit) + 1];
	CRITICAL_SECTION cs;

//	Node * volatile free_list[num_sizes];
	class FreeList1 {
		Node *head;
		int num_blocks;
	public:
		void init() {
			head = NULL;
			num_blocks = 0;
		}
//		bool _is_empty() const {return head == NULL;}
		Node *pop() {
			::EnterCriticalSection(&cs);
			Node *rv = head;
			if (rv) head = rv->next;
			::LeaveCriticalSection(&cs);
			return rv;
		}
		void push(Node* ptr) {
			::EnterCriticalSection(&cs);
			ptr->next = head;
			head = ptr;
			::LeaveCriticalSection(&cs);
		}
		void push_block ( Node *base_ptr, Node *end_ptr, int size ) {
			EnterCriticalSection(&cs);
			for (; base_ptr < end_ptr; base_ptr = (Node*)(int(base_ptr)+size)) {
				push(base_ptr);
			}
			num_blocks++;
			LeaveCriticalSection(&cs);
		}
	};
	struct FreeList2 {
		CRITICAL_SECTION cs;
		Node *head;
		int num_blocks;
		void init() {
			::InitializeCriticalSectionAndSpinCount(&cs, 99999);
			num_blocks = 0;
		}
//		bool _is_empty() const {return head == NULL;}
		Node *pop() {
			::EnterCriticalSection(&cs);
			Node *rv = head;
			if (rv) head = rv->next;
			::LeaveCriticalSection(&cs);
			return rv;
		}
		void push(Node* ptr) {
			::EnterCriticalSection(&cs);
			ptr->next = head;
			head = ptr;
			::LeaveCriticalSection(&cs);
		}
		void push_block ( Node *base_ptr, Node *end_ptr, int size ) {
			EnterCriticalSection(&cs);
			for (; base_ptr < end_ptr; base_ptr = (Node*)(int(base_ptr)+size)) {
				push(base_ptr);
			}
			num_blocks++;
			LeaveCriticalSection(&cs);
		}
	};
	struct FreeList3 {
		struct Ways {
			CRITICAL_SECTION cs;
			Node *head;
			char padding[64-28];
		};
		int num_blocks;
		enum {WAYS=4};
		Ways ways[WAYS];
		char padding[64];
		volatile long push_switcher;
		char padding2[64];
		volatile long pop_switcher;
		void init() {
			for (int i = 0; i < WAYS; i++) {
				::InitializeCriticalSectionAndSpinCount(&ways[i].cs, 99999);
				ways[i].head = NULL;
			}
			num_blocks = 0;
			push_switcher = 0;
			pop_switcher = 0;
		}
		Node *pop() {
			int index = ::InterlockedIncrement(&pop_switcher) & (WAYS-1);
			Ways &way = ways[index];
			::EnterCriticalSection(&way.cs);
			Node *rv = way.head;
			if (rv) way.head = rv->next;
			::LeaveCriticalSection(&way.cs);
			return rv;
		}
		void push(Node* ptr) {
			int index = ::InterlockedIncrement(&push_switcher) & (WAYS-1);
			Ways &way = ways[index];
			::EnterCriticalSection(&way.cs);
			ptr->next = way.head;
			way.head = ptr;
			::LeaveCriticalSection(&way.cs);
		}
		void push_block ( Node *base_ptr, Node *end_ptr, int size ) {
//			int mask = 0;
//			for (int i = 0; i < WAYS; i++) {
//				if (!ways[i].head)
//			}
			for (int i = 0; i < WAYS; i++) ::EnterCriticalSection(&ways[i].cs);
			int w = 0;
			for (; base_ptr < end_ptr; base_ptr = (Node*)(int(base_ptr)+size)) {
				Node *& head = ways[(w++)&(WAYS-1)].head;
				base_ptr->next = head;
				head = base_ptr;
			}
			num_blocks++;
			for (int i = 0; i < WAYS; i++) ::LeaveCriticalSection(&ways[i].cs);
		}
	};

	typedef FreeList1 FreeList;
	FreeList free_list[num_sizes];

	int size_to_si ( size_t size) {return size_lookup[(size + (size_unit - 1)) >> size_unit_shift];}
	int si_to_size ( int si) {return sizes[si];}
	enum {ADDRESS_TO_SI__NOT_IN_SUPERBLOCK =-1};
	enum {ADDRESS_TO_SI__INVALID_BLOCK     =-2};
	enum {ADDRESS_TO_SI__MISALIGNED        =-3};
	int address_to_si ( void *mem ) {
		UInt32 offset = UInt32(mem) - UInt32(superblock);
		UInt32 bi = offset >> subblock_size_shift;
		if (bi >= total_subblocks) return ADDRESS_TO_SI__NOT_IN_SUPERBLOCK;
		int si = subblock_lookup[bi];
		if (si < 0) {
			MemoryBarrier();
			si = subblock_lookup[bi];
			if (si < 0) {
				return ADDRESS_TO_SI__INVALID_BLOCK;
			}
		}
#ifdef SIMPLEHEAP_FASTBLOCKS
		if ((offset & (subblock_size-1)) & (sizes[si]-1)) return ADDRESS_TO_SI__MISALIGNED;
#else
		if ((offset & (subblock_size-1)) % sizes[si]) return ADDRESS_TO_SI__MISALIGNED;
#endif
		return si;
	}
	int address_to_bi ( void *mem ) {
		UInt32 offset = UInt32(mem) - UInt32(superblock);
		UInt32 bi = offset >> subblock_size_shift;
		if (bi >= total_subblocks) return ADDRESS_TO_SI__NOT_IN_SUPERBLOCK;
		int si = subblock_lookup[bi];
		if (si < 0) {
			MemoryBarrier();
			si = subblock_lookup[bi];
			if (si < 0) {
				return ADDRESS_TO_SI__INVALID_BLOCK;
			}
		}
#ifdef SIMPLEHEAP_FASTBLOCKS
		if ((offset & (subblock_size-1)) & (sizes[si]-1)) return ADDRESS_TO_SI__MISALIGNED;
#else
		if ((offset & (subblock_size-1)) % sizes[si]) return ADDRESS_TO_SI__MISALIGNED;
#endif
		return bi;
	}

	void *_allocate ( int si ) {
//		message("[  _allocate");
		Node *rv = free_list[si].pop();
		if (rv) return rv;
		if (used_subblocks == total_subblocks) {
			message("   _allocate using malloc");
			return ::malloc(sizes[si]);
		}
//		message("   _allocate failed, expanding freelist (si:%d)", si);
		::EnterCriticalSection(&cs);
		rv = free_list[si].pop();
		if (rv) {
//			message(" ] freelist non-empty by the time we entered the critical section (si:%d)", si);
			::LeaveCriticalSection(&cs);
			return rv;
		}
		if (used_subblocks == total_subblocks) {
			message(" ] all blocks used by the time we entered the critical section (si:%d)", si);
			::LeaveCriticalSection(&cs);
			return ::malloc(sizes[si]);
		}
		int bi = used_subblocks++;
		subblock_lookup[bi] = si;
		::LeaveCriticalSection(&cs);
		int base = int(superblock) + subblock_size * bi;
		free_list[si].push_block( (Node*)base, (Node*)(base + subblock_size - sizes[si] + 1), sizes[si] );
		if (Settings::Heap::bEnableMessages) message("   _allocate: freelist expanded (si:%d, bi:%d, time:%d)", si, bi, get_time_ms()/1000);
		return _allocate(si);
	}
	void *allocate ( size_t size ) {
//		message("[  allocate");
		if (size > max_size) {
//			message(" ] allocate (exceeded max size)");
			return ::malloc(size);
		}
		int si = size_to_si(size);
		void *rv = _allocate(si);
//		message(" ] allocate %X %d", rv, size);
		return rv;
	}
	void _deallocate ( void *mem, int si ) {
		if (si < 0) {
			if (si == ADDRESS_TO_SI__NOT_IN_SUPERBLOCK) return ::free(mem);
			else error("SimpleHeap2::_deallocate - invalid block");
			return;
		}
		free_list[si].push((Node*)mem);
	}
	void deallocate ( void *mem ) {
//		message("[  deallocate");
		_deallocate(mem, address_to_si(mem));
//		message(" ] deallocate");
	}
	void *reallocate ( void *mem, size_t size ) {
		if (!mem) return allocate(size);
//		message("[  reallocate");
		int si = address_to_si(mem);
		if (si < 0) {
			if (si == ADDRESS_TO_SI__NOT_IN_SUPERBLOCK) {
//				message(" ] reallocate malloced mem");
				return ::realloc(mem, size);
			}
			error("ERROR: SimpleHeap2::reallocate - invalid block");
		}
		int si2 = size_to_si(size);
		if (si2 == si) {
//			message(" ] reallocate using same memory (%X)", mem);
			return mem;
		}

		void *mem2 = _allocate(si2);
		int minsize = si_to_size(si);
		if (minsize > size) minsize = size;
		memcpy(mem2, mem, minsize);
#ifndef SIMPLEHEAP__DISABLE_DEALLOCATION
		_deallocate(mem, si);
#endif
		int bi=address_to_bi(mem), bi2 = address_to_bi(mem2);
//		message(" ] reallocate %X -> %X (si:%d->%d, bi:%d->%d, size:%d->%d(%d))", mem, mem2, si, si2, bi, bi2, si_to_size(si), size, minsize);
		return mem2;
	}

	int _size_to_index(size_t s) {
		if (s > max_size) return -1;
		for (int i = 0; i < num_sizes; i++) if (sizes[i] >= s) return i;
		error("ERROR: SimpleHeap2::size_to_index - this should not happen");
		return -1;
	}
	void init() {
		::InitializeCriticalSectionAndSpinCount(&cs, 4000);
		total_subblocks = (Settings::Heap::iHeapSize << 20) >> subblock_size_shift;
		used_subblocks = 0;
		enum {ALIGN_SUPERBLOCK=4096};
		superblock = (char*)malloc(total_subblocks * subblock_size + ALIGN_SUPERBLOCK);
		superblock = (char*)((int(superblock)+ALIGN_SUPERBLOCK-1)&(0xFFffFFfful - (ALIGN_SUPERBLOCK-1)));
		if (!superblock) error("ERROR: SimpleHeap2::init - main block allocation failed, shutting down");
		subblock_lookup = (char*)malloc(total_subblocks);
		for (int i = 0; i < total_subblocks; i++) subblock_lookup[i] = -1;
		for (int i = 0; i < num_sizes; i++) free_list[i].init();
		for (int i = 0; i < num_sizes; i++) if (sizes[i] % size_unit) {
			error("SimpleHeap2::init() - invalid size %d, terminating", sizes[i]);
		}
		for (int i = 0; i <= (max_size / size_unit); i++) {
			int s = i * size_unit;
			int x = -1;
			if (s <= max_size) for (int j = 0; j < num_sizes; j++) if (sizes[j] >= s) {x=j; break;}
			size_lookup[i] = x;
		}
#ifdef SIMPLEHEAP_FASTBLOCKS
		//sanity-check fastblocks requirements
		for (int i = 0; i < num_sizes; i++) {
			int x = sizes[i];
			int L2x = int(floor(log((double)x) / log(2.) + 0.5));
			if ((1<<L2x) != x) error("SimpleHeap - bad internal configuration (size %d conflicts with fastblocks)", sizes[i]);
		}
#endif
		//make sure at least 1 block is allocated to each bucket... that might improve startup stutter
		for (int i = 0; i < num_sizes; i++) {
			deallocate(allocate(sizes[i]));
		}
	}
};//*/







//2nd per-thread heap, not just a thin wrapper this time
//order name										locking		usage				
//  1. buffer (comparable to std::deque)			per-thread	bi-directional
//  2. partial-bunch (a singly linked list)			per-thread	bi-direction		
//  3. partial-block								per-thread	alloc-only			comes from shared blocks
//  4. bunch-lists (similar to singly linked list)	***			bi-directional
//  5. blocks										Interlocked	alloc-only			extremely simple
//  6. malloc/free									???			bi-directional		OS-dependent
//in the order that allocate() checks them in... if there is something available from #1 it doesn't check #2 etc
//deallocate() checks them in roughly the same order but skips many of them since some are alloc-only
//NOTE: this leaks when threads are destroyed... which is almost never in Oblivion
namespace ThreadHeap2 {
	//basics:
//	enum {superblock_size_in_MegaBytes = 250 };//TUNE, min 1
//	enum {max_size = 2048}; const int sizes[] = {8, 16, 32, 64, 128, 256, 512, 1024, max_size};//TUNE, must be ordered, smallest must be >= 8, must be multiples of size_unit
	enum {max_size = 2048}; const int sizes[] = {8, 16, 24, 32, 48, 64, 96, 128, 192, 256, 384, 512, 768, 1024, 1536, max_size};//TUNE, must be ordered, smallest must be >= 8, must be multiples of size_unit
	enum {num_sizes = sizeof(sizes) / sizeof(int)};
	enum {size_unit_shift = 3};//TUNE
	enum {size_unit = 1 << size_unit_shift};
	int size_lookup[(max_size / size_unit) + 1];
	int tls_slot = TLS_OUT_OF_INDEXES;
	struct Node {
		Node *next;
	};
	int size_to_si ( size_t size) {return size_lookup[(size + (size_unit - 1)) >> size_unit_shift];}
	inline void prefetch_block(int block) {}
//	inline void prefetch_node (void *node) {__asm PREFETCH node}
	inline void prefetch_node (void *node) {}
//	inline void message(...) {}

	//5. blocks:
	enum {block_size_shift = 18};//TUNE, min log2(max_size)+1
	enum {block_size = 1 << block_size_shift};
	char *superblock;
	volatile long used_blocks;
//	enum {total_blocks = (superblock_size_in_MegaBytes << 20) >> block_size_shift};
	int total_blocks;
	volatile char *block_to_si_lookup; //block-index to size-index
	char * allocate_block(int si) {
		if (used_blocks == total_blocks) {
			return NULL;
		}
		int block = ::InterlockedIncrement(&used_blocks) - 1;
		if (block >= total_blocks) {
			used_blocks = total_blocks;
			return NULL;
		}
		if (block == total_blocks -1) {
			message("ThreadHeap2 - exhausted all blocks");
		}
		block_to_si_lookup[block] = si;
		char *rv = &superblock[block << block_size_shift];
		prefetch_block(block+1);
		if (Settings::Heap::bEnableMessages) message("%d byte block #%d allocated to pool for size %d at time %d", block_size, block, si, (get_time_ms() / 1000));
		return rv;
	}
	enum {ADDRESS_TO_SI__NOT_IN_SUPERBLOCK =-1};
	enum {ADDRESS_TO_SI__INVALID_BLOCK     =-2};
	enum {ADDRESS_TO_SI__MISALIGNED        =-3};
	int address_to_si(void *mem) {
		unsigned int offset = int(mem) - int(superblock);
		unsigned int block = offset >> block_size_shift;
		if (block >= total_blocks) return ADDRESS_TO_SI__NOT_IN_SUPERBLOCK;
		if (block >= used_blocks) {
			MemoryBarrier();
			if (block >= used_blocks) {
				return ADDRESS_TO_SI__INVALID_BLOCK;
			}
		}
		int si = block_to_si_lookup[block];
#ifdef SIMPLEHEAP_CHECK_ALIGNMENT
#ifdef SIMPLEHEAP_FASTBLOCKS
		if (offset & (sizes[si]-1)) return ADDRESS_TO_SI__MISALIGNED;
#else
		if ((offset&(block_size-1)) % sizes[si]) return ADDRESS_TO_SI__MISALIGNED;
#endif
#endif
		return si;
	}
	bool is_in_heap_fast(void *ptr) {
		unsigned int offset = int(ptr) - int(superblock);
		return offset < ((unsigned int)total_blocks) << block_size_shift;
	}
	size_t get_size(void *ptr) {
		int si = address_to_si(ptr);
		if (si < 0) return 0;
		return sizes[si];
	}

	//4. bunch-lists:
	enum {MAX_BUNCH_COUNT = 10};//TUNE, min 1
	enum {BUNCH_SIZE = 256 * MAX_BUNCH_COUNT};//TUNE
	int bunch_size[num_sizes];
	struct Bunch {
		Node *next;
		Bunch *next_bunch;
	};
	struct BunchList {
		CRITICAL_SECTION cs;//lock for bunch list
		Bunch *head;
		BunchList() {
			::InitializeCriticalSectionAndSpinCount(&cs, 4000);
			head = NULL;
		}
		void push ( Node * node ) {
			::EnterCriticalSection(&cs);
			((Bunch*)node)->next_bunch = head;
			head = ((Bunch*)node);
			::LeaveCriticalSection(&cs);
		}
		Node *pop ( ) {
			::EnterCriticalSection(&cs);
			Node *rv = (Node*)head;
			if (head) {
				head = head->next_bunch;
				prefetch_node(head);
			}
			::LeaveCriticalSection(&cs);
			return rv;
		}
	};
	struct BunchList2 {
		Bunch *volatile head;
		BunchList2() {
			head = NULL;
		}
		void push ( Node * node_ ) {
			Bunch *node = (Bunch*)node_;
			while (1) {
				Bunch *next = head;
				if (next == ((void*)0x00000011)) {
//					MemoryBarrier();
					continue;
				}
				node->next_bunch = next;
				if (::InterlockedCompareExchangePointer( (void**)&head, node, next ) == next)
					break;
				//else message("ThreadHeap2::BunchList2::push() - failed to push, retrying");
			}
		}
		Node *pop ( ) {
			while (1) {
				Bunch *next = head;
				if (next == ((void*)0x00000011)) {
//					MemoryBarrier();
					continue;
				}
				if (::InterlockedCompareExchangePointer( (void**)&head, ((void*)0x00000011), next ) != next)//locks the list
//					{message("ThreadHeap2::BunchList2::pop() - failed to lock, retrying");continue;}
					continue;
				Bunch *next2;
				next2 = next ? next->next_bunch : NULL;
				prefetch_node(next2);
#if 1
				head = next2;//unlocks the list
#elif 1
//def DEBUG_SIMPLEHEAP
				if ( InterlockedExchangePointer( &head, next2 ) == ((void*)0x00000011))//unlocks the list
					return (Node*)next;
				else error("ThreadHeap2::BunchList2::pop - impossible");
#else
				head = next2;//unlocks the list
				MemoryBarrier();
#endif
				return (Node*)next;
			}
		}
	};
	BunchList2 free_bunch_list[num_sizes];

	//begining of shared stuff:
	struct PerThread {
		enum {BUFFER_SIZE = 8};//TUNE, must be a power of 2
		enum {MASK = BUFFER_SIZE-1};
		struct PerSize {
			//1. buffer
			Node *buffer[BUFFER_SIZE];
			int buffer_head, num_buffered;

			//2. partial-bunch
			Node *partial_bunch;
			int partial_bunch_size;

			//3. partial-block
			char *partial_block;
			char *partial_block_end;

			PerSize() {
				buffer_head = 0;
				num_buffered = 0;
				partial_bunch = NULL;
				partial_bunch_size = 0;
				partial_block = NULL;
				partial_block_end = NULL;
//				for (int i = 0; i < BUFFER_SIZE; i++) buffer[i] = NULL;
			}
		};
		PerSize per_size[num_sizes];

		void *pop(int si) {
			Node *rv;
			//try buffer
			PerSize &ps = per_size[si];
			if (ps.num_buffered != 0) {
				ps.num_buffered -= 1;
				rv = ps.buffer[ps.buffer_head];
				ps.buffer_head = (ps.buffer_head - 1) & MASK;
//				prefetch_node(ps.buffer[ps.buffer_head]);
//				message("ThreadHeap2 - popping from buffer (si:%d)", si);
				return rv;
			}

			//try partial-bunch
			if (ps.partial_bunch) {
				ps.partial_bunch_size -= 1;
				rv = ps.partial_bunch;
				ps.partial_bunch = rv->next;
				ps.buffer[ps.buffer_head] = ps.partial_bunch;
				prefetch_node(ps.partial_block);
//				message("ThreadHeap2 - popping from partial-bunch (si:%d)", si);
				return rv;
			}

			//try partial-block
			if (ps.partial_block) {
				rv = (Node*)ps.partial_block;
				ps.partial_block += sizes[si];
				if (ps.partial_block > ps.partial_block_end) ps.partial_block = NULL;
				else prefetch_node(ps.partial_block);
//				message("ThreadHeap2 - popping from partial-block (si:%d)", si);
				return rv;
			}

			//shared bunch-list
			rv = free_bunch_list[si].pop();
			if (rv) {
				ps.partial_bunch_size = bunch_size[si] - 1;
				ps.partial_bunch = rv->next;
				prefetch_node(rv->next);
//				message("ThreadHeap2 - popping from shared bunchs (si:%d)", si);
				return rv;
			}

			//shared blocks (last resort path)
			ps.partial_block = allocate_block(si);
			if (!ps.partial_block) {//malloc (REALLY last resort path)
//				message("ThreadHeap2 - popping from malloc (si:%d)", si);
				return ::malloc(sizes[si]);
			}
			ps.partial_block_end = ps.partial_block + block_size - sizes[si] + 1;
			rv = (Node*) ps.partial_block;
			ps.partial_block += sizes[si];
			prefetch_node(ps.partial_block);
//			message("ThreadHeap2 - popping from shared blocks (si:%d)", si);
			return rv;
		}
		void push(Node *mem, int si) {
			//try buffer
			PerSize &ps = per_size[si];

/*			if (ps.num_buffered != 0) {
				ps.num_buffered -= 1;
				rv = ps.buffer[ps.buffer_head];
				ps.buffer_head = (ps.buffer_head - 1) & MASK;
//				prefetch_node(ps.buffer[ps.buffer_head]);
//				message("ThreadHeap2 - popping from buffer (si:%d)", si);
				return rv;
			}*/

//			if (ps.num_buffered < MASK+1) {
			if (true) {
				mem->next = ps.buffer[ps.buffer_head];;
				ps.buffer_head = (ps.buffer_head + 1) & MASK;
				if (!ps.num_buffered) mem->next = ps.partial_bunch;
				else if (ps.num_buffered == BUFFER_SIZE) {
					if (ps.partial_bunch_size == bunch_size[si]) {
						free_bunch_list[si].push(ps.partial_bunch);
						ps.partial_bunch_size = 0;
						ps.partial_bunch = NULL;
						ps.buffer[(ps.buffer_head+0) & MASK]->next = NULL; 
//						message("ThreadHeap2 - push spilled a bunch to shared-bunch-list");
					}
					ps.partial_bunch = ps.buffer[ps.buffer_head];
					ps.partial_bunch_size += 1;
					ps.num_buffered -= 1;
				}
				ps.buffer[ps.buffer_head] = mem;
				ps.num_buffered += 1;
			}
			
			//if buffer is full, spill to shared bunch list
//			message("ThreadHeap2 - pushed on to buffer (si:%d)", si);
//			if (ps.num_buffered != BUFFER_SIZE) return;
			

		}

	};
	PerThread *get_PT() {
		void *rv = TlsGetValue(tls_slot);
		if (rv) return (PerThread*) rv;
//		message("ThreadHeap2 - no PerThread for this thread, creating");
		rv = new PerThread();
		if (!TlsSetValue(tls_slot, rv)) {
			error("ERROR: ThreadHeap2: failed to set TLS");
		}
		return (PerThread*) rv;
	}
	void *allocate(size_t size) {
		if (size > max_size) return ::malloc(size);//too big, revert to malloc
		int si = size_to_si(size);
		PerThread *pt = get_PT();
		return pt->pop(si);
	}
	void deallocate(void *ptr) {
		//check address
		int si = address_to_si(ptr);
		if (si < 0) {
			if (si == ADDRESS_TO_SI__NOT_IN_SUPERBLOCK) {
//				message("address not in superblock (%X not in %X)", ptr, superblock);
				::free(ptr);
				return;
			}
			message("ThreadHeap2::deallocate - invalid address, ignoring");
			return;
		}

		PerThread *pt = get_PT();
		pt->push((Node*)ptr, si);
	}
	void *reallocate(void *mem, size_t size) {
		if (!mem) return allocate(size);
		int si = address_to_si(mem);
		int si2;
		if (size > max_size) si2 = -1;
		else si2 = size_to_si(size);
		void *mem2;
		int minsize;
		PerThread *pt = get_PT();

		if (si == ADDRESS_TO_SI__NOT_IN_SUPERBLOCK) {
			if (si2 == -1 || size >= 512) {
				return realloc(mem, size);
			}
			else {
				mem = realloc(mem, size);
				mem2 = pt->pop(si2);
				memcpy(mem2, mem, size);
				free(mem);
				return mem2;
			}
		}
		else if (si < 0) {
			error("ThreadHeap2::reallocate - invalid address");
			//message("ThreadHeap2::reallocate - invalid address, attempting to ignore, likely to crash");
			return mem;
		}
		else if (si2 == si) {
			return mem;
		}

		if (si2 != -1) mem2 = pt->pop(si2);
		else mem2 = malloc(size);
		minsize = sizes[si];
		if (minsize > size) minsize = size;
		memcpy(mem2, mem, minsize);
		pt->push((Node*)mem, si);
		return mem2;
	}

	void init() {
		tls_slot = TlsAlloc();
		if (tls_slot == TLS_OUT_OF_INDEXES) error("ERROR: ThreadHeap2 - out of TLS slots?");

		total_blocks = (Settings::Heap::iHeapSize << 20) >> block_size_shift;
		block_to_si_lookup = new char[total_blocks];
		for (int i = 0; i < total_blocks; i++) block_to_si_lookup[i] = -1;
		used_blocks = 0;
		if (!Settings::Master::bExperimentalStuff || !Settings::Experimental::iHeapMainBlockAddress) {
			//enum {ALIGN_SUPERBLOCK=4096};
			//superblock = (char*)malloc(total_blocks * block_size + ALIGN_SUPERBLOCK);
			//superblock = (char*)((int(malloc(total_subblocks * subblock_size + ALIGN_SUPERBLOCK))+ALIGN_SUPERBLOCK-1)&(0xFFffFFfful - (ALIGN_SUPERBLOCK-1)));
			//superblock = (char*)((int(superblock)+ALIGN_SUPERBLOCK-1)&(0xFFffFFfful - (ALIGN_SUPERBLOCK-1)));
			superblock = (char*)VirtualAlloc(NULL, total_blocks * block_size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
		}
		else {
			superblock = (char*)::VirtualAlloc((void*)Settings::Experimental::iHeapMainBlockAddress, total_blocks * block_size, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
			message("ThreadHeap2::init - VirtualAlloc: %08X %08x %X", superblock, Settings::Experimental::iHeapMainBlockAddress, total_blocks * block_size);
		}

		if (!superblock) error("ERROR: ThreadHeap2::init - main block allocation failed, shutting down");
		for (int i = 0; i < num_sizes; i++) if (sizes[i] % size_unit) {
			error("ThreadHeap2::init() - invalid size %d, terminating", sizes[i]);
		}
		for (int i = 0; i <= (max_size / size_unit); i++) {
			int s = i * size_unit;
			int x = -1;
			if (s <= max_size) for (int j = 0; j < num_sizes; j++) if (sizes[j] >= s) {x=j; break;}
			size_lookup[i] = x;
		}
#ifdef SIMPLEHEAP_FASTBLOCKS
		//sanity-check fastblocks requirements
		for (int i = 0; i < num_sizes; i++) {
			int x = sizes[i];
			int L2x = int(floor(log((double)x) / log(2.) + 0.5));
			if ((1<<L2x) != x) error("SimpleHeap - bad internal configuration (size %d conflicts with fastblocks)", sizes[i]);
		}
#endif
		//make sure at least 1 block is allocated to each bucket... that might improve startup stutter
//		for (int i = 0; i < num_sizes; i++) {
//			deallocate(allocate(sizes[i]));
//		}
		for (int i = 0; i < num_sizes; i++) {
			int k = BUNCH_SIZE / sizes[i];
			if (k > MAX_BUNCH_COUNT) k = MAX_BUNCH_COUNT;
			if (k > PerThread::BUFFER_SIZE) k = PerThread::BUFFER_SIZE;
			if (k < 1) k = 1;
			bunch_size[i] = k;
		}
	}
};

//3nd per-thread heap, cleaning up a few concepts
//order name				Per-Thread	Per-Bin		bi-directional	other
//  1. list					yes			yes			yes				power-of-2 elements
//  2. bunch-list			no			yes			yes				CS locking
//  3. spillage				no			yes			yes				rarely used (CS locking)
//  4. partial-block		no			yes			no				interlocked
//  5. blocks				no			no			no				interlocked
//  6. malloc/free			???			no			yes				OS-dependent
//in the order that allocate() checks them in... if there is something available from #1 it doesn't check #2 etc
//deallocate() checks them in roughly the same order but skips the alloc-only ones
//spills are a special case, malloc never checks them, they're looked at only on thread destruction
//  if spills fill up during a threads destruction then they will go in to bunch-list
namespace ThreadHeap3 {
	//basics:
//	enum {max_size = 2048}; const int sizes[] = {8, 16, 32, 64, 128, 256, 512, 1024, max_size};//TUNE, must be ordered, smallest must be >= 8, must be multiples of size_unit
//	enum {max_size = 2048}; const int sizes[] = {8, 16, 24, 32, 48, 64, 96, 128, 192, 256, 384, 512, 768, 1024, 1536, max_size};//TUNE, must be ordered, smallest must be >= 8, must be multiples of size_unit
//	enum {max_size = 2048}; const int sizes[] = {8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 448, 512, 640, 768, 896, 1024, 1280, 1536, 1792, max_size};
	enum {max_size = 3584}; const int sizes[] = {8,16,24,32,40,48,56,64,80,96,112,128,160,192,224,256,320,384,448,512,640,768,896,1024,1280,1536,1792,2048,2560,3072,3584};
	enum {num_sizes = sizeof(sizes) / sizeof(int)};
	enum {size_unit_shift = 3};//TUNE
	enum {size_unit = 1 << size_unit_shift};
	int size_lookup[(max_size / size_unit) + 1];
	int tls_slot = TLS_OUT_OF_INDEXES;
	struct Node {
		Node *next;
	};
	struct Bunch : public Node {
		Bunch *next_bunch;
	};
	inline int size_to_si ( size_t size) {return size_lookup[(size + (size_unit - 1)) >> size_unit_shift];}
	inline void prefetch_block(int block) {}
//	inline void prefetch_node (void *node) {__asm PREFETCH node}
	inline void prefetch_node (void *node) {}
	void push_spillage(int si, Node *list, int num, int bunches_pushed);
	enum {MAX_BUNCH_COUNT = 128};//TUNE, min 1, should be a power of 2
	enum {MAX_BUNCH_SIZE = 8192};//TUNE, must be <= block_size, should be >= max_size
	int bunch_size[num_sizes];
//	int bunch_count[num_sizes];
	int bunch_count_shift[num_sizes];
	inline void bunch_push(int si, Bunch *bunch);

	//5. blocks:
	enum {block_size_shift = 21};//TUNE, min log2(max_size)+1
	enum {block_size = 1 << block_size_shift};
	char *superblock;
	volatile long used_blocks;
	int total_blocks;
	volatile char *block_to_si_lookup; //block-index to size-index
	long allocate_block(int si) {
		if (used_blocks >= total_blocks) {
			return -1;
		}
		int block = InterlockedIncrement(&used_blocks) - 1;
		if (block >= total_blocks) {
			used_blocks = total_blocks;
			return -1;
		}
		if (block == total_blocks -1) {
			message("ThreadHeap3 - exhausted all blocks");
		}
		else if (block == total_blocks - num_sizes){
			message("ThreadHeap3 - almost exhausted all blocks");
		}
		block_to_si_lookup[block] = si;
		prefetch_block(block+1);
		if (Settings::Heap::bEnableMessages) message("%d byte block #%d allocated to pool for size %d at time %d", block_size, block, si, (get_time_ms() / 1000));
		return block;
	}
	enum {ADDRESS_TO_SI__NOT_IN_SUPERBLOCK =-1};
	enum {ADDRESS_TO_SI__INVALID_BLOCK     =-2};
	enum {ADDRESS_TO_SI__MISALIGNED        =-3};
	int address_to_si(void *mem) {
		unsigned int offset = int(mem) - int(superblock);
		unsigned int block = offset >> block_size_shift;
		if (block >= total_blocks) return ADDRESS_TO_SI__NOT_IN_SUPERBLOCK;
		if (block >= used_blocks) {
			MemoryBarrier();
			if (block >= used_blocks) {
				return ADDRESS_TO_SI__INVALID_BLOCK;
			}
		}
		int si = block_to_si_lookup[block];
#ifdef SIMPLEHEAP_CHECK_ALIGNMENT
#ifdef SIMPLEHEAP_FASTBLOCKS
		if (offset & (sizes[si]-1)) return ADDRESS_TO_SI__MISALIGNED;
#else
		if ((offset&(block_size-1)) % sizes[si]) return ADDRESS_TO_SI__MISALIGNED;
#endif
#endif
		return si;
	}
	bool is_in_heap_fast(void *ptr) {
		unsigned int offset = int(ptr) - int(superblock);
		return offset < ((unsigned int)total_blocks) << block_size_shift;
	}
	size_t get_size(void *mem) {
		int si = address_to_si(mem);
		if (si >= 0) return sizes[si];
#ifdef SIMPLEHEAP_USE_MSIZE
		if (si == ADDRESS_TO_SI__NOT_IN_SUPERBLOCK) return ::_msize(mem);
#endif
		error("ThreadHeap3::get_size - invalid parameter");
		return -1;
	}

	//4. spillage
	//rarely used
	//recieves from blocks and from PerThreads of dieing threads
	//sends to bunch-list
	struct Spill {
		CRITICAL_SECTION cs;
		int num_nodes;
		Node *free_list;
		int stats_bunches_popped;
		void clear() {num_nodes = 0; free_list = NULL;}
		Spill() {
			InitializeCriticalSectionAndSpinCount(&cs, 4000);
			clear();
			stats_bunches_popped = 0;
		}
	};
	Spill spills[num_sizes];
	void push_spillage(int si, Node *list, int num, int bunches_popped) {
		if (!num && !bunches_popped) return;
		Spill &spill = spills[si];
		EnterCriticalSection(&spill.cs);
		if (Settings::Heap::bEnableMessages) message("ThreadHeap3::push_spill si=%d, num=%d+%d, bunches=%d+%d", si, num, spill.num_nodes, bunches_popped, spill.stats_bunches_popped);
		Bunch *rv = NULL;
		int thresh = 1 << bunch_count_shift[si];
		for (int i = 0; i < num; i++) {
			Node *tmp = list;
			list = list->next;
			tmp->next = spill.free_list;
			spill.free_list = tmp;
			if (++spill.num_nodes == thresh) {
				bunch_push(si, (Bunch*)spill.free_list);
				bunches_popped--;
				spill.clear();
			}
		}
		spill.stats_bunches_popped += bunches_popped;
		LeaveCriticalSection(&spill.cs);
	}

	//3. partial-blocks
	/*void *volatile partial_blocks[num_sizes];
	static bool is_same_block(int address1, int address2) {return !((address1 ^ address2) >> block_size_shift);}
	Node *allocate_from_partial_block(int si, int size) {
		while (1) {
			char *rv = (char*)partial_blocks[si];
			if (!rv) continue;
			char *next = rv + size;
			//!!!!!!!!!
			if (!is_same_block((int)rv, (int)next)) next = NULL;
			char *actual = (char*)::InterlockedCompareExchangePointer(&partial_blocks[si], next, rv);
			if (actual != rv) continue;
			if (next) return (Node*)rv;
			//to do: add spill code for left-overs
			rv  = allocate_block(si);
			if (rv) partial_blocks[si] = rv + size;
			return (Node*)rv;
		}
	}*/
	const unsigned long PARTIAL_BLOCK_FROZEN = 1;//must be an odd number
	const unsigned long PARTIAL_BLOCK_EXHAUSTED = 3;//must be an odd number
	volatile unsigned long partial_blocks[num_sizes];//offsets in to superblock
	int make_nodes_from_raw(int si, char *mem, int size_left) {
		int size = sizes[si];
		size_left -= size;
		if (size_left < 0) return 0;
		int num = 1;
		while (size_left >= size) {
			char *next = mem+size;
			((Node*)mem)->next = (Node*)next;
			mem = next;
			num++;
			size_left -= size;
		}
		return num;
	}
	Bunch *allocate_from_partial_block(int si, unsigned long size) {
		while (1) {
			unsigned long before = partial_blocks[si];
			if (before & 1) {
				if (before == PARTIAL_BLOCK_FROZEN) continue;
				if (before == PARTIAL_BLOCK_EXHAUSTED) return NULL;
				error("ThreadHeap3::allocate_from_partial_block - impossible");
			}
			unsigned long after = before + size;
			unsigned long boundary = before | (block_size-1);
			if (after <= boundary) {//still in same block after allocation
				if (InterlockedCompareExchange((volatile long *)&partial_blocks[si], after, before) != before)
					continue;
				char *rv = &superblock[before];
				make_nodes_from_raw(si, rv, size);
				return (Bunch*)rv;
			}
			//exhausted that block, a new one will need to be allocated if possible
			//first, make sure the change is atomic
			if (InterlockedCompareExchange((volatile long *)&partial_blocks[si], PARTIAL_BLOCK_FROZEN, before) != before)
				continue;
			long new_block = allocate_block(si);
			int leftovers = boundary+1 - before;
			int spills = make_nodes_from_raw(si, &superblock[before], leftovers);
			if (new_block < 0) {//all blocks exhausted
				partial_blocks[si] = PARTIAL_BLOCK_EXHAUSTED;
				if (leftovers == size) return (Bunch*)&superblock[before];//one last bunch from the old block
				push_spillage(si, (Node*)&superblock[before], spills, 0);
				return NULL;
			}
			else {
				new_block <<= block_size_shift;
				if (leftovers == size) {//one last bunch from the old block
					partial_blocks[si] = new_block;
					return (Bunch*)&superblock[before];
				}
				partial_blocks[si] = new_block + size;
				push_spillage(si, (Node*)&superblock[before], spills, 0);
				make_nodes_from_raw(si, &superblock[new_block], size);
				return (Bunch*)&superblock[new_block];
			}
		}
	}

	//2. bunch-lists:
	struct BunchList {
		CRITICAL_SECTION cs;
		Bunch *head;
		char _padding[64-sizeof(CRITICAL_SECTION) - sizeof(Bunch*)];
		BunchList() {
			InitializeCriticalSectionAndSpinCount(&cs, 12000);
			head = NULL;
		}
		void push ( Bunch * node ) {
			EnterCriticalSection(&cs);
			((Bunch*)node)->next_bunch = head;
			head = ((Bunch*)node);
			LeaveCriticalSection(&cs);
		}
		Bunch *pop ( ) {
			EnterCriticalSection(&cs);
			Bunch *rv = (Bunch*)head;
			if (head) {
				head = head->next_bunch;
				prefetch_node(head);
			}
			LeaveCriticalSection(&cs);
			return rv;
		}
	};
	struct BunchList2 {
		enum {NULL2 = 0x00000001};//reserved pointer value
		Bunch *volatile head;
		char _padding[64-sizeof(Bunch*)];
		BunchList2() {
			head = NULL;
		}
		void push ( Bunch * bunch ) {
			while (1) {
				Bunch *next = head;
				if (next == ((void*)NULL2)) continue;
				bunch->next_bunch = next;
				if (::InterlockedCompareExchangePointer( (void**)&head, bunch, next ) == next) break;
				//else message("ThreadHeap3::BunchList2::push() - failed to push, retrying");
			}
		}
		Bunch *pop ( ) {
			while (1) {
				Bunch *rv = head;
				if (rv == NULL) return NULL;
				if (rv == ((void*)NULL2)) continue;
				if (::InterlockedCompareExchangePointer( (void**)&head, ((void*)NULL2), rv ) != rv)//locks the list
					continue;
				Bunch *next = rv->next_bunch;
				prefetch_node(next);
				head = next;//unlocks the list
				return rv;
			}
		}
	};
	typedef BunchList BunchList_t;
	BunchList_t *free_bunch_list;//[num_sizes];
/*	void check_list_length(void *ptr, int expected) {
		Node *mem = (Node*)ptr;
		int len = 0;
		while (mem) {mem = mem->next; len++;}
		if (len != expected) message("check_list_length %d != %d", len, expected);
	}*/
	Bunch *bunch_pop(int si) {
		BunchList_t &bunch_list = free_bunch_list[si];
		Bunch *rv = bunch_list.pop();
		if (rv) return rv;
		rv = allocate_from_partial_block(si, bunch_size[si]);
		if (!rv) return NULL;//bunch_list.pop();
		return rv;
	}
	inline void bunch_push(int si, Bunch *bunch) {
		free_bunch_list[si].push(bunch);
	}

	//1. per-thread lists:
	struct PerThread {
		struct PerSize {
			int nodes_left;
			Node *free_list;
			enum {NUM_MARKERS = 2};//possibly tunable, but 2 is likely optimal anyway
			Node *markers[NUM_MARKERS];
			int stats_bunches_popped, padding[3];

			PerSize() {
				nodes_left = 0;
				free_list = NULL;
				//for (int i = 0; i < NUM_MARKERS; i++) markers[i] = NULL;//unnecessary
				stats_bunches_popped = 0;
			}
		};
		PerSize per_size[num_sizes];
		char _padding[4096 - sizeof(PerSize) * num_sizes];

		PerThread() {
		}
		~PerThread() {
			for (int si = 0; si < num_sizes; si++) {
				PerSize &ps = per_size[si];
				push_spillage(si, ps.free_list, ps.nodes_left, ps.stats_bunches_popped);
			}
		}
		void *pop(int si) {
			PerSize &ps = per_size[si];
			if (ps.nodes_left) {
				ps.nodes_left -= 1;
				Node *rv = ps.free_list;
				ps.free_list = rv->next;
				return rv;
			}
			else {
				Node *rv = bunch_pop(si);
				if (!rv) return ::malloc(sizes[si]);
				ps.stats_bunches_popped++;
				ps.free_list = rv->next;
				ps.nodes_left = (1 << bunch_count_shift[si]) - 1;
				return rv;
			}
		}
		void push(Node *mem, int si) {
			PerSize &ps = per_size[si];
			int marker_index = ps.nodes_left >> bunch_count_shift[si];
			mem->next = ps.free_list;
			ps.free_list = mem;
			if (marker_index != PerSize::NUM_MARKERS) {
				ps.markers[marker_index] = ps.free_list;
				ps.nodes_left++;
			}
			else {
				bunch_push(si, (Bunch*)ps.markers[0]);
				ps.nodes_left -= (1 << bunch_count_shift[si]) - 1;
				for (int i = 0; i < PerSize::NUM_MARKERS-1; i++) ps.markers[i] = ps.markers[i+1];
				ps.markers[PerSize::NUM_MARKERS-1] = ps.free_list;
				ps.stats_bunches_popped--;
			}
		}
	};
	CRITICAL_SECTION all_threads_lock;
	std::set<PerThread*> all_threads;
	PerThread *get_PT() {
		PerThread *rv = (PerThread*) TlsGetValue(tls_slot);
		if (rv) return rv;
//		message("ThreadHeap3 - no PerThread for this thread, creating");
		//To Do: make sure this is aligned to like 4KB or at least 64B boundaries
		rv = new PerThread();
		EnterCriticalSection(&all_threads_lock);
		all_threads.insert(rv);
		LeaveCriticalSection(&all_threads_lock);
		if (!TlsSetValue(tls_slot, rv)) {
			error("ERROR: ThreadHeap3: failed to set TLS");
		}
		return rv;
	}
	void thread_destruction_callback() {
		PerThread *pt = (PerThread*) TlsGetValue(tls_slot);
		if (!pt) return;
		EnterCriticalSection(&all_threads_lock);
		all_threads.erase(pt);
		delete pt;
		LeaveCriticalSection(&all_threads_lock);
	}
	size_t get_bytes_used() {
		//not thread-safe, but all that can happen is we end up with the wrong total
		int totals[num_sizes] = {0};
		EnterCriticalSection(&all_threads_lock);
		for (std::set<PerThread*>::iterator it = all_threads.begin(); it != all_threads.end(); it++) {
			for (int si = 0; si < num_sizes; si++) {
				totals[si] -= (**it).per_size[si].nodes_left;
				totals[si] += (**it).per_size[si].stats_bunches_popped << bunch_count_shift[si];
			}
		}
		LeaveCriticalSection(&all_threads_lock);
		for (int si = 0; si < num_sizes; si++) {
			EnterCriticalSection(&spills[si].cs);
			totals[si] -= spills[si].num_nodes;
			totals[si] += spills[si].stats_bunches_popped << bunch_count_shift[si];
			LeaveCriticalSection(&spills[si].cs);
		}
		size_t sum = 0;
		for (int si = 0; si < num_sizes; si++) sum += totals[si] * sizes[si];
		return sum;
	}
	void *allocate(size_t size) {
		if (size > max_size) return ::malloc(size);
		int si = size_to_si(size);
		PerThread *pt = get_PT();
		return pt->pop(si);
	}
	void deallocate(void *ptr) {
		unsigned int offset = int(ptr) - int(superblock);
		unsigned int block = offset >> block_size_shift;
		if (block < total_blocks) {
			int si = block_to_si_lookup[block];
			PerThread *pt = get_PT();
			pt->push((Node*)ptr, si);
		}
		else {
			::free(ptr);
		}
	}
	void *reallocate(void *mem, size_t size) {
		if (!mem) return allocate(size);
		if (!size) {
			deallocate(mem);
			return NULL;
		}
		unsigned int offset = int(mem) - int(superblock);
		unsigned int block = offset >> block_size_shift;
		if (block < total_blocks) {
			int old_si = block_to_si_lookup[block];
			PerThread *pt = get_PT();
			if (size <= max_size) {//realloc from in-heap to in-heap
				int new_si = size_to_si(size);
				if (old_si == new_si) return mem;
				void *mem2 = pt->pop(new_si);
				int old_size = sizes[old_si];
				memcpy(mem2, mem, size < old_size ? size : old_size);
				pt->push((Node*)mem, old_si);
				return mem2;
			}
			else {//realloc from in-heap to out-of-heap
				void *mem2 = ::malloc(size);
				int old_size = sizes[old_si];
				memcpy(mem2, mem, size < old_size ? size : old_size);
				pt->push((Node*)mem, old_si);
				return mem2;
			}
		}
		else {
#ifdef SIMPLEHEAP_USE_MSIZE
			if (size <= max_size) {//realloc from out-of-heap to in-heap
				int new_si = size_to_si(size);
				PerThread *pt = get_PT();
				int old_size = _msize(mem);
				void *mem2 = pt->pop(new_si);
				memcpy(mem2, mem, size < old_size ? size : old_size);
				::free(mem);
				return mem2;
			}
#else
			if (false) ;
#endif
			else {//realloc from out-of-heap to out-of-heap
				return ::realloc(mem, size);
			}
		}
	}

	void init() {
		//basics
		tls_slot = TlsAlloc();
		if (tls_slot == TLS_OUT_OF_INDEXES) error("ERROR: ThreadHeap3 - out of TLS slots?");
		register_thread_destruction_callback(thread_destruction_callback);
		for (int i = 0; i < num_sizes; i++) if (sizes[i] % size_unit) {
			error("ThreadHeap3::init() - invalid size %d, terminating", sizes[i]);
		}
		for (int i = 0; i <= (max_size / size_unit); i++) {
			int s = i * size_unit;
			int x = -1;
			if (s <= max_size) for (int j = 0; j < num_sizes; j++) if (sizes[j] >= s) {x=j; break;}
			size_lookup[i] = x;
		}
#ifdef SIMPLEHEAP_FASTBLOCKS
		//sanity-check fastblocks requirements
		for (int si = 0; si < num_sizes; si++) {
			int x = sizes[si];
			if (x & (x-1)) error("ThreadHeap3 - bad internal configuration (size %d conflicts with fastblocks)", sizes[si]);
		}
#endif

		//4. blocks
		total_blocks = (Settings::Heap::iHeapSize << 20) >> block_size_shift;
		block_to_si_lookup = new char[total_blocks];
		for (int i = 0; i < total_blocks; i++) block_to_si_lookup[i] = -1;
		used_blocks = 0;
		if (!Settings::Master::bExperimentalStuff || !Settings::Experimental::iHeapMainBlockAddress) {
			//enum {ALIGN_SUPERBLOCK=4096};
			//superblock = (char*)malloc(total_blocks * block_size + ALIGN_SUPERBLOCK);
			//superblock = (char*)((int(malloc(total_subblocks * subblock_size + ALIGN_SUPERBLOCK))+ALIGN_SUPERBLOCK-1)&(0xFFffFFfful - (ALIGN_SUPERBLOCK-1)));
			//superblock = (char*)((int(superblock)+ALIGN_SUPERBLOCK-1)&(0xFFffFFfful - (ALIGN_SUPERBLOCK-1)));
			superblock = (char*)VirtualAlloc(NULL, total_blocks * block_size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
		}
		else {
			superblock = (char*)::VirtualAlloc((void*)Settings::Experimental::iHeapMainBlockAddress, total_blocks * block_size, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
			message("ThreadHeap3::init - VirtualAlloc: %08X %08x %X", superblock, Settings::Experimental::iHeapMainBlockAddress, total_blocks * block_size);
		}
		if (!superblock) error("ERROR: ThreadHeap3::init - main block allocation failed, shutting down");

		//3. partial blocks
		for (int si = 0; si < num_sizes; si++) partial_blocks[si] = allocate_block(si) << block_size_shift;

		//2. bunches
		free_bunch_list = new BunchList_t[num_sizes];
		for (int si = 0; si < num_sizes; si++) {
			int k = MAX_BUNCH_SIZE / sizes[si];
			if (k > MAX_BUNCH_COUNT) k = MAX_BUNCH_COUNT;
			int b;
			for (b = 0; b < 25; b++) {
				if ((2<<b) > k) {
					k = 1 << b;
					break;
				}
			}
			//if (k < 1) k = 1;
			if (b >= 16) error("ThreadHeap3::init - bunch_count calculations failed");
			bunch_count_shift[si] = b;
//			bunch_count[si] = 1 << b;
			bunch_size[si] = k * sizes[si];
		}
		//1. PerThread
		InitializeCriticalSectionAndSpinCount(&all_threads_lock, 4000);
	}
};


/*
Ideas for ThreadHeap4:

superblock:
	support non-contiguous resizeable memory
		instead of subtracting base address and rightshifting, just right-shift
		set block size to 16MB to force extra similarities to Oblivions native heap?
			really that's too large to be ideal for non-Oblivion purposes
			maybe 4 MB for non-Oblivion?

thread-locals:
	alternative bunch-markers:
		figure out marker index by array lookup?
			would allow non-power-of-2 sized bunches
			might be a tiny bit faster?
			would also make it easy to keep both first & last pointers for each bunch
				might make some other things easier?
				that would double the size of marker array
	size to allow thread-local pools
		pools sizes that vary over time...
			create problems - how to efficiently transfer them
				trees?
			but may still be worth considering...
		bigger is faster, but can waste a lot of memory
			memory CAN NOT be reclaimed assynchronously
				or not without defeating the purpose of thread-locals
			jemalloc decreases size as total threadcount increases
				which sounds possibly really smart
			jemalloc also increases size of specific bins with usage
				which... could be smart... but...
					I'd only do it if there was a way to assynchronously shrink it...
			SO... assynchronous access to thread-local pools:
				Interlocked* calls are too slow, too complex, on their own
				CSes are way too slow on their own
				lockless queues can't work on their own without a dedicated thread
				how about... 
					split local pool in to two
						one component is small pool, bunch=(4 or 8), lockless
						the other is a variable sized pool with CS locking?
							slow to access, but not too slow since there's zero contention
	sharing / locking
		the thread itself needs to be able to access thread-locals at very very high speeds
		but, lots of stuff should happen per thread even if it stops getting used
			freeing excess resources back to shared pools
				maybe only for large object pools?
			flushing AF-pools
				maybe use a lockless queue?
					maybe share between bin sizes?
			stats
				eh, errors are acceptable

bunch lists:
	make the bunch-list not a list anymore, more like a vector
		that would allow minimum block size of 1 pointer instead of 2 pointers
		would unfortunately require the allocation of 1KB? blocks internally for use 
			in the pseudo-vector
	allow the bunch-list to include irregular bunches?
		to allow more flexibility for other features?

fragmentation:
	possibly attempt to use only lowest available addresses
		not hard to optimize, even in thread-local:
			we know how many total nodes of a bin size have been created
			we likewise know how many have been pulled off the shared pool
			from this we can calculate the highest address that might be required
			so each free gets compared to a global variable
				if < then free as normal
				otherwise, free to an alternate pool or path
			dump alternate pool to shared alternate pool when it reaches a full bunch
		possibly rarely also do it to some portion of the bunch-list ?
			possibly have some system to track which portion that's been done for
			shared emptying-list:
				possibly some sort of semi-balanced tree instead?
				possibly tracked on a per-block basis?
				attempt to get a count on a per-block basis
					at least when there's a lot
					see if blocks can be given back to the OS

give memory back to the OS:
	requires a free count on each block?  how to implement?
		distinguish between regular blocks and those that we are emptying
			the regular ones
				we keep track of the number of nodes extracted from, but nothing more
			the emptying ones
				are chosen by the anti-fragmentation code
				go through all the nodes from the AF alternate pools
					move each one one to a per-block list, count nodes in per-block list

cache line sharing:
	make the application take care of it?
		by explicitly allocating cache-line aligned blocks for important data
	the alternative is multiple arenas
		I am NOT thrilled by this idea
		it's probably not that hard, but...

support large allocations in addition to small allocations?
	more work than I want to do now, maybe later
	but it might be worthwhile to support FastMM4 or somesuch for large allocations
	might have a function pointer / vtable on a per-large-block basis
		to handle switching between different heap types
		would also make it easier to support tiny allocations (smaller than a pointer)
	jemalloc guy claims that medium allocations need per-thread pools sometimes too
		too much memory to handle that way normally?

memory debuging / profiling helpers:
	too much work & performance cost, avoid
	tell them to use valgrind, or tcmalloc if they need performance

		

*/

