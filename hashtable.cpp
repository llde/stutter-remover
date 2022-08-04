
#include <set>
#include <map>
#include <list>
#include <string>
#include <vector>
#include "main.h"
#include "locks.h"
#include "hashtable.h"
#include "memory.h"
#include "struct_text.h"

#include "settings.h"

struct Hashtable {
	struct Entry
	{
		Entry	* next;
		UInt32	key;//or a string pointer, or something
		void	* data;
	};

	void **vtable;  //00
	/*
		vtable offset
		n/a		-4		RTTI of some kind
		n/a		vt_0	destructor (often not called through the vtable)
		int		vt_4	hash function (returns bucket index)
		bool	vt_8	(Key k1, Key k2)						{return k1 == k2;}
		?		vt_C	(Entry*, int bucketindex, void *data)	at least sometimes called right before insertion with the data to be inserted
		?		vt_10	(Entry*)								possibly called as an Entry is removed/deleted ?
		Entry *	vt_14	()			allocate node
		Entry *	vt_18	(Entry*)	deallocate node ; sometimes it deletes Entrys passed to it, other times it does something dumb involving DecodePointer
	*/
	UInt32	m_numBuckets;//04
	Entry	** m_buckets;//08
	UInt32	m_numItems;  //0C
	//char/bool unkown;//10

	UInt32 get_valid_size() {
		UInt32 rv = m_numBuckets;
		if (rv != UInt32(-1)) return rv;
		int tries = 0;
		while (true) {
			Sleep( 1 + (Settings::Hashtables::iHashtableResizeDelay>>2) );
			MemoryBarrier();
			rv = m_numBuckets;
			if (rv != UInt32(-1)) return rv;
			if (tries++ > 4) {
				message("Hashtable::get_valid_size() - taking too long (%d tries)", tries);
			}
		}
	};
};



bool is_hashtable_on_stack( Hashtable *ht ) {
	return (UInt32(ht) - UInt32(&ht)) < (1<<15);
}
namespace HT_Perf1 {
	static CRITICAL_SECTION htcs;
	enum {MEMSET_CALLER_TABLE_SIZE = 1<<16};
	struct PerMemset {
		void *memory;
		UInt32 caller;
	};
	volatile PerMemset *memset_callers;
	static UInt32 memset_address_hash(void *address) {
		UInt32 index = (UInt32)address;
		index += index << 3;
		index ^= index >> 16;
		index *= 0x9C145B25;
		index ^= index >> 16;
		index *= 0xA749E665;
		index ^= index >> 16;
		return index;
	}
	static void *__cdecl fake_memset(void *dest, int value, size_t size) {
		volatile PerMemset &pm = memset_callers[memset_address_hash(dest) & (MEMSET_CALLER_TABLE_SIZE-1)];
		pm.memory = dest;
		pm.caller = (UInt32)((&dest)[-1]);
		return ::memset(dest, value, size);
	}
	static UInt32 get_memsetter(void *memory) {
		volatile PerMemset &pm = memset_callers[memset_address_hash(memory) & (MEMSET_CALLER_TABLE_SIZE-1)];
		if (pm.memory != memory) return NULL;
		return pm.caller;
	}

	struct DeletedHashtableData {
		//when a hashtable is deleted, it is associated with one of these on a per (size,memsetter) basis
		UInt32 accesses;
		UInt64 wastage;
		UInt32 memsetter;
		UInt32 size;
		UInt32 peak;
		UInt32 death_count;
		std::set<UInt32> vtables;
		DeletedHashtableData() : accesses(0), wastage(0), memsetter(0), size(0), peak(0), death_count(0) {}
	};
	std::map<std::pair<UInt32,UInt32>, DeletedHashtableData> dhtd_map;
	enum {MAX_HASHTABLES = 1<<15};
	struct HashtableFakeVTable;
	extern HashtableFakeVTable *hashtable_marked_vtable_space;
	struct HashtableFakeVTable {
		void *rtti;			//0 maybe?
		void *destructor;	//1
		void *others[6];	//2-7
		void **old_vtable;	//8
		UInt32 address;		//9
		UInt64 wastage;		//10-11
		UInt32 accesses;	//12
		UInt32 size;		//13
		UInt32 peak;		//14 - largest # of items seen in hashtables
		UInt32 memsetter;	//15
		//UInt32 padding[1];
		volatile bool allocate(Hashtable *ht);
		static UInt32 destruction_helper(Hashtable *ht, HashtableFakeVTable *fv) {
			//int slot = (UInt32(fv) - UInt32(hashtable_marked_vtable_space)) / sizeof(HashtableFakeVTable);
			UInt32 odtor = UInt32(fv->old_vtable[0]);

			::EnterCriticalSection(&htcs);
			if (fv->destructor) {
				DeletedHashtableData &dhtd = dhtd_map[std::pair<UInt32,UInt32>(fv->memsetter, fv->size)];
				dhtd.accesses += fv->accesses;
				dhtd.wastage += fv->wastage;
				if (dhtd.peak < fv->peak) dhtd.peak = fv->peak;
				if (!dhtd.memsetter) dhtd.memsetter = fv->memsetter;
				if (!dhtd.size) dhtd.size = fv->size;
				dhtd.death_count++;
				dhtd.vtables.insert(UInt32(fv->old_vtable));
				fv->destructor = NULL;
			}
			::LeaveCriticalSection(&htcs);
			return odtor;
		}
		UInt32 _destruction_helper() {
			Hashtable *ht = (Hashtable*)this;
			HashtableFakeVTable *fv = (HashtableFakeVTable*)(UInt32(ht->vtable) - 4);
			return destruction_helper(ht, fv);
		}
	};
	static __declspec(naked) void fake_destructor() {
		__asm {
			push ecx
			call HashtableFakeVTable::_destruction_helper
			pop ecx
			jmp eax
		}
	}
	volatile bool HashtableFakeVTable::allocate(Hashtable *ht) {
		if (InterlockedCompareExchangePointer(&destructor, &fake_destructor, NULL)) {
			return false;
		}
		void **rvt = ht->vtable;
		rtti = rvt[-1];
		old_vtable = ht->vtable;
		for (int i = 0; i < 6; i++) others[i] = rvt[i+1];

		address = UInt32(ht);
		accesses = 0;
		wastage = 0;
		size = ht->m_numBuckets;
		peak = ht->m_numItems;
		memsetter = get_memsetter(ht->m_buckets);
		return true;
	}
	int kill_undead_hashtables() {//returns number slain
		int slain_undead = 0;
		EnterCriticalSection(&htcs);
		for (int i = 0; i < MAX_HASHTABLES; i++) {
			HashtableFakeVTable &d = hashtable_marked_vtable_space[i];
			void *dtor = (void*)d.destructor;
			if (!dtor) continue;
			Hashtable *ht = (Hashtable*)d.address;
			if (ht->vtable == &d.destructor) continue;
			if (HashtableFakeVTable::destruction_helper(NULL, &d) != NULL)
				slain_undead++;
		}
		LeaveCriticalSection(&htcs);
		return slain_undead;
	}
	HashtableFakeVTable *hashtable_marked_vtable_space = NULL;


