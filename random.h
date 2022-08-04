class RNG_fast32_16 {
/*
	To be used in a drop-in binary replacement for a libc rand() on 32-bit x86 that returns 15 bits per call.  
	Intended to be both faster and higher quality than the rand() it replaces (which is a shitty m=2**31 LCG with the bottom 16 bits dropped)
	Unfortunately, the combination of the odd format (15 bits per call) and high speed (single word LCG) of the RNG it replaces means that achieving this is quite difficult without slower seeding.  
	But hopefully this will be acceptable anyway.  
	This calculates 32 bits internally and buffers the output for efficient conversion to 2 16 bit words instead of 1 32 bit word.  
	A wrapper for this will then do the final 16->15 conversion by masking.  
	Statistical quality is not great, but it doesn't matter since it only has to be better than the libc rand()
*/
	enum {LENGTH=11};
	union {
		UInt32 cbuf[LENGTH];
		UInt16 results[LENGTH*2];
	};
	UInt32 left16;
	UInt16 refill();
public:
	inline UInt32 raw16() {
		if (left16) return results[--left16];
		else return refill();
	}
	void seed_64(UInt64 s);
	void seed_32(UInt32 s) {seed_64(s);}
};

class RNG_jsf32 {
	/*
		Robert Jenkins small fast prng
		reasonably small, reasonably fast, high quality output
	*/
protected:
	UInt32 a, b, c, d;
public:
	UInt32 raw32();
	UInt64 raw64() {UInt64 rv = raw32(); rv |= ((UInt64)raw32())<<32; return rv;}
	void seed_32(UInt32 s);
	void seed_64(UInt64 s);
};
extern RNG_jsf32 global_rng;

void initialize_random_hooks();