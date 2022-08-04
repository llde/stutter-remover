
#include <string>
#include <sstream>
#include <set>
#include <map>
#include <vector>
#include <list>
#include "main.h"
#include "struct_text.h"


static bool is_white_space(char c) {return c == ' ' || c == '\t';}

/*bool TextSection::cmpTSindirect::operator() ( const TextSection *ts1, const TextSection *ts2 ) {
	return ts1->name < ts2->name;
}*/

TextSection *TextSection::load_file(const char *filename) {
	std::vector<char> file_contents;
	FILE *file = fopen(filename, "rt");
	if (!file) {
		printf("TextSection::load_file: error opening file \"%s\"\n", filename);
		return NULL;
	}
	file_contents.push_back('{');
	int size = 1;
	for (int max = 4096; max < 1024 * 1024 * 1024; max *= 2) {//max 1 GB file size
		file_contents.resize(max);
		int r = fread(&file_contents[size], 1, max - size-2, file);
		if (r < 0) {
			printf("TextSection::load_file: error reading file \"%s\"\n", filename);
			fclose(file);
			return NULL;
		}
		size += r;
		if (r < max - size - 2) break;
	}
	file_contents[size++] = '}';
	file_contents[size++] = '\n';
	fclose(file);//file fully loaded;
	TextSection *r = NULL;
	r = new TextSection();
	try {
		r->_parse(&file_contents[0], size, 31);
	}
	catch (const _my_exception *e) {
		delete r;
		return NULL;
	}
	return r;
}
bool TextSection::save_file(const char *filename) {
	FILE *file = fopen(filename, "wt");
	if (!file) {
		message("TextSection::save_file: error opening file \"%s\"\n", filename);
		return false;
	}

	std::stringstream buffer;
	serialize(buffer, true, "\t", "");
	int r = fwrite(&buffer.str()[0], 1, buffer.str().size(), file);
	fclose(file);
	if (r != buffer.str().size()) {
		message("TextSection::load_file: error writing file \"%s\"\n", filename);
		return false;
	}
	return true;

/*	std::vector<char> file_contents;
	for (int max = 4096; max < 1024 * 1024 * 1024; max *= 2) {//max 1 GB file size
		file_contents.resize(max);
		int size = serialize(&file_contents[0], max);
		if (size == max) continue;
		int r = fwrite(&file_contents[0], 1, size, file);
		fclose(file);
		if (r != size) {
			error("TextSection::save_file: error writing file \"%s\"\n", filename);
			return false;
		}
		return true;
	}*/
	fclose(file);
	return false;
}

/*
Section:	(Line\n)*
Line:		SimpleLine | MultiLine | Nesting
SimpleLine:	Field = Value\n
MultiLine:	Field = Value \n
Nesting:	Field = Value {Section}\n
	_parse_section
	_parse_line
	_parse_value
*/
class CharSet {
	UInt32 table[256/32];
public:
	CharSet ( const char *chars, int num_chars ) {
		for (int i = 0; i < 256 / 32; i++) table[i] = 0;
		for (int i = 0; i < num_chars; i++) table[chars[i] >> 5] |= (1 << (chars[i] & 31));
	}
	bool operator[]( int ch ) const {return bool(table[ch>>5] & (1 << (ch & 31)));}
};
int strspn(const char *str, const CharSet &cs, int length) {
	int r;
	for (r = 0; r < length; r++) if (!cs[str[r]]) return r;
	return length;
}
int strcspn(const char *str, const CharSet &cs, int length) {
	int r;
	for (r = 0; r < length; r++) if (cs[str[r]]) return r;
	return length;
}
#define MAKE_CHARSET(name, const_str, length) \
	static const CharSet * name = NULL; \
	if (1) {static bool done = false; if (!done) {name = new CharSet(const_str, length);}}

