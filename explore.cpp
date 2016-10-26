#include <cmath>
#include <set>
#include <list>
#include <map>
#include <string>
#include <vector>
#include "main.h"
#include "locks.h"
#include "hashtable.h"
#include "memory.h"
#include "struct_text.h"
#include "settings.h"


#if defined USE_SCRIPT_EXTENDER && defined OBLIVION
	#include "obse/NiTypes.h"
	#include "obse/NiNodes.h"
	#include "obse/NiObjects.h"
	#include "obse/NiRenderer.h"


	//A7E610 = NiCullingProcess
	std::pair<UInt32, const char *> _node_vtables[] = {
	//#ifdef OBLIVION
		std::pair<UInt32,const char *>(0xA7E38C, "NiNode"),
		std::pair<UInt32,const char *>(0xA31BD4, "SceneGraph"),
		std::pair<UInt32,const char *>(0xA372EC, "BSTempNodeManager"),
		std::pair<UInt32,const char *>(0xA3738C, "BSTempNode"),
	//	std::pair<UInt32,const char *>(0x, "BSCellNode"),
		std::pair<UInt32,const char *>(0xA3F894, "BSClearZNode"),
		std::pair<UInt32,const char *>(0xA3F944, "BSFadeNode"),
		std::pair<UInt32,const char *>(0xA3FB54, "BSScissorNode"),
	//	std::pair<UInt32,const char *>(0x, "BSTimingNode"),
		std::pair<UInt32,const char *>(0xA64F5C, "BSFaceGenNiNode"),
		std::pair<UInt32,const char *>(0xA65854, "BSTreeNode"),
		std::pair<UInt32,const char *>(0xA45134, "NiBillboardNode"),
	//	std::pair<UInt32,const char *>(0x, "NiSwitchNode"),
		std::pair<UInt32,const char *>(0xA7F97C, "NiLODNode"),
	//	std::pair<UInt32,const char *>(0x, "NiBSLODNode"),
		std::pair<UInt32,const char *>(0xA814E4, "NiSortAdjustNode"),
		std::pair<UInt32,const char *>(0xA81A64, "NiBSPNode"),
		std::pair<UInt32,const char *>(0xA904B4, "ShadowSceneNode"),
	//#endif
		std::pair<UInt32,const char *>(0x0, NULL)
	};
	std::pair<UInt32, const char *> _nonnode_avobject_vtables[] = {
	//#ifdef OBLIVION
		std::pair<UInt32,const char *>(0xA7E0F4, "NiDynamicEffect"),
		std::pair<UInt32,const char *>(0xA7F3DC, "NiLight"),
		std::pair<UInt32,const char *>(0xA7F1DC, "NiDirectionalLight"),
		std::pair<UInt32,const char *>(0xA43044, "NiPointLight"),
		std::pair<UInt32,const char *>(0xA813D4, "NiSpotLight"),
		std::pair<UInt32,const char *>(0xA81C1C, "NiAmbientLight"),
		std::pair<UInt32,const char *>(0xA8120C, "NiTextureEffect"),
		std::pair<UInt32,const char *>(0xA7E4C4, "NiCamera"),
		std::pair<UInt32,const char *>(0xA94854, "BSCubeMapCamera"),
		std::pair<UInt32,const char *>(0xA80FAC, "NiScreenSpaceCamera"),
		std::pair<UInt32,const char *>(0xA7F8AC, "NiGeometry"),
		std::pair<UInt32,const char *>(0xA7EE0C, "NiLines"),
		std::pair<UInt32,const char *>(0xA7F7FC, "NiTriBasedGeom"),
		std::pair<UInt32,const char *>(0xA7ED5C, "NiTriShape"),
		std::pair<UInt32,const char *>(0xA3FC04, "BSScissorTriShape"),
		std::pair<UInt32,const char *>(0xA7E29C, "NiScreenElements"),
		std::pair<UInt32,const char *>(0xA80E34, "NiScreenGeometry"),
		std::pair<UInt32,const char *>(0xA9595C, "TallGrassTriShape"),
		std::pair<UInt32,const char *>(0xA7F27C, "NiTriStrips"),
		std::pair<UInt32,const char *>(0xA9587C, "TallGrassTriStrips"),
		std::pair<UInt32,const char *>(0xA85B0C, "NiParticles"),
		std::pair<UInt32,const char *>(0xA85B0C, "NiParticleSystem"),
		std::pair<UInt32,const char *>(0xA860D4, "NiMeshParticleSystem"),
		std::pair<UInt32,const char *>(0xA8183C, "NiParticleMeshes"),
	//	std::pair<UInt32,const char *>(0x, ""),
	//#endif
		std::pair<UInt32,const char *>(0x0, NULL)
	};
	std::map<UInt32, const char *> node_vtables;
	std::map<UInt32, const char *> nonnode_avobject_vtables;
	class do_on_startup {
	public:
		do_on_startup() {
			for (int i = 0; _node_vtables[i].second; i++) 
				node_vtables.insert(_node_vtables[i]);
			for (int i = 0; _nonnode_avobject_vtables[i].second; i++) 
				nonnode_avobject_vtables.insert(_nonnode_avobject_vtables[i]);
		}
	} _do_on_startup;
	struct _NiNode {
		UInt32 vtable;
		char blah1[0xAC - 4];
		NiTArray <_NiNode *> children; //0xAC
		NiTListBase <UInt32 *> effects;//0xBC
		NiSphere combinedBounds;       //0xCC
		bool is_node() const {
			return node_vtables.count(vtable) ? true : false;
		}
		int count_descendants() const {
			int r = children.numObjs;
			for (int i = 0; i < children.capacity; i++) {
				if (children.data[i] && children.data[i]->is_node()) {
					r += children.data[i]->count_descendants();
				}
			}
			return r;
		}
		int count_nonnode_descendants() const {
			int r = 0;
			for (int i = 0; i < children.capacity; i++) if (children.data[i]) {
				if (!children.data[i]->is_node()) r++;
				else r += children.data[i]->count_nonnode_descendants();
			}
			return r;
		}
	};
	void print_node_tree( _NiNode *node, int indentation = 0, bool no_recursion = false ) {
		enum {MAX_INDENT_LEVEL = 3};
		enum {INDENT_LEVEL_SPACES = 4};
		char buffy[MAX_INDENT_LEVEL * INDENT_LEVEL_SPACES + 1];
		if (indentation > MAX_INDENT_LEVEL) return;
		memset(buffy, ' ', indentation * INDENT_LEVEL_SPACES);
		buffy[indentation * INDENT_LEVEL_SPACES] = 0;
		if (node == NULL) {
			message("%sNULL", buffy);
			return;
		}
		std::map<UInt32, const char *>::iterator it;
		it = node_vtables.find(node->vtable);
		const char *name = NULL;
		bool is_node = false;
		if (it != node_vtables.end()) {
			name = it->second;
			is_node = true;
		}
		else {
			it = nonnode_avobject_vtables.find(node->vtable);
			if (it != nonnode_avobject_vtables.end()) name = it->second;
		}
		if (is_node) {
			message("%s%s (%d / %d / %d children / descendants / non-node-descendants)", buffy, name, node->children.numObjs, node->count_descendants(), node->count_nonnode_descendants());
			if (no_recursion) return;
			for (int i = 0; i < node->children.capacity; i++) {
				if (node->children.data[i]) 
					print_node_tree(node->children.data[i], indentation+1);
			}
		}
		else {
			if (name) message("%s%s", buffy, name);
			else message("%s%08X (unknown)", buffy, node->vtable);
		}
	}

	bool inside_primary_culling = false;
	UInt32 primary_culling_entry_time = 0;
	UInt32 last_culling_print_time = 0;

	void __fastcall NiNode__render_7073d0( _NiNode *_self, int nothing, NiCullingProcess *culling ) {
		static int recursion_level = 0;
		NiNode *self = (NiNode *)_self;
		if (self->m_flags & NiNode::kFlag_AppCulled) return;
		UInt64 start_time;
		if (recursion_level < 4) start_time = get_time_ticks();
		recursion_level++;
		__asm {
			mov eax, self
			mov ecx, culling
			push eax
			mov eax, [ecx]
			mov eax, [eax + 4]
			call eax
		}
		if (_self->vtable == 0xA904B4 && recursion_level != 2) {//the slow one
			for (int i = 0; i < _self->children.capacity; i++) {
				if (_self->children.data[i]) NiNode__render_7073d0(_self->children.data[i], 0, culling);
			}
	//		return;
		}
		recursion_level--;
		if (inside_primary_culling && recursion_level < Settings::Experimental::iPrintSceneGraphDepth) {
			int indentation = recursion_level;
			enum {MAX_INDENT_LEVEL = 10};
			enum {INDENT_LEVEL_SPACES = 4};
			char buffy[MAX_INDENT_LEVEL * INDENT_LEVEL_SPACES + 1];
			if (indentation > MAX_INDENT_LEVEL) error("");
			memset(buffy, ' ', indentation * INDENT_LEVEL_SPACES);
			buffy[indentation * INDENT_LEVEL_SPACES] = 0;
			double p = get_tick_period();
			std::map<UInt32, const char *>::iterator it;
			it = node_vtables.find(_self->vtable);
			const char *name = NULL;
			bool is_node = false;
			if (it != node_vtables.end()) {
				name = it->second;
				is_node = true;
			}
			else {
				it = nonnode_avobject_vtables.find(_self->vtable);
				if (it != nonnode_avobject_vtables.end()) name = it->second;
			} 
			int num_descendants = -1;
			if (is_node) ;//num_descendants = _self->count_descendants();
			UInt64 delta_time = get_time_ticks() - start_time;
			if (!inside_primary_culling) message("outside primary culling");
			if (is_node) {
				message("%srecursion_level=%d, time_used=%.2fms, object=\"%s\", descendants=%d", 
					buffy, recursion_level, delta_time * p * 1000.0, name, num_descendants);
			}
			else if (name) {
				message("%srecursion_level=%d, time_used=%.2fms, object=\"%s\"", 
					buffy, recursion_level, delta_time * p * 1000.0, name);
			}
			else {
				message("%srecursion_level=%d, time_used=%.2fms, object=%08X (unknown)", 
					buffy, recursion_level, delta_time * p * 1000.0, _self->vtable);
			}
		}
		return;
	}

	namespace CallPerf {
		enum {CP_MAX_THREADS = 32};
		enum {MAX_SLOTS = 512};
		CRITICAL_SECTION internal_cs;
		bool initialized = false;
		UInt32 last_print_time = 0;

		struct SlotData {
			const char *name;
			UInt32 flags;
			UInt32 callee_address;
			UInt32 caller_address;
			UInt32 return_address;
			UInt32 wrapper_address;
			void reset() {
				name = NULL;
				flags = 0;
				callee_address = 0;
				caller_address = 0;
				return_address = 0;
				wrapper_address = 0;
			}
		};
		SlotData slots[MAX_SLOTS];
		int num_slots = 0;
		int allocate_slot() {
			if (num_slots == MAX_SLOTS) error("CallPerf - maximum slots exceeded");
			int rv = num_slots++;
			slots[rv].reset();
			return rv;
		}


		enum {
			EVENT_TYPE_NONE = 0,
			EVENT_TYPE_ENTRANCE = 17,
			EVENT_TYPE_EXIT = 22
		};
		struct EventData {
			UInt64 ticks;
			UInt16 slot;
			UInt8 event_type;
			UInt8 padding;
		};
		struct ThreadSlotData {
			UInt64 total_ticks;
			UInt64 total_calls;
			UInt64 entrance_ticks;
			//UInt8 last_event_type;
			int recursion_level;
			int max_recursion;
			//std::vector<UInt64> last_entrance_ticks_stack;
			ThreadSlotData() : /*last_event_type(EVENT_TYPE_NONE),*/ total_ticks(0), total_calls(0), entrance_ticks(0), recursion_level(0), max_recursion(0) {}
		};
		struct ThreadData {
			enum {BUFFER_SIZE = 64};
			int num_events;
			EventData events[BUFFER_SIZE];
			ThreadSlotData tsd[MAX_SLOTS];
		};
		ThreadData thread_data[CP_MAX_THREADS];
		void flush_thread_buffer(int thread);
		void print_data();
		void __stdcall _do_nothing1(int) {return;}
		void __stdcall _do_nothing4(int,int,int,int) {return;}

		UInt32 __fastcall do_event_call(UInt32 *object, int slot) {

			UInt64 tick = get_time_ticks();
			int thread = ::PerThread::get_PT()->thread_number;
			if (thread >= CP_MAX_THREADS) error("too many threads for CallPerf");
			ThreadData &td = thread_data[thread];
			if (td.num_events == ThreadData::BUFFER_SIZE) flush_thread_buffer(thread);
			EventData &e = td.events[td.num_events++];
			e.ticks = tick;
			e.slot = slot;
			e.event_type = EVENT_TYPE_ENTRANCE;


			if (slots[slot].caller_address == 0x0040CCD3) {
				if (inside_primary_culling) {
					error("already in primary culling");
				}
				inside_primary_culling = true;
				primary_culling_entry_time = get_time_ms();
			}

			return slots[slot].callee_address;
		}
		UInt32 __fastcall do_event_return(UInt32 *object, int slot) {
			UInt64 tick = get_time_ticks();
			int thread = ::PerThread::get_PT()->thread_number;

			if (thread >= CP_MAX_THREADS) error("too many threads for CallPerf");
			ThreadData &td = thread_data[thread];
			if (td.num_events == ThreadData::BUFFER_SIZE) flush_thread_buffer(thread);
			EventData &e = td.events[td.num_events++];
			e.ticks = tick;
			e.slot = slot;
			e.event_type = EVENT_TYPE_EXIT;

			if (slots[slot].caller_address == 0x0040CCD3) {
				if (!inside_primary_culling) {
					error("not in primary culling");
				}
				inside_primary_culling = false;
				primary_culling_entry_time = 0;
			}

			return slots[slot].return_address;
		}
		template<const int slot> void __declspec(naked) wrapper() {
			enum {X=slot};
			__asm {
				push ecx
				mov edx, X
				call do_event_call
				pop ecx
				call eax

				push eax
				mov edx, X
				call do_event_return
				mov edx, eax
				pop eax
				jmp edx
			}
		}
		void flush_thread_buffer(int thread) {
			ThreadData &td = thread_data[thread];
			EnterCriticalSection(&internal_cs);
			for (int i = 0; i < td.num_events; i++) {
				EventData &e = td.events[i];
				int slot = e.slot;
				ThreadSlotData &tsd = td.tsd[e.slot];
				//message("thread %d, slot %d, type %d, old_type %d", thread, slot, e.event_type, tsd.last_event_type);
				if (e.event_type == EVENT_TYPE_ENTRANCE) {
					if (!tsd.recursion_level) tsd.entrance_ticks = e.ticks;
					else {
						if (tsd.recursion_level > tsd.max_recursion) tsd.max_recursion = tsd.recursion_level;
						if (tsd.recursion_level >= 100) {
							message("CallPerf::CP_flush_thread_buffer - thread %d, slot %d: recursion level %d too high", thread, slot, tsd.recursion_level);
						}
					}
					tsd.recursion_level++;
				}
				else if (e.event_type == EVENT_TYPE_EXIT) {
					if (!tsd.recursion_level) {
						error("CallPerf::CP_flush_thread_buffer - thread %d, slot %d: recursion level negative", thread, slot, tsd.recursion_level);
					}
					tsd.recursion_level--;
					if (!tsd.recursion_level) {
						UInt64 delta = e.ticks - tsd.entrance_ticks;
						tsd.total_calls++;
						tsd.total_ticks += delta;
					}
				}
				else {
					error("CallPerf::CP_flush_thread_buffer - slot %d, thread %d, et %d, recursion level %d", slot, thread, e.event_type, tsd.recursion_level);
				}
	/*			if (e.event_type == EVENT_TYPE_ENTRANCE) {
	//				if (tsd.last_event_type != EVENT_TYPE_EXIT && tsd.last_event_type != EVENT_TYPE_NONE) {
					if (tsd.last_event_type == EVENT_TYPE_ENTRANCE) {
						//error("CallPerf::CP_flush_thread_buffer - slot %d, thread %d, et %d, old et %d", slot, thread, e.event_type, int(tsd.last_event_type));
						tsd.last_entrance_ticks_stack.push_back(tsd.last_event_ticks);
					}
					tsd.last_event_ticks = e.ticks;
					tsd.last_event_type = e.event_type;
				}
				else if (e.event_type == EVENT_TYPE_EXIT) {
					UInt64 delta;
					if (tsd.last_event_type == EVENT_TYPE_ENTRANCE) {
						delta = e.ticks - tsd.last_event_ticks;
					}
					else if (tsd.last_event_type == EVENT_TYPE_EXIT && !tsd.last_entrance_ticks_stack.empty()) {
						//UInt64 prev = tsd.last_entrance_ticks_stack.back();
						//delta = e.ticks - prev;
						delta = e.ticks - tsd.last_event_ticks;
						tsd.last_entrance_ticks_stack.pop_back();
					}
					else {
						error("CallPerf::CP_flush_thread_buffer - slot %d, thread %d, et %d, old et %d", slot, thread, e.event_type, int(tsd.last_event_type));
					}
					tsd.last_event_ticks = e.ticks;
					tsd.last_event_type = e.event_type;
					tsd.total_calls++;
					tsd.total_ticks += delta;
				}
				else {
					error("CallPerf::CP_flush_thread_buffer - slot %d, thread %d, et %d, old et %d", slot, thread, e.event_type, int(tsd.last_event_type));
				}*/
			}
			UInt32 time = get_time_ms();
			if (thread == 1 && UInt32(time - last_print_time) >= 15000) {
				last_print_time = time;
				print_data();
			}
			td.num_events = 0;
			LeaveCriticalSection(&internal_cs);
		}

		void print_data() {
			EnterCriticalSection(&internal_cs);
			UInt32 time = get_time_ms();
			double period = get_tick_period() * 1000;
			message("beginning of CallPerf data @ time %.1f", time / 1000.0);
			for (int slot = 0; slot < num_slots; slot++) {
				UInt64 total_ticks = 0;
				UInt64 total_calls = 0;
				UInt64 highest_ticks = 0;
				int highest_thread = -1;
				for (int thread = 1; thread < CP_MAX_THREADS; thread++) {
					total_ticks += thread_data[thread].tsd[slot].total_ticks;
					total_calls += thread_data[thread].tsd[slot].total_calls;
					if (thread != 1 && thread_data[thread].tsd[slot].total_ticks > highest_ticks) {
						highest_ticks = thread_data[thread].tsd[slot].total_ticks;
						highest_thread = thread;
					}
				}

				char buffy[1024];
				int len = 0;
				len += sprintf(&buffy[len], "  %3d %08X->%08X ", 
					slot, slots[slot].caller_address, slots[slot].callee_address);
				if      (total_calls < 1000)             len += sprintf(&buffy[len], "      %3d",     int(total_calls));
				else if (total_calls < 10000)            len += sprintf(&buffy[len], "     %3.1fK",   int(total_calls / 100) * 0.1);
				else if (total_calls < 1000000)          len += sprintf(&buffy[len], "    %3d K",     int(total_calls  / 1000));
				else if (total_calls < 10000000)         len += sprintf(&buffy[len], "    %3.1fM ",   int(total_calls/ 100000) * 0.1);
				else if (total_calls < 1000000000)       len += sprintf(&buffy[len], "   %3d M ",     int(total_calls  / 1000000));
				else if (total_calls < 10000000000)      len += sprintf(&buffy[len], "   %3.1fB  ",  int(total_calls/ 100000000) * 0.1);
				else if (total_calls < 1000000000000)    len += sprintf(&buffy[len], "  %3d B  ",    int(total_calls  / 1000000000));
				else if (total_calls < 10000000000000)   len += sprintf(&buffy[len], "  %3.1fT   ",  int(total_calls/ 100000000000) * 0.1);
				else if (total_calls < 1000000000000000) len += sprintf(&buffy[len], " %3d T   ",   int(total_calls  / 1000000000000));
				else if (total_calls < 10000000000000000)len += sprintf(&buffy[len], " %3.1fQ    ", int(total_calls/ 100000000000000) * 0.1);
				else                                     len += sprintf(&buffy[len], "%3d Q    ",  int(total_calls  / 1000000000000000));
				if (0);
				else if (time < 99000)
					len += sprintf(&buffy[len], "%9.2fms ", total_ticks * period);
				else if (time < 999000)
					len += sprintf(&buffy[len], "%9.1fms ", total_ticks * period);
	//			else if (time < 9999000)
	//				len += sprintf(&buffy[len], "%9.1fms ", total_ticks * period);
				else
					len += sprintf(&buffy[len], "%9.0fms ", total_ticks * period);
				if (total_ticks) len += sprintf(&buffy[len], "  %2d%%/%2d%%/%2d%% ", 
					(total_ticks - 1 + 99 * thread_data[1].tsd[slot].total_ticks) / total_ticks,
					(total_ticks - 1 + 99 * highest_ticks) / total_ticks,
					(total_ticks - 1 + 99 * (total_ticks - thread_data[1].tsd[slot].total_ticks)) / total_ticks
	//				ceil(thread_data[1].tsd[slot].total_ticks / double(total_ticks) * 99),
	//				ceil(highest_ticks / double(total_ticks) * 99),
	//				ceil((total_ticks - thread_data[1].tsd[slot].total_ticks - highest_ticks) / double(total_ticks) * 99)
				);
				else len += sprintf(&buffy[len], "   0%%/ 0%%/ 0%% ");
				if (highest_thread != -1) len += sprintf(&buffy[len], "%2d ", highest_thread);
				else len += sprintf(&buffy[len], "   ", highest_thread);
				if (slots[slot].name) len += sprintf(&buffy[len], "%s", slots[slot].name);
				message("%s", buffy);
			}
			message("end of CallPerf data @ time %.1f", time / 1000.0);
			LeaveCriticalSection(&internal_cs);
		}

		typedef void (*SIMPLE_FUNCPTR) ();
		SIMPLE_FUNCPTR get_call_wrapper(int slot) {
			if (slot < 0 || slot >= MAX_SLOTS) error("CallPerf::get_call_wrapper - bad slot %d", slot);
			switch (slot) {
	#define DO1SLOT(x) case x: return &wrapper<x>;
	#define DO4SLOTS(x) DO1SLOT(x) DO1SLOT(x+1) DO1SLOT(x+2) DO1SLOT(x+3)
	#define DO16SLOTS(x) DO4SLOTS(x) DO4SLOTS(x+4) DO4SLOTS(x+8) DO4SLOTS(x+12)
	#define DO64SLOTS(x) DO16SLOTS(x) DO16SLOTS(x+16) DO16SLOTS(x+32) DO16SLOTS(x+48)
				DO64SLOTS(0)
				DO64SLOTS(64)
				DO64SLOTS(128)
				DO64SLOTS(192)
				DO64SLOTS(256)
				DO64SLOTS(320)
				DO64SLOTS(384)
				DO64SLOTS(448)
			}
			error("CallPerf::get_call_wrapper - should be unreachable %d", slot);
			return NULL;
		}
		void hook_relative_call(UInt32 instruction_address, const char *name) {
			UInt32 old_rel_target = *(UInt32*)(instruction_address+1);
			UInt32 old_target = old_rel_target + instruction_address + 5;
			UInt32 return_address = instruction_address + 5;
			int slot = allocate_slot();
			UInt32 new_target = UInt32(get_call_wrapper(slot));
			UInt32 new_rel_target = new_target - instruction_address - 5;
			slots[slot].caller_address = instruction_address;
			slots[slot].callee_address = old_target;
			slots[slot].return_address = return_address;
			slots[slot].wrapper_address = new_target;
			slots[slot].flags = 0;
			slots[slot].name = name;
			WriteRelJump(instruction_address, new_target);
	//		SafeWrite32(instruction_address + 1, new_rel_target);
		}
		void hook_indirect_call(UInt32 instruction_address, const char *name) {
			UInt32 old_target_holder = *(UInt32*)(instruction_address+2);
			UInt32 old_target = *(UInt32*)old_target_holder;
			UInt32 return_address = instruction_address + 6;
			int slot = allocate_slot();
			UInt32 new_target = UInt32(get_call_wrapper(slot));
			UInt32 new_target_holder = UInt32(&slots[slot].wrapper_address);
			slots[slot].caller_address = instruction_address;
			slots[slot].callee_address = old_target;
			slots[slot].return_address = return_address;
			slots[slot].wrapper_address = new_target;
			slots[slot].flags = 0;
			slots[slot].name = name;
			WriteRelJump(instruction_address, new_target);
	//		SafeWrite32(instruction_address + 2, new_target_holder);
		}
		void hook_caller ( UInt32 caller, const char *name = "" ) {
			UInt8 *inst = (UInt8*)caller;
			if (inst[0] == 0xE8) hook_relative_call(caller, name);
			else if (inst[0] == 0xFF && inst[1] == 0x15) hook_indirect_call(caller, name);
			else {
				message("CallPerf::hook_call(%08X / \"%s\") - unrecognized call type", caller, name);
				message("inst[0]:%02X  inst[1]:%02X  inst[2]:%02X", inst[0], inst[1], inst[2]);
			}
		}
		int custom_hook ( const char *name = "" ) {
			int slot = allocate_slot();
			slots[slot].caller_address = NULL;
			slots[slot].callee_address = NULL;
			slots[slot].return_address = NULL;
			slots[slot].wrapper_address = NULL;
			slots[slot].flags = 0;
			slots[slot].name = name;
			return slot;
		}
	//	void suppress_caller ( UInt32 caller ) {
	//		UInt8 *inst = (UInt8*)caller;
	//		if (inst[0] == 0xE8) hook_relative_call(caller);
	//		else if (inst[0] == 0xFF && inst[1] == 15) hook_indirect_call(caller);
	//		else error("CallPerf::hook_call(%08X) - unrecognized call type", caller);
	//		slots[num_slots].callee_address = (UInt32)&_do_nothing;
	//	}
	};

	//int p_bssa_v14_level = 0;
	//UInt32 p_bssa_v14_time = 0;
	//void __stdcall profile_entering_bsshaderaccumulator_0x14() {
	//	if (p_bssa_v14_level) message("profile_entering_bsshaderaccumulator_0x14 - already %d", p_bssa_v14_level);
	//	else p_bssa_v14_time = timeGetTime();
	//	p_bssa_v14_level++;
	//	CallPerf::do_event_call(NULL, p_bssa_slot);
	//}
	//void __fastcall profile_exiting_bsshaderaccumulator_0x14(int who) {
	//	p_bssa_v14_level--;
	//	if (p_bssa_v14_level) message("profile_entering_bsshaderaccumulator_0x14 - still %d", p_bssa_v14_level);
	//	else message("bsshaderaccumulator_0x14 took %d milliseconds (%d)", timeGetTime() - p_bssa_v14_time, who);
	//	CallPerf::do_event_return(NULL, p_bssa_slot);
	//}
	int cp_slot_0x7ae070;
	void __declspec(naked) _profile_entering_0x7ae070() {
		__asm {
			push ecx
			push edx
			xor ecx, ecx
			mov edx, cp_slot_0x7ae070
			call CallPerf::do_event_call
			pop ecx
			pop edx
			push 0xFFFFFFFF
			push 0x009cd4d3
			mov eax, 0x7ae077
			jmp eax
		}
	}
	void __declspec(naked) _profile_exiting_0x7ae070() {
		__asm {
			push eax
			xor ecx, ecx
			mov edx, cp_slot_0x7ae070
			call CallPerf::do_event_return
			pop eax

			pop ecx
			pop edi
			pop esi
			pop ebp
			pop ebx
			add esp, 0x38
			ret
		}
	}
	void do_hook_0x7ae070() {
		message("monitoring 0x7ae070 (BSShaderAccumulator::vtable_entry_0x14 ?");
		cp_slot_0x7ae070 = CallPerf::custom_hook("0x7ae070 *** BSShaderAccumulator::vtable_entry_0x14 (all callers)");
		WriteRelJump(0x7ae070, UInt32(&_profile_entering_0x7ae070));
		WriteRelJump(0x7aec09, UInt32(&_profile_exiting_0x7ae070));
		WriteRelJump(0x7ae128, UInt32(&_profile_exiting_0x7ae070));
	}
	int cp_slot_0x6245b0;
	static const double negative_one = -1;
	void __declspec(naked) _profile_entering_0x6245b0() {
		__asm {
			fld negative_one

			push ecx
			xor ecx, ecx
			mov edx, cp_slot_0x6245b0
			call CallPerf::do_event_call
			pop ecx

			mov eax, 0x6245b6
			jmp eax
		}
	}
	void __declspec(naked) _profile_exiting_0x6245b0() {
		__asm {
			push eax
			xor ecx, ecx
			mov edx, cp_slot_0x6245b0
			call CallPerf::do_event_return
			pop eax

			add esp, 0xC
			retn 4
		}
	}
	void do_hook_0x6245b0() {
		message("monitoring 0x6245b0 (a method of CombatController, vtable=0xA70A14, involving character, vtable=0xA6FC9C)");
		cp_slot_0x6245b0 = CallPerf::custom_hook("0x6245b0 *** CombatController::??? Combat AI? (all callers)");
		WriteRelJump(0x6245b0, UInt32(&_profile_entering_0x6245b0));
		WriteRelJump(0x6246cc, UInt32(&_profile_exiting_0x6245b0));
		WriteRelJump(0x6246d6, UInt32(&_profile_exiting_0x6245b0));
		WriteRelJump(0x624728, UInt32(&_profile_exiting_0x6245b0));
		WriteRelJump(0x624776, UInt32(&_profile_exiting_0x6245b0));
		WriteRelJump(0x6247bd, UInt32(&_profile_exiting_0x6245b0));
		WriteRelJump(0x6248a1, UInt32(&_profile_exiting_0x6245b0));
		WriteRelJump(0x6248fe, UInt32(&_profile_exiting_0x6245b0));
		WriteRelJump(0x624924, UInt32(&_profile_exiting_0x6245b0));
		WriteRelJump(0x62493f, UInt32(&_profile_exiting_0x6245b0));
		WriteRelJump(0x624957, UInt32(&_profile_exiting_0x6245b0));
		WriteRelJump(0x6249ad, UInt32(&_profile_exiting_0x6245b0));
		WriteRelJump(0x6249dd, UInt32(&_profile_exiting_0x6245b0));
		WriteRelJump(0x624a2e, UInt32(&_profile_exiting_0x6245b0));
		WriteRelJump(0x624a60, UInt32(&_profile_exiting_0x6245b0));
		WriteRelJump(0x624a77, UInt32(&_profile_exiting_0x6245b0));
		WriteRelJump(0x624aff, UInt32(&_profile_exiting_0x6245b0));
		WriteRelJump(0x624b25, UInt32(&_profile_exiting_0x6245b0));
		WriteRelJump(0x624b38, UInt32(&_profile_exiting_0x6245b0));
		WriteRelJump(0x624b4b, UInt32(&_profile_exiting_0x6245b0));
		WriteRelJump(0x624b5e, UInt32(&_profile_exiting_0x6245b0));
		WriteRelJump(0x624b71, UInt32(&_profile_exiting_0x6245b0));
		WriteRelJump(0x624b84, UInt32(&_profile_exiting_0x6245b0));
		WriteRelJump(0x624b97, UInt32(&_profile_exiting_0x6245b0));
		WriteRelJump(0x624baa, UInt32(&_profile_exiting_0x6245b0));
		WriteRelJump(0x624bbd, UInt32(&_profile_exiting_0x6245b0));
		WriteRelJump(0x624bd0, UInt32(&_profile_exiting_0x6245b0));
		WriteRelJump(0x624be3, UInt32(&_profile_exiting_0x6245b0));
		WriteRelJump(0x624bf6, UInt32(&_profile_exiting_0x6245b0));
		WriteRelJump(0x624c1e, UInt32(&_profile_exiting_0x6245b0));
		WriteRelJump(0x624c48, UInt32(&_profile_exiting_0x6245b0));
	}

	static void initialize_oblivion_callperfs() {
	#if OBLIVION_VERSION == OBLIVION_VERSION_1_2_416
		CallPerf::hook_caller(0x0040F270, "calling do_frame from main_loop");//main per-frame function
		//beginning of oblivion main per-frame function 40D800
		CallPerf::hook_caller(0x0040D809, "first call from do_frame");//
		CallPerf::hook_caller(0x0040D80F);//
		CallPerf::hook_caller(0x0040D817);//
		CallPerf::hook_caller(0x0040D830);//in framefunc-loop1
		CallPerf::hook_caller(0x0040D837);//in framefunc-loop1
		CallPerf::hook_caller(0x0040D84F);//in framefunc-loop1
		CallPerf::hook_caller(0x0040D862);//not yet suppred - it's a "call edx",  in mainfunc-loop1
		CallPerf::hook_caller(0x0040D86B);//in framefunc-loop1
		CallPerf::hook_caller(0x0040D87F);
		CallPerf::hook_caller(0x0040D893);
		CallPerf::hook_caller(0x0040D8C2);
		CallPerf::hook_caller(0x0040D8CD);
		CallPerf::hook_caller(0x0040D8D8);//significant
		CallPerf::hook_caller(0x0040D90E);
		CallPerf::hook_caller(0x0040D91D);
		CallPerf::hook_caller(0x0040D924);
		CallPerf::hook_caller(0x0040D957);
		CallPerf::hook_caller(0x0040D966);
		CallPerf::hook_caller(0x0040D96D);
		CallPerf::hook_caller(0x0040D9AC);
		CallPerf::hook_caller(0x0040D9C6);
		CallPerf::hook_caller(0x0040D9CF);
		CallPerf::hook_caller(0x0040D9D4);
		CallPerf::hook_caller(0x0040D9E4);
		CallPerf::hook_caller(0x0040D9F2);
		CallPerf::hook_caller(0x0040DA01);
		CallPerf::hook_caller(0x0040DA13);
		CallPerf::hook_caller(0x0040DA25);
		CallPerf::hook_caller(0x0040DA37);
		CallPerf::hook_caller(0x0040DA43);
		CallPerf::hook_caller(0x0040DA62);
		CallPerf::hook_caller(0x0040DA78);
		CallPerf::hook_caller(0x0040DA87);
		CallPerf::hook_caller(0x0040DA93);
		CallPerf::hook_caller(0x0040DAA2);
		CallPerf::hook_caller(0x0040DAB4);
		CallPerf::hook_caller(0x0040DAC0);
		CallPerf::hook_caller(0x0040DAD1);
		CallPerf::hook_caller(0x0040DAEA);
		CallPerf::hook_caller(0x0040DAFD);//not yet supported - indirect call
		CallPerf::hook_caller(0x0040DB13);//not yet supported - "call eax"
		CallPerf::hook_caller(0x0040DB39);
		CallPerf::hook_caller(0x0040DB49);//semi-significant
		CallPerf::hook_caller(0x0040DB56);
		CallPerf::hook_caller(0x0040DB5B);
		CallPerf::hook_caller(0x0040DB7B);
		CallPerf::hook_caller(0x0040DB86);
		CallPerf::hook_caller(0x0040DB9A);
		CallPerf::hook_caller(0x0040DBA5);//significant
		CallPerf::hook_caller(0x0040DBAB);
		CallPerf::hook_caller(0x0040DBB4);//semi-significant
		CallPerf::hook_caller(0x0040DBD1);
		CallPerf::hook_caller(0x0040DBDC);//significant
		CallPerf::hook_caller(0x0040DBEC);
		CallPerf::hook_caller(0x0040DBFB);
		CallPerf::hook_caller(0x0040DC0C);
		CallPerf::hook_caller(0x0040DC2A);//significant
		CallPerf::hook_caller(0x0040DC3E);
		CallPerf::hook_caller(0x0040DC52);
		CallPerf::hook_caller(0x0040DC61);
		CallPerf::hook_caller(0x0040DC70);
		CallPerf::hook_caller(0x0040DC82);//not yet supported - "call eax"
		CallPerf::hook_caller(0x0040DCA0);//not yet supported - "call eax"
		CallPerf::hook_caller(0x0040DCA8);
		CallPerf::hook_caller(0x0040DCD3);
		CallPerf::hook_caller(0x0040DCE3);
		CallPerf::hook_caller(0x0040DCF0);
		CallPerf::hook_caller(0x0040DD0E);
		CallPerf::hook_caller(0x0040DD31);
		CallPerf::hook_caller(0x0040DD3E);
		CallPerf::hook_caller(0x0040DD43);
		CallPerf::hook_caller(0x0040DD4E);
		CallPerf::hook_caller(0x0040DD5F);
		CallPerf::hook_caller(0x0040DD66);
		CallPerf::hook_caller(0x0040DD6E);
		CallPerf::hook_caller(0x0040DD98);//semi-significant?
		CallPerf::hook_caller(0x0040DDBA);
		CallPerf::hook_caller(0x0040DDC7);
		CallPerf::hook_caller(0x0040DDE1);//not yet supported - "call eax"
		CallPerf::hook_caller(0x0040DDF4);//not yet supported - "call eax"
		CallPerf::hook_caller(0x0040DDFD);
		CallPerf::hook_caller(0x0040DE04);
		CallPerf::hook_caller(0x0040DE3A);
		CallPerf::hook_caller(0x0040DE3F);
		CallPerf::hook_caller(0x0040DE48);
		CallPerf::hook_caller(0x0040DE59);
		CallPerf::hook_caller(0x0040DE71);
		CallPerf::hook_caller(0x0040DE76);
		CallPerf::hook_caller(0x0040DEA1);//significant
		CallPerf::hook_caller(0x0040DEA6);
		CallPerf::hook_caller(0x0040DEC1);
		CallPerf::hook_caller(0x0040DED9);
		CallPerf::hook_caller(0x0040DEDE);
		CallPerf::hook_caller(0x0040DEEA);
		CallPerf::hook_caller(0x0040DEF5);
		CallPerf::hook_caller(0x0040DEFC);
		CallPerf::hook_caller(0x0040DF01);
		CallPerf::hook_caller(0x0040DF06, "possibly console or debug text?");//significant
		CallPerf::hook_caller(0x0040DF0B);
		CallPerf::hook_caller(0x0040DF29);
		CallPerf::hook_caller(0x0040DF36);
		CallPerf::hook_caller(0x0040DF4D);
		CallPerf::hook_caller(0x0040DF54, "graphics1");//very significant - this is the big one
		CallPerf::hook_caller(0x0040DF61);
		//end of oblivion main per-frame function 40D800

		//beginning of function 40D4D0
		CallPerf::hook_caller(0x0040D4EB, "first call from graphics1");
		CallPerf::hook_caller(0x0040D4F3);
		CallPerf::hook_caller(0x0040D508);
		CallPerf::hook_caller(0x0040D515);
		CallPerf::hook_caller(0x0040D54A);
		CallPerf::hook_caller(0x0040D578);
		CallPerf::hook_caller(0x0040D58E);
		CallPerf::hook_caller(0x0040D5A1);//not yet supported - indirect call
		CallPerf::hook_caller(0x0040D5B7);//not yet supported - "call edx"
		CallPerf::hook_caller(0x0040D5E7);
		CallPerf::hook_caller(0x0040D5F1);
		CallPerf::hook_caller(0x0040D606);
		CallPerf::hook_caller(0x0040D620);//not yet supported - call edx
		CallPerf::hook_caller(0x0040D62E);
		CallPerf::hook_caller(0x0040D650);
		CallPerf::hook_caller(0x0040D658, "graphics2?");//very significant - this is the big one (40C830)
		CallPerf::hook_caller(0x0040D67E);
		CallPerf::hook_caller(0x0040D68D);
		CallPerf::hook_caller(0x0040D6A6);
		CallPerf::hook_caller(0x0040D6B3);
		CallPerf::hook_caller(0x0040D6E3);//not yet supported - call edx
		CallPerf::hook_caller(0x0040D6EC);
		CallPerf::hook_caller(0x0040D6F1);
		CallPerf::hook_caller(0x0040D6F7);//significant
		CallPerf::hook_caller(0x0040D706);
		CallPerf::hook_caller(0x0040D71C);
		CallPerf::hook_caller(0x0040D72F);
		CallPerf::hook_caller(0x0040D76E);
		CallPerf::hook_caller(0x0040D77C);//not yet supported - indirect call (CreateDirectoryA
		CallPerf::hook_caller(0x0040D7BD);
		CallPerf::hook_caller(0x0040D7CE);
		CallPerf::hook_caller(0x0040D7D9);
		CallPerf::hook_caller(0x0040D7E0);
		CallPerf::hook_caller(0x0040D7F4);
		//end of function 40D4D0

		//beginning of function 40C830
		CallPerf::hook_caller(0x0040C89D, "first call from graphics2");
		CallPerf::hook_caller(0x0040C8B8);
		CallPerf::hook_caller(0x0040C8BF);
		CallPerf::hook_caller(0x0040C8EC);
		CallPerf::hook_caller(0x0040C90A);
		CallPerf::hook_caller(0x0040C911);
		CallPerf::hook_caller(0x0040C91B);
		CallPerf::hook_caller(0x0040C945);
		CallPerf::hook_caller(0x0040C9A5);
		CallPerf::hook_caller(0x0040C9F0);
		CallPerf::hook_caller(0x0040CA27);
		CallPerf::hook_caller(0x0040CA3E);//call eax
		CallPerf::hook_caller(0x0040CA43);
		CallPerf::hook_caller(0x0040CA59);//call eax
		CallPerf::hook_caller(0x0040CA63);
		CallPerf::hook_caller(0x0040CA78);//call eax
		CallPerf::hook_caller(0x0040CA82);
		CallPerf::hook_caller(0x0040CA9B);//call eax
		CallPerf::hook_caller(0x0040CAA5);
		CallPerf::hook_caller(0x0040CAF1);//call eax
		CallPerf::hook_caller(0x0040CAFB);
		CallPerf::hook_caller(0x0040CB3C);//call eax
		CallPerf::hook_caller(0x0040CB46);
		CallPerf::hook_caller(0x0040CB92);//call eax
		CallPerf::hook_caller(0x0040CB9C);
		CallPerf::hook_caller(0x0040CC21);
		CallPerf::hook_caller(0x0040CC29);
		CallPerf::hook_caller(0x0040CC41);
		CallPerf::hook_caller(0x0040CC49);
		CallPerf::hook_caller(0x0040CC54);
		CallPerf::hook_caller(0x0040CCBB);
		CallPerf::hook_caller(0x0040CCD3, "culling process");//this is the big one (70C0B0)
		CallPerf::hook_caller(0x0040CCE3);
		CallPerf::hook_caller(0x0040CD22);
		CallPerf::hook_caller(0x0040CD29);
		CallPerf::hook_caller(0x0040CD30);
		CallPerf::hook_caller(0x0040CD44);
		CallPerf::hook_caller(0x0040CD64);//call eax
		CallPerf::hook_caller(0x0040CD69);
		CallPerf::hook_caller(0x0040CD83);
		CallPerf::hook_caller(0x0040CDAD);
		CallPerf::hook_caller(0x0040CDB5);
		CallPerf::hook_caller(0x0040CDBD);
		CallPerf::hook_caller(0x0040CDD7);//indirect
		CallPerf::hook_caller(0x0040CE14);//call eax
		CallPerf::hook_caller(0x0040CE1B);
		CallPerf::hook_caller(0x0040CE28);//call eax
		CallPerf::hook_caller(0x0040CE48);
		CallPerf::hook_caller(0x0040CE58);
		CallPerf::hook_caller(0x0040CE6B);//call eax
		CallPerf::hook_caller(0x0040CE79);
		CallPerf::hook_caller(0x0040Ce95);
		CallPerf::hook_caller(0x0040Ce9c);
		CallPerf::hook_caller(0x0040Cea3);
		CallPerf::hook_caller(0x0040Ceb8);//indirect
		CallPerf::hook_caller(0x0040Ceca);//call edx
		CallPerf::hook_caller(0x0040Ced2);
		CallPerf::hook_caller(0x0040Cefc);
		CallPerf::hook_caller(0x0040Cf13);//call edx
		CallPerf::hook_caller(0x0040Cf21);//call eax
		CallPerf::hook_caller(0x0040Cf29);
		CallPerf::hook_caller(0x0040Cf35);//call eax
		CallPerf::hook_caller(0x0040Cf5d);//call eax
		CallPerf::hook_caller(0x0040Cf6e);
		CallPerf::hook_caller(0x0040Cfc7);
		CallPerf::hook_caller(0x0040Cfcf);
		CallPerf::hook_caller(0x0040Cfe4);
		CallPerf::hook_caller(0x0040Cffb);//call eax
		CallPerf::hook_caller(0x0040D009);//call eax
		CallPerf::hook_caller(0x0040D00f);
		CallPerf::hook_caller(0x0040D01b);//call eax
		CallPerf::hook_caller(0x0040D043);//call eax
		CallPerf::hook_caller(0x0040D054);
		CallPerf::hook_caller(0x0040D06a);//indirect
		CallPerf::hook_caller(0x0040D078);//indirect
		CallPerf::hook_caller(0x0040D099);
		CallPerf::hook_caller(0x0040D09F);
		CallPerf::hook_caller(0x0040D0A9);
		CallPerf::hook_caller(0x0040D0ef);
		CallPerf::hook_caller(0x0040D114);//indirect
		CallPerf::hook_caller(0x0040D12a);//call eax
		//end of function 40C830

		//begin 70C0B0
		CallPerf::hook_caller(0x0070C0de, "first call from culling process");
		CallPerf::hook_caller(0x0070C0f8);//call edx
		CallPerf::hook_caller(0x0070C0fc);
		CallPerf::hook_caller(0x0070C112);//call edx
		//end 70C0B0

		//begin 70E0A0
		CallPerf::hook_caller(0x0070e0e9, "first call from NiCullingProcess::main");
		CallPerf::hook_caller(0x0070e124);
		CallPerf::hook_caller(0x0070e139);//call edx
		CallPerf::hook_caller(0x0070e140, "NiCullingProcess - walking the scenegraph");
		CallPerf::hook_caller(0x0070e150);//call edx
		CallPerf::hook_caller(0x0070e174);//indirect - InterlockedDecrement
		CallPerf::hook_caller(0x0070e186);//call eax
		//end 70E0A0

		//begin 70dfb0
		CallPerf::hook_caller(0x0070dff9, "NiCullingProcess::vtable_entry_0x1: testing bounding sphere vs clipping planes");
		//end 70dfb0
		//begin 70ab40
		CallPerf::hook_caller(0x0070ab6e, "NiNode::vtable_entry_0x1: cull children");
		//end 70ab40
		//begin 7c78d0
		CallPerf::hook_caller(0x007c7940, "calls from ShadowSceneNode::");
		CallPerf::hook_caller(0x007c7955, "");
		CallPerf::hook_caller(0x007c796c, "");//call edx
		CallPerf::hook_caller(0x007c7988, "");
		CallPerf::hook_caller(0x007c7996, "");
		CallPerf::hook_caller(0x007c79a6, "");
		CallPerf::hook_caller(0x007c79b0, "");
		CallPerf::hook_caller(0x007c79c3, "");
		CallPerf::hook_caller(0x007c79e7, "");
		CallPerf::hook_caller(0x007c79fd, "");//call eax
		CallPerf::hook_caller(0x007c7a0d, "");
		CallPerf::hook_caller(0x007c7a1e, "");
		CallPerf::hook_caller(0x007c7a38, "");
		CallPerf::hook_caller(0x007c7a55, "");
		CallPerf::hook_caller(0x007c7a62, "");//call eax
		CallPerf::hook_caller(0x007c7a6b, "ShadowSceneNode children culling (1)");
		CallPerf::hook_caller(0x007c7a77, "");//call eax
		CallPerf::hook_caller(0x007c7a80, "");
		CallPerf::hook_caller(0x007c7b2b, "");
		CallPerf::hook_caller(0x007c7b85, "");
		CallPerf::hook_caller(0x007c7be2, "");
		CallPerf::hook_caller(0x007c7bee, "");
		CallPerf::hook_caller(0x007c7ccb, "");
		CallPerf::hook_caller(0x007c7cde, "");
		CallPerf::hook_caller(0x007c7cf6, "");
		CallPerf::hook_caller(0x007c7d10, "");
		CallPerf::hook_caller(0x007c7d22, "");
		CallPerf::hook_caller(0x007c7d2f, "");//call eax
		CallPerf::hook_caller(0x007c7d41, "ShadowSceneNode children culling (2)");
		CallPerf::hook_caller(0x007c7d54, "");//call eax
		CallPerf::hook_caller(0x007c7d5d, "");
		CallPerf::hook_caller(0x007c7d6f, "");
		CallPerf::hook_caller(0x007c7d82, "");//call eax
		CallPerf::hook_caller(0x007c7d8d, "");
		CallPerf::hook_caller(0x007c7d9f, "");//call eax
		//end 7c78d0
		//begin 4a0920
		CallPerf::hook_caller(0x004a0a55, "first call fom 4a0920 BSFadeNode::vtable_entry_0x1f");
		CallPerf::hook_caller(0x004a09b2, "BSFadeNode children culling (1)");
		CallPerf::hook_caller(0x004a0b65, "");
		CallPerf::hook_caller(0x004a0b9c, "");
		CallPerf::hook_caller(0x004a0bae, "");
		CallPerf::hook_caller(0x004a0bdc, "");
		CallPerf::hook_caller(0x004a0bf2, "");
		CallPerf::hook_caller(0x004a0bfb, "");
		CallPerf::hook_caller(0x004a0c09, "");
		CallPerf::hook_caller(0x004a0c12, "");
		CallPerf::hook_caller(0x004a0c19, "");
		CallPerf::hook_caller(0x004a0c25, "");
		CallPerf::hook_caller(0x004a0c46, "");
		CallPerf::hook_caller(0x004a0c52, "");
		CallPerf::hook_caller(0x004a0c63, "");
		CallPerf::hook_caller(0x004a0c6c, "");
		CallPerf::hook_caller(0x004a0c7a, "");
		CallPerf::hook_caller(0x004a0c81, "");
		CallPerf::hook_caller(0x004a0c9b, "");
		CallPerf::hook_caller(0x004a0cba, "");
		CallPerf::hook_caller(0x004a0cc1, "");
		CallPerf::hook_caller(0x004a0ccc, "");
		CallPerf::hook_caller(0x004a0ced, "BSFadeNode children culling (2)");
		CallPerf::hook_caller(0x004a0d01, "");
		//end 4a0920

		CallPerf::hook_caller(0x0070e1c6, "resizing culledGeo array");

		CallPerf::hook_caller(0x007ae0ed, "");//call eax
		CallPerf::hook_caller(0x007ae109, "");//call eax
		CallPerf::hook_caller(0x007ae118, "first call from BSShaderAccumulator::vtable_entry_0x14");
		CallPerf::hook_caller(0x007ae133, "");
		CallPerf::hook_caller(0x007ae15b, "");
		CallPerf::hook_caller(0x007ae16a, "");
		CallPerf::hook_caller(0x007ae175, "");
		CallPerf::hook_caller(0x007ae18d, "");
		CallPerf::hook_caller(0x007ae198, "");
		CallPerf::hook_caller(0x007ae213, "");
		CallPerf::hook_caller(0x007ae22f, "");
		CallPerf::hook_caller(0x007ae285, "");
		CallPerf::hook_caller(0x007ae2b1, "");
		CallPerf::hook_caller(0x007ae2e4, "");
		CallPerf::hook_caller(0x007ae348, "");
		CallPerf::hook_caller(0x007ae372, "");
		CallPerf::hook_caller(0x007ae386, "");
		CallPerf::hook_caller(0x007ae390, "");
		CallPerf::hook_caller(0x007ae3a4, "");
		CallPerf::hook_caller(0x007ae3bf, "");
		CallPerf::hook_caller(0x007ae409, "");
		CallPerf::hook_caller(0x007ae41c, "");
		CallPerf::hook_caller(0x007ae423, "");
		CallPerf::hook_caller(0x007ae43d, "");
		CallPerf::hook_caller(0x007ae45c, "");
		CallPerf::hook_caller(0x007ae47f, "");
		CallPerf::hook_caller(0x007ae485, "");
		CallPerf::hook_caller(0x007ae4a6, "");
		CallPerf::hook_caller(0x007ae4d9, "");
		CallPerf::hook_caller(0x007ae4ff, "");
		CallPerf::hook_caller(0x007ae514, "");
		CallPerf::hook_caller(0x007ae59f, "");
		CallPerf::hook_caller(0x007ae5cf, "");
		CallPerf::hook_caller(0x007ae5e9, "");
		CallPerf::hook_caller(0x007ae62c, "");
		CallPerf::hook_caller(0x007ae66e, "");
		CallPerf::hook_caller(0x007ae68b, "");
		CallPerf::hook_caller(0x007ae6a4, "");
		CallPerf::hook_caller(0x007ae6b2, "");
		CallPerf::hook_caller(0x007ae6cc, "");
		CallPerf::hook_caller(0x007ae6dd, "");
		CallPerf::hook_caller(0x007ae6f0, "");
		CallPerf::hook_caller(0x007ae714, "");
		CallPerf::hook_caller(0x007ae721, "");
		CallPerf::hook_caller(0x007ae745, "");
		CallPerf::hook_caller(0x007ae76b, "");
		CallPerf::hook_caller(0x007ae7e4, "");
		CallPerf::hook_caller(0x007ae7f2, "");
		CallPerf::hook_caller(0x007ae812, "");
		CallPerf::hook_caller(0x007ae822, "");
		CallPerf::hook_caller(0x007ae82e, "");
		CallPerf::hook_caller(0x007ae839, "");
		CallPerf::hook_caller(0x007ae85f, "");
		CallPerf::hook_caller(0x007ae86f, "");
		CallPerf::hook_caller(0x007ae87b, "");
		CallPerf::hook_caller(0x007ae890, "");
		CallPerf::hook_caller(0x007ae927, "");
		CallPerf::hook_caller(0x007ae949, "");
		CallPerf::hook_caller(0x007ae969, "");
		CallPerf::hook_caller(0x007ae98e, "");
		CallPerf::hook_caller(0x007ae99f, "");
		CallPerf::hook_caller(0x007ae9ac, "");
		CallPerf::hook_caller(0x007ae9be, "");
		CallPerf::hook_caller(0x007ae9d3, "");
		CallPerf::hook_caller(0x007ae9eb, "");
		CallPerf::hook_caller(0x007aea0a, "");
		CallPerf::hook_caller(0x007aea22, "");
		CallPerf::hook_caller(0x007aea4b, "");
		CallPerf::hook_caller(0x007aea4f, "");
		CallPerf::hook_caller(0x007aea6b, "");
		CallPerf::hook_caller(0x007aea9d, "");
		CallPerf::hook_caller(0x007aeaa1, "");
		CallPerf::hook_caller(0x007aeac6, "");
		CallPerf::hook_caller(0x007aead3, "");
		CallPerf::hook_caller(0x007aeaf7, "");
		CallPerf::hook_caller(0x007aeb2d, "");
		CallPerf::hook_caller(0x007aeb61, "");
		CallPerf::hook_caller(0x007aeba5, "");
		CallPerf::hook_caller(0x007aebb3, "");
		CallPerf::hook_caller(0x007aebea, "");
		CallPerf::hook_caller(0x007aebf2, "");
		CallPerf::hook_caller(0x007aebf9, "");


		//all calls to FindNextFileA
		CallPerf::hook_caller(0x0042D507, "first call in the FindNextFileA set");
		CallPerf::hook_caller(0x00431AE2);
		CallPerf::hook_caller(0x0044A51E);
		CallPerf::hook_caller(0x0045D5A6);
		CallPerf::hook_caller(0x0045E8F5);
		CallPerf::hook_caller(0x006A8f29);//not a call, a "mov ebp, func" instead
		CallPerf::hook_caller(0x006a8f3a);//call ebp
		CallPerf::hook_caller(0x006a8f49);//call ebp
		CallPerf::hook_caller(0x006a8f8d);//call ebp
		CallPerf::hook_caller(0x00984673);

		CallPerf::hook_caller(0x0040);
	#endif
	}
