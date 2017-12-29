#ifndef TIMSORT_H
#define TIMSORT_H

#include <iostream>
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <array>
#include <utility>
#include <functional>
#include <vector>
#include <limits>
#include <numeric>
#include <mutex>
#include "utils.h"
#include "timsort_stack_buffer.h"

namespace tim {

/*
 * Modified variant of the compute_minrun() function used in CPython's 
 * list_sort().  
 * 
 * The CPython version of this function chooses a value in [32, 65) for 
 * minrun.  Unlike in CPython, C++ objects aren't guaranteed to be the 
 * size of a pointer.  A heuristic is used here under the assumption 
 * that std::move(some_arbitrary_cpp_object) is basically a bit-blit.  
 * If the type is larger that 4 pointers than minrun maxes out at 32 
 * instead of 64.  Similarly, if the type is larger than 8 pointers, 
 * it maxes out at 16.  This is a major win for large objects 
 * (think tuple-of-strings).
 * Four pointers is used as the cut-off because libstdc++'s std::string
 * implementation was slightly, but measurably worse in the benchmarks
 * when the max minrun was 32 instead of 64.  
 */
template <std::size_t SizeofType>
static constexpr std::size_t compute_minrun(std::size_t n)
{
	constexpr std::size_t sizeof_ptr = sizeof(void*);
	constexpr std::size_t minrun_max = SizeofType > (sizeof_ptr * 8) ?
						 16 : 
						 SizeofType > (sizeof_ptr * 4) ?
						 32 :
						 64; 
	std::size_t r = 0;
	while (n >= minrun_max) 
	{
		r |= (n & 1);
		n >>= 1;
	}
	return (n + r);
}



template <class SizeType, 
	  class It,
	  class Comp>
struct TimSort
{
	using value_type = iterator_value_type_t<It>;
	TimSort(It begin_it, It end_it, Comp comp_func):
		stack_buffer{},
		heap_buffer{},
		start(begin_it), 
		stop(end_it),
		position(begin_it),
		comp(std::move(comp_func)), 
		minrun(compute_minrun<sizeof(value_type)>(end_it - begin_it)),
		min_gallop(default_min_gallop)
	{
		try_get_cached_heap_buffer(heap_buffer);
		// COMPILER_ASSUME_(minrun > 31 and minrun < 65);
		fill_run_stack();
		collapse_run_stack();
		try_cache_heap_buffer(heap_buffer);
	}
	
	
	/* 
	 * Continually push runs onto the run stack, merging adjacent runs
	 * to resolve invariants on the size of the runs in the stack along 
	 * the way.
	 */
	inline void fill_run_stack()
	{
		// push the first two runs on to the run stack, unless there's
		// only one run.
		push_next_run();
		if(not (position < stop))
			return;
		push_next_run();
		// loop until we've walked over the whole array
		while(position < stop)
		{
			// resolve invariants before pushing the next run
			resolve_invariants();
			push_next_run();
		}
	}
	
	/* 
	 * Grand finale.  Keep merging the top 2 runs on the stack until there
	 * is only one left.
	 */
	inline void collapse_run_stack()
	{
		for(auto count = stack_buffer.run_count(); count > 1; --count)
			merge_BC();
	}

	/* 
	 * Get the next run of already-sorted elements.  If the length of the 
	 * natural run is less than minrun, force it to size with an insertion sort.
	 */
	void push_next_run()
	{
		if(auto run_end = count_run(position, stop, comp); 
			run_end - position < minrun)
		{
			auto limit = stop;
			if(stop - position > minrun)
				limit = position + minrun;
			partial_insertion_sort(position, run_end, limit, comp);
			position = limit;
		}
		else
			position = run_end;
		stack_buffer.push(position - start);
	}

	/*
	 * MERGE PATTERN STUFF
	 */
	
