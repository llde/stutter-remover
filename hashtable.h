
struct Hashtable;

int    __fastcall hashint_normal   ( Hashtable *ht, int dummy, UInt32 key );
int __fastcall hashstring_normal   ( Hashtable *ht, int dummy, const char *key );
int    __fastcall hashint_log      ( Hashtable *ht, int dummy, UInt32 key );
int __fastcall hashstring_log      ( Hashtable *ht, int dummy, const char *key );
int    __fastcall hashint_grow     ( Hashtable *ht, int dummy, UInt32 key );
int __fastcall hashstring_grow     ( Hashtable *ht, int dummy, const char *key );
int    __fastcall hashint_grow_log ( Hashtable *ht, int dummy, UInt32 key );
int __fastcall hashstring_grow_log ( Hashtable *ht, int dummy, const char *key );
int    __fastcall hashint_alternate( Hashtable *ht, int dummy, UInt32 key );
int __fastcall hashstring_alternate( Hashtable *ht, int dummy, const char *key );

void set_hashint    ( int (__fastcall *) ( Hashtable *, int, UInt32 ) );
void set_hashstring ( int (__fastcall *) ( Hashtable *, int, const char * ) );

void initialize_hashtable_hooks();