#endif

void initialize_callperf() {
#if defined USE_SCRIPT_EXTENDER && defined OBLIVION
	::InitializeCriticalSectionAndSpinCount( &CallPerf::internal_cs, 4000 );
#endif
#if defined OBLIVION
	if (Settings::Master::bExtraProfiling) {
		initialize_oblivion_callperfs();
		message("CallPerf enabled");
	}
#endif
}

void suppressed_srand(int seed) {}
int improved_rand1a(int seed) {
	//wtf?
	PerThread *pt = PerThread::get_PT();
	if (seed) {
//		message("rand1a seeding thread %d value %x", PerThread::get_PT()->thread_number, seed);
		pt->oblivion_rng.seed_32(seed);
	}
	return pt->oblivion_rng.raw16() & ((1 << 15) - 1);
}
int improved_rand1aX(int seed) {
	return PerThread::get_PT()->oblivion_rng.raw16() & ((1 << 15) - 1);
}
int improved_rand2a(int max, int min) {
	return min + int((PerThread::get_PT()->oblivion_rng.raw16() * UInt32(max - min + 1)) >> 16);
}
/*
//Oblivion's code for this is:
//.text:0047DFD0 RNG2_47DFD0     proc near               ; CODE XREF: sub_564E30+3CDp
//.text:0047DFD0
//.text:0047DFD0 arg_0           = dword ptr  4
//.text:0047DFD0 arg_4           = dword ptr  8
//.text:0047DFD0
//.text:0047DFD0                 cmp     must_seed_RNG_B069C3, 0
//.text:0047DFD7                 jz      short loc_47DFF0
//.text:0047DFD9                 push    0
//.text:0047DFDB                 call    lib_something_GetTime_985994
//.text:0047DFE0                 push    eax
//.text:0047DFE1                 call    c_srand_9859D0
//.text:0047DFE6                 add     esp, 8
//.text:0047DFE9                 mov     must_seed_RNG_B069C3, 0
//.text:0047DFF0
//.text:0047DFF0 loc_47DFF0:                             ; CODE XREF: RNG2_47DFD0+7j
//.text:0047DFF0                 push    esi
//.text:0047DFF1                 call    c_rand_9859DD
//.text:0047DFF6                 mov     esi, [esp+4+arg_0]
//.text:0047DFFA                 mov     ecx, eax
//.text:0047DFFC                 mov     eax, [esp+4+arg_4]
//.text:0047E000                 sub     eax, esi
//.text:0047E002                 imul    ecx, eax
//.text:0047E005                 mov     eax, 80010003h
//.text:0047E00A                 imul    ecx
//.text:0047E00C                 add     edx, ecx
//.text:0047E00E                 sar     edx, 0Eh
//.text:0047E011                 mov     eax, edx
//.text:0047E013                 shr     eax, 1Fh
//.text:0047E016                 add     eax, edx
//.text:0047E018                 add     eax, esi
//.text:0047E01A                 pop     esi
//.text:0047E01B                 retn
//.text:0047E01B RNG2_47DFD0     endp
//Which is roughly, but not exactly (why not exactly?):
static int oblivions_random_something(int max, int min) {
	if (must_seed) {//must_seed is a global non-TLS variable initialized to 1
		srand(get_current_time());
		must_seed = false;
	}
	SInt32 a = rand() * (max-min);
	SInt32 a = (PerThread::get_PT()->oblivion_rng.raw16() & 0x7fFF) * (max-min);
	SInt64 b = SInt64(a) * -2147418109;
	SInt32 c = SInt32(b >> 32) + a;
	c >>= 14;
	if (c < 0) c++;
	return c + min;
}
static void test_random_stuff() {
	enum {N = 10};
	int counts_a[N+1] = {0};
	int counts_b[N+1] = {0};
	((bool*)0x0B069C3)[0] = false;
	WriteRelCall(UInt32(0x47DFF1), UInt32(rand));
	WriteRelCall(UInt32(0x47DFE1), UInt32(srand));
	for (int i = 0; i < 100000; i++) {
		typedef int (* obl_func_proto)(int,int);
		//counts_a[((obl_func_proto)0x0047dfd0)(N,0)]++;
		counts_a[oblivions_random_something(N, 0)]++;
		counts_b[improved_rand2a(N, 0)]++;
	}
	for (int i = 0; i < N+1; i++) {
		message("%5d: %.5f    %d / %d", i, counts_a[i] / double(counts_b[i]), counts_a[i], counts_b[i]);
	}
}
//The actual behavior appears to return a random number in [min+1..max] most of the time
//but occasionally it returns min instead
//*/