	/*
	 * Assume the run stack has the following form:
	 * 	[ ..., W, X, Y, Z]
         * Where Z is the length of the run at the top of the run stack.
	 * 
	 * This function continually merges with Y with Z or X with Y until
	 * the following invariants are satisfied:
	 * 	(1) X > Y + Z
	 * 	  (1.1) W > X + Y
	 *      (2) Y > Z
	 * 
	 * If (1) or (1.1) are not satisfied, Y is merged with the smaller of 
	 * X and Z.  Otherwise if (2) is not satisfied, Y and Z are merged.
	 * 
	 * This gives a reasonable upper bound on the size of the run stack.
	 *
	 *   NOTE: 
	 *   invariant (1.1) implements a fix for a bug in the original
	 *   implementation described here:
	 *   http://envisage-project.eu/wp-content/uploads/2015/02/sorting.pdf
	 *   
	 *   The original description of these invariants written by Tim Peters 
	 *   accounts for only the top three runs and refers to them as: A, B, 
	 *   and C.  This implementation uses Tim's labelling scheme in some
	 *   function names, but implements the corrected invariants as
	 *   described above.
	 * 
	 * ALSO NOTE:
	 *
	 * For more details see:
	 * https://github.com/python/cpython/blob/master/Objects/listsort.txt
	 */
	void resolve_invariants()
	{
		auto run_count = stack_buffer.run_count();
		
		// Check all of the invariants in a loop while the size of the 
		// run stack permits.  It run_count >= 3, then we can't check
		// invariant (1.1) because there are only three runs.
		for(; run_count > 3; --run_count)
		{
			if(const bool abc = stack_buffer.merge_ABC(); abc and stack_buffer.merge_AB())
				merge_AB();
			else if(abc or stack_buffer.merge_BC())
				merge_BC();
			else
				return;
		}
		// only three runs.  check invariant (1.1)
		if((run_count > 2) and stack_buffer.merge_ABC_case_1())
		{
			if(stack_buffer.merge_AB())
				merge_AB();
			else
				merge_BC();
			--run_count;
		}

		// only two or three runs.  check invariant (2)
		if((run_count > 1) and stack_buffer.merge_BC())
			merge_BC();
	}

	/* RUN STACK STUFF */

	/*
	 * @brief Merge the second-to-last run with the last run.
	 */
	void merge_BC() 
	{
		merge_runs(start + get_offset<2>(),
			   start + get_offset<1>(),
			   start + get_offset<0>());
		stack_buffer.template remove_run<1>();
	}
	
	/*
	 * @brief Merge the third-to-last run with the second-to-last run.
	 */
	void merge_AB()
	{
		merge_runs(start + get_offset<3>(),
			   start + get_offset<2>(),
			   start + get_offset<1>());
		stack_buffer.template remove_run<2>();
	}

	/*
	 * @brief Fetches the offset at the Ith position down from the top of
	 * the run stack.
	 */
	template <std::size_t I>
	inline auto get_offset() const
	{
		return stack_buffer.template get_offset<I>();
	}

	/*
	 * MERGE/SORT IMPL STUFF
	 */
	
	/* 
	 * @brief Merges the range [begin, mid) with the range [mid, end). 
	 * @param begin  Iterator to the first item in the left range.
	 * @param mid    Iterator to the last/first item in the left/right range.
	 * @param end    Iterator to the last item in the right range.
	 *
	 * Requires:
	 *     begin < mid and mid < end.
	 *     std::is_sorted(begin, mid, this->comp)
	 *     std::is_sorted(mid, end, this->comp)
	 */
	inline void merge_runs(It begin, It mid, It end) 
	{
		// We're going to need to copy the smaller of these ranges 
		// into a temporary buffer (which may end up having to be on 
		// the heap).  Before we do any copying, try to reduce the 
		// effective size of each range by looking for position 'p1'
		// in [begin, mid) that mid[0] belongs in. Similarly look 
		// for mid[-1] in [mid, end) and call that 'p2'.
		// We then only need to merge [p1, mid) with [mid, p2).  
		// 
		// This also may reduce the number of items we need to copy
		// into the temporary buffer, and if we're lucky, it may even
		// make it possible to use just the space we have on the stack. 
		
		begin = gallop_upper_bound(begin, mid, *mid, comp);
		end = gallop_upper_bound(std::make_reverse_iterator(end), 
					 std::make_reverse_iterator(mid), 
					 mid[-1], 
					 [comp=this->comp](auto&& a, auto&& b){
					    return comp(std::forward<decltype(b)>(b), std::forward<decltype(a)>(a));
					 }).base();
		
		if(COMPILER_LIKELY_(begin < mid or mid < end))
		{
			if((end - mid) > (mid - begin))
				// merge from the left
				do_merge(begin, mid, end, comp);
			else 
				// merge from the right
				do_merge(std::make_reverse_iterator(end),
					 std::make_reverse_iterator(mid),
					 std::make_reverse_iterator(begin), 
					 [comp=this->comp](auto&& a, auto&& b){ 
						return comp(std::forward<decltype(b)>(b), 
							    std::forward<decltype(a)>(a)); 
					 }
				);
		}
	}

