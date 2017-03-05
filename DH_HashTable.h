

/*
	This is a fairly quick robinhood hashtable implementation:

	USAGE:

	MANDATORY DEFINES:
		HT_NAME : the type-name of the new hashtable
		HT_KEY  : the type of the key
		HT_HASH(key) : the hashfunction to the key eg. HT_HASH(key) hash(key)

	OPTIONAL DEFINES:
		HT_MULTIPLE_VALUES: indicates that one key might map to multiple values which you may iterate over. HT_VALUE must be defined
		HT_MULTIPLE_VALUES_ORDERED: tells the hashtable to keep the insertion order of the values mapping to one key. HT_MULTIPLE_VALUES is implicit.
		HT_VALUE: the type of the value you want the key to map to. If not defined this is a hashset
		HT_EQUAL(a,b): the equality function for the keys. If not defined this this defaults to HT_EQUAL(a,b) a==b
		HT_FAST_KEY_COMP 0/1: indicates that HT_EQUAL it is faster to run HT_EQUAL(a,b) instead of hash_a == hash_b && HT_EQUAL(a,b). defaults to 0 if HT_EQUAL is defined, 1 otherwise
		HT_GROW_FACTOR: at which load factor the hashtable should double the size: this defaults to .8
		HT_SHRINK_FACTOR: at which load factor the hashtable should half the size: if not defined the hashtable does not shrink.
		HT_ALLOCATOR: the type of a custom allocator. You must define ALLOC and FREE if this is defined!
		HT_ALLOC(num_bytes)    : the function to dynamically alloc memory: defaults to HT_ALLOC(num_bytes) malloc(num_bytes)
		HT_FREE (ptr,num_bytes): the function to dynamically deallocate memory: defaults to HT_FREE (ptr,num_bytes) free(ptr)
	GOTCHAS:
		We're quadratic if you insert from one hashtable into another in order without reserving. I don't feel like this is a bug but you should certianly be aware of it!
			A detailed blogpost about the same problem for the rust hashtable can be found at: https://accidentallyquadratic.tumblr.com/post/153545455987/rust-hash-iteration-reinsertion
			Usually the solution here is to reserve in advance or to use different hashfunctions for the different tables or same but xor with random number (during compiletime)

		when iterating over the hashtable (or a key in the hashtable when having mutiple values) 
			you *MUST NOT* call remove twice. This will silently fuck shit up. 
			you must not do anything to the underlying hashtable(insert remove change a key etc.) this will silently fuck shit up. (iterator.remove() is fine of course)

		when calling remove_at(index) you *MUST* make sure that the index holds a entry. Failing to do that will silently fuck shit up.

		SHRINK_FACTOR *MUST* be smaller than GROW_FACTOR/2. otherwise we'll pingpong between growing and shrinking... This is statically checked though.


		if requested I might provide a BABY_SIT_ME define that removes some of these gotchas (but slows things down).

*/



#ifdef HT_MULTIPLE_VALUES_ORDERED
#define HT_MULTIPLE_VALUES
#endif

#ifndef HT_NAME
#error HT_NAME macro must be defined
#define HT_ERROR
#endif

#ifndef HT_KEY
#error HT_KEY macro must be defined
#define HT_ERROR
#endif

#ifndef HT_HASH
#error HT_HASH macro must be defined
#define HT_ERROR
#endif



#ifndef HT_FAST_KEY_CMP
#ifdef HT_EQUAL 
#define HT_FAST_KEY_CMP 0
#else
#define HT_FAST_KEY_CMP 1
#endif
#endif

#ifndef HT_EQUAL
#define HT_EQUAL(a,b) a==b
#endif

#ifndef HT_GROW_FACTOR 
#define HT_GROW_FACTOR 0.80
#endif

#ifdef HT_SHRINK_FACTOR
static_assert(HT_GROW_FACTOR / 2 > HT_SHRINK_FACTOR, "HT_SHRINK_FACTOR must be smaller than HT_GROW_FACTOR/2 otherwise we get will bounce between them!");
#define HT_ERROR
#endif

#ifdef HT_ALLOCATOR
#ifndef HT_ALLOC
#error if HT_ALLOCATOR is defined so must HT_ALLOC
#define HT_ERROR
#endif
#ifndef HT_FREE
#error if HT_ALLOCATOR is defined so must HT_FREE
#define HT_ERROR
#endif
#else
#define HT_ALLOC(num_bytes)	  malloc(num_bytes)
#define HT_FREE(ptr, num_bytes) free(ptr)
#endif

