/*
	Created by Daniel Hesslow 28th of January 2017

	License:
	This software is dual-licensed to the public domain and under the following license:
	you are granted a perpetual, irrevocable license to copy, modify, publish, and distribute this file as you see fit.


	A fast fixed-size thread-safe & lock-free queue


	usage:
	define the following:
		MAX_NUM_ELEMENTS		
		TYPE
		NUM_PRODUCERS
		NUM_CONSUMERS
	optinally also
		NAME

	//remember to zero initialize if not static
	static DH_RingBuffer ringbuffer;

	call 
		ringbuffer.push(id, elem); 
	to enqueue an item

	call 
		ringbuffer.pop(id, elem); 
	to dequeue an item 

	where id is your producer/consumer id
	every thread that produces(push) needs to have a unique id {0 .. NUM_PRODUCERS-1}
	every thread that consumes(pop)  needs to have a unique id {0 .. NUM_CONSUMERS-1}

	
	NOTE: DO NOT put the ringbuffer on unaligned memory, needs to be atleast aligned to sizeof(uint_fast32_t)
		  malloc_allignment is sufficient
	
	DISCLAIMER:
		Testing mutlithreaded data structures is hard. I've done my best but that might not have been good enough.
		There might still be bugs in there.
		I've only tested this on x86/64. 
		AMD/PowerPC has less guearantees, I've tried to keep them in mind, but I havn't tested.
		I have some (most likely) rendundant memory_order_stuff especially in get_trailing_read/get_trailing_write
		they compile away on x86/64
		Other platforms will see a perf hit.
*/






#ifndef MAX_NUM_ELEMENTS
static_assert(false, "need to define 'MAX_NUM_ELEMENTS', the maximum number of elements the ringbuffer can hold");
#else
static_assert(MAX_NUM_ELEMENTS < UINT_FAST32_MAX / 2, "'MAX_NUM_ELEMENTS' must be smaller than UINT_FAST32_MAX/2");
static_assert(!(MAX_NUM_ELEMENTS & (MAX_NUM_ELEMENTS - 1)), "'MAX_NUM_ELEMENTS' must be power of two");
#endif

#ifndef TYPE
static_assert(false, "need to define 'TYPE', the type the ring buffer holds");
#endif

#ifndef NUM_PRODUCERS
static_assert(false, "need to define 'NUM_PRODUCERS', the number of producer threads");
#endif

#ifndef NUM_CONSUMERS
static_assert(false, "need to define 'NUM_CONSUMERS', the number of consumer threads");
#endif



#ifdef _WIN32
#define ALIGN_CACHE_LINE __declspec(align(64))
#else
#define ALIGN_CACHE_LINE __attribute__ ((aligned (64)))
#endif