int improved_rand3a(int max) {
	//not 100% sure this is what it's supposed to do
	return int((PerThread::get_PT()->oblivion_rng.raw16() * UInt32(max + 1)) >> 16);
}
float improved_rand4a(float max, float min) {//min to max
	//it's supposed to be inclusive on both ends, but who cares?
	return min + std::ldexp(float(PerThread::get_PT()->oblivion_rng.raw16()), -16) * (max - min);
}
float improved_rand5a(float max) {//0 to max
	//it's supposed to be inclusive on both ends, but who cares?
	return std::ldexp(float(PerThread::get_PT()->oblivion_rng.raw16()), -16) * max;
}
float improved_rand6a() {//0 to 1
	//it's supposed to be inclusive on both ends, but who cares?
	return std::ldexp(float(PerThread::get_PT()->oblivion_rng.raw16()), -16);
}
float improved_rand7a() {//-1 to 1
	//it's supposed to be inclusive on both ends, but who cares?
	return std::ldexp(float(PerThread::get_PT()->oblivion_rng.raw16()), -15) - 1;
}
bool improved_rand8a(float chance) {//(0 to 1) < chance
	//it's supposed to be inclusive on both ends, but who cares?
	return std::ldexp(float(PerThread::get_PT()->oblivion_rng.raw16()), -16) < chance;
}
void __stdcall reduced_Sleep(int ms) {
//	if (!ms) ::Sleep(0);
/*	if (PerThread::get_PT()->thread_number == 1) {
		int reduced = (ms+1) >> 1;
//		message("long sleep reduced from %d to %d ms for thread #%d", ms, reduced, PerThread::get_PT()->thread_number);
		::Sleep(reduced);
	}
	else*/
	{
		int reduced = (ms+1) >> 1;
//		message("long sleep reduced from %d to %d ms for thread #%d", ms, reduced, PerThread::get_PT()->thread_number);
		::Sleep(reduced);
	}
}
/*void __stdcall selective_Sleep(int ms) {
	if (Settings::Experimental::iThreadsFixedToCPUs && PerThread::get_PT()->thread_number==1 ) {
		if (ms == 0) {
			__asm MFENCE
			return;
		}
		Sleep(ms >> 1);
	}
	int reduced = (ms + _sleep_add) >> _sleep_rshift;
	::Sleep(reduced);
}*/
UInt32 _sleep_address;