#define HT_SWAP(type, a, b)do{type tmp = a; a = b; b = tmp;}while(0);


#ifndef HT_ERROR
struct HT_NAME {
	struct Bucket {
		HT_KEY key;
#ifdef HT_VALUE
		HT_VALUE value;
#endif
		uint32_t hash;
	};

#define HT_EMPTY 0
#ifdef HT_MULTIPLE_VALUES
	struct ValueIterator {
		uint64_t idx;
		HT_NAME *ht;
		HT_KEY  fst_key;
		uint32_t fst_hash;
		bool next(HT_VALUE *value) {
			// Note if we fill the entire table this won't work. But we don't (not even with a load factor of one) so that's fine.
			if (fst_hash == ht->buckets[idx].hash && ht->equal(fst_key, ht->buckets[idx].key)) {
				*value = ht->buckets[idx].value;
				idx = ht->mask(idx + 1);
				return true;
			}
			return false;
		}

		void remove() {
			idx = ht->mask(idx - 1);
			if (ht->buckets[idx].key == 985)__debugbreak;
			ht->remove_at(idx);
			if (ht->buckets[idx].hash == HT_EMPTY) idx = ht->mask(idx+1);
		}

};
#endif



#ifdef HT_ALLOCATOR 
	HT_ALLOCATOR allocator;
#endif

	inline void *_alloc(size_t num_bytes) {
		return HT_ALLOC(num_bytes);
	}
	inline void *_alloc_and_zero(size_t num_bytes) {
		void *ptr = _alloc(num_bytes);
		memset(ptr, 0, num_bytes);
		return ptr;
	}

	inline void _free(void *ptr, size_t num_bytes) {
		HT_FREE(ptr, num_bytes);
	}