#ifdef NAME
struct NAME
#else
struct DH_RingBuffer
#endif
{

	TYPE arr[MAX_NUM_ELEMENTS];
	ALIGN_CACHE_LINE std::atomic<uint_fast32_t> read;
	ALIGN_CACHE_LINE std::atomic<uint_fast32_t> write;

	struct ThreadData
	{
		ALIGN_CACHE_LINE
		std::atomic<uint_fast32_t> trailing;
		std::atomic<bool> active;
	} consumers[NUM_CONSUMERS], producers[NUM_PRODUCERS];

	int mask(uint_fast32_t value)
	{
		return value & MAX_NUM_ELEMENTS - 1;
	}

	// so cpp atomic code is totally super readable... 
	// all this eye candy to do interlocked_compare_exchange without platform specific code
	// totally worth it heh. 
	
	// Optimization on non-x86/64: we should get by without the memory_order_acquire on all places below
	// since get_last_read/write is a non monotonically increasing lower bound it's fine to not work with fresh values
	//		note: read is not allowed to be newer than active/trailing
	// however I don't ahve access to amd/powerpc so I just keep them here for now
	// on x86/64 they compile to nothing anyway

	uint_fast32_t get_trailing_read()
	{
		uint_fast32_t ret = read.load(std::memory_order_acquire);
		for (int i = 0; i < NUM_CONSUMERS; i++)
		{
			if (consumers[i].active.load(std::memory_order_acquire))
			{
				uint_fast32_t tmp = consumers[i].trailing.load(std::memory_order_acquire);
				std::atomic_signal_fence(std::memory_order_acq_rel);
				if (tmp < ret) ret = tmp;
			}
		}
		return ret;
	}


	uint_fast32_t get_trailing_write()
	{
		uint_fast32_t ret = write.load(std::memory_order_acquire);
		for (int i = 0; i < NUM_PRODUCERS; i++)
		{
			if (producers[i].active.load(std::memory_order_acquire))
			{
				uint_fast32_t tmp = producers[i].trailing.load(std::memory_order_acquire);
				std::atomic_signal_fence(std::memory_order_acq_rel);
				if (tmp < ret) ret = tmp;
			}
		}
		return ret;
	}

	bool push(int producer_id, TYPE elem)
	{
		producers[producer_id].trailing.store(write.load(std::memory_order_acquire),std::memory_order_relaxed);
		producers[producer_id].active.store(true, std::memory_order_relaxed);
		atomic_thread_fence(std::memory_order_release);
		uint_fast32_t trailing_read = get_trailing_read();
		bool success;
		
		// if we're not going to override, increment write & store old value in w 
		// relies on the overflow and not simply eq since trailing_read is a lower_bound and not monotonically increasing
		uint_fast32_t w = atomic_post_increment_if_difference_less_than
								(write, trailing_read, MAX_NUM_ELEMENTS, &success);

		if (success)
		{
			arr[mask(w)] = elem;
			atomic_thread_fence(std::memory_order_release);
		}

		producers[producer_id].active.store(false, std::memory_order_release);
		return success;
	}

	bool pop(int consumer_id, TYPE *elem)
	{
		consumers[consumer_id].trailing.store(read.load(std::memory_order_acquire),std::memory_order_relaxed);
		consumers[consumer_id].active.store(true, std::memory_order_relaxed);
		atomic_thread_fence(std::memory_order_release);
		uint_fast32_t trailing_write = get_trailing_write();
		bool success;
		
		// atomic if there is new memory to read, increment read & store old value in r 
		// relies on the overflow and not simply eq since trailing_write is a lower_bound and not monotonically increasing
		uint_fast32_t r = atomic_post_increment_if_difference_less_than_inv
								(read, trailing_write-1, MAX_NUM_ELEMENTS, &success);

		if (success)
		{
			atomic_thread_fence(std::memory_order_acquire);
			*elem = arr[mask(r)];
		}

		consumers[consumer_id].active.store(false, std::memory_order_release);
		return success;
	}

	private:
	inline uint_fast32_t atomic_post_increment_if_difference_less_than
		(std::atomic<uint_fast32_t> &value, uint_fast32_t sub, uint_fast32_t comparend, bool *success)
	{
		for (;;)
		{
			uint_fast32_t curr = value.load(std::memory_order_acquire);
			if (curr - sub >= comparend)
			{
				*success = false;
				return curr;
			}
			else if (value.compare_exchange_weak(curr, curr + 1))
			{
				*success = true;
				return curr;
			}
		}
	}

	inline uint_fast32_t atomic_post_increment_if_difference_less_than_inv
		(std::atomic<uint_fast32_t> &value, uint_fast32_t sub, uint_fast32_t comparend, bool *success)
	{
		for (;;)
		{
			uint_fast32_t curr = value.load(std::memory_order_acquire);
			if (sub - curr >= comparend)
			{
				*success = false;
				return curr;
			}
			else if (value.compare_exchange_weak(curr, curr + 1))
			{
				*success = true;
				return curr;
			}
		}
	}
};

#ifdef MAX_NUM_ELEMENTS
#undef MAX_NUM_ELEMENTS
#endif

#ifdef TYPE
#undef TYPE
#endif

#ifdef NUM_CONSUMERS
#undef NUM_CONSUMERS
#endif

#ifdef NUM_CONSUMERS
#undef NUM_CONSUMERS
#endif

#ifdef NAME
#undef NAME 
#endif