void initialize_experimental_stuff() {
//	test_random_stuff();
	if (Hook_srand && Settings::Experimental::bSuppressRandomSeeding) {
		message("suppressing libc srand()");
		::WriteRelJump(Hook_srand, UInt32(&suppressed_srand));
	}
	if (Hook_Sleep && Settings::Experimental::bReduceSleep) {
		message("hooking libc Sleep()" );
		//if (Settings::Experimental::iThreadsFixedToCPUs) _sleep_address = UInt32(&selective_Sleep);
		//else
		_sleep_address = UInt32(&reduced_Sleep);
		::SafeWrite32(Hook_Sleep, _sleep_address);
	}
#if defined OBLIVION && (OBLIVION_VERSION == OBLIVION_VERSION_1_2_416)
	if (Settings::Experimental::bMonitorBSShaderAccumulator) {
		do_hook_0x7ae070();
		do_hook_0x6245b0();
	}
	if (Settings::Experimental::iPrintSceneGraphDepth > 0) {//hook NiNode::method_7073d0
		message("scene graph printing enabled to depth %d", Settings::Experimental::iPrintSceneGraphDepth);
		WriteRelJump(0x7073d0, UInt32(&NiNode__render_7073d0));
	}
	if (false) {//suppress ShadowSceneNode
		message("killing ShadowSceneNode");
		SafeWrite8(0x7878d0+0, 0xc2);
		SafeWrite8(0x7878d0+1, 0x04);
		SafeWrite8(0x7878d0+2, 0x00);
	}
	if (Settings::Experimental::bReplaceRandomWrappers) {//hook NiNode::method_7073d0
		message("replacing various wrappers of rand/srand");
//		if (Settings::Experimental::iRandomQuality <= 0) {
			if (Settings::Experimental::bSuppressRandomSeeding) {//hook NiNode::method_7073d0
				::WriteRelJump(Hook_rand1, UInt32(&improved_rand1aX));
			}
			else {
				::WriteRelJump(Hook_rand1, UInt32(&improved_rand1a));
			}
			::WriteRelJump(Hook_rand2, UInt32(&improved_rand2a));
			::WriteRelJump(Hook_rand3, UInt32(&improved_rand3a));
			::WriteRelJump(Hook_rand4, UInt32(&improved_rand4a));
			::WriteRelJump(Hook_rand5, UInt32(&improved_rand5a));
			::WriteRelJump(Hook_rand6, UInt32(&improved_rand6a));
			::WriteRelJump(Hook_rand7, UInt32(&improved_rand7a));
			::WriteRelJump(Hook_rand8, UInt32(&improved_rand8a));
//		}
//		else {
//		}
	}
#endif
}



