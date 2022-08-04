
enum {
	MEMORY_BENCHMARK_SIMPLE=0,
	MEMORY_BENCHMARK_ISOLATED=1,
	MEMORY_BENCHMARK_UNIDIRECTIONAL=2,
	MEMORY_BENCHMARK_BIDIRECTIONAL=3,
	MEMORY_BENCHMARK_CHAINED=4,
};

double benchmark_heap(SR_HEAP *heap, int mode, int threads, int milliseconds);
void benchmark_heap(SR_HEAP *heap);