	UInt32 last_print_time;
	void print_summary();
	void _notify(Hashtable *ht, char type) {
		HashtableFakeVTable *fv;
		if (UInt32(ht->vtable) - UInt32(hashtable_marked_vtable_space) < MAX_HASHTABLES * sizeof(HashtableFakeVTable)) {
			fv = (HashtableFakeVTable*)(ht->vtable-1);//already marked
		}
		else {
			if (!hashtable_marked_vtable_space) {
				error("hashtable stuff not properly initialized");
			}
			PerThread *pt = PerThread::get_PT();
			for (int tries = 0; true; tries++) {
				if (tries >= 10) {//how bloody full is the FVT space?
					message("HT_Perf1::_notify - attempt #%d at allocating an FVT slot failed (getting full?)", tries);
					if (tries == 10) {
						message("   maybe full of undead hashtables?  trying to kill undead hashtables now");
						int slain = kill_undead_hashtables();
						message("   %d undead hashtables were killed", slain);
					}
					if (tries > 99) error("HT_Perf1::_notify - FVT slots too full, aborting");
				}
				int index = pt->internal_rng.raw16() & (MAX_HASHTABLES-1);
				if (hashtable_marked_vtable_space[index].allocate(ht)) {
					fv = &hashtable_marked_vtable_space[index];
					ht->vtable = (void**)&fv->destructor;
					break;
				}
			}
		}

		fv->accesses++;
		if (ht->m_numItems > ht->m_numBuckets) fv->wastage += (ht->m_numItems << 4) / ht->m_numBuckets - 16;
		if (ht->m_numItems > fv->peak) fv->peak = ht->m_numItems;
		UInt32 time = get_time_ms();
		if (time - last_print_time > 13000) {
			last_print_time = time;
			print_summary();
		}
	}
	void notify_int( Hashtable *ht ) {
		_notify(ht, 'i');
	}
	void notify_string( Hashtable *ht ) {
		_notify(ht, 's');
	}
	void print_summary() {
		EnterCriticalSection(&htcs);
		message("HT_Perf1::print_summary (time=%ds)", get_time_ms()/1000);
		if (true) {
			message("  alive hashtables");
			enum {MAX_ALIVE = 50};
			int undead_hashtables = 0;
			std::map<double, HashtableFakeVTable*> sorted;
			for (int i = 0; i < MAX_HASHTABLES; i++) {
				HashtableFakeVTable &d = hashtable_marked_vtable_space[i];
				if (!d.destructor) continue;
				Hashtable *ht = (Hashtable*)d.address;
				if (ht->vtable != &d.destructor) {
					undead_hashtables++;
					continue;
				}
				double weight = d.accesses + d.wastage/16.0;
				sorted.insert(std::pair<double, HashtableFakeVTable*>(weight, &d));
			}
			std::map<double, HashtableFakeVTable*>::reverse_iterator it2 = sorted.rbegin();
			if (sorted.size() > MAX_ALIVE) message("  (%d entries, capped at %d)", sorted.size(), MAX_ALIVE);
			if (undead_hashtables) {
				message("  (%d undead hashtables found... trying to destroy them now)", undead_hashtables);
				int slain_undead = kill_undead_hashtables();
				message("  (%d of %d undead hashtables slain)", slain_undead, undead_hashtables);
			}
			int n = 0;
			for (;it2 != sorted.rend() && n < MAX_ALIVE; it2++,n++) {
				HashtableFakeVTable &d = *it2->second;
				if (!d.destructor) continue;
				char buffy[800];
				char *p = buffy;
				p += sprintf(p, "   %9d%9.0f  (%4.1f)    %6d/%6d     ms:0x%08X  addr:0x%08X  vt:0x%08X", 
					d.accesses, double(d.wastage>>4), double(d.wastage)/d.accesses / 16.0, d.peak, d.size, d.memsetter, d.address, d.old_vtable
				);
				message(buffy);
			}
		}
		if (true) {
			message("  dead hashtables");
			enum {MAX_DEAD = 50};
			std::map<double, DeletedHashtableData*> sorted;
			//std::map<std::pair<UInt32,UInt32>, DeletedHashtableData> dhtd_map;
			std::map<std::pair<UInt32,UInt32>, DeletedHashtableData>::iterator it1 = dhtd_map.begin();
			for (;it1 != dhtd_map.end(); it1++) {
				double weight = it1->second.accesses + it1->second.wastage/16.0 + it1->second.death_count * 0.5 * it1->second.size;
				sorted.insert(std::pair<double,DeletedHashtableData*>(weight, &it1->second));
			}
			std::map<double, DeletedHashtableData*>::reverse_iterator it2 = sorted.rbegin();
			if (sorted.size() > MAX_DEAD) message("  (%d entries, capped at %d)", sorted.size(), MAX_DEAD);
			int n = 0;
			for (;it2 != sorted.rend() && n < MAX_DEAD; it2++,n++) {
				DeletedHashtableData &d = *it2->second;
				char buffy[800];
				char *p = buffy;
				p += sprintf(p, "   %9d%9.0f  (%4.1f)  %6d/%6d ms:0x%08X  %6d (%4.1f)", 
					d.accesses, double(d.wastage>>4), double(d.wastage)/d.accesses/16, d.peak, d.size, d.memsetter, d.death_count, 
					d.size * d.death_count * 0.5 / (d.wastage / 16.0 +  d.accesses)
				);
				for (std::set<UInt32>::iterator it = d.vtables.begin(); it != d.vtables.end(); it++) if (*it) p += sprintf(p, " vt:0x%08x", *it);
				message(buffy);
			}
		}
		message("end HT_Perf1::print_summary");
		LeaveCriticalSection(&htcs);
	}
	void init() {
		if (Settings::Hashtables::bEnableMessages) message("HT_Perf1::init");
		InitializeCriticalSectionAndSpinCount(&htcs,1000);
		last_print_time = get_time_ms() - 15000;
		if (sizeof(HashtableFakeVTable) != 64) message("HashtableFakeVTable size not well-aligned (%d)", sizeof(HashtableFakeVTable));
		hashtable_marked_vtable_space = new HashtableFakeVTable[MAX_HASHTABLES];
		memset(hashtable_marked_vtable_space, 0, MAX_HASHTABLES * sizeof(HashtableFakeVTable));
		memset_callers = new PerMemset[MEMSET_CALLER_TABLE_SIZE];
		memset((void*)memset_callers, 0, MEMSET_CALLER_TABLE_SIZE * sizeof(PerMemset));
		if (Hook_memset1) {
			if (Settings::Hashtables::bEnableMessages && Settings::Hashtables::bEnableExtraMessages) message("hooking memset");
			WriteRelJump(Hook_memset1, (UInt32)&fake_memset);
			if (Hook_memset2) WriteRelJump(Hook_memset2, (UInt32)&fake_memset);
		}
	}
};
/*namespace HT_Perf2 {
	typedef std::map<void*, UInt32> known_memsetters_type;
	static CRITICAL_SECTION memsetters_access;
	static known_memsetters_type known_memsetters;
	static void *__cdecl fake_memset(void *dest, int value, size_t size) {
		//	if (size != 37*4 || value != 0) return ::memset(dest, value, size);
		::EnterCriticalSection(&memsetters_access);
		known_memsetters[dest] = (UInt32)((&dest)[-1]);
		::LeaveCriticalSection(&memsetters_access);
		return ::memset(dest, value, size);
	}
	static UInt32 get_memsetter(void *memblock) {
		::EnterCriticalSection(&memsetters_access);
		known_memsetters_type::const_iterator it = known_memsetters.find(memblock);
		UInt32 rv;
		if (it == known_memsetters.end()) rv = 0;
		else rv = it->second;
		::LeaveCriticalSection(&memsetters_access);
		return rv;
	}
	static void clear_memsetter(void *memblock) {
		::EnterCriticalSection(&memsetters_access);
		known_memsetters.erase(memblock);
		::LeaveCriticalSection(&memsetters_access);
	}
	static void begin_access_memsetters() { ::EnterCriticalSection(&memsetters_access); }
	static void end_access_memsetters() { ::LeaveCriticalSection(&memsetters_access); }


	std::map<Hashtable*, void *> hashtables_i;
	std::map<Hashtable*, void *> hashtables_s;
	Hashtable *last = NULL;
	int next_print_time = 0;
	void print_summary();
	void notify_int( Hashtable *ht ) {
		if (last == ht) return;
		last = ht;
		if (is_hashtable_on_stack(ht)) {
//			if (ht->m_numItems == 0) return;
//			message("stack-based integer hashtable %X has %d entries in %d buckets", ht, ht->m_numItems, ht->m_numBuckets);
			return;
		}
		if (int(timeGetTime() - base_time) - next_print_time >= 0) {
			next_print_time = timeGetTime() - base_time + 30000;
			EnterCriticalSection(&htcs);
			print_summary();
			LeaveCriticalSection(&htcs);
		}
		hashtables_i.insert(std::pair<Hashtable*, void*>(ht, ht->vtable));
	}
	void notify_string( Hashtable *ht ) {
		if (last == ht) return;
		last = ht;
		if (is_hashtable_on_stack(ht)) {
//			if (ht->m_numItems == 0) return;
//			message("stack-based string hashtable %X has %d entries in %d buckets", ht, ht->m_numItems, ht->m_numBuckets);
			return;
		}
		if (int(timeGetTime() - base_time) - next_print_time >= 0) {
			next_print_time = timeGetTime() - base_time + 30000;
			EnterCriticalSection(&htcs);
			print_summary();
			LeaveCriticalSection(&htcs);
		}
		hashtables_s.insert(std::pair<Hashtable*, void*>(ht, ht->vtable));
	}
	int find_chain_length ( Hashtable::Entry *e ) {
		int len = 0;
		while (e) {
			e = e->next;
			len++;
		}
		return len;
	}
	void print_summary() {
		begin_access_memsetters();
		std::map<Hashtable*, void *>::iterator it, it2;
		message("hashtables summary, time=%d", (timeGetTime() - base_time) / 1000);
		for (it = hashtables_i.begin(); it != hashtables_i.end(); it++) {
			Hashtable *ht = it->first;
			if (ht->vtable == it->second) {
				if (ht->m_numItems > ht->m_numBuckets || ht->m_numBuckets > 500) {
					int longest = 0;
					for (unsigned int i = 0; i < ht->m_numBuckets; i++) {
						int len = find_chain_length(ht->m_buckets[i]);
						if (len > longest) longest = len;
					}
					message("  i:%08X :%9d%5.1f longest: %3d  caller: 0x%08x  vtable:0x%08x", ht, ht->m_numBuckets, ht->m_numItems / (double) ht->m_numBuckets, longest, get_memsetter(ht->m_buckets), ht->vtable);
				}
			}
			else {
				//message("removing hashtable i:%08x", ht);
				it2 = it++;
				hashtables_i.erase(it2);
				it--;
			}
		}
		for (it = hashtables_s.begin(); it != hashtables_s.end(); it++) {
			Hashtable *ht = it->first;
			if (ht->vtable == it->second) {
				if (ht->m_numItems > ht->m_numBuckets || ht->m_numBuckets > 500) {
					int longest = 0;
					for (unsigned int i = 0; i < ht->m_numBuckets; i++) {
						int len = find_chain_length(ht->m_buckets[i]);
						if (len > longest) longest = len;
					}
					message("  s:%08X :%9d%5.1f longest: %3d  caller: 0x%08x  vtable:0x%08x", ht, ht->m_numBuckets, ht->m_numItems / (double) ht->m_numBuckets, longest, get_memsetter(ht->m_buckets), ht->vtable);
				}
			}
			else {
				//message("removing hashtable s:%08x", ht);
				it2 = it++;
				hashtables_s.erase(it2);
				it--;
			}
		}
		end_access_memsetters();
	}
	void init() {
		if (Hook_memset1) {
			if (Settings::Hashtables::bEnableMessages && Settings::Hashtables::bEnableExtraMessages) message("hooking memset");
			::InitializeCriticalSectionAndSpinCount(&memsetters_access, 2500);
			WriteRelJump(Hook_memset1, (UInt32)&fake_memset);
			if (Hook_memset2) WriteRelJump(Hook_memset2, (UInt32)&fake_memset);
		}
	}
}
namespace HT_Perf3 {
	// NOT exactly thread-safe, but nothing bad should happen I think

	struct HashtableData {
		char type;//int or string
		bool destroyed;
		UInt32 address;
		int size;
		int peak;
		UInt64 accesses;
		UInt64 wastage;
		UInt32 vtable;
		void *memset_block;
		UInt32 memset_caller;
		UInt32 creation_time;
		HashtableData(char type_, Hashtable *ht, UInt32 memset_caller_)
		:
			type(type_), 
			destroyed(false), 
			address((UInt32)ht),
			size(ht ? ht->m_numBuckets : 0), 
			peak(0),
			accesses(0), 
			wastage(0), 
			vtable(ht ? (UInt32)ht->vtable : 0),
			memset_block(ht ? ht->m_buckets : NULL),
			memset_caller(memset_caller_), 
			creation_time(timeGetTime())
		{
		}
	};
	HashtableData unidentified_stack_ht('?', NULL, NULL);

	std::map<UInt32, HashtableData*> stack_hts;//distinct if there is a distinct combination of 
	std::multimap<UInt32,HashtableData*> ht_masterlist;
		//sorted by m_buckets value (which is a pointer)
		//distinct if there is a distinct memset call, or no known memset caller & a distinct (size | vtable | access point)

	enum { TABLE_SIZE = 1 << 14 };
	struct TableEntry { void *memset_block; UInt32 memset_caller; void *ht_block; HashtableData *ht_data; };
	TableEntry volatile *table;
	static UInt32 hash_address_to_index(void *address) {
		UInt32 index = (UInt32)address;
		index ^= index >> 16;
		index += index << 3;
		index ^= index >> 8;
		index *= 0x9C145B25;
		index ^= index >> 16;
		index *= 0xA749E665;
		index ^= index >> 16;
		return index & (TABLE_SIZE - 1);
	}
	static void *__cdecl fake_memset(void *dest, int value, size_t size) {
		UInt32 index = hash_address_to_index(dest);
		if (table[index].ht_block == dest) {//block was re-used, quite possibly by a newer hashtable
			HashtableData *d = table[index].ht_data;
			table[index].ht_block = NULL;
			table[index].ht_data = NULL;
			if (d) d->destroyed = true;
		}
		table[index].memset_caller = (UInt32)((&dest)[-1]);
		table[index].memset_block = dest;
		return ::memset(dest, value, size);
	}
	static UInt32 get_memsetter(void *memblock) {
		UInt32 index = hash_address_to_index(memblock);
		if (table[index].memset_block == memblock) return table[index].memset_caller;
		return NULL;
	}

	UInt32 last_print_time;
	void print_summary();
	void _notify(Hashtable *ht, char type) {
		int overful = (ht->m_numBuckets << 2) / (ht->m_numItems|1) - 4;
		if (overful < 0) overful = 0;
		UInt32 index = hash_address_to_index(ht->m_buckets);
		TableEntry *entry = (TableEntry *)&table[index];
		HashtableData *data = NULL;
		if (entry->ht_block == ht->m_buckets) {
			//hashtable found in cache
			data = entry->ht_data;
		}
		else if (is_hashtable_on_stack(ht)) {
			if (entry->memset_block == ht->m_buckets) {
				std::multimap<UInt32, HashtableData*>::iterator it = stack_hts.find(entry->memset_caller);
				if (it != stack_hts.end()) {
					data = it->second;
				}
				else {
					data = new HashtableData(type, ht, entry->memset_caller);
					entry->ht_block = ht->m_buckets;
					entry->ht_data = data;
					entry->memset_block = NULL;
					entry->memset_caller = NULL;
					::EnterCriticalSection(&htcs);
					stack_hts.insert(std::pair<UInt32, HashtableData*>(data->memset_caller, data));
					ht_masterlist.insert(std::pair<UInt32, HashtableData*>(TABLE_SIZE, data));
					if (!(ht_masterlist.size() & 1023)) message("Hashtable Masterlist reached %d K entries", ht_masterlist.size() >> 10);
					::LeaveCriticalSection(&htcs);
				}
				entry->memset_block = NULL;
				entry->memset_caller = NULL;
			}
			else {
				//std::map<UInt32,HashtableData*>::iterator = stack_hts.find((UInt32);
				data = NULL;
			}
		}
		else {
			//unrecognized hashtable... 
			if (entry->memset_block == ht->m_buckets) {
				//it's a new hashtable
				data = new HashtableData(type, ht, entry->memset_caller);
				entry->memset_block = NULL;
				::EnterCriticalSection(&htcs);
				ht_masterlist.insert(std::pair<UInt32, HashtableData*>(index, data));
				if (!(ht_masterlist.size() & 1023)) message("Hashtable Masterlist reached %d K entries", ht_masterlist.size() >> 10);
				::LeaveCriticalSection(&htcs);
			}
			else {
				//can't tell if it is new... search old hashtables
				::EnterCriticalSection(&htcs);
				std::multimap<UInt32,HashtableData*>::iterator it = ht_masterlist.find(index);
				while (it != ht_masterlist.end() && it->first == index) {
					HashtableData *data2 = it->second;
					//if (data2->address == (UInt32)ht && data2->vtable == (UInt32)ht->vtable && data2->memset_block == ht->m_buckets && !data->destroyed) {
					if (false) {
						data = data2;//already have an entry for it
						break;
					}
					it++;
				}
				if (!data) {
					data = new HashtableData(type, ht, NULL);//it's new, and we don't know who the memset caller is
				}
				ht_masterlist.insert(std::pair<UInt32, HashtableData*>(index, data));
				if (!(ht_masterlist.size() & 1023)) message("Hashtable Masterlist reached %d K entries", ht_masterlist.size() >> 10);
				::LeaveCriticalSection(&htcs);
			}
			entry->ht_block = ht->m_buckets;
			entry->ht_data = data;
		}
		if (data) {
			data->accesses++;
			data->wastage += overful;
			if (data->peak < ht->m_numItems) data->peak = ht->m_numItems;
		}
		else {
			PerThread *pt = PerThread::get_PT();
			pt->ht_perf3_accesses ++;
			pt->ht_perf3_wastage += overful;
			if (pt->ht_perf3_accesses + pt->ht_perf3_wastage > (1<<10)) {
				::EnterCriticalSection(&htcs);
				unidentified_stack_ht.accesses += pt->ht_perf3_accesses;
				unidentified_stack_ht.wastage += pt->ht_perf3_wastage;
				::LeaveCriticalSection(&htcs);
				pt->ht_perf3_accesses = pt->ht_perf3_wastage = 0;
			}
		}
		if (UInt32(timeGetTime() - last_print_time) > 5000) {
			last_print_time = timeGetTime();
			print_summary();
		}
	}
	void notify_int(Hashtable *ht) {
		_notify(ht, 'i');
	}
	void notify_string(Hashtable *ht) {
		_notify(ht, 's');
	}
	void print_summary() {
		message("HT performance summary");
		return;
		std::multimap<double, HashtableData*> sorted;
		::EnterCriticalSection(&htcs);
		{
			std::multimap<UInt32, HashtableData*>::iterator it = ht_masterlist.begin();
			for (;it != ht_masterlist.end();it++) {
				HashtableData *data = it->second;
				double weight = data->wastage;
				sorted.insert(std::pair<double, HashtableData*>(weight, data));
			}
		}
		sorted.insert(std::pair<double,HashtableData*>(unidentified_stack_ht.wastage, &unidentified_stack_ht));
		std::multimap<double, HashtableData*>::reverse_iterator rit = sorted.rbegin();
		int n = 0;
		for (;rit != sorted.rend() && n < 50; rit++,n++) {
			HashtableData *data = rit->second;
			//double weight = rit->first;
			char buffy[800];
			char *p = buffy;
			double L10 = std::log(data->wastage * 0.25) / std::log(10.0);
			p += sprintf(p, "   %4.1f*10^%02d", std::exp(L10 - std::floor(L10)), (int)std::floor(L10));
			L10 = std::log(data->accesses * 1.0) / std::log(10.0);
			p += sprintf(p, "   %4.1f*10^%02d", std::exp(L10 - std::floor(L10)), (int)std::floor(L10));
			p += sprintf(p, "  %6d  %6d", data->peak, data->size);
			p += sprintf(p, "   %c: %08X  %08X  %08X", data->type, data->address, data->memset_caller, data->vtable );
			message(buffy);
		}
		::LeaveCriticalSection(&htcs);
	}
	void init() {
		last_print_time = timeGetTime() - 15000;
		table = new TableEntry[TABLE_SIZE];
		if (Hook_memset1) {
			if (Settings::Hashtables::bEnableMessages && Settings::Hashtables::bEnableExtraMessages) message("hooking memset");
			WriteRelJump(Hook_memset1, (UInt32)&fake_memset);
			if (Hook_memset2) WriteRelJump(Hook_memset2, (UInt32)&fake_memset);
		}
	}
}*/

