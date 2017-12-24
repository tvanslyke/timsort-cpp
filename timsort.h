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






/* Stolen from cpython source... */
static constexpr std::size_t compute_minrun(std::size_t n)
{
    std::size_t r = 0;           /* becomes 1 if any 1 bits are shifted off */
    while (n >= 64) {
        r |= (n & 1);
        n >>= 1;
    }
    return n + r;
}





template <class Comp>
static auto reverse_comparator(Comp& comp)
{
	return [&](auto && a, auto && b) {
		using a_t = decltype(a);
		using b_t = decltype(b);
		return comp(std::forward<b_t>(b), std::forward<a_t>(a));
	};
}
template <class Comp>
static auto invert_comparator(Comp& comp)
{
	return [&](auto && a, auto && b) {
		using a_t = decltype(a);
		using b_t = decltype(b);
		return not comp(std::forward<a_t>(a), std::forward<b_t>(b));
	};
}
template <class Comp>
static auto invert_and_reverse_comparator(Comp& comp)
{
	return [&](auto && a, auto && b) {
		using a_t = decltype(a);
		using b_t = decltype(b);
		return not comp(std::forward<b_t>(b), std::forward<a_t>(a));
	};
}



template <class SizeType, 
	  class It,
	  class Comp>
struct TimSort
{
	using value_type = typename std::iterator_traits<It>::value_type;
	TimSort(It begin_it, It end_it, Comp comp_func):
		stack_buffer{},
		temp_buffer{},
		start(begin_it), 
		comp(std::move(comp_func)), 
		minrun(compute_minrun(end_it - begin_it)),
		min_gallop(default_min_gallop)
	{
		COMPILER_ASSUME_(minrun > 0);
		COMPILER_ASSUME_(minrun < 65);
		fill_run_stack(end_it);
		collapse_run_stack();
	}
	
	
	inline void fill_run_stack(It end)
	{
		auto pos = push_next_run(start, end);
		if(not (pos < end))
			return;
		pos = push_next_run(pos, end);
		while(pos < end)
		{
			resolve_invariants();
			pos = push_next_run(pos, end);
		}
	}
	
	inline void collapse_run_stack()
	{
		// resolve_invariants();
		// std::size_t bottom_run_len = stack_buffer.get_bottom_run_size();
		// temp_buffer.reserve(std::min(bottom_run_len, total_length - bottom_run_len));
		
		while(stack_buffer.run_count() > 1)
		{
			merge_top_2();
		}
	}

	inline It push_next_run(It pos, It end)
	{
		pos = get_run(pos, end);
		stack_buffer.push(pos - start);
		return pos;
	}

	/*
	 * MERGE PATTERN STUFF
	 */
	
	template <std::size_t RunIndex>
	inline auto run_len() const noexcept
	{
		return get_offset<RunIndex>() - get_offset<RunIndex + 1>();
	}
	
	void resolve_invariants()
	{
		for(auto run_count = stack_buffer.run_count(); run_count > 3; --run_count)
		{
			if((run_len<2>() <= run_len<1>() + run_len<0>()) 
			   or (run_len<3>() <= run_len<2>() + run_len<1>()))
				merge_top_2();
			else if(run_len<1>() <= run_len<0>())
				merge_top_3();
			else
				return;
		}

		if((stack_buffer.run_count() > 2) and (run_len<2>() <= run_len<1>() + run_len<0>()))
			merge_top_3();

		if((stack_buffer.run_count() > 1) and (run_len<1>() <= run_len<0>()))
			merge_top_2();
	}

	/* RUN STACK STUFF */


	inline void merge_top_2() 
	{
		merge_runs(start + get_offset<2>(),
			   start + get_offset<1>(),
			   start + get_offset<0>());
		stack_buffer.template remove_run<1>();
	}

	inline void merge_top_3()
	{
		if(run_len<2>() < run_len<0>()) 
		{
			merge_runs(start + get_offset<3>(),
				   start + get_offset<2>(),
				   start + get_offset<1>());
			
			stack_buffer.template remove_run<2>();
		}
		else
			merge_top_2();
	}
	

	template <std::size_t I>
	inline auto get_offset() const
	{
		return stack_buffer.template get_offset<I>();
	}

