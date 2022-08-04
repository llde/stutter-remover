#include <string>
#include <sstream>
#include <list>
#include <set>
#include <map>
#include <cmath>
#include <queue>

#include "main.h"
#include "struct_text.h"
#include <shlobj.h>
#include <shlwapi.h>
#pragma comment (lib, "shlwapi.lib")
#pragma comment (lib, "winmm.lib")
#	define DECLARE_SETTING_SECTION(section)
#	define DECLARE_SETTING_C(section,name,default_value)
#	define DECLARE_SETTING_I(section,name,default_value,comment) namespace Settings {namespace section {int name;}}
#	define DECLARE_SETTING_F(section,name,default_value,comment) namespace Settings {namespace section {float name;}}
#	define DECLARE_SETTING_B(section,name,default_value,comment) namespace Settings {namespace section {bool name;}}
#include "settings.h"
#	undef DECLARE_SETTING_SECTION
#	undef DECLARE_SETTING_C
#	undef DECLARE_SETTING_I
#	undef DECLARE_SETTING_F
#	undef DECLARE_SETTING_B


#if defined OBLIVION
	#define RELATIVE_INI_FILE_PATH "Data\\obse\\plugins\\"
	#define PLUGIN_NAME   "sr_Oblivion_Stutter_Remover"
	#define LOG_FILE_NAME PLUGIN_NAME ## ".log"
	#define INI_FILE_NAME PLUGIN_NAME ## ".ini"
	#define EXE_FILE_NAME "Oblivion.exe";
#elif defined FALLOUT
	#define RELATIVE_INI_FILE_PATH "Data\\fose\\plugins\\"
	#define PLUGIN_NAME "sr_Fallout_Stutter_Remover"
	#define LOG_FILE_NAME PLUGIN_NAME ## ".log"
	#define INI_FILE_NAME PLUGIN_NAME ## ".ini"
	#define EXE_FILE_NAME "Fallout3.exe";
#elif defined NEW_VEGAS
	#define RELATIVE_INI_FILE_PATH "Data\\nvse\\plugins\\"
	#define PLUGIN_NAME "sr_New_Vegas_Stutter_Remover"
	#define LOG_FILE_NAME PLUGIN_NAME ## ".log"
	#define INI_FILE_NAME PLUGIN_NAME ## ".ini"
	#define EXE_FILE_NAME "FalloutNV.exe";
#else
	#error Oblivion or Fallout 3 or Fallout New Vegas?
#endif

TextSection *config = NULL;