int TextSection::_parse_section ( const char *baseptr, int max_length, int max_level ) {
	MAKE_CHARSET(skip_these, "\n\r \t", 4);
	int off = 0;
	while (1) {
		off += strspn(baseptr+off, *skip_these, max_length - off);
		if (off == max_length) return off;
		if (baseptr[off] == '}') return off;
		off += _parse_line(baseptr+off, max_length - off, max_level);
	}
}
int TextSection::_parse_line ( const char *baseptr, int max_length, int max_level ) {
	MAKE_CHARSET(terminators, "\n\r=}", 4);
	MAKE_CHARSET(whitespace, " \t", 2);
	//begining of name after whitespace
	int off1 = strspn(baseptr, *whitespace, max_length);
	//end of name after whitespace
	int off = strcspn(baseptr+off1, *terminators, max_length-off1);
	//end of name, before whitespace
	int off2;
	for (off2 = off-1; off2 > off1 && (*whitespace)[baseptr[off2]]; off2--) ;
	if (off2 <= off1) {
		error("TextSection::_parse_line: not a valid line");
	}
	TextSection *child = new TextSection();
//	TextSection *child = new TextSection(baseptr+off+1, max_length-off-1, newname);
	child->parent = this;
	children_list.push_back(child);
	child->self_in_list = --children_list.rbegin().base();
	child->self_in_set = children_set.insert(
		std::pair<std::string,TextSection*>(std::string( baseptr+off1, off2-off1+1), child)
	);

	if (off < max_length && baseptr[off] == '=') {
		off += 1;
		int off3 = off;
		int off4 ;
		off += child->_parse_value(baseptr+off, max_length-off, max_level, off4);
		off4 += off3;
		int off5 = off;
		child->raw_value = std::string(baseptr+off3, baseptr+off4);
		child->_value_changed();
	}
	return off;
}
int TextSection::_parse_value ( const char *baseptr, int max_length, int max_level, int &base_len ) {
	int off = 0;
	//look for end of line or begining of nested section
	while (1) {
		MAKE_CHARSET(charset, "{}\n\r", 4);//todo: add backslash
		off += strcspn( baseptr + off, *charset, max_length-off);
		base_len = off;
		if (off == max_length) break;
		if (0) ;
		else if (baseptr[off] == '{') {
			if (!max_level) {
				error("TextSection::_parse_value: max_level exceeded");
			}
			off++;
			off += _parse_section( baseptr + off, max_length - off, max_level - 1 );
			if (off == max_length || baseptr[off] != '}') {
				enum {EXCERPT_LENGTH = 64};
				char buffy[EXCERPT_LENGTH];
				int ml = (max_length < EXCERPT_LENGTH-1) ? max_length : EXCERPT_LENGTH-1;
				strncpy(buffy, baseptr, ml);
				buffy[ml] = 0;
//				error("TextSection: parse failed\n(%s)", buffy);
				error("TextSection: parse failed\n(%s)\n", buffy);
				return off;
			}
			off++;
			break;
		}
//		else if (baseptr[off] = '\\') {
//		}
		else break;//carriage return or line feed or }
	}

//	numerical_value = 0;
	return off;
}
void TextSection::_parse ( const char *raw_text, int length, int max_level ) {
	int baselen;
	int len = _parse_value(raw_text, length, max_level, baselen);
	raw_value = std::string(raw_text, baselen);
	_value_changed();

	if (len != length) {
//		printf("TextSection::_parse: not all characters used\n");
	}
}
//void save ( const char *file );
//void serialize ( char *destination, int max_length );
void TextSection::init_copy ( const TextSection &old ) {
	raw_value = old.raw_value;
	trimmed_value = old.trimmed_value;
	for (ChildrenSetType::const_iterator it = old.children_set.begin(); it != old.children_set.end(); it++) {
		TextSection *child = add_section(it->first);
		child->init_copy(*it->second);
	}
}
TextSection::TextSection( const TextSection &old ) : flags(0), parent(NULL) {
	init_copy(old);
}
TextSection::TextSection( ) : flags(0), parent(NULL) {}
TextSection::TextSection( const char *raw_text, int length ) 
: 
	flags(0), 
	parent(NULL)
{
	_parse(raw_text, length, 32);
}
void TextSection::_tree_kill ( ) {
	parent = NULL;
	delete this;
}
TextSection::~TextSection ( ) {
	if (parent) {
		parent->children_list.erase(self_in_list);
		parent->children_set.erase(self_in_set);
//		parent = NULL;
	}
	std::list<TextSection*>::iterator it;
	for (it = children_list.begin(); it != children_list.end(); it++) {
		(*it)->_tree_kill();
	}
}

