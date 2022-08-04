namespace SimpleHeap1 {
	void *allocate(size_t size);
	void deallocate(void *mem);
	void *reallocate(void *mem, size_t size);
	bool is_in_heap_fast(void *mem);
	bool is_in_heap(void *mem);
	size_t get_size(void *mem);
	void init();
}
namespace ThreadHeap1 {
	void *allocate(size_t size);
	void deallocate(void *mem);
	void *reallocate(void *mem, size_t size);
	void init();
}
namespace SimpleHeap2 {
	void *allocate(size_t size);
	void deallocate(void *mem);
	void *reallocate(void *mem, size_t size);
	void init();
}
namespace ThreadHeap2 {
	void *allocate(size_t size);
	void deallocate(void *mem);
	void *reallocate(void *mem, size_t size);
	bool is_in_heap_fast(void *mem);
	size_t get_size(void *mem);
	void init();
}
namespace DumbHeap1 {
	void *allocate(size_t size);
	void deallocate(void *mem);
	void *reallocate(void *mem, size_t size);
	void init();
}
namespace ThreadHeap3 {
	void *allocate(size_t size);
	void deallocate(void *mem);
	void *reallocate(void *mem, size_t size);
	bool is_in_heap_fast(void *mem);
	size_t get_size(void *mem);
	void init();
	size_t get_bytes_used();
}