#define HT_Perf HT_Perf1




int    __fastcall hashint_normal ( Hashtable *ht, int dummy, UInt32 key ) {
	return key % ht->m_numBuckets;
}
int __fastcall hashstring_normal ( Hashtable *ht, int dummy, const char *key ) {
	UInt32 hash = 0;
	for (signed char c; c = *key; key++) {
		hash = (hash << 5) + hash + c;
	}
	return hash % ht->m_numBuckets;
}
int    __fastcall hashint_log ( Hashtable *ht, int dummy, UInt32 key ) {
	HT_Perf::notify_int(ht);
	return key % ht->m_numBuckets;
}
int __fastcall hashstring_log ( Hashtable *ht, int dummy, const char *key ) {
	HT_Perf::notify_string(ht);
	UInt32 hash = 0;
	for (signed char c; c = *key; key++) {
		hash = (hash << 5) + hash + c;
	}
	return hash % ht->m_numBuckets;
}


int hashfunc_normal_int ( UInt32 key, UInt32 buckets ) {
	return key % buckets;
}
int hashfunc_normal_string ( UInt32 key_, UInt32 buckets ) {
	char *key = (char*)key_;
	UInt32 hash = 0;
	for (signed char c; c = *key; key++) {
		hash = (hash << 5) + hash + c;
	}
	return hash % buckets;
}
int hashfunc_alt_int ( UInt32 key, UInt32 buckets ) {
	__asm {
		mov eax, key
		mov edx, 0xB234DD0B
		mul edx
		shl edx, 15
		xor edx, key
		add eax, edx
		mul buckets
		mov eax, edx
	}
}
int hashfunc_alt_string ( UInt32 key_, UInt32 buckets ) {
	signed char *key = (signed char*)key_;
	UInt32 hash = *key;
	if (hash) {
		hash ^= hash << 8;
		hash ^= hash >> 3;
		key++;
		for (signed char c; c = *key; key++) {
			UInt32 rot = ((hash << 19) | (hash >> 13));
			hash += c + 1;
			hash ^= rot;
		}
	}
	hash ^= (hash << 4);
	hash += (hash << 24);
	__asm {
		mov eax, hash
		mul buckets
		mov eax, edx
	}
}