double TextSection::get_float() const {
	return atof(trimmed_value.c_str());
}

int TextSection::get_int() const {
	const char *cstr = trimmed_value.c_str();
	int i = strtol(cstr, NULL, 0);
	return i;
}
UInt32 TextSection::get_uint() const {
	const char *cstr = trimmed_value.c_str();
	UInt32 i = strtoul(cstr, NULL, 0);
	return i;
}
SInt64 TextSection::get_int64() const {
	const char *cstr = trimmed_value.c_str();
	SInt64 i = _strtoi64(cstr, NULL, 0);
	return i;
}
UInt64 TextSection::get_uint64() const {
	const char *cstr = trimmed_value.c_str();
	UInt64 i = _strtoui64(cstr, NULL, 0);
	return i;
}
const char *TextSection::get_c_string() const {
	return trimmed_value.c_str();
}
const std::string &TextSection::get_string() const {
	return trimmed_value;
}
const std::string &TextSection::get_raw_string() const {
	return raw_value;
}
const TextSection *TextSection::get_section(std::string name) const {
	ChildrenSetType::const_iterator it = children_set.find(name);
	if (it == children_set.end()) return NULL;
	else return it->second;
}
TextSection *TextSection::get_section(std::string name) {
	if (!this) return NULL;//brutal hack
 	ChildrenSetType::iterator it = children_set.find(name);
	if (it == children_set.end()) return NULL;
	else return it->second;
}
TextSection *TextSection::get_or_add_section(std::string name) {
 	ChildrenSetType::iterator it = children_set.find(name);
	if (it != children_set.end()) return it->second;

	TextSection *child = new TextSection(" ",1);
	child->parent = this;
	children_list.push_back(child);
	child->self_in_list = --children_list.rbegin().base();
	child->self_in_set = children_set.insert(
		std::pair<std::string,TextSection*>(name, child)
	);
	return child;
}
void TextSection::_value_changed() {
	MAKE_CHARSET(whitespace, " \t", 2);
	int a, b;
	for (a = 0; a < raw_value.length() && (*whitespace)[raw_value[a]]; a++) ;
	for (b = raw_value.length()-1; b >= 0 && (*whitespace)[raw_value[b]]; b--) ;
	b++;
	if (b <= a) trimmed_value = std::string();
	else trimmed_value = std::string(&raw_value[a], &raw_value[b]);
}
void TextSection::set_text  ( std::string text ) {
	raw_value = text;
	_value_changed();
}
void TextSection::set_text  ( const char *text ) {
	raw_value = text;
	_value_changed();
}
void TextSection::set_int   ( int value ) {
	char buffy[4096];
	sprintf(buffy, " %d", value);
	set_text(buffy);
}
void TextSection::set_float ( double value ) {
	char buffy[4096];
	sprintf(buffy, " %f", value);
	set_text(buffy);
}
TextSection *TextSection::add_section ( std::string name ) {
	TextSection *child = new TextSection(" ", 1);
	child->parent = this;
	children_list.push_back(child);
	child->self_in_list = --children_list.rbegin().base();
	child->self_in_set = children_set.insert(
		std::pair<std::string,TextSection*>(name, child)
	);
	return child;
}


const std::string root_textsection_name = "(root)";
const std::string &TextSection::get_name() const {
	if (!parent) return root_textsection_name;
	else return self_in_set->first;
}
const TextSection *TextSection::get_first_section() const {
	if (children_list.empty()) return NULL;
	else return *children_list.begin();
}
const TextSection *TextSection::get_last_section() const {
	if (children_list.empty()) return NULL;
	else return *children_list.rbegin();
}
const TextSection *TextSection::get_next_section() const {
	if (!parent) return NULL;
	std::list<TextSection*>::iterator it = self_in_list;
	if (++it == parent->children_list.end()) return NULL;
	else return *it;
}
const TextSection *TextSection::get_prev_section() const {
	if (!parent) return NULL;
	std::list<TextSection*>::iterator it = self_in_list;
	if (--it == parent->children_list.end()) return NULL;
	else return *it;
}

