#ifndef _struct_text_h
#define _struct_text_h

typedef int _my_exception;

class TextSection {
/*	class cmpTSindirect {
	public:
		bool operator() ( const TextSection *ts1, const TextSection *ts2 );
		bool operator() ( const TextSection *ts1, std::string ts2 ) {
			return ts1->name < ts2;
		}
		bool operator() ( std::string ts1, const TextSection *ts2 ) {
			return ts1 < ts2->name;
		}
	};*/
//	enum {
//		FLAG_DIRTY,
//		FLAG_FILE,
//		FLAG_NUMERICAL,
//		FLAG_INTEGER,
//		FLAG_TREE
//	};
	UInt32 flags;
//	std::string name;
	std::string raw_value;
	std::string trimmed_value;

//	double numerical_value;

	typedef std::multimap<std::string, TextSection*> ChildrenSetType;
	ChildrenSetType children_set;
	ChildrenSetType::iterator self_in_set;//only valid if parent is non-NULL
	std::list<TextSection *> children_list;
	std::list<TextSection *>::iterator self_in_list;//only valid if parent is non-NULL

	TextSection *parent;

	int _parse_value   ( const char *baseptr, int max_length, int max_level, int &base_len );
	int _parse_section ( const char *baseptr, int max_length, int max_level );
	int _parse_line    ( const char *baseptr, int max_length, int max_level );
	void _parse ( const char *raw_text, int length, int max_level );
	void _tree_kill();
	void init_copy ( const TextSection &old );
	void _value_changed();
public:
	int get_num_children() const {return children_list.size();}
//	typedef std::list<TextSection *>::iterator iterator;
	int get_int () const;
	double get_float () const;
	UInt32 get_uint () const;
	SInt64 get_int64 () const;
	UInt64 get_uint64 () const;
	const char *get_c_string() const;
	const std::string &get_raw_string() const;
	const std::string &get_string() const;
	const TextSection *get_section(std::string name) const;
	TextSection *get_section(std::string name);

	const std::string &get_name() const;
	const TextSection *get_first_section() const;
	TextSection *get_first_section() {
		return const_cast<TextSection*>(const_cast<const TextSection*>(this)->get_first_section());}
	const TextSection *get_last_section() const;
	TextSection *get_last_section() {
		return const_cast<TextSection*>(const_cast<const TextSection*>(this)->get_last_section());}
	const TextSection *get_next_section() const;
	TextSection *get_next_section() {
		return const_cast<TextSection*>(const_cast<const TextSection*>(this)->get_next_section());}
	const TextSection *get_prev_section() const;
	TextSection *get_prev_section() {
		return const_cast<TextSection*>(const_cast<const TextSection*>(this)->get_prev_section());}

	void set_text  ( std::string text );
	void set_text  ( const char *text );
	void set_int   ( int value );
	void set_float ( double value );
	TextSection *add_section ( std::string name );
	TextSection *get_or_add_section ( std::string name );
/*
//	const char *get_name() const {return name;}

	TextSection *get_child( const char *name_ );
	const TextSection *get_child( const char *name_ ) const;
	TextSection *get_next_child( TextSection *child );
	const TextSection *get_next_child( TextSection *child ) const;
	TextSection *add_child( const char *name_, const char *raw_text );
	TextSection *add_child( const char *name_, TextSection *node );
	void remove_child( const char *name_ );
	void remove_child( TextSection *child );

	void save ( const char *file );*/
//	int serialize ( char *destination, int max_length );
	void serialize ( std::stringstream &dest, bool omit_outer_braces=false, const std::string &indentation="\t", const std::string &prefix="" );
	TextSection ( const char *raw_text, int length );
	TextSection ( );
	TextSection ( const TextSection &old );
	~TextSection ( );
	static TextSection *load_file(const char *filename );
	bool save_file(const char *filename );

//	bool operator< ( const TextSection &other ) const;
//	bool operator< ( std::string str ) const {return str < name;}

	static void self_test();
};


#endif//_struct_text_h