static bool create_settings(char *path) {
	int dummy_value = -777;
	int fps_min = dummy_value;
	int fps_max = dummy_value;
	int sleep_extra  = dummy_value;
	int spam_frametimes = dummy_value;

	message("Creating new .ini file at path %s", path);

//	char buffy[4096];
//#	define DECLARE_SETTING_SECTION(section) ;
//#	define DECLARE_SETTING_I(section,name,default_value) sprintf(buffy, "%d", int(default_value));      WritePrivateProfileStringA(#section,#name,buffy,path);
//#	define DECLARE_SETTING_F(section,name,default_value) WritePrivateProfileStringA(#section,#name,default_value,path);
//#	define DECLARE_SETTING_B(section,name,default_value) sprintf(buffy, "%d", (default_value) ? 1 : 0); WritePrivateProfileStringA(#section,#name,buffy,path);
//#	include "settings.h"
//#	undef DECLARE_SETTING_SECTION
//#	undef DECLARE_SETTING_I
//#	undef DECLARE_SETTING_F
//#	undef DECLARE_SETTING_B
	TextSection ts;
#	define DECLARE_SETTING_SECTION(section) ;
#	define DECLARE_SETTING_C(section,name,default_value) ts.get_or_add_section(#section)->add_section(#name)->set_text(default_value);
#	define DECLARE_SETTING_I(section,name,default_value,comment) ts.get_or_add_section(#section)->add_section(#name)->set_int(default_value);
#	define DECLARE_SETTING_F(section,name,default_value,comment) ts.get_or_add_section(#section)->add_section(#name)->set_text(default_value);
#	define DECLARE_SETTING_B(section,name,default_value,comment) ts.get_or_add_section(#section)->add_section(#name)->set_int(default_value ? 1 : 0);
#	include "settings.h"
#	undef DECLARE_SETTING_SECTION
#	undef DECLARE_SETTING_C
#	undef DECLARE_SETTING_I
#	undef DECLARE_SETTING_F
#	undef DECLARE_SETTING_B
	TextSection *orl = ts.add_section("OverrideList");
	TextSection *or;
#if defined OBLIVION
#define CS_BY_CALLER(caller,mode,spin,description) or=orl->add_section("CriticalSection");or->add_section("CallerAddress")->set_text(caller);or->add_section("comment")->set_text(description);if (mode!=-1) or->add_section("Mode")->set_int(mode);if (spin!=-1) or->add_section("Spin")->set_int(spin)
#define CS_BY_OBJECT(object,mode,spin,description) or=orl->add_section("CriticalSection");or->add_section("ObjectAddress")->set_text(object);or->add_section("comment")->set_text(description);if (mode!=-1) or->add_section("Mode")->set_int(mode);if (spin!=-1) or->add_section("Spin")->set_int(spin)
#define HASHTABLE(address,osize,nsize,bits,description) or=orl->add_section("Hashtable");or->add_section("comment")->set_text(description);or->add_section("SizeAddress")->set_text(address);or->add_section("OldSize")->set_int(osize);or->add_section("NewSize")->set_int(nsize);if (bits!=32) or->add_section("WordBits")->set_int(bits)
	CS_BY_CALLER(" 0x701748", 5,-1, " Renderer+0x180, recommendation=suppress (mode 5)");
	CS_BY_OBJECT(" 0xB32B80", 3,1500, " MemoryHeap CS, recommendation=stagger (mode 3)");
	CS_BY_CALLER(" 0x70172A", 2,-1, " Renderer+0x80, recommendation= modes 2(for stability) or 5(for performance)");
	CS_BY_OBJECT(" 0xB3FA00", 3,-1, " Unknown4, recommendation=stagger (mode 3)");
	CS_BY_OBJECT(" 0xB33800", 3,-1, " BaseExtraList, recommendation=stagger (mode 3)");
	CS_BY_OBJECT(" 0xB3F600", 3,-1, " recommendation=stagger (mode 3)");
	CS_BY_OBJECT(" 0xB3FC00", 2,-1, "");
	CS_BY_OBJECT(" 0xB39C00", 2,-1, "");
	HASHTABLE(" 0x00418DDB",    37,   149, 32, "caller 0x00418E16");
	HASHTABLE(" 0x0045A866",  5039,133123, 32, "caller 0x0045a8a1");
	HASHTABLE(" 0x004A2586",   523,  2711, 32, "caller 0x004A25BC");
	HASHTABLE(" 0x004E610F",    37,    47, 32, "multipart 1/2 - caller 0x004e614f");
	HASHTABLE(" 0x004E612C",    37,    47, 32, "multipart 2/2 - caller 0x004e614f");
	HASHTABLE(" 0x004E8FD7",    37,   739, 32, "caller 0x004E9014");
	HASHTABLE(" 0x004F1B44",    37,   127,  8, "caller 0x004f0e20");
	HASHTABLE(" 0x004F220A",  7001,  7001, 32, "caller 0x004f1d60");
	HASHTABLE(" 0x004F222E",   701,   901, 32, "also caller 0x004f1d60");
	HASHTABLE(" 0x004F2B70",    37,   127,  8, "also caller 0x004f1d60");
	HASHTABLE(" 0x004F2A8B",    37,   713, 32, "multipart 1/2 - caller 0x004F2ACB");
	HASHTABLE(" 0x004F2AA8",    37,   713, 32, "multipart 2/2 - caller 0x004F2ACB");
	HASHTABLE(" 0x004F2AEF",    37,  1301, 32, "multipart 1/2 - caller 0x004f2b3e");
	HASHTABLE(" 0x004F2B12",    37,  1301, 32, "multipart 2/2 - caller 0x004f2b3e");
	HASHTABLE(" 0x006C5396",    37,    83, 32, "caller 0x0067fbb0");
	HASHTABLE(" 0x0067FD35",   191,  3019, 32, "also caller 0x0067fbb0");
	HASHTABLE(" 0x0067FE5F",   191,  2021, 32, "also caller 0x0067fbb0");
	HASHTABLE(" 0x006C5674",    37,   299, 32, "caller 0x006C56B0");
	HASHTABLE(" 0x00714752",    59,   239, 32, "caller 0x00714788");
	HASHTABLE(" 0x00769BEB",    37,   297, 32, "many callers: 0x00769C3D, 0x00769CAD, 0x00769D03, 0x00769D53, 0x00769DA1");
	HASHTABLE(" 0x009DBF03",131213,905671, 32, "multipart 1/2 - caller 0x009dbf36");
	HASHTABLE(" 0x00B06140",131213,905671, 32, "multipart 2/2 - caller 0x009dbf36");
	HASHTABLE(" 0x009E26F3",    37,   297, 32, "caller 0x009e2726");
	HASHTABLE(" 0x00A10DB3",    37,   297, 32, "caller 0x00a10de6");
#elif defined FALLOUT
#define CS_BY_CALLER(caller,mode,spin,description) or=orl->add_section("CriticalSection");or->add_section("CallerAddress")->set_text(caller);or->add_section("comment")->set_text(description);if (mode!=-1) or->add_section("Mode")->set_int(mode);if (spin!=-1) or->add_section("Spin")->set_int(spin)
#define CS_BY_OBJECT(object,mode,spin,description) or=orl->add_section("CriticalSection");or->add_section("ObjectAddress")->set_text(object);or->add_section("comment")->set_text(description);if (mode!=-1) or->add_section("Mode")->set_int(mode);if (spin!=-1) or->add_section("Spin")->set_int(spin)
#define HASHTABLE(address,osize,nsize,bits,description) or=orl->add_section("Hashtable");or->add_section("comment")->set_text(description);or->add_section("SizeAddress")->set_text(address);or->add_section("OldSize")->set_int(osize);or->add_section("NewSize")->set_int(nsize);if (bits!=32) or->add_section("WordBits")->set_int(bits)
#define HASHTABLE_SPECIAL(type,address,osize,nsize,description) or=orl->add_section(type);or->add_section("comment")->set_text(description);or->add_section("Address")->set_text(address);or->add_section("OldSize")->set_int(osize);or->add_section("NewSize")->set_int(nsize)
	CS_BY_CALLER(" 0x828509", 5,-1, " Renderer+0x180, recommendation=suppress (mode 5)");
	CS_BY_CALLER(" 0x8284F7", 2,-1, " Renderer+0x80, recommendation=fair or suppress (modes 2 or 5)");
	CS_BY_CALLER(" 0x86E55A", 3,-1, " Mem2 CS, recommendation=stagger (mode 3)");
#define LCS_BY_CALLER(caller,mode,spin,description) or=orl->add_section("LightCriticalSection");or->add_section("CallerAddress")->set_text(caller);or->add_section("comment")->set_text(description);if (mode!=-1) or->add_section("Mode")->set_int(mode);if (spin!=-1) or->add_section("Spin")->set_int(spin)
#define LCS_BY_OBJECT(object,mode,spin,description) or=orl->add_section("LightCriticalSection");or->add_section("ObjectAddress")->set_text(object);or->add_section("comment")->set_text(description);if (mode!=-1) or->add_section("Mode")->set_int(mode);if (spin!=-1) or->add_section("Spin")->set_int(spin)
//	LCS_BY_CALLER(" 0x86D7EA", 3,-1, " MemoryPool (multiple LCSes), recommendation=stagger (mode 3)");
//	LCS_BY_CALLER(" 0x86D94C", 3,-1, " MemoryPool alternate caller (multiple LCSes), recommendation=stagger (mode 3)");
//	LCS_BY_OBJECT(" 0x1079FE0", 2,-1, " GarbageCollector, recommendation=fair (mode 2)");
//	LCS_BY_OBJECT(" 0x106C980", " ExtraData, no recommendation", -1);
//	LCS_BY_OBJECT(" 0xDCD0A8", " TESObjectCell::CellRefLockEnter, no recommendation", -1);
	//LCS at 0x0106C980, ExtraData
	//LCS at 0x00DCD0A8, TESObjectCell::CellRefLockEnter (called from only 1 function, but that func is inlined all over the place)
	//LCS at 0x01079FE0, GarbageCollector
	//LCS at NavMeshObstacleManager+0x100, called from 0x005E920D among other places
	//LCSes called from 0x86D7E5
	//unknown static LCSes at 106E5A0, 108F080, ...
	HASHTABLE_SPECIAL("HashtableEarlyIndirect", " 0x106DD14",131213,161213, "vtable 0x00dbd2d0");
	HASHTABLE_SPECIAL("HashtableEarly", " 0x0106A3D8",37,5701, "vtable 0x00ea96ac");
	HASHTABLE_SPECIAL("HashtableEarly", " 0x0106A3C8",37,59, "vtable 0x00ea970c");
	HASHTABLE_SPECIAL("HashtableEarly", " 0x00F4FA78",37,493, "vtable 0x00dcd35c");

	HASHTABLE(" 0x00448589", 1001, 1001, 32, "caller 0x00447BC4");
	HASHTABLE(" 0x004560FB", 4001, 4001, 32, "caller 0x00456024");
	HASHTABLE(" 0x004989D2",   37,   177, 32, "multipart 1/5, callers 0x00498A04, 0x00498A3A, 0x00498A6B, 0x00498AAC");
	HASHTABLE(" 0x004989D7", 37*4, 177*4, 32, "multipart 2/5, size*4");
	HASHTABLE(" 0x00498A0D", 37*4, 177*4, 32, "multipart 3/5, size*4");
	HASHTABLE(" 0x00498A3E", 37*4, 177*4, 32, "multipart 4/5, size*4");
	HASHTABLE(" 0x00498A73", 37*4, 177*4, 32, "multipart 5/5, size*4");
	HASHTABLE(" 0x004C0B04", 37*4, 177*4, 32, "multipart 1/2, caller 0x004c0b34, size*4");
	HASHTABLE(" 0x004C0B11",   37,   177, 32, "multipart 2/2, caller 0x004c0b34");
	HASHTABLE(" 0x00505AAA",   37,   471, 32, "multipart 1/4, callers 0x00505adc & 0x00505B29 & 00505BBA");
	HASHTABLE(" 0x00505AAF", 37*4, 471*4, 32, "multipart 2/4, size*4");
	HASHTABLE(" 0x00505AED", 37*4, 471*4, 32, "multipart 3/4, size*4");
	HASHTABLE(" 0x00505B92", 37*4, 471*4, 32, "multipart 4/4, size*4");
	HASHTABLE(" 0x00612256",   37,   421, 32, "multipart 1/3, caller 0x0061228d");
	HASHTABLE(" 0x0061225B", 37*4, 421*4, 32, "multipart 2/3, caller 0x0061228d, size*4");
	HASHTABLE(" 0x00612298", 37*4, 421*4, 32, "multipart 3/3, caller 0x0061228d, size*4");
	HASHTABLE(" 0x006CFBC7", 5039, 8333, 32, "callers 0x006ce22c & 0x006ce25b");
	HASHTABLE(" 0x006CFBF7",   37,   37,  8, "also callers 0x006ce22c & 0x006ce25b");
	HASHTABLE(" 0x006D0A52", 5039, 8339, 32, "also callers 0x006ce22c & 0x006ce25b");
	HASHTABLE(" 0x0082CE97", 59*4, 413*4, 32, "multipart 1/2, caller 0x0082cec2, size*4");
	HASHTABLE(" 0x0082CEA4",   59,   413, 32, "multipart 2/2, caller 0x0082cec2");
	HASHTABLE(" 0x008A9338", 37*4, 271*4, 32, "multipart 1/2, caller 0x008a9364, size*4");
	HASHTABLE(" 0x008A9346",   37,   271, 32, "multipart 2/2, caller 0x008a9364");

#elif defined NEW_VEGAS
#define CS_BY_CALLER(caller,mode,spin,description) or=orl->add_section("CriticalSection");or->add_section("CallerAddress")->set_text(caller);or->add_section("comment")->set_text(description);if (mode!=-1) or->add_section("Mode")->set_int(mode);if (spin!=-1) or->add_section("Spin")->set_int(spin);or->add_section("Version")->set_text(version)
#define CS_BY_OBJECT(object,mode,spin,description) or=orl->add_section("CriticalSection");or->add_section("ObjectAddress")->set_text(object);or->add_section("comment")->set_text(description);if (mode!=-1) or->add_section("Mode")->set_int(mode);if (spin!=-1) or->add_section("Spin")->set_int(spin);or->add_section("Version")->set_text(version)
#define HASHTABLE(address,osize,nsize,bits,description) or=orl->add_section("Hashtable");or->add_section("comment")->set_text(description);or->add_section("SizeAddress")->set_text(address);or->add_section("OldSize")->set_int(osize);or->add_section("NewSize")->set_int(nsize);if (bits!=32) or->add_section("WordBits")->set_int(bits);or->add_section("Version")->set_text(version)
#define HASHTABLE_SPECIAL(type,address,osize,nsize,description) or=orl->add_section(type);or->add_section("comment")->set_text(description);or->add_section("Address")->set_text(address);or->add_section("OldSize")->set_int(osize);or->add_section("NewSize")->set_int(nsize);or->add_section("Version")->set_text(version)
	const char *version;
	version = "FalloutNV 1.4.0.525";
	CS_BY_CALLER(" 0xA62B29",  5,-1, " Renderer+0x180, recommendation=suppress (mode 5)");
	CS_BY_CALLER(" 0xA62B17",  2,-1, " Renderer+0x080, recommendation=fair (mode 2) or stagger (mode 3) or suppress (mode 5)");
	CS_BY_CALLER(" 0xA044FE",  3,6000, " ?, recommendation=stagger (mode 3), maybe high spin?");
	CS_BY_CALLER(" 0xA5B577",  3,-1, " ?, recommendation=stagger (mode 3)");
	CS_BY_CALLER(" 0x4538F1",  2,-1, " ?, recommendation=fair (mode 2)");


	HASHTABLE_SPECIAL("HashtableEarly", " 0x011F3358", 37, 8701, " vtable:0x01094e7c");
	HASHTABLE_SPECIAL("HashtableEarly", " 0x011F3308", 37, 371, " vtable:0x01094e3c, caller 0x00A0D777, important during initial game loading?");
	HASHTABLE(" 0x0084AB60",    37,      117,  8, "caller 0x0084B7AB, vtable 0x0107f494, may be active during loading?");
	HASHTABLE(" 0x00473F69",   1001,    5005, 32, "caller 0x004746BB");
	//HASHTABLE(" 0x00483A99", 131213,  161213, 32, "caller 0x004869EB ???");//not working
	HASHTABLE(" 0x00582CA2",     37,     119,  8, "caller 0x0058911b");//ought to be bigger, but it's a signed 8 bit value
	HASHTABLE(" 0x00587AC9",     37,      43,  8, "also caller 0x0058911b");
	HASHTABLE(" 0x00582CEF",     37,      49,  8, "caller 0x0058921B");
	HASHTABLE(" 0x00582D64",     37,      31,  8, "caller 0x0058931b");
	HASHTABLE(" 0x00583F90",   7001,    7001, 32, "also caller 0x0058931b");
	HASHTABLE(" 0x00583FF6",    701,    1703, 32, "also caller 0x0058931b");
	HASHTABLE(" 0x006B5C76",  10009,   10009, 32, "caller 0x006b7f0b");
	HASHTABLE(" 0x006B7A30",   1009,    2809, 32, "caller 0x006B81BB");
	HASHTABLE(" 0x006C02F8",     37,     121,  8, "caller 0x006c62bb - this one is important");//important, ought to be bigger
	HASHTABLE(" 0x006C035F",     37,      95,  8, "also caller 0x006c62bb");
	HASHTABLE(" 0x006C0397",     37,      97,  8, "also caller 0x006c62bb");
	HASHTABLE(" 0x006C03AB",     37,      89,  8, "caller 0x006c6b6b");
	HASHTABLE(" 0x006E13AF",     37,      53,  8, "caller 0x006e213b");
	HASHTABLE(" 0x00845558",   5039,    7049, 32, "caller 0x00845BEB");//
	HASHTABLE(" 0x008470FA",     37,      55,  8, "also caller 0x006e213b");
	HASHTABLE(" 0x00846FFB",   5039,    5031, 32, "also caller 0x006e213b");
	HASHTABLE(" 0x0084703E",     37,      57,  8, "also caller 0x006e213b");
	HASHTABLE(" 0x00848072",   5039,   12041, 32, "also caller 0x006e213b");
	HASHTABLE(" 0x00544FA7",     37,      39,  8, "caller 0x00558F0B");
	HASHTABLE(" 0x00544FC9",     37,	  29,  8, "also caller 0x00558F0B");
	HASHTABLE(" 0x00AD9169",     37,     111,  8, "address 0x011F6F44, should be caller 0x00AE7BA7, but showing up as NULL");
	HASHTABLE(" 0x00AD9189",     37,     111,  8, "address 0x011F6F54, should be caller 0x00AE7C27, but showing up as NULL");
	HASHTABLE(" 0x00AD91A9",     37,     111,  8, "address 0x011F6F64, should be caller 0x00AE7C27, but showing up as NULL");
	HASHTABLE(" 0x00AD91CC",     37,      39,  8, "address 0x011F6F74, should be caller 0x00AE7CA7, but showing up as NULL");
	HASHTABLE(" 0x00A2EFDF", 37 * 4, 151 * 4, 32, "caller 0x00a2f00b, multiplied by 4");
	HASHTABLE(" 0x00A2EFED", 37 * 1, 151 * 1, 32, "caller 0x00a2f00b, must be 1/4th of the preceding one");
	HASHTABLE(" 0x00A660B7", 59 * 4, 159 * 4, 32, "caller 0x00a660e2, multiplied by 4");
	HASHTABLE(" 0x00A660C4", 59 * 1, 159 * 1, 32, "caller 0x00a660e2, must be 1/4th of the preceding one");
	HASHTABLE(" 0x00B61841",101 * 4, 301 * 4, 32, "caller 0x00B61872, multiplied by 4");
	HASHTABLE(" 0x00B61854",101 * 1, 301 * 1, 32, "caller 0x00B61872, must be 1/4th of the preceding one");
	HASHTABLE(" 0x00B7FF73", 37 * 4, 247 * 4, 32, "caller 0x00B7FFA8, multiplied by 4");
	HASHTABLE(" 0x00B7FF85", 37 * 1, 247 * 1, 32, "caller 0x00B7FFA8, must be 1/4th of the preceding one");
	HASHTABLE(" 0x00B9A5EB", 37 * 4, 157 * 4, 32, "caller 0x00b9a61b, multiplied by 4");
	HASHTABLE(" 0x00B9A5FD", 37 * 1, 157 * 1, 32, "caller 0x00b9a61b, must be 1/4th of the preceding one");
	


	version = "FalloutNV 1.3.0.452";
	CS_BY_CALLER(" 0xA5D9F7",  5,-1, " Renderer+0x180, recommendation=suppress (mode 5)");
	CS_BY_CALLER(" 0xA5FA67",  2,-1, " Renderer+0x080, recommendation=fair (mode 2) or suppress (mode 5)");
	version = "FalloutNV 1.2.0.352";
	CS_BY_CALLER(" 0xA5D9F7",  5,-1, " Renderer+0x180, recommendation=suppress (mode 5)");
	CS_BY_CALLER(" 0xA5D9E5",  2,-1, " Renderer+0x080, recommendation=fair (mode 2) or suppress (mode 5)");
	version = "FalloutNV 1.2.0.314";
	CS_BY_CALLER(" 0xA5DB09",  5,-1, " Renderer+0x180, recommendation=suppress (mode 5)");
	CS_BY_CALLER(" 0xA5DAF7",  2,-1, " Renderer+0x080, recommendation=fair (mode 2) or suppress (mode 5)");
	CS_BY_CALLER(" 0x9FF32E",  3,-1, " ???, recommendation=stagger (mode 3)");
	CS_BY_CALLER(" 0xAFCE77",  3,-1, " ???, recommendation=stagger (mode 3)");
	CS_BY_CALLER(" 0xAD40E8", -1,-1, " ???, recommendation=???");
	CS_BY_CALLER(" 0xA56607",  3,-1, " ???, recommendation=stagger (mode 3)");
	HASHTABLE_SPECIAL("HashtableEarlyIndirect", " 0x011BE3C0",131213,161219, "vtable 0x010169C8");
	HASHTABLE(" 0x4ACBA7",   257,   307, 32, "caller 0x004AFA4B");
	HASHTABLE(" 0x4EE211",   257,   311, 32, "also caller 0x004AFA4B");
	HASHTABLE(" 0x652607",   257,   313, 32, "also caller 0x004AFA4B");
	HASHTABLE(" 0x8724C4",   257,   317, 32, "also caller 0x004AFA4B");
	HASHTABLE(" 0xA586F8",   257,   319, 32, "also caller 0x004AFA4B");
	HASHTABLE(" 0xB525E9",   257,   323, 32, "also caller 0x004AFA4B");
	HASHTABLE(" 0xB65467",   257,   329, 32, "also caller 0x004AFA4B");
	HASHTABLE(" 0xCB4F06",   257,   331, 32, "also caller 0x004AFA4B");
	HASHTABLE(" 0x6B6030",  1009,  2501, 32, "caller 0x006B670B");
	HASHTABLE(" 0x84441B",  5039, 51121, 32, "caller 0x006E01DB");
	HASHTABLE(" 0x845492",  5039, 52213, 32, "also caller 0x006E01DB");
	HASHTABLE(" 0x842958",  5039, 70103, 32, "caller 0x0084302B");
	HASHTABLE(" 0x84F698",    37,   121,  8, "caller 0x0084FF9B, max 127");
	HASHTABLE(" 0x84F917",    37,   121,  8, "caller 0x0085014B, max 127");
	HASHTABLE(" 0x84F937",    37,   121,  8, "caller 0x0085024B, max 127");
	HASHTABLE(" 0x851D28",  5039,  5039, 32, "caller 0x008604AB");
	HASHTABLE(" 0xA61057",  4*59, 4*293, 32, "caller 0x00A61082, 1 of 2, multiplied by 4");
	HASHTABLE(" 0xA61064",    59,   293, 32, "caller 0x00A61082, 2 of 2");
	HASHTABLE(" 0xA29E9F",  4*37, 4*401, 32, "caller 0x00A29ECB, 1 of 2, multiplied by 4");
	HASHTABLE(" 0xA29EAD",    37,   401, 32, "caller 0x00A29ECB, 2 of 2");
	HASHTABLE(" 0xAD4009",    37,    71,  8, "caller 0x00AE25F7, max 127");
	HASHTABLE(" 0xAD4029",    37,    73,  8, "caller 0x00AE2677, max 127");
	HASHTABLE(" 0xAD4049",    37,    79,  8, "also caller 0x00AE2677, max 127");
	HASHTABLE(" 0xAD406C",    37,    81,  8, "caller 0x00AE26F7, max 127");
	version = "FalloutNV 1.2.0.285";
	CS_BY_CALLER(" 0xA5D129", 5,-1, " Renderer+0x180, recommendation=suppress (mode 5)");
	CS_BY_CALLER(" 0xA5D117", 2,1500, " Renderer+0x080, recommendation=fair (mode 2)");
	CS_BY_CALLER(" 0xFACB0E", 3,1500, " high contention CS, recommendation=stagger (mode 3)");
	CS_BY_CALLER(" 0x6805A1", 2,-1, " unknown set of CSes, recommendationfair (mode 2)");
	CS_BY_CALLER(" 0xAA3302", 2,-1, " unkown CS, recommendation=fair (mode 2)");
//	HASHTABLE(" 0x00483BF9", 131213, 181213, 32, "");
//	HASHTABLE(" 0x00569259", 37, 93,  8, "");
//	HASHTABLE(" 0x0058EB27", 37, 97,  8, "");
//	HASHTABLE(" 0x00420793", 37, 101, 8, "");
//	HASHTABLE(" 0x004A7A3F", 37, 103, 8, "");
#endif
	if (!ts.save_file(path)) {
		message("failed to create new configuration file");
		return false;
	}
	return true;
}