	Bucket *buckets;
	int min_size;
	uint32_t capacity;
	uint64_t length;

#ifdef HT_ALLOCATOR
	HT_NAME(void *allocator, int capacity) {
		this->allocator = allocator;
#else
	HT_NAME(int capacity) {
#endif
		this->capacity = capacity;
		this->buckets = (Bucket *)_alloc_and_zero(capacity * sizeof(Bucket));

		this->min_size = capacity;
		this->length = 0;
	}
#ifdef HT_ALLOCATOR
	HT_NAME(void *allocator) : HT_NAME(allocator, 128) {
	}
#else
	HT_NAME() : HT_NAME(128) {
	}
#endif



	void resizeTo(int new_capacity) {
		if (new_capacity < min_size) return;

#ifdef HT_ALLOCATOR
		HT_NAME new_ht(allocator, new_capacity);
#else
		HT_NAME new_ht(new_capacity);
#endif
		new_ht.min_size = min_size;

		int new_len = 0;
		if (length != 0)
			for (uint64_t i = 0; i < capacity; i++) {
				if (buckets[i].hash != HT_EMPTY) {
					int prev_len = new_ht.length;
					new_ht.insert(buckets[i]);
					++new_len;
				}
			}
		assert(new_ht.length == length);
		_free(buckets, capacity * sizeof(Bucket));
		*this = new_ht;
	}
	void maybe_double() {
		// @Perf we might want to store capacity*growthfactor
		// that would save a couple of cyckles (we have two converstions there each of 2 cyckles)
		// plus a mul so 5 cycles. I don't think we pay for the latency though, which is ~20 cycles so thats good
		if (length >= (uint64_t)(capacity*HT_GROW_FACTOR)) {
			double_capacity();
		}
	}
	void maybe_half() {
#ifdef HT_SHRINK_FACTOR
		if (length < capacity*HT_SHRINK_FACTOR && capacity > min_size) {
			resizeTo(capacity / 2);
		}
#endif
	}


	inline uint32_t hash_key(HT_KEY key) {
		// note underscore to avoid name collisions with HT_HASH
		uint32_t _hash = (uint32_t)HT_HASH(key);
		_hash |= _hash == 0;
		return _hash;
	}

	uint64_t mask(uint64_t hash) {
		return hash & (capacity - 1);
	}

	inline uint64_t probe_count(uint64_t index) {
		return mask(index - buckets[index].hash);
	}

	inline bool equal(HT_KEY a, HT_KEY b) {
		return HT_EQUAL(a, b);
	}

	inline bool equal(uint32_t hash_a, uint32_t hash_b, HT_KEY a, HT_KEY b) {
#if HT_FAST_KEY_CMP
		return equal(a, b);
#else
		return hash_a == hash_b && equal(a, b);
#endif
	}

#ifndef HT_MULTIPLE_VALUES
	inline void insert(Bucket to_insert) {
		uint64_t pos = mask(to_insert.hash);
		uint64_t dist = 0;
		for (;;) {
			if (buckets[pos].hash == HT_EMPTY) {
				buckets[pos] = to_insert;
				++length;
				maybe_double();
				return;
			} else {
				if (equal(to_insert.hash, buckets[pos].hash, to_insert.key, buckets[pos].key)) {
					#ifdef HT_VALUE
					buckets[pos].value = to_insert.value;
					#endif
					return;
				}
				uint64_t other_dist = probe_count(pos);
				if (dist > other_dist) {
					assert(to_insert.key == to_insert.value);
					assert(buckets[pos].key == buckets[pos].value);
					HT_SWAP(Bucket, buckets[pos], to_insert);
					dist = other_dist;
				}
			}
			++dist;
			pos = mask(pos + 1);
		}
	}
#else
	inline void insert(Bucket to_insert) {
		uint64_t pos = mask(to_insert.hash);
		uint64_t prev_pos = mask(pos - 1);
		uint64_t dist = 0;
		for (;;) {
			if (buckets[pos].hash == HT_EMPTY) {
				buckets[pos] = to_insert;
				++length;
				maybe_double();
				return;
			} else {
				uint64_t other_dist = probe_count(pos);
				if (dist > other_dist 
					|| equal(to_insert.hash,buckets[prev_pos].hash,to_insert.key,buckets[prev_pos].key)
					#ifdef HT_MULTIPLE_VALUES_ORDERED 
					// note this can probably be speed up a bit but that would complicate the code 
					// more than I like to atm.
						&& !equal(to_insert.hash, buckets[pos].hash, to_insert.key, buckets[pos].key)
					#endif		
					) {
					HT_SWAP(Bucket, buckets[pos], to_insert);
					dist = other_dist;
				}
			}
			++dist;
			prev_pos = pos;
			pos = mask(pos + 1);
		}
	}
#endif



#ifdef HT_VALUE
	void insert(HT_KEY key, HT_VALUE value) {
		Bucket to_insert = { key,value,hash_key(key) };
		insert(to_insert);
	}

#else
	void insert(HT_KEY key) {
		Bucket to_insert = { key,hash_key(key) };
		insert(to_insert);
	}
#endif


	inline bool ilookup(HT_KEY key, uint64_t *idx) {
		uint32_t hash = hash_key(key);
		*idx = mask(hash);
		uint64_t dist = 0;
		for (;;) {
			if (buckets[*idx].hash == HT_EMPTY) return false;
			else if (equal(buckets[*idx].hash, hash, buckets[*idx].key, key)) return true;
			else if (dist > probe_count(*idx))  return false;
			*idx = mask(*idx + 1);
			++dist;
		}
	}
#ifdef HT_MULTIPLE_VALUES
	inline void remove_all_at(uint64_t index) { // Note, no checking, buckets[index] better not be empty
		// @PERF profile compared to iteration + normal remove.
		// most cases will probably be one anyway? in which case this is slower... (but only like 12 cycles or something if I've counted my ops right)
		// but this should be way faster if otherwise especcially on long probe chains.. 
		// for now this seems fine. 

		assert(index == mask(index));
		uint64_t read = index;
		uint64_t write = index;
		bool cont;

		do {
			uint64_t nxt_read = mask(read + 1);
			cont = buckets[read].hash == buckets[nxt_read].hash && equal(buckets[read].key, buckets[nxt_read].key);
			buckets[read].hash = HT_EMPTY;
			--length;
			read = nxt_read;
		} while (cont);

		for (;;) {
			if (buckets[read].hash == HT_EMPTY)  break;
			uint64_t pc = probe_count(read);
			if (pc == 0) break;
			write = mask(read-(min(mask(read-write),pc))); //heyo this is some mess all right...
			buckets[write] = buckets[read];
			buckets[read].hash = HT_EMPTY;
			read = mask(read + 1);
			write = mask(write + 1); 
		}
		maybe_half();
	}
#endif
	inline void remove_at(uint64_t index) { // Note, no checking, buckets[index] better not be empty
		buckets[index].hash = HT_EMPTY;
		for (;;) {
			uint64_t prev_index = index;
			index = mask(index + 1);
			if (buckets[index].hash == HT_EMPTY)  break;
			if (probe_count(index) == 0)          break;
			buckets[prev_index] = buckets[index];
			buckets[index].hash = HT_EMPTY;
		}
		--length;
		maybe_half();
	}

	bool remove(HT_KEY key) {
		uint64_t index;
		if (!ilookup(key, &index))return false;
#ifdef HT_MULTIPLE_VALUES
#if 0
		ValueIterator it;
		lookup(key, &it);
		HT_VALUE val;
		while (it.next(&key)) {
			it.remove();
		}
#endif
		remove_all_at(index);
#else 
		remove_at(index);
#endif
		return true;
	}

#ifdef HT_MULTIPLE_VALUES
bool lookup(HT_KEY key, ValueIterator *it) {
	it->ht = this;
	bool ret = ilookup(key, &it->idx);
	it->fst_key = buckets[it->idx].key;
	it->fst_hash = buckets[it->idx].hash;
	return ret;
}
#elif defined HT_VALUE
	bool lookup(HT_KEY key, HT_VALUE *value) {
		uint64_t index;
		if (!ilookup(key, &index)) return false;
		*value = buckets[index].value;
		return true;
	}
#else
	bool lookup(HT_KEY key) {
		uint64_t index;
		return ilookup(key, &index);
	}
#endif

	void destroy() {
		_free(buckets, capacity * sizeof(Bucket));
		this->capacity = 0;
		this->length = 0;
		this->buckets = 0;
	}

	void clear() {
		length = 0;
		memset(buckets, 0, sizeof(Bucket)*capacity);
	}

	void clear_and_shrink() {
		destroy();
		*this = HT_NAME(min_size);
	}

	void double_capacity() {
#ifdef HT_ALLOCATOR
		HT_NAME new_ht(allocator, capacity * 2);
#else
		HT_NAME new_ht(capacity * 2);
#endif
		new_ht.min_size = min_size;
		int old_len = length;
		// find the first bucket that hasn't wrapped around
		uint64_t fst;
		for (fst = 0; buckets[fst].hash != HT_EMPTY && mask(buckets[fst].hash) > fst; ++fst);
		length -= fst;
		new_ht.length = length;

		uint64_t low = 0;
		uint64_t high = capacity;
		for (uint64_t i = fst; i < capacity; i++) {
			if (buckets[i].hash == HT_EMPTY) continue;
			if (buckets[i].hash & capacity) { // do we go to high or low half of the new hashtable
				high = max(high, new_ht.mask(buckets[i].hash));
				new_ht.buckets[high++] = buckets[i];
			} else {
				low = max(low, new_ht.mask(buckets[i].hash));
				new_ht.buckets[low++] = buckets[i];
			}
		}

		for (uint64_t i = 0; i < fst; i++) {
			new_ht.insert(buckets[i]);
		}

		destroy();
		*this = new_ht;
	}

	struct Iterator {
		int idx;
		HT_NAME *ht;
		bool next(Bucket *bucket) {
			while (idx < ht->capacity) {
				if (ht->buckets[idx].hash != HT_EMPTY) {
					*bucket = ht->buckets[idx];
					++idx;
					return true;
				}
				++idx;
			}
			return false;
		}

		void remove() {
			ht->remove_at(--idx);
			if (ht->buckets[idx].hash == HT_EMPTY)++idx;
		}
	};


#undef HT_EMPTY
};
#endif

#undef HT_ERROR
#undef HT_ALLOCATOR
#undef HT_ALLOC
#undef HT_FREE
#undef HT_NAME
#undef HT_KEY
#undef HT_VALUE
#undef HT_HASH
#undef HT_EQUAL
#undef HT_GROW_FACTOR
#undef HT_SHRINK_FACTOR
#undef HT_SWAP
#undef HT_FAST_KEY_CMP
#undef HT_MULTIPLE_VALUES
#undef HT_MULTIPLE_VALUES_ORDERED



