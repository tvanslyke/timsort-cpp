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
    return (n + r);
}


struct LowerBound
{
	template <class ... Args>
	auto operator()(Args&& ... args) const
	{
		return std::lower_bound(std::forward<Args>(args)... );
	}
};
struct UpperBound
{
	template <class ... Args>
	auto operator()(Args&& ... args) const
	{
		return std::upper_bound(std::forward<Args>(args)... );
	}
};

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
		heap_buffer{},
		start(begin_it), 
		stop(end_it),
		position(begin_it),
		comp(std::move(comp_func)), 
		minrun(compute_minrun(end_it - begin_it)),
		min_gallop(default_min_gallop)
	{
		try_get_cached_heap_buffer(heap_buffer);
		COMPILER_ASSUME_(minrun > 31 and minrun < 65);
		fill_run_stack();
		collapse_run_stack();
		try_cache_heap_buffer(heap_buffer);
	}
	
	
	inline void fill_run_stack()
	{
		push_next_run();
		if(not (position < stop))
			return;
		push_next_run();
		while(position < stop)
		{
			resolve_invariants();
			push_next_run();
		}
	}
	
	inline void collapse_run_stack()
	{
		for(auto count = stack_buffer.run_count(); count > 1; --count)
		{
			merge_BC();
		}
	}

	inline void push_next_run()
	{
		if(auto run_end = count_run(position, stop, comp); run_end - position < minrun)
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
	
	template <std::size_t RunIndex>
	inline auto run_len() const noexcept
	{
		return get_offset<RunIndex>() - get_offset<RunIndex + 1>();
	}
	
	void resolve_invariants()
	{
		auto run_count = stack_buffer.run_count();
		for(; run_count > 3; --run_count)
		{
			if(const bool abc = stack_buffer.merge_ABC(); abc and stack_buffer.merge_AB())
				merge_AB();
			else if(abc or stack_buffer.merge_BC())
				merge_BC();
			else
				return;
		}

		if((run_count > 2) and stack_buffer.merge_ABC_case_1())
		{
			if(stack_buffer.merge_AB())
				merge_AB();
			else
				merge_BC();
		}

		if((run_count > 1) and stack_buffer.merge_BC())
			merge_BC();
	}

	/* RUN STACK STUFF */

	inline void merge_BC_fast() 
	{
		merge_runs(start + get_offset<2>(),
			   start + get_offset<1>(),
			   stop);
	}

	inline void merge_BC() 
	{
		merge_runs(start + get_offset<2>(),
			   start + get_offset<1>(),
			   start + get_offset<0>());
		stack_buffer.template remove_run<1>();
	}
	
	inline void merge_AB()
	{
		merge_runs(start + get_offset<3>(),
			   start + get_offset<2>(),
			   start + get_offset<1>());
		stack_buffer.template remove_run<2>();
	}

	template <std::size_t I>
	inline auto get_offset() const
	{
		return stack_buffer.template get_offset<I>();
	}

	/*
	 * MERGE/SORT IMPL STUFF
	 */
	It get_run() const
	{
		if(auto run_end = count_run(position, stop, comp); run_end - position < minrun)
		{
			auto limit = stop;
			if(stop - position > minrun)
				limit = position + minrun;
			partial_insertion_sort(position, run_end, limit, comp);
			return limit;
		}
		else
			return run_end;
	}

	It _get_run(It begin, It end)
	{

		// TODO: keep track of whether last two were ascending or descending for next run
		// 	 when merging only do search if there has been at least one run that hada decent
		// 	 run size
		// auto pos = my_is_sorted_until<false>(begin, end, comp);
		// if(pos - begin < 2)
		// {
		// 	// pos = std::adjacent_find(pos, end, 
		// 	// 	[comp=this->comp](auto&& a, auto && b) { 
		// 	// 		return not comp(std::forward<decltype(b)>(b), 
		// 	// 			        std::forward<decltype(a)>(a)); 
		// 	// 	}
		// 	// );
		// 	pos += (pos < end);
		// 	pos = my_is_sorted_until<true>(pos, end, comp);
		// 	std::reverse(begin, pos);
		// }
		auto pos = get_existing_run(begin, end, comp);
		if(pos - begin < minrun)
		{
			if(end - begin > minrun)
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
		begin = gallop_upper_bound(begin, mid, *mid, comp);
		end = gallop_upper_bound(std::make_reverse_iterator(end), 
					 std::make_reverse_iterator(mid), 
					 mid[-1], 
					 [comp=this->comp](auto&& a, auto&& b){
					    return comp(std::forward<decltype(b)>(b), std::forward<decltype(a)>(a));
					 }).base();
		// begin = std::upper_bound(begin, mid, *mid, comp);
		// end = std::lower_bound(mid, end, mid[-1], comp);
		// TODO: remove this if ?
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
		}
		else
		{
			// fall back to a std::vector<> for the merge buffer 
			// try to use memcpy if possible
			if constexpr(can_forward_memcpy_v<Iter> or not can_reverse_memcpy_v<Iter>)
			{
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
			else // if constexpr(can_reverse_memcpy_v<Iter>)
			{
				heap_buffer.resize(mid - begin);
				std::memcpy(heap_buffer.data(), get_memcpy_iterator(mid - 1), (mid - begin) * sizeof(value_type));
				min_gallop = gallop_merge_ex(heap_buffer.rbegin(), heap_buffer.rend(),
							     mid, end, 
							     begin, cmp, min_gallop);
			}
		}
	}
	
	static constexpr void try_get_cached_heap_buffer(std::vector<value_type>& vec)
	{
		if(std::unique_lock lock(heap_buffer_cache_mutex, std::try_to_lock_t{}); lock.owns_lock())
		{
			vec = std::move(heap_buffer_cache);
		}
	};

	static constexpr void try_cache_heap_buffer(std::vector<value_type>& vec)
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

	// TODO:  See if passing minrun up the call stack is faster than having it as a member variable
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

struct MinimizeStackUsageTag {};
struct AssumeComparisonsExpensiveTag {};

template <class It, class Comp, class ... Tags>
static inline void _timsort(It begin, It end, Comp comp, Tags ... tags)
{
	std::size_t len = end - begin;
	if(len > 64)
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
	else
	{
		partial_insertion_sort(begin, begin, end, comp);
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
