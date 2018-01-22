#ifndef TIMSORT_H
#define TIMSORT_H

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <utility>
#include <functional>
#include <vector>
#include <limits>
#include "utils.h"
#include "timsort_stack_buffer.h"
#include "minrun.h"
#include "compiler.h"

namespace tim {

namespace internal {

template <class It,
	  class Comp>
struct TimSort
{
	/**
	 * @brief Perform a timsort on the range [begin_it, end_it).
	 * @param begin_it   Random access iterator to the first element in the range.
	 * @param end_it     Past-the-end random access iterator.
	 * @param comp_func  Comparator to use.
	 */ 
	using value_type = iterator_value_type_t<It>;
	TimSort(It begin_it, It end_it, Comp comp_func):
		stack_buffer{},
		heap_buffer{},
		start(begin_it), 
		stop(end_it),
		position(begin_it),
		comp(comp_func), 
		minrun(compute_minrun<value_type>(end_it - begin_it)),
		min_gallop(default_min_gallop)
	{
		// try_get_cached_heap_buffer(heap_buffer);
		fill_run_stack();
		collapse_run_stack();
		// try_cache_heap_buffer(heap_buffer);
	}
	
	
	/* 
	 * Continually push runs onto the run stack, merging adjacent runs
	 * to resolve invariants on the size of the runs in the stack along 
	 * the way.
	 */
	void fill_run_stack()
	{
		// push the first two runs on to the run stack, unless there's
		// only one run.
		push_next_run();
		if(not (position < stop))
			return;
		push_next_run();
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
		for(auto count = stack_buffer.run_count() - 1; count > 0; --count)
			merge_BC();
	}

	/* 
	 * Get the next run of already-sorted elements.  If the length of the 
	 * natural run is less than minrun, force it to size with an insertion sort.
	 */

	void push_next_run() 
	{
		// check if the next run is at least two elements long
		if(const std::size_t remain = stop - position;
		   COMPILER_LIKELY_(remain > 1))
		{
			std::size_t idx = 2;
			// descending?
			if(comp(position[1], position[0]))
			{
				// see how long it is descending for and then reverse it
				while(idx < remain and comp(position[idx], position[idx - 1]))
					++idx;
				std::reverse(position, position + idx);
			}
			// ascending 
			// even if the run was initially descending, after reversing it the
			// following elements may form an ascending continuation of the 
			// now-reversed run.
			// unconditionally attempt to continue the ascending run
			while(idx < remain and not comp(position[idx], position[idx - 1])) 
				++idx;
			// if needed, force the run to 'minrun' elements, or until all elements 
			// in the range are exhausted (whichever comes first) with an insertion
			// sort.  
			if(idx < remain and idx < minrun)
			{
				auto extend_to = std::min(minrun, remain);
				finish_insertion_sort(position, position + idx, position + extend_to, comp);
				idx = extend_to;
			}
			// advance 'position' by the length of the run we just found
			position += idx;
		}
		else
		{
			// only one element
			position = stop;
		}
		// push the run on to the run stack.
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
		// Check all of the invariants in a loop while there are at least
		// two runs.  
		auto run_count = stack_buffer.run_count();
		do{
			if(((run_count > 2) and stack_buffer.merge_ABC_case_1()) 
			   or ((run_count > 3) and stack_buffer.merge_ABC_case_2()))
				if(stack_buffer.merge_AB())
					merge_AB();
				else
					merge_BC();
			else if(stack_buffer.merge_BC())
				merge_BC();
			else
				break;
			--run_count;
		} while(run_count > 1);
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
	 *
	 * Each offset on the run stack is the end position of each run.  So
	 * if we wanted two iterators defining the run at the top of the stack
	 * we would do something like this:
	 * 	
	 * 	auto begin = this->start + this->get_offset<1>();
	 * 	auto end   = this->start + this->get_offset<0>();
	 */
	template <std::size_t I>
	inline auto get_offset() const noexcept
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
	void merge_runs(It begin, It mid, It end) 
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
						// reverse the comparator
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
	void do_merge(Iter begin, Iter mid, Iter end, Cmp cmp)
	{
		// check to see if we can use the run stack as a temporary buffer
		if(stack_buffer.can_acquire_merge_buffer(begin, mid)) 
		{
			// allocate the merge buffer on the stack
			auto stack_mem = stack_buffer.move_to_merge_buffer(begin, mid);
			gallop_merge(stack_mem, stack_mem + (mid - begin), 
						     mid, end, 
						     begin, cmp);
		}
		else
		{
			// TODO: clean this up
			// fall back to a std::vector<> for the merge buffer 
			// try to use memcpy if possible
			// 
			// bypass speculative overallocation of std::vector.
			// this measurably improves benchmarks for all cases.
			heap_buffer.reserve(mid - begin);
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
				gallop_merge(heap_buffer.begin(), heap_buffer.end(),
							     mid, end, 
							     begin, cmp);
			}
			else
			{
				heap_buffer.resize(mid - begin);
				std::memcpy(heap_buffer.data(), get_memcpy_iterator(mid - 1), (mid - begin) * sizeof(value_type));
				gallop_merge(heap_buffer.rbegin(), heap_buffer.rend(),
							     mid, end, 
							     begin, cmp);
			}
			// clear the vector so that the next call to reserve() 
			// doesn't attempt to copy anything if it reallocates
			// it also helps to call the destructors while the
			// heap buffer is still hot (possibly cached)
			heap_buffer.clear();
		}
	}