/*int TextSection::serialize ( char *destination, int max_length ) {
	int off = 0;
	if (children_list.empty()) off += _snprintf(destination, max_length, "%s", raw_value.c_str());
	else {
		off += _snprintf(destination+off, max_length-off, "%s{\n", raw_value.c_str());
		for (std::list<TextSection*>::iterator it = children_list.begin(); it != children_list.end(); it++) {
			if (off == max_length) break;
			off += _snprintf(destination+off, max_length-off, "%s =", (*it)->self_in_set->first.c_str());
			off += (*it)->serialize(destination+off, max_length-off);
			off += _snprintf(destination+off, max_length-off, "\n");
		}
		off += _snprintf(destination+off, max_length-off, "}", raw_value.c_str());
	}
	return off;
}*/
void TextSection::serialize ( std::stringstream &dest, bool omit_outer_braces, const std::string &indentation, const std::string &prefix ) {
	if (children_list.empty()) dest << raw_value;
	else {
		std::string prefix2 = omit_outer_braces ? prefix : prefix + indentation;
		if (!omit_outer_braces) dest << raw_value << "{\n";
		for (std::list<TextSection*>::iterator it = children_list.begin(); it != children_list.end(); it++) {
			dest << prefix2 << (*it)->self_in_set->first << " =";
			(*it)->serialize(dest, false, indentation, prefix2);
			dest << "\n";
		}
		if (!omit_outer_braces) dest << prefix << "}";
	}
	return;
}


void TextSection::self_test() {
	const char *sample = "{\nfred = 14\nbob = 1\ncomplex ={\nReal =1\nimag =2\n}\n}";
//	char buffy[4096];
	std::stringstream buffy;
	TextSection ts(sample, strlen(sample));
	ts.serialize(buffy, false, "", "");
	if (!strcmp(sample, buffy.str().c_str())) printf("TextSection::self_test() successful\n");
	else printf("TextSection::self_test(): text did not match\n");
}



/*
extern "C" {
	#include <lua.h>
	#include <lualib.h>
	#include <lauxlib.h>
}

static void _lua_table_to_structured_text ( lua_State *state, TextSection *ts ) {
	lua_pushnil(state);
	while (lua_next(state, -2) != 0) {
		//table, key, value
		const char *lp=NULL, *rp=NULL;
		char buffy_left[512];
		char buffy_right[512];
		switch (lua_type(state, -2)) {
			case LUA_TNUMBER: {
				sprintf(buffy_left, "%f", lua_tonumber(state, -2));
				lp = &buffy_left[0];
			}
			break;
			case LUA_TSTRING: {
				lp = lua_tostring(state, -2);
			}
			break;
		}
		switch (lua_type(state, -1)) {
			case LUA_TNUMBER: {
				sprintf(buffy_right, "%f", lua_tonumber(state, -1));
				rp = &buffy_right[0];
				if (lp && rp) {TextSection *child = ts->add_section(lp); child->set_text(rp);}
			}
			break;
			case LUA_TSTRING: {
				rp = lua_tostring(state, -1);
				if (lp && rp) {TextSection *child = ts->add_section(lp); child->set_text(rp);}
			}
			break;
			case LUA_TTABLE: {
				if (lp && rp) {TextSection *child = ts->add_section(lp); _lua_table_to_structured_text(state, child);}
			}
			break;
		}
		lua_pop(state, 1);
	}
}
static TextSection *lua_table_to_structured_text ( lua_State *state ) {
	if (lua_type(state, -1) != LUA_TTABLE) return NULL;
	TextSection *ts = new TextSection();
	_lua_table_to_structured_text(state, ts);
	return ts;
}

static int lua_save_table_as_structured_text ( lua_State *state ) {
	int numargs = lua_gettop(state);
	if (numargs != 2) error("lua_save_table_as_structured_text: wrong number of parameters");
	size_t namelen;
	const char *nameptr = lua_tolstring(state, -2, &namelen);
	if (!nameptr) error("lua_save_table_as_structured_text: parameter 1 is not a file name");
	TextSection *ts = lua_table_to_structured_text(state);
	char buffy[512];
	_snprintf(buffy, 511, "%s.txt");
	bool r = ts->save_file(buffy);
	lua_pushboolean(state, r);
	return 1;
}*/




