
struct SR_HEAP {
	void* ( * malloc)         ( size_t size );
	void  ( * free)           ( void *mem );
	void* ( * realloc)        ( void *mem, size_t size );
	size_t( * get_size)       ( void *mem );
	bool  ( * is_in_heap_fast)( void *mem );
	bool  ( * is_in_heap)     ( void *mem );
	void  ( * init)           ( );
	size_t( * get_bytes_used) ( );
	void  ( * CreatePool)     ( size_t size );
	int   ( * MemStats)       ( );
	void  ( * FreePages)      ( );
};
extern SR_HEAP heap;//active heap (if using heap replacement)
extern SR_HEAP heap_SimpleHeap1;
extern SR_HEAP heap_ThreadHeap2;
extern SR_HEAP heap_ThreadHeap3;
extern SR_HEAP heap_Windows;
extern SR_HEAP heap_Oblivion;//works with or without heap replacement

bool optimize_memoryheap(int mode, bool do_profiling);

void *allocate_from_memoryheap ( size_t size );
void  free_to_memoryheap       ( void *ptr   );
void *realloc_from_memoryheap  ( void *ptr, size_t size );