	/**
	 * @brief Implementation of the merge routine.
	 * @param lbegin  Iterator to the begining of the left range.
	 * @param lend    Iterator to the end of the left range.
	 * @param rbegin  Iterator to the begining of the right range.
	 * @param rend    Iterator to the end of the right range.
	 * @param cmp     Comparator.  Either this->comp or a lambda reversing
	 *	  	  the direction of this->comp.
	 * 
	 * This merge rountine has been fine-tuned to take advantage of the 
	 * the preconditions imposed by the other components of this timsort
	 * implementation.  This is not a general-purpose merge routine and
	 * it should not be used outside of this implementation.
	 * 
	 * Most of the logic of this merge routine is implemented inline, and 
	 * questionable constructs (such as gotos) are used where benchmarking
	 * has shown that they speed up the routine. The code isn't pretty, 
	 * but it's damn fast.
	 * 
	 * requires:
	 * 	[lbegin, lend) does not overlap with [rbegin, rend)
	 *	lend - lbegin > 0 and rend - rbegin > 0
	 *      cmp(*rbegin, *lbegin)
	 *      cmp(rend[-1], lend[-1]) 
	 */ 
	template <class LeftIt, class RightIt, class DestIt, class Cmp>
	void gallop_merge(LeftIt lbegin, LeftIt lend, RightIt rbegin, RightIt rend, DestIt dest, Cmp cmp)
	{
		// God bless you if you're reading this.  I'll try to explain 
		// what I'm doing here to the best of my ability.  Much like the
		// merge routine in CPython's list_sort(), there are two modes 
		// of operation, linear and galloping.  Linear mode is simply
		// the classic merge routine with the caveat that after one of
		// the two ranges wins 'this->min_gallop' times in a row, we
		// jump into the gallop loop.
		// 
		// To understand the gallop loop, see the required reading here:
		// https://github.com/python/cpython/blob/master/Objects/listsort.txt
		//	
		// Here are few refinements we make over CPython's list_sort().
		// Each of these were benchmarked on a variety of test cases 
		// to ensure that they actually sped up the merge routine.
		// 	- When switching to galloping mode from linear mode, 
		// 	  jump straight to the left range if we were already
		// 	  merging the left range in linear mode, and jump 
		// 	  do the opposite if we were merging the right range.
		//      - If we're already in galloping mode and we're changing
		//        to the other range (e.g. just finished the left range
		// 	  and are now going to gallop in the right range) set
		// 	  start galloping at index 1 instead of 0.  This saves
		// 	  a redundant comparison.
		// 	  This is achieved by setting num_galloped to 1 just
		//	  before the entry points for the gotos in the linear 
		// 	  mode loop.  Every time we enter the linear mode loop,
		//  	  num_galloped is set to zero again.
		// 	- Since, before each call to gallop_merge() we first
		// 	  make sure to cut off all elements at the end of the 
		//	  right range that are already in the correct place, we
		// 	  know that we will exhaust the right range before the
		// 	  left.  This means we can skip any "lbegin < lend" 
		// 	  checks and instead just unconditionally copy the 
		// 	  remainder of the left range into the destination
		// 	  range once we've used up the right range.
		// 	- Another small optimization that speeds up the merge
		// 	  if we're sorting trivially copyable types, is using 
		// 	  memcpy() when copying from the left range.  Since 
		// 	  the left range is always the one that got copied into
		// 	  the merge buffer, we know that the left range doesn't
		//	  overlap with the destination range.  Some additional 
		// 	  compile-time checks are done to ensure that memcpy()
		// 	  can be done without invoking UB. (e.g. checking that
		// 	  'dest' is a contiguous iterator or that both 'DestIt' 
		// 	  and 'LeftIt' are reverse contiguous iterators and 
		// 	  ensuring that values being sorted are trivially 
		// 	  copyable.
		for(std::size_t num_galloped=0, lcount=0, rcount=0 ; ;)
		{
			// LINEAR SEARCH MODE
			// do a naive merge until evidence shows that galloping may be faster.
			 
			// set lcount to 1 if num_galloped > 0. if we got here from exiting the
			// gallop loop, then we already copied one extra element from the 
			// left range and we should count that towards lcount here.
			for(lcount=(num_galloped > 0), rcount=0, num_galloped=0;;)
			{
				if(cmp(*rbegin, *lbegin))
				{
					// move from the right-hand-side
					*dest = std::move(*rbegin);
					++dest;
					++rbegin;
					++rcount;
					// merge is done.  copy the remaining elements from the left range and return
					if(not (rbegin < rend))
					{
						move_or_memcpy(lbegin, lend, dest);
						return;
					}
					else if(rcount >= min_gallop)
						goto gallop_right; // continue this run in galloping mode
					lcount = 0;
				}
				else
				{
					// move from the left-hand side
					*dest = std::move(*lbegin);
					++dest;
					++lbegin;
					++lcount;
					// don't need to check if we reached the end.  that will happen on the right-hand-side 
					if(lcount >= min_gallop) 
						goto gallop_left; // continue this run in galloping mode
					rcount = 0;
				}
			}
			COMPILER_UNREACHABLE_;
			// GALLOP SEARCH MODE
			while(lcount >= gallop_win_dist or rcount >= gallop_win_dist) {
				// decrement min_gallop every time we continue the gallop loop
				if(min_gallop > 1) 
					--min_gallop;

				// we already know the result of the first comparison, so set num_galloped to 1 and skip it
				num_galloped = 1;
			    gallop_left: // when jumping here from the linear merge loop, num_galloped is set to zero
				lcount = lend - lbegin;
				// gallop through the left range
				while((num_galloped < lcount) and not cmp(*rbegin, lbegin[num_galloped])) 
					num_galloped = 2 * num_galloped + 1;
				if(lcount > num_galloped)
					lcount = num_galloped;
				// do a binary search in the narrowed-down region
				lcount = std::upper_bound(lbegin + (num_galloped / 2), lbegin + lcount, *rbegin, cmp) - lbegin;
				dest = move_or_memcpy(lbegin, lbegin + lcount, dest);
				lbegin += lcount;

				// don't need to check if we reached the end.  that will happen on the right-hand-side 
				// we already know the result of the first comparison, so set num_galloped to 1 and skip it
				num_galloped = 1;
			    gallop_right: // when jumping here from the linear merge loop, num_galloped is set to zero
				rcount = rend - rbegin;
				// gallop through the right range
				while((num_galloped < rcount) and cmp(rbegin[num_galloped], *lbegin)) 
					num_galloped = 2 * num_galloped + 1;
				if(rcount > num_galloped)
					rcount = num_galloped;
				// do a binary search in the narrowed-down region
				rcount = std::lower_bound(rbegin + (num_galloped / 2), rbegin + rcount, *lbegin, cmp) - rbegin;
				dest = std::move(rbegin, rbegin + rcount, dest);
				rbegin += rcount;

				// merge is done.  copy the remaining elements from the left range and return
				if(not (rbegin < rend))
				{
					move_or_memcpy(lbegin, lend, dest);
					return;
				}
			}
			// exiting the loop means we just finished galloping 
			// through the right-hand side.  We know for a fact 
			// that 'not cmp(*rbegin, *lbegin)', so do one copy for free.
			++min_gallop;
			*dest = std::move(*lbegin);
			++dest;
			++lbegin;
		}
		COMPILER_UNREACHABLE_;
	}

