
//   This is a implementation for memsets of 4 byte values (eg. ints)
//   written by Daniel Hesslow in August 2016
//   It is not too thoroughly tested and is provided as is. (I'm _pretty_ sure it works though)
//   in case anybody wan't to use it (for what ever) feel free to.
//
//   This software is dual-licensed to the public domain and under the following
//   license: you are granted a perpetual, irrevocable license to copy, modify,
//   publish, and distribute this file as you see fit.

// NOTE: THERE IS NO FALLBACK ON MACHENES THAT DOES NOT SUPPORT SSE2
// intel machenes after roughly 2003 should all have it. 

// On my machene the performance tests on msvc -O2 yields slightly less than twice as fast up to 100 (sweet spot at 32-48 where it is 170% faster)
// then gradually leveling out round about 500 (as memory bandwidth becomes the limiting factor)
// Clang (-O3) does sligly better, especially for values < 32 where mine is only 20 % faster, otherwise it's about the same.
// NOTE: It is optimized on my machene so you may not get the same results
// all performance test are done by taking about 50 samples and then taking the median. Followed by an avarage of 16 next lengths and allignments



#define DH_MEMSET_IMPLEMENTATION
#define DH_MEMSET_TEST

#ifndef DH_MEMSET_HEADER
#define DH_MEMSET_HEADER
#include "intrin.h" // simd stuff 
#include "stdint.h" // uint64, int32_t 
void DH_memset_32(int *dst, int value, int number_of_values);
#endif

#ifdef DH_MEMSET_IMPLEMENTATION


// NOTE: this is used meant for internal use in the DH_memset_32
//		 for values larger than 64 it will be slow.
//		 if you want avoid the conditional and know that you will only set tiny values, you're free to set them yourself.

inline void DH_memset_32_small(char *dst, int32_t value, int number_of_values)
{
	__m128i sse_value = _mm_set1_epi32(value);

	for (; number_of_values >= 16; number_of_values -= 16, dst += 64)
	{ // do the work
		_mm_storeu_si128((__m128i *)dst, sse_value);
		_mm_storeu_si128((__m128i *)dst + 1, sse_value);
		_mm_storeu_si128((__m128i *)dst + 2, sse_value);
		_mm_storeu_si128((__m128i *)dst + 3, sse_value);
	}
	for (; number_of_values > 3; number_of_values -= 4, dst += 16)
	{// finnish up the last simd writes
		_mm_storeu_si128((__m128i *)dst, sse_value);
	}

	// finnish up the last values
	switch (number_of_values)
	{ 
	case 3: ((int *)dst)[2] = value;
	case 2: ((int *)dst)[1] = value;
	case 1: ((int *)dst)[0] = value;
	}
}

// NOTE: this is used meant for internal use in the DH_memset_32
//		 for values fewer than 4 it will overwrite the buffer. (and also be significantly slower then DH_memset_32_small
//		 if you want to avoid that conditional and know that it will set more than 4 values be my guest
inline void DH_memset_32_large(char *dst, int32_t value, int number_of_values)
{
	char *dst_alligned_prev_bound = (char *)((uint64_t)dst & 0xfffffffffffffff0); // allign down to previous boundary
	char *final_write = dst + (number_of_values-1) * 4;
	int diff = dst - dst_alligned_prev_bound;
	int inv_diff = 16 - diff;
	{	// set the values between start and next allignment boundary
		// this is fine since we should only be called on values bigger than 64 bytes anyway.
		((int32_t *)dst)[3] = value;
		((int32_t *)dst)[2] = value;
		((int32_t *)dst)[1] = value;
		((int32_t *)dst)[0] = value;

		number_of_values -= (inv_diff +3)/ 4;
	}
	dst = dst_alligned_prev_bound + 16;// align to next, allready handled the diff

	int p_value = _rotl(value, (diff & 0x03) * 8); // rotate the value appropriate to how we're alligned
	__m128i sse_value = _mm_set1_epi32(p_value);

	// do the work simd
	for (; number_of_values >= 16; number_of_values -= 16, dst += 64)
	{ 
		_mm_store_si128((__m128i *)dst, sse_value);
		_mm_store_si128((__m128i *)dst + 1, sse_value);
		_mm_store_si128((__m128i *)dst + 2, sse_value);
		_mm_store_si128((__m128i *)dst + 3, sse_value);
	}
	// finnish up simd
	for (; number_of_values > 3; number_of_values -= 4, dst += 16)
	{ 
		_mm_store_si128((__m128i *)dst, sse_value);
	}
	
	// finnish up alligned
	switch (number_of_values) 
	{ 
	case 3: ((int32_t *)dst)[2] = p_value;
	case 2: ((int32_t *)dst)[1] = p_value;
	case 1: ((int32_t *)dst)[0] = p_value;
	}
	
	// write final unalligned to set the 0-3 last bytes
	*((int32_t *)final_write) = value; 
}

void DH_memset_32(int *dst, int32_t value, int number_of_values)
{
	if (number_of_values < 64)
	{
		DH_memset_32_small((char *)dst, value, number_of_values); //simd, don't allign, storu
	}
	else 
	{
		DH_memset_32_large((char *)dst, value, number_of_values); //simd allign, store
	}
}

#endif