	/* 
	 * @brief Merges the range [begin, mid) with the range [mid, end). 
	 * @param begin  Iterator to the first item in the left range.
	 * @param mid    Iterator to the last/first item in the left/right range.
	 * @param end    Iterator to the last item in the right range.
	 * @param cmp    Comparator to merge with respect to.
	 * 
	 * Requires:
	 *     begin < mid and mid < end.
	 *     std::is_sorted(begin, mid, cmp)
	 *     std::is_sorted(mid, end, cmp)
	 */
	template <class Iter, class Cmp>
	inline void do_merge(Iter begin, Iter mid, Iter end, Cmp cmp)
	{
		// check to see if we can use the stack as a temporary buffer
		if(stack_buffer.can_acquire_merge_buffer(begin, mid)) 
		{
			// allocate the merge buffer on the stack
			auto stack_mem = stack_buffer.move_to_merge_buffer(begin, mid);
			min_gallop = gallop_merge_ex(stack_mem, stack_mem + (mid - begin), 
						     mid, end, 
						     begin, cmp, min_gallop);
		}
		else
		{
			// TODO: clean this up
			// fall back to a std::vector<> for the merge buffer 
			// try to use memcpy if possible
			if constexpr(can_forward_memcpy_v<Iter> or not can_reverse_memcpy_v<Iter>)
			{
				// memcpy() it if we can
				if constexpr (can_forward_memcpy_v<Iter>)
				{
					heap_buffer.resize(mid - begin);
					std::memcpy(heap_buffer.data(), get_memcpy_iterator(begin), (mid - begin) * sizeof(value_type));
				}
				else
				{
					heap_buffer.assign(std::make_move_iterator(begin), std::make_move_iterator(mid));
				}
				min_gallop = gallop_merge_ex(heap_buffer.begin(), heap_buffer.end(),
							     mid, end, 
							     begin, cmp, min_gallop);
			}
			else
			{
				heap_buffer.resize(mid - begin);
				std::memcpy(heap_buffer.data(), get_memcpy_iterator(mid - 1), (mid - begin) * sizeof(value_type));
				min_gallop = gallop_merge_ex(heap_buffer.rbegin(), heap_buffer.rend(),
							     mid, end, 
							     begin, cmp, min_gallop);
			}
		}
	}
	
	static void try_get_cached_heap_buffer(std::vector<value_type>& vec)
	{
		if(std::unique_lock lock(heap_buffer_cache_mutex, std::try_to_lock_t{}); lock.owns_lock())
		{
			vec = std::move(heap_buffer_cache);
		}
	};

	static void try_cache_heap_buffer(std::vector<value_type>& vec)
	{
		if(std::unique_lock lock(heap_buffer_cache_mutex, std::try_to_lock_t{}); 
			lock.owns_lock() and vec.size() > heap_buffer_cache.size())
		{
			heap_buffer_cache = std::move(vec);
			heap_buffer_cache.clear();
		}
	};

	static constexpr const std::size_t extra_stack_max_bytes = sizeof(void*) * 0;
	timsort_stack_buffer<SizeType, value_type, extra_stack_max_bytes / sizeof(value_type)> stack_buffer; 
	std::vector<value_type> heap_buffer;
	const It start;
	const It stop;
	It position;
	Comp comp;

	const std::ptrdiff_t minrun;
	std::size_t min_gallop;
	static std::vector<value_type> heap_buffer_cache;
	static std::mutex heap_buffer_cache_mutex;
	static constexpr const std::size_t default_min_gallop = gallop_win_dist;
};

template <class S, 
	  class It,
	  class C>
std::vector<typename TimSort<S, It, C>::value_type> TimSort<S, It, C>::heap_buffer_cache{};

template <class S, 
	  class It,
	  class C>
std::mutex TimSort<S, It, C>::heap_buffer_cache_mutex{};


template <class It, class Comp, class ... Tags>
static inline void _timsort(It begin, It end, Comp comp, Tags ... tags)
{
	std::size_t len = end - begin;
	if(len > 64)
		TimSort<std::size_t, It, Comp>(begin, end, comp);
	else
		partial_insertion_sort(begin, begin, end, comp);
}
 

template <class It, class Comp>
inline void timsort(It begin, It end, Comp comp)
{
	_timsort(begin, end, comp);
}


template <class It>
inline void timsort(It begin, It end)
{
	timsort(begin, end, std::less<>());
}

} /* namespace tim */
#endif /* TIMSORT_H */
