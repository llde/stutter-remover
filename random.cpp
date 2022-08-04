#include "main.h"
#include "settings.h"
#include "locks.h"
//#include "random.h"


UInt16 RNG_fast32_16::refill() {
	enum {LAG = 5, C_LAG = LENGTH - LAG};
	int i;
	for (i = 0; i < LAG; i++) cbuf[i] += _lrotr(cbuf[i+C_LAG], 9);
	for (; i < LENGTH; i++)   cbuf[i] += _lrotr(cbuf[i-  LAG], 9);

	left16 = LENGTH*2;
	return results[--left16];
}
void RNG_fast32_16::seed_64(UInt64 s) {
	UInt32 s1 = UInt32(s), s2 = UInt32(s>>32);
	for (int i = 0; i < LENGTH; i++) {
		UInt32 tmp = s1 * 0x9e8f1355;
		cbuf[i] = s1;
		s1 = _lrotr(s1, 13) + s2+i;
		s2 = tmp;
	}
	cbuf[0] ^= s1;
	cbuf[1] ^= s2;
	left16 = LENGTH*2;
}

UInt32 RNG_jsf32::raw32() {
/*
	statistical quality is good
	there are some bad cycles, but the odds of hitting them are 
	low, believed to be well under 1 in 2^60
*/
	UInt32 e = a - _lrotr(b, 27);
	a = b ^ _lrotr(c, 17);
	b = c + d;
	c = d + e;
	d = e + a;
	return d;
}
void RNG_jsf32::seed_32(UInt32 s) {
	a = s; b = c = d = 1;
	for (int i = 0; i < 8; i++) raw32();
}
void RNG_jsf32::seed_64(UInt64 s) {
	a = UInt32(s); b = UInt32(s>>32);
	c = d = 1;
	for (int i = 0; i < 8; i++) raw32();
}
RNG_jsf32 global_rng;

int improved_rand() {
//	PerThread *pt = PerThread::get_PT();
//	if (!(++pt->rng_usage & ((1<<10)-1))) message("thread %d has used %d K random numbers", pt->thread_number, pt->rng_usage >> 10);
//	return pt->oblivion_rng.raw32() & ((1<<15)-1);//>> 1;
	return PerThread::get_PT()->oblivion_rng.raw16() & ((1<<15)-1);//>> 1;
}
void improved_srand(int seed) {
//	message("explicit seeding thread %d value %x", PerThread::get_PT()->thread_number, seed);
	PerThread::get_PT()->oblivion_rng.seed_32(seed);
}
void initialize_random_hooks() {
	if (Hook_rand && Hook_srand) {
		message("replacing libc rand(), srand()");
		::WriteRelJump(Hook_rand , UInt32(&improved_rand));
		::WriteRelJump(Hook_srand, UInt32(&improved_srand));
	}
	else {
		message("hooks not available for replacing libc rand(), srand()");
	}
}