	/*
	 * MERGE/SORT IMPL STUFF
	 */
	It get_run(It begin, It end)
	{
		auto pos = std::is_sorted_until(begin, end, comp);
		if(pos - begin < 2)
		{
			pos = std::adjacent_find(pos, end, 
				[comp=this->comp](auto&& a, auto && b) { 
					return not comp(std::forward<decltype(b)>(b), 
						        std::forward<decltype(a)>(a)); 
				}
			);
			std::reverse(begin, pos);
		}
		// need to extend pos until pos - begin >= minrun or pos == end
		if(pos - begin < minrun)
		{
			// having a run of length 1 breaks the assumptions of some other subroutines
			// these checks ensure that 
			//      1) there are at least 'minrun' elements between 'begin' and 'end'
			// and
			//      2) if so, there will be more than 1 element in the next run (not the current run)
			// 
			// if 1 is not satisfied, this is the last run and trying to sort to 'begin + minrun' would
			// be a no-no.
			// if 2 is not satisfied, we extend to minrun + 1 elements (by leaving 'end' alone), thus
			// ensuring that there is no run of length 1
			if(auto len = end - begin; len > minrun and len - minrun > 1)
				end = begin + minrun;
			partial_insertion_sort(begin, pos, end, comp);
			return end;
		}
		else
		{
			return pos;
		}
	}

	inline void merge_runs(It begin, It mid, It end)
	{
		begin = std::upper_bound(begin, mid, *mid, comp);
		end = std::lower_bound(mid, end, *(mid - 1), comp);
		// TODO: remove this if ?
		if(begin == mid or mid == end)
			return;
		else if((end - mid) > (mid - begin))
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

	template <class Iter, class Cmp>
	inline void do_merge(Iter begin, Iter mid, Iter end, Cmp cmp)
	{
		if(stack_buffer.can_acquire_merge_buffer(begin, mid)) 
		{
			// allocate the merge buffer on the stack
			auto stack_mem = stack_buffer.move_to_merge_buffer(begin, mid);
			min_gallop = gallop_merge_ex(stack_mem, stack_mem + (mid - begin), 
						     mid, end, 
						     begin, cmp, min_gallop);
			// destroy the now moved-from temporary buffer.
			// the stack merge buffer would be cleaned up in 'stack_buffer's destructor
			// anyway, but we need to manually free it when we're done because the buffer
			// is also used (from the other end) to store the positions of each run.
			// stack_buffer.destroy_merge_buffer();
		}
		else
		{
			// fall back to a std::vector<> for the merge buffer 
			temp_buffer.assign(std::make_move_iterator(begin), std::make_move_iterator(mid));
			min_gallop = gallop_merge_ex(temp_buffer.begin(), temp_buffer.end(), 
						     mid, end, 
						     begin, cmp, min_gallop);
		}
	}
	

	timsort_stack_buffer<SizeType, value_type> stack_buffer; 
	std::vector<value_type> temp_buffer;
	const It start;
	Comp comp;
	
	// TODO:  See if passing minrun up the call stack is faster than having it as a member variable
	const std::ptrdiff_t minrun;
	std::size_t min_gallop;
	
	static constexpr const std::size_t default_min_gallop = gallop_win_dist;
};


struct MinimizeStackUsageTag {};
struct AssumeComparisonsExpensiveTag {};

template <class It, class Comp, class ... Tags>
static inline void _timsort(It begin, It end, Comp comp, Tags ... tags)
{
	std::size_t len = end - begin;
	if(len > 1)
	{
		// the first type parameter decides what type is used to 
		// store offsets on the stack.  We want to use the smallest integral 
		// type we can so that we can keep our cache footprint as low as possible.
		// unfortunately, this does mean we're more likely to head-allocate our
		// merge buffer storage when we 
		// if(len <= std::size_t(std::numeric_limits<unsigned char>::max()))
		// 	TimSort<unsigned char, It, Comp>(begin, end, comp);
		// else if(len <= std::size_t(std::numeric_limits<unsigned short>::max()))
		// 	TimSort<unsigned short, It, Comp>(begin, end, comp);
		// if(len <= std::size_t(std::numeric_limits<unsigned int>::max()))
		// 	TimSort<unsigned int, It, Comp>(begin, end, comp);
		// else if(len <= std::size_t(std::numeric_limits<unsigned long>::max()))
		// 	TimSort<unsigned long, It, Comp>(begin, end, comp);
		// else
			TimSort<unsigned long long, It, Comp>(begin, end, comp);
	}
}
 

template <class It, class Comp>
inline void timsort(It begin, It end, Comp comp, bool synchronize)
{
	_timsort(begin, end, comp);
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