	/**
	 * Stack-allocated stack data structure that holds location (end position) 
	 * of each run.  Bottom of the stack always holds 0.
	 * Empty stack space is used for merge buffer when possible.
	 */
	timsort_stack_buffer<std::size_t, value_type> stack_buffer; 
	/** Fallback heap-allocated array used for merge buffer. */
	std::vector<value_type> heap_buffer;
	/** 'begin' iterator to the range being sorted. */
	const It start;
	/** 'end' iterator to the range being sorted. */
	const It stop;
	/** 
	 * Iterator to keep track of how far we've scanned into the range to be
	 * sorted.  [start, position) contains already-found runs while 
	 * [position, stop) is still untouched.  When position == end, the run stack
	 * is collapsed.
	 */
	It position;
	/** Comparator used to sort the range. */
	Comp comp;
	/** Minimum length of a run */
	const std::size_t minrun;
	/** 
	 * Minimum number of consecutive elements for which one side of the 
	 * merge must "win" in a row before switching from galloping mode to
	 * linear mode.
	 */
	std::size_t min_gallop = default_min_gallop;
	
	static constexpr const std::size_t default_min_gallop = gallop_win_dist;
};




template <class It, class Comp>
static void _timsort(It begin, It end, Comp comp)
{
	using value_type = iterator_value_type_t<It>;
	std::size_t len = end - begin;
	if(len > max_minrun<value_type>())
		TimSort<It, Comp>(begin, end, comp);
	else
		finish_insertion_sort(begin, begin + (end > begin), end, comp);
}
 
} /* namespace internal */


template <class It, class Comp>
void timsort(It begin, It end, Comp comp)
{
	internal::_timsort(begin, end, comp);
}


template <class It>
void timsort(It begin, It end)
{
	timsort(begin, end, tim::internal::DefaultComparator{}); 
}



} /* namespace tim */


#include "undef_compiler.h"

#endif /* TIMSORT_H */