int    __fastcall hashint_alternate ( Hashtable *ht, int dummy, UInt32 key ) {
	return hashfunc_alt_int(key, ht->m_numBuckets);
}
int __fastcall hashstring_alternate ( Hashtable *ht, int dummy, const char *key ) {
	return hashfunc_alt_string(UInt32(key), ht->m_numBuckets);
}

bool is_prime(int value) {
	static UInt32 small_primes = 
		(1 << 2) + (1 << 3) + (1 << 5) + (1 << 7) + (1 <<11) + (1 <<13) + 
		(1 <<17) + (1 <<19) + (1 <<23) + (1 <<29);
	if (value <= 0) error("invalid parameter to is_prime (%08X)", value);
	if (value < 32) return ((small_primes >> value) & 1) ? true : false;
	if (!(value % 3)) return false;
	if (!(value % 5)) return false;
	if (!(value % 7)) return false;
	if (!(value % 11)) return false;
	else if (!(value % 13)) return false;
	else if (value < 17 * 17) return true;
	else if (!(value % 17)) return false;
	else if (!(value % 19)) return false;
	else if (!(value % 23)) return false;
	else if (value < 29 * 29) return true;
	else if (!(value % 29)) return false;
	else if (!(value % 31)) return false;
	else if (!(value % 37)) return false;
	else if (value < 41 * 41) return true;
	else if (!(value % 41)) return false;
	else if (!(value % 43)) return false;
	else if (!(value % 47)) return false;
	else if (!(value % 53)) return false;
	//don't really care much about large divisors for this purpose, ignore them
	return true;
}
int find_next_prime ( int size ) {
	size |= 1;
	if (size > (3<<17)) return size;
	while (!is_prime(size)) size += 2;
	return size;
}
void resize_hashtable ( Hashtable *ht, int old_size, int (*hashfunc)(UInt32, UInt32) ) {
	using namespace Settings::Hashtables;
	if (bEnableMessages) message("thinking about resizing HT %X with %d entries from %d buckets", ht, ht->m_numItems, old_size);
	if (PerThread::get_PT()->thread_number != 1) {
		if (bEnableMessages) message("  not in main thread, aborting");
		return;
	}
	if (old_size == -1) {
		if (bEnableMessages) message("  already locked, aborting");
		return;
	}
//	if (old_size != ht->m_numBuckets) {
//		if (bEnableMessages) message(" size changed, aborting");
//		return;
//	}
	if (InterlockedCompareExchange((long*)&ht->m_numBuckets, UInt32(-1), old_size) != old_size) {
		if (bEnableMessages) message("  locking failed, aborting");
		return;
	}
	int new_size = find_next_prime((old_size << iHashtableResizeScale2) + (1<<iHashtableResizeScale2)/2 + 1);
	if (new_size <= old_size) {
		if (bEnableMessages) message("  is iHashtableScaleBits2 misconfigured? aborting (%d, %d)", new_size, old_size);
		return;
	}
	if (bEnableMessages) message("  actually resizing HT %X with %d entries from %d buckets to %d buckets", ht, ht->m_numItems, old_size, new_size);
	if (iHashtableResizeDelay >= 0)
		Sleep(iHashtableResizeDelay);
	Hashtable::Entry **old_buckets = ht->m_buckets;
	Hashtable::Entry **new_buckets = (Hashtable::Entry **)allocate_from_memoryheap(new_size * 4);
	memset(new_buckets, 0, new_size * 4);
	ht->m_buckets = new_buckets;
	for (int i = 0; i < old_size; i++) {
		Hashtable::Entry *e = old_buckets[i];
		while (e) {
			Hashtable::Entry *ne = e->next;
			int k = hashfunc(e->key, new_size);
			e->next = new_buckets[k];
			new_buckets[k] = e;
			e = ne;
		}
	}
	MemoryBarrier();
	free_to_memoryheap(old_buckets);
	::InterlockedExchange((long*)&ht->m_numBuckets, new_size);
	ht->m_numBuckets = new_size;
}
int    __fastcall hashint_grow ( Hashtable *ht, int dummy, UInt32 key ) {
	UInt32 m = ht->m_numBuckets;
	while (m == UInt32(-1)) {
		message("thread %X blocking on resizing hashtable %X", PerThread::get_PT()->thread_number, ht);
		::Sleep(1+(Settings::Hashtables::iHashtableResizeDelay>>2));
		MemoryBarrier();
		m = ht->m_numBuckets;
	}
	if ((ht->m_numItems > (m<<Settings::Hashtables::iHashtableResizeScale1)) && !is_hashtable_on_stack(ht)) {
		if (PerThread::get_PT()->thread_number == 1) {
			resize_hashtable(ht, m, hashfunc_normal_int);
			m = ht->get_valid_size();
		}
	}
	return key % m;
}
int __fastcall hashstring_grow ( Hashtable *ht, int dummy, const char *key ) {
	UInt32 hash = 0;
	for (signed char c; c = *key; key++) {
		hash = (hash << 5) + hash + c;
	}

	UInt32 m = ht->m_numBuckets;
	while (m == UInt32(-1)) {
		if (Settings::Hashtables::iHashtableResizeDelay > 1)
			Sleep( 1 + (Settings::Hashtables::iHashtableResizeDelay>>2) );
		else Sleep(1);
		MemoryBarrier();
		m = ht->m_numBuckets;
	}
	if ((ht->m_numItems > (m<<Settings::Hashtables::iHashtableResizeScale1)) && !is_hashtable_on_stack(ht)) {
		if (PerThread::get_PT()->thread_number == 1) {
			resize_hashtable(ht, m, hashfunc_normal_string);
			m = ht->get_valid_size();
		}
	}

	return hash % m;
}
int    __fastcall hashint_grow_log ( Hashtable *ht, int dummy, UInt32 key ) {
	HT_Perf::notify_int(ht);
	UInt32 m = ht->m_numBuckets;
	while (m == UInt32(-1)) {
		if (Settings::Hashtables::bEnableMessages) message("thread %X blocking on resizing hashtable %X", PerThread::get_PT()->thread_number, ht);
		::Sleep(1+(Settings::Hashtables::iHashtableResizeDelay>>2));
		MemoryBarrier();
		m = ht->m_numBuckets;
	}
	if ((ht->m_numItems > (m<<Settings::Hashtables::iHashtableResizeScale1)) && !is_hashtable_on_stack(ht)) {
		if (PerThread::get_PT()->thread_number == 1) {
			resize_hashtable(ht, m, hashfunc_normal_int);
			m = ht->get_valid_size();
		}
	}
	return key % m;
}
int __fastcall hashstring_grow_log ( Hashtable *ht, int dummy, const char *key ) {
	HT_Perf::notify_string(ht);
	UInt32 hash = 0;
	for (signed char c; c = *key; key++) {
		hash = (hash << 5) + hash + c;
	}

	UInt32 m = ht->m_numBuckets;
	while (m == UInt32(-1)) {
		if (Settings::Hashtables::bEnableMessages) message("thread %X blocking on resizing hashtable %X", PerThread::get_PT()->thread_number, ht);
		if (Settings::Hashtables::iHashtableResizeDelay > 1)
			Sleep( 1 + (Settings::Hashtables::iHashtableResizeDelay>>2) );
		else Sleep(1);
		MemoryBarrier();
		m = ht->m_numBuckets;
	}
	if ((ht->m_numItems > (m<<Settings::Hashtables::iHashtableResizeScale1)) && !is_hashtable_on_stack(ht)) {
		if (PerThread::get_PT()->thread_number == 1) {
			resize_hashtable(ht, m, hashfunc_normal_string);
			m = ht->get_valid_size();
		}
	}

	return hash % m;
}