static bool get_ini_path ( char *buffer, int bufsize ) {
	if (!GetModuleFileName(GetModuleHandle(NULL), &buffer[0], bufsize - 64)) {
		return false;
	}
	char *slash = strrchr(buffer, '\\');
	if (!slash) return false;
	slash[1] = 0;
	strcat(buffer, RELATIVE_INI_FILE_PATH);
	if (!PathFileExistsA(buffer)) {
		return false;
	}
	strcat(buffer, INI_FILE_NAME);
	return true;
}

void load_settings() {
	char path[8192];
	if (!get_ini_path(path, 8192)) {
		message("Stutter Remover: failed to find ini path");
	}

	config = TextSection::load_file(path);
	if (!config || !config->get_section("Master")) {
		delete config;
		if (create_settings(path)) {
			config = TextSection::load_file(path);
			if (!config) error("failed to load configuration file, even after creating it");
		}
		message("using default configuration without a configuration file");
	}

	TextSection *ts;
#	define DECLARE_SETTING_SECTION(section) ts = config->get_section(#section); if (!ts) config->add_section(#section);
#	define DECLARE_SETTING_C(section,name,default_value) ;
#	define DECLARE_SETTING_I(section,name,default_value,comment) ts = config->get_section(#section)->get_section(#name); Settings :: section :: name = ts ? ts->get_int() : default_value;
#	define DECLARE_SETTING_F(section,name,default_value,comment) ts = config->get_section(#section)->get_section(#name); Settings :: section :: name = ts ? float(ts->get_float()) : float(atof(default_value));
#	define DECLARE_SETTING_B(section,name,default_value,comment) ts = config->get_section(#section)->get_section(#name); Settings :: section :: name =(ts ? ts->get_int() : default_value) ? true : false;
#	include "settings.h"
#	undef DECLARE_SETTING_SECTION
#	undef DECLARE_SETTING_C
#	undef DECLARE_SETTING_I
#	undef DECLARE_SETTING_F
#	undef DECLARE_SETTING_B
}