void initialize_hashtable_hooks() {
	using namespace Settings::Hashtables;
	if (bAllowDynamicResizing && (Hook_Hashtable_IntToIndex1 || Hook_Hashtable_StringToIndex1)) {
		if (bEnableProfiling) {
			message("dynamic-size hashtables with profiling");
			if (Hook_Hashtable_IntToIndex1)    WriteRelJump(Hook_Hashtable_IntToIndex1, UInt32(hashint_grow_log));
			if (Hook_Hashtable_IntToIndex2)    WriteRelJump(Hook_Hashtable_IntToIndex2, UInt32(hashint_grow_log));
			if (Hook_Hashtable_IntToIndex3)    WriteRelJump(Hook_Hashtable_IntToIndex3, UInt32(hashint_grow_log));
			if (Hook_Hashtable_IntToIndex4)    WriteRelJump(Hook_Hashtable_IntToIndex4, UInt32(hashint_grow_log));
			if (Hook_Hashtable_StringToIndex1) WriteRelJump(Hook_Hashtable_StringToIndex1, UInt32(hashstring_grow_log));
			if (Hook_Hashtable_StringToIndex2) WriteRelJump(Hook_Hashtable_StringToIndex2, UInt32(hashstring_grow_log));
			if (Hook_Hashtable_StringToIndex3) WriteRelJump(Hook_Hashtable_StringToIndex3, UInt32(hashstring_grow_log));
			if (Hook_Hashtable_StringToIndex4) WriteRelJump(Hook_Hashtable_StringToIndex4, UInt32(hashstring_grow_log));
			HT_Perf::init();
		}
		else {
			message("dynamic-size hashtables");
			if (Hook_Hashtable_IntToIndex1)    WriteRelJump(Hook_Hashtable_IntToIndex1, UInt32(hashint_grow));
			if (Hook_Hashtable_IntToIndex2)    WriteRelJump(Hook_Hashtable_IntToIndex2, UInt32(hashint_grow));
			if (Hook_Hashtable_IntToIndex3)    WriteRelJump(Hook_Hashtable_IntToIndex3, UInt32(hashint_grow));
			if (Hook_Hashtable_IntToIndex4)    WriteRelJump(Hook_Hashtable_IntToIndex4, UInt32(hashint_grow));
			if (Hook_Hashtable_StringToIndex1) WriteRelJump(Hook_Hashtable_StringToIndex1, UInt32(hashstring_grow));
			if (Hook_Hashtable_StringToIndex2) WriteRelJump(Hook_Hashtable_StringToIndex2, UInt32(hashstring_grow));
			if (Hook_Hashtable_StringToIndex3) WriteRelJump(Hook_Hashtable_StringToIndex3, UInt32(hashstring_grow));
			if (Hook_Hashtable_StringToIndex4) WriteRelJump(Hook_Hashtable_StringToIndex4, UInt32(hashstring_grow));
			//if (Hook_Hashtable_CharToIndex1) WriteRelJump ( Hook_Hashtable_CharToIndex1, UInt32(hashchar_grow) );
		}
	}
	else
	if (bEnableProfiling && (Hook_Hashtable_IntToIndex1 || Hook_Hashtable_StringToIndex1)) {
		message("static-size hashtables with profiling");
		if (Hook_Hashtable_IntToIndex1)    WriteRelJump(Hook_Hashtable_IntToIndex1, UInt32(hashint_log));
		if (Hook_Hashtable_IntToIndex2)    WriteRelJump(Hook_Hashtable_IntToIndex2, UInt32(hashint_log));
		if (Hook_Hashtable_IntToIndex3)    WriteRelJump(Hook_Hashtable_IntToIndex3, UInt32(hashint_log));
		if (Hook_Hashtable_IntToIndex4)    WriteRelJump(Hook_Hashtable_IntToIndex4, UInt32(hashint_log));
		if (Hook_Hashtable_StringToIndex1) WriteRelJump(Hook_Hashtable_StringToIndex1, UInt32(hashstring_log));
		if (Hook_Hashtable_StringToIndex2) WriteRelJump(Hook_Hashtable_StringToIndex2, UInt32(hashstring_log));
		if (Hook_Hashtable_StringToIndex3) WriteRelJump(Hook_Hashtable_StringToIndex3, UInt32(hashstring_log));
		if (Hook_Hashtable_StringToIndex4) WriteRelJump(Hook_Hashtable_StringToIndex4, UInt32(hashstring_log));
		//if (Hook_Hashtable_CharToIndex1) WriteRelJump ( Hook_Hashtable_CharToIndex1, UInt32(hashchar_log) );

		HT_Perf::init();
	}
	if (bUseOverrides) {
		int num_hashtable_overrides = 0;
		if (bEnableMessages) message("using hashtable size overrides");
		TextSection *ts = config->get_section("OverrideList");
		if (ts) for (ts = ts->get_first_section(); ts; ts = ts->get_next_section()) {
			if (ts->get_name() == "Hashtable") {
				TextSection *addr = ts->get_section("SizeAddress");
				TextSection *size = ts->get_section("NewSize");
				TextSection *old = ts->get_section("OldSize");
				TextSection *bits = ts->get_section("WordBits");
				if (!addr) {
					message("Hashtable override ignored - missing SizeAddress");
					continue;
				}
				if (!size) {
					message("Hashtable override ignored - missing NewSize");
					continue;
				}
				if (!old) {
					message("Hashtable override ignored - missing OldSize");
					continue;
				}
				UInt32 a, s, o, b;
				a = addr->get_int();
				s = size->get_int();
				o = old->get_int();
				b = bits ? bits->get_int() : 32;
				if (!a) { message("Hashtable override ignored: SizeAddress = 0?"); continue; }
				if (!s) { message("Hashtable override ignored: NewSize = 0?"); continue; }
				if (!o) { message("Hashtable override ignored: OldSize = 0?"); continue; }
#if defined NEW_VEGAS
				TextSection *version = ts->get_section("Version");
				const char *v = version ? version->get_c_string() : "";
				if (strcmp(v, Hook_target_s)) continue;
#endif
				if (b != 8 && b != 16 && b != 32) { message("Hashtable override ignored: WordBits not in {8,16,32}"); continue; }
				UInt32 max = (256 << (b - 8)) - 1;
				if (s > max) { message("Hashtable override ignored: NewSize invalid for that number of bits (%d > %d)", s, max); continue; }
				if (bEnableMessages && bEnableExtraMessages) message("Hashtable override - passed safe checks, attempting unsafe check");
				UInt32 actual_old;
				if (b == 32) actual_old = *((UInt32*)a);
				if (b == 16) actual_old = *((UInt16*)a);
				if (b == 8) actual_old = *((UInt8 *)a);
				if (o != actual_old) {
					message("Hashtable override ignored - failed unsafe check (%d != %d)", o, actual_old);
					continue;
				}
				else if (bEnableMessages && bEnableExtraMessages) message("Hashtable override - passed unsafe check (%d == %d)", o, actual_old);
				if (bEnableMessages) message("Hashtable size override: %08X : %d -> %d", a, o, s);
				if (false);
				else if (b == 32) SafeWrite32(a, s);
				else if (b == 16) SafeWrite16(a, s);
				else if (b == 8) SafeWrite8(a, s);
				num_hashtable_overrides++;
			}
			else if (ts->get_name() == "HashtableEarly") {
				TextSection *addr = ts->get_section("Address");
				TextSection *size = ts->get_section("NewSize");
				TextSection *old = ts->get_section("OldSize");
				if (!addr) {
					message("HashtableEarly override ignored - missing Address");
					continue;
				}
				if (!size) {
					message("HashtableEarly override ignored - missing NewSize");
					continue;
				}
				if (!old) {
					message("HashtableEarly override ignored - missing OldSize");
					continue;
				}
				UInt32 a, s, o;
				a = addr->get_int();
				s = size->get_int();
				o = old->get_int();
				if (!a) { message("HashtableEarly override ignored: Address = 0?"); continue; }
				if (!s) { message("HashtableEarly override ignored: NewSize = 0?"); continue; }
				if (!o) { message("HashtableEarly override ignored: OldSize = 0?"); continue; }
#if defined NEW_VEGAS
				TextSection *version = ts->get_section("Version");
				const char *v = version ? version->get_c_string() : "";
				if (strcmp(v, Hook_target_s)) continue;
#endif
				Hashtable *ht = (Hashtable *)a;
				UInt32 actual_old = ht->m_numBuckets;
				if (o != actual_old) {
					message("HashtableEarly override ignored - failed unsafe check (%d != %d)", o, actual_old);
					continue;
				}
				if (!ht->m_buckets) {
					message("HashtableEarly override ignored - not yet constructed? (%x)", a);
					continue;
				}
				if (ht->m_numItems) {
					message("HashtableEarly size override ignored - not empty (%X)", a);
					continue;
				}
				if (bEnableMessages) message("HashtableEarly size override: %08X : %d -> %d", a, o, s);
				ht->m_numBuckets = s; ht->m_buckets = (Hashtable::Entry**)malloc(4 * s); memset(ht->m_buckets, 0, 4 * s);
				num_hashtable_overrides++;
			}
			else if (ts->get_name() == "HashtableEarlyIndirect") {
				TextSection *addr = ts->get_section("Address");
				TextSection *size = ts->get_section("NewSize");
				TextSection *old = ts->get_section("OldSize");
				if (!addr) {
					message("HashtableEarlyIndirect override ignored - missing Address");
					continue;
				}
				if (!size) {
					message("HashtableEarlyIndirect override ignored - missing NewSize");
					continue;
				}
				if (!old) {
					message("HashtableEarlyIndirect override ignored - missing OldSize");
					continue;
				}
				UInt32 a, s, o;
				a = addr->get_int();
				s = size->get_int();
				o = old->get_int();
				if (!a) { message("HashtableEarlyIndirect override ignored: Address = 0?"); continue; }
				if (!s) { message("HashtableEarlyIndirect override ignored: NewSize = 0?"); continue; }
				if (!o) { message("HashtableEarlyIndirect override ignored: OldSize = 0?"); continue; }
#if defined NEW_VEGAS
				TextSection *version = ts->get_section("Version");
				const char *v = version ? version->get_c_string() : "";
				if (strcmp(v, Hook_target_s)) continue;
#endif
				Hashtable *ht = *(Hashtable **)a;
				if (!ht) {
					message("HashtableEarlyIndirect override ignored - pointer is NULL (%x)", a);
					continue;
				}
				UInt32 actual_old = ht->m_numBuckets;
				if (o != actual_old) {
					message("HashtableEarlyIndirect override ignored - failed unsafe check (%d != %d)", o, actual_old);
					continue;
				}
				if (ht->m_numItems) {
					message("HashtableEarlyIndirect size override ignored - not empty (%X)", a);
					continue;
				}
				if (bEnableMessages) message("HashtableEarlyIndirect size override: %08X:%08X : %d -> %d", a, ht, o, s);
				ht->m_numBuckets = s; ht->m_buckets = (Hashtable::Entry**)malloc(4 * s); memset(ht->m_buckets, 0, 4 * s);
				num_hashtable_overrides++;
			}
		}
		if (num_hashtable_overrides) message("used %d hashtable size overrides", num_hashtable_overrides);
	}
}
