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



/* Stolen from cpython source... */
static std::size_t compute_minrun(std::size_t n)
{
    std::size_t r = 0;           /* becomes 1 if any 1 bits are shifted off */

    while (n >= 64) {
        r |= (n & 1);
        n >>= 1;
    }
    return n + r;
}


static constexpr std::size_t div_by_log2phi(std::size_t num)
{
	constexpr const double log2phi = 0.69424191363061737991557720306445844471454620361328125;
	return static_cast<std::size_t>(num / log2phi) + 1;
}





struct TimSortRunStack
{
	using size_type = std::size_t;
	using stack_pointer_t = std::reverse_iterator<size_type*>;
	TimSortRunStack(stack_pointer_t stack_pointer):
		stack_top(stack_pointer),
		stack_bottom(stack_pointer)
	{
		*stack_top++ = 0;
	}
	size_type size() const
	{
		return (size_type)std::distance(stack_bottom, stack_top);
	}
	size_type run_count() const
	{
		return size() - 1;
	}
	void push(size_type v)
	{
		*stack_top++ = v;
	}
	void pop()
	{
		--stack_top;
	}
	size_type& top()
	{
		return stack_top[-1];
	}
	const size_type& top() const
	{
		return stack_top[-1];
	}
	size_type& top(size_type pos)
	{
		return stack_top[-std::ptrdiff_t(pos + 1)];
	}
	const size_type& top(size_type pos) const
	{
		return stack_top[-std::ptrdiff_t(pos + 1)];
	}
	void erase(size_type pos)
	{
		std::move(stack_top - pos, stack_top, stack_top - (pos + 1));
		pop();
	}


	///// debugging
	void print_run_stack() const
	{
		std::ostream_iterator<std::size_t> ostr_iter(std::cout, ", ");
		std::adjacent_difference(stack_bottom, stack_top, ostr_iter);
		std::cout << std::endl;
	}
	
	
	auto _top_ptr()
	{
		return stack_top.base();
	}
	auto _top_ptr() const
	{
		return stack_top.base();
	}
private:
	stack_pointer_t stack_top;
	stack_pointer_t stack_bottom;
};


template <class T>
struct TimSortStackBuffer
{
	using value_type = T;
	using size_type = std::size_t;
	TimSortStackBuffer():
		buffer{},
		borrowed_count{0},
		run_stack(std::make_reverse_iterator(buffer_end<size_type>()))
	{
		
	}
	
	~TimSortStackBuffer()
	{
		destroy_merge_buffer();
	}
	
	TimSortStackBuffer(const TimSortStackBuffer<T>&) = delete;
	TimSortStackBuffer(TimSortStackBuffer<T>&&) = delete;
	TimSortStackBuffer<T>& operator=(const TimSortStackBuffer<T>&) = delete;
	TimSortStackBuffer<T>& operator=(TimSortStackBuffer<T>&&) = delete;

	template <class U>
	U* buffer_begin()
	{
		return reinterpret_cast<U*>(buffer);
	}
	template <class U>
	U* buffer_end()
	{
		return reinterpret_cast<U*>(buffer + stack_buffer_size());
	}
	

	size_type bytes_used_top() const
	{
		return run_stack.size() * sizeof(size_type);
	}


	size_type bytes_avail_for_merge_buff() const
	{
		return (buffer_bytes - bytes_used_top());
	}
	
	size_type bytes_used_bottom() const
	{
		return sizeof(value_type) * borrowed_count;
	}
	
	size_type count_avail_for_merge_buff() const
	{
		if constexpr(value_type_too_big)
		{
			return 0;
		}
		else
		{
			const size_type bytes = bytes_avail_for_merge_buff();
			return (bytes / sizeof(value_type));
		}
	}
	
	value_type* merge_buffer_begin()
	{
		return buffer_begin<value_type>();
	}
	value_type* merge_buffer_end()
	{
		return buffer_begin<value_type>() + borrowed_count;
	}

	void destroy_merge_buffer()
	{
		if constexpr(not value_type_too_big)
		{
			// destroy existing elements in the buffer
			auto mb_rbegin = std::make_reverse_iterator(merge_buffer_end());
			auto mb_rend = std::make_reverse_iterator(merge_buffer_begin());
			// destroy from top-to-bottom
			for(auto it = mb_rbegin; it != mb_rend; ++it)
			{
				it->~value_type();
				--borrowed_count;
			}
		}
	}
	template <class It>
	void initialize_merge_buffer(It begin, It end)
	{
		// TODO: things differently under different noexcept circumstances
		if(borrowed_count > 0)
			destroy_merge_buffer();
		auto* buff = merge_buffer_begin();
		while(begin != end)
		{
			new(buff) value_type(std::move(*begin));
			++buff;
			++begin;
			++borrowed_count;
		}
	}
	template <class It>
	value_type* get_merge_buffer(It begin, It end)
	{
		if constexpr(value_type_too_big)
		{
			return nullptr;
		}
		else
		{
			const size_type count = std::distance(begin, end);
			const size_type avail = count_avail_for_merge_buff();
			// TODO: check that '<=' is for-sure the right op here

			auto bot_lim = reinterpret_cast<const char*>(buffer) + (avail + 1) * sizeof(value_type);
			auto top_lim = reinterpret_cast<const char*>(buffer + stack_buffer_size() - run_stack.size());
			if(count <= avail)
			{
				initialize_merge_buffer(begin, end);
				return merge_buffer_begin();
			}
			else
				return nullptr;
		}
	}
	bool release_merge_buffer(value_type* buff)
	{
		bool owns_buffer = buff == merge_buffer_begin();
		if(owns_buffer)
			destroy_merge_buffer();
		return owns_buffer;
	}
	////// run-stack stuff //////
	TimSortRunStack& get_run_stack()
	{
		return run_stack;
	}
	const TimSortRunStack& get_run_stack() const
	{
		return run_stack;
	}

private:
	static constexpr std::size_t stack_buffer_size()
	{
		return div_by_log2phi(std::numeric_limits<size_type>::digits) + 1;
	}
	static constexpr const std::size_t buffer_bytes = sizeof(std::size_t) * stack_buffer_size();
	static constexpr const bool value_type_too_big = buffer_bytes <= sizeof(value_type);
	using stack_pos_t = std::reverse_iterator<size_type*>;
	
	alignas(std::max_align_t) size_type buffer[stack_buffer_size()];
	std::size_t borrowed_count;
	TimSortRunStack run_stack;
};

template <class It>
struct TimSortMemoryManager
{
	
	TimSortMemoryManager(It begin):
		stack_buffer(),
		local_buffer(),
		global_buffer_lock(global_buffer_mutex, std::defer_lock),
		start(begin)
	{
		
	}
	using value_t = typename std::iterator_traits<It>::value_type;


	template <class SrcIt>
	value_t* get_merge_memory(SrcIt begin, SrcIt end)
	{
		// local_buffer.assign(begin, end);
		// return local_buffer.data();
		bool has_global_buffer = global_buffer_lock.owns_lock();
		// first try to use stack space otherwise intended for the timsort() merge stack
		value_t* mem_begin = stack_buffer.get_merge_buffer(begin, end);
		if(mem_begin)
		{
			if(has_global_buffer)
				global_buffer_lock.unlock();
			return mem_begin;
		}
		// if the above failed, try the global buffer 
		// avoids reallocation on subsequent calls to timsort().
		// also it's probably hotter in memory if timsort() is being called more than once
		else if(has_global_buffer or global_buffer_lock.try_lock())	
		{
			global_buffer.assign(std::make_move_iterator(begin), std::make_move_iterator(end));
			return global_buffer.data();
		}
		// fall back to a temporary vector
		else
		{
			local_buffer.assign(std::make_move_iterator(begin), std::make_move_iterator(end));
			return local_buffer.data();
		}
	}
	void release_merge_memory(value_t* mem)
	{
		if(stack_buffer.release_merge_buffer(mem))
			return;
		else if(global_buffer_lock.owns_lock())
		{
			// global_buffer.resize(0);
			global_buffer_lock.unlock();
		}
		else
		{
			// assert(local_buffer.size() > 0 and (local_buffer.data() == mem));
		}
	}
	
	std::size_t pending_run_count() const
	{
		return run_stack().run_count();
	}
	void push_run(It run)
	{
		run_stack().push(std::distance(start, run));
	}
	It a_begin()
	{
		return start + run_stack().top(3);
	}
	It a_end()
	{
		return start + run_stack().top(2);
	}	
	It b_begin()
	{
		return a_end();
	}	
	It b_end()
	{
		return start + run_stack().top(1);
	}	
	It c_begin()
	{
		return b_end();
	}	
	It c_end()
	{
		return start + run_stack().top();
	}
	void merge_ab()
	{
		run_stack().erase(2);
	}
	void merge_bc()
	{
		run_stack().erase(1);
	}
	/////// debug //////
	void print_run_stack() const 
	{
		run_stack().print_run_stack();
	}
private:
	TimSortRunStack& run_stack()
	{
		return stack_buffer.get_run_stack();
	}
	const TimSortRunStack& run_stack() const
	{
		return stack_buffer.get_run_stack();
	}
	TimSortStackBuffer<value_t> stack_buffer;
	std::vector<value_t> local_buffer;
	std::unique_lock<std::mutex> global_buffer_lock;
	It start;
	static std::vector<value_t> global_buffer;
	static std::mutex global_buffer_mutex;
};

template <class It>
std::vector<typename TimSortMemoryManager<It>::value_t> TimSortMemoryManager<It>::global_buffer{};
template <class It>
std::mutex TimSortMemoryManager<It>::global_buffer_mutex;

template <class It, class Comp>
bool relaxed_is_sorted(It begin, It end, Comp comp)
{
	if(std::distance(begin, end) < 2)
		return true;
	It pos = begin;
	auto prev = *pos;
	++pos;
	auto current = *pos;
	while(pos != end)
	{
		if(not (comp(prev, current) or (not comp(current, prev))))
			return false;
		prev = current;
		current = *pos++;
	}
	return true;
}

template <class It, 
	  class Comp, 
	  class NComp, 
	  class RComp, 
	  class NRComp>
struct TimSort
{
	using value_type = typename std::iterator_traits<It>::value_type;
	TimSort(It begin_it, It end_it, Comp& comp_func, NComp& ncomp_func, RComp& rcomp_func, NRComp& nrcomp_func):
		memory_manager(begin_it),
		start(begin_it), 
		stop(end_it), 
		comp(comp_func), 
		ncomp(ncomp_func), 
		rcomp(rcomp_func), 
		nrcomp(nrcomp_func), 
		minrun(compute_minrun(std::distance(start, stop))),
		min_gallop(default_min_gallop)
	{
		fill_run_stack();
		collapse_run_stack();
	}
	
	
	void fill_run_stack()
	{
		push_next_run();
		if(not (start < stop))
			return;
		push_next_run();
		while(start < stop)
		{
			while(try_resolve_invariants()) 
			{
				// LOOP
			}
			push_next_run();
		}
	}
	void collapse_run_stack()
	{
		while(try_resolve_invariants())
		{
			// LOOP
		}
		while(pending_run_count() > 1)
		{
			merge_bc();
		}
	}
	void push_next_run()
	{
		start = push_run(get_run(start, stop));
	}

	/*
	 * MERGE PATTERN STUFF
	 */
	
	bool try_resolve_invariants()
	{
		std::size_t run_count = pending_run_count();
		if(run_count < 3)
		{
			if(run_count > 1) 
			{
				auto [B, C] = get_bc_run_sizes();
				try_resolve_invariant_2(B, C);
			}
			return false; 
		}
		else
		{
			auto [A, B, C] = get_abc_run_sizes();
			return try_resolve_invariant_1(A, B, C) or try_resolve_invariant_2(B, C);
		}
	}
	bool try_resolve_invariant_1(std::size_t A, std::size_t B, std::size_t C)
	{
		if(not (A > (B + C))) 
		{
			if (A < C) 
				merge_ab();
			else 
				merge_bc();
			return true;
		} 
		else 
			return false;
	}
	bool try_resolve_invariant_2(std::size_t B, std::size_t C)
	{
		if(not (B > C)) 
		{
			merge_bc();
			return true;
		} 
		else 
		{
			return false;
		}
	}

	/*
	 * RUN STACK STUFF
	 */	
	It push_run(It it)
	{
		memory_manager.push_run(it);
		return it;
	}
	std::size_t pending_run_count()
	{
		return memory_manager.pending_run_count();
	}
	void pop_ab()
	{
		memory_manager.merge_ab();
	}
	void pop_bc()
	{
		memory_manager.merge_bc();
	}

	void merge_ab()
	{
		auto [begin, mid, end] = get_ab();
		merge_runs(begin, mid, end);
		pop_ab();
	}
	void merge_bc()
	{
		auto [begin, mid, end] = get_bc();
		merge_runs(begin, mid, end);
		pop_bc();
	}
	
	////// run access //////
	It a_begin()
	{
		return memory_manager.a_begin();
	}
	It a_end()
	{
		return memory_manager.a_end();
	}	
	It b_begin()
	{
		return memory_manager.b_begin();
	}	
	It b_end()
	{
		return memory_manager.b_end();
	}	
	It c_begin()
	{
		return memory_manager.c_begin();
	}	
	It c_end()
	{
		return memory_manager.c_end();
	}	
	

	std::array<It, 3> get_ab()
	{
		return {a_begin(), a_end(), b_end()};
	}
	std::array<It, 3> get_bc()
	{
		return {b_begin(), b_end(), c_end()};
	}
	std::array<It, 4> get_abc()
	{
		return {a_begin(), a_end(), b_end(), c_end()};
	}
	std::array<std::size_t, 3> get_abc_run_sizes(It a, It ab, It bc, It c)
	{
		return {
			static_cast<std::size_t>(std::distance(a, ab)), 
			static_cast<std::size_t>(std::distance(ab, bc)), 
			static_cast<std::size_t>(std::distance(bc, c))
		};
	}
	std::array<std::size_t, 3> get_abc_run_sizes()
	{
		auto [a, ab, bc, c] = get_abc();
		return get_abc_run_sizes(a, ab, bc, c);
	}
	std::array<std::size_t, 2> get_bc_run_sizes(It b, It bc, It c)
	{
		return {
			static_cast<std::size_t>(std::distance(b, bc)), 
			static_cast<std::size_t>(std::distance(bc, c))
		};
	}
	std::array<std::size_t, 2> get_bc_run_sizes()
	{
		auto [b, bc, c] = get_bc();
		return get_bc_run_sizes(b, bc, c);
	}
	
	/*
	 * MERGE/SORT IMPL STUFF
	 */
	It get_run(It begin, It end)
	{
		return _get_run(begin, end);
	}
	It _get_run_(It begin, It end)
	{
		const bool is_des = comp(begin[1], *begin);
		It pos;
		if(is_des)
		{
			pos = std::adjacent_find(std::next(begin), end, nrcomp);
			std::reverse(begin, pos);
		}
		else
		{
			pos = std::is_sorted_until(std::next(begin), end, comp);
		}
		if(pos < end)
			return complete_run(begin, pos, end);
		else
			return end;
		
		
	}
	It _get_run(It begin, It end)
	{
		
		auto pos_asc = std::is_sorted_until(begin, end, comp);
		// use nrcomp for stability
		// can't use std::is_sorted() because it requires the comparator to impose strict weak ordering
		auto pos_des = std::adjacent_find(begin, end, nrcomp);
		
		// TODO: check if we can '++' either of the 'pos_***' values without breaking anything. 
		//       should work because theyre past-the-end but do actually satisfy the ordering
		auto pos = pos_des;
	
		if(pos_asc <= pos_des)
			std::reverse(begin, pos_des);
		else
			pos = pos_asc;
		
		if(pos < end)
			return complete_run(begin, pos, end);
		else
			return pos;
	}

	It complete_run(It begin, It mid, It end)
	{
		auto runlen = std::distance(begin, mid);
		auto run_end_pos = end;
		// check if we've satisfied the minrun requirement
		if (runlen > minrun)
		{
			// we want to garuntee that there are always at least 2 elements in the next run
			// TODO: probably a reasonable place for '__builtin_expect()'?  
			if(std::distance(mid, end) == 1)
			{
				partial_insertion_sort(begin, mid, end, comp);
				return end;
				// TODO: __builtin_unreachable()? (that the result != end) 
				// return end;
			}
			return mid;
		}
		else
		{
			// add items to the run until we satisfy minrun or run out of items to add
			auto limit = std::min(end, begin + minrun);
			const std::size_t remain = std::distance(limit, end);
			if(remain < minrun)
				limit = end;
			partial_insertion_sort(begin, mid, limit, comp);
			return limit;
		}
	}
	
	void merge_runs(It begin, It mid, It end)
	{
		// TODO:  DECIDE WHICH SIDE TO MERGE FROM AFTER ADJUSTING FOR FRONT AND BACK CLIPPING
		_merge_runs(begin, mid, end);
	}

	void _merge_runs(It begin, It mid, It end)
	{
		begin = std::upper_bound(begin, mid, *mid, comp);
		end = std::lower_bound(mid, end, *(mid - 1), comp);
		// TODO: remove this if ?
		if(begin == mid or mid == end)
			return;

		if((end - mid) > (mid - begin))
		{
			// merge from the left
			do_merge(begin, mid, end, comp);
		}
		else 
		{
			// merge from the right
			do_merge(std::make_reverse_iterator(end), 
				 std::make_reverse_iterator(mid),
				 std::make_reverse_iterator(begin), 
				 rcomp);
		}
	}

	template <class Iter, class Cmp>
	void do_merge(Iter begin, Iter mid, Iter end, Cmp cmp)
	{
		auto mem_begin = get_merge_mem(begin, mid);
		auto mem_end = mem_begin + (mid - begin);
		min_gallop = gallop_merge_ex(mem_begin, mem_end, mid, end, begin, cmp, min_gallop);
		release_merge_mem(mem_begin);
	}

	template <class Iter>
	auto get_merge_mem(Iter begin, Iter end)
	{
		// TODO: precheck that begin < end;
		return memory_manager.get_merge_memory(begin, end);
	}
	template <class T>
	void release_merge_mem(T* mem)
	{
		memory_manager.release_merge_memory(mem);
	}

	
	/*
	 * DEBUG STUFF
	 */

	void print_run_stack()
	{
		memory_manager.print_run_stack();
		std::cout << ": " << std::distance(start, stop) << std::endl;
	}
	
	// TODO: maybe move the logic for the comparator types to the template system
	//       and have _timsort() deduce them.	
	using ncomp_t = decltype(std::not_fn(std::declval<Comp>()));
	using value_t = typename std::iterator_traits<It>::value_type;

	TimSortMemoryManager<It> memory_manager;	
	It start;
	It stop;
	Comp& comp;
	NComp& ncomp;
	RComp& rcomp;
	NRComp& nrcomp;

	const std::size_t minrun;
	std::size_t min_gallop;

	
	static constexpr const std::size_t default_min_gallop = gallop_win_dist;
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


template <class It, class Comp, class NComp, class RComp, class NRComp>
static void _timsort(It begin, It end, Comp& comp, NComp& ncomp, RComp& rcomp, NRComp& nrcomp)
{
	if(begin < end)
		TimSort(begin, end, comp, ncomp, rcomp, nrcomp);
} 


template <class It, class Comp>
static void _timsort(It begin, It end, Comp& comp)
{
	auto ncomp = invert_comparator(comp);
	auto rcomp = reverse_comparator(comp);
	auto nrcomp = invert_comparator(rcomp);
	_timsort(begin, end, comp, ncomp, rcomp, nrcomp);
}
template <class It, class Comp>
void timsort(It begin, It end, Comp comp, bool synchronize)
{
	_timsort(begin, end, comp);
}
template <class It, class Comp>
void timsort(It begin, It end, Comp comp)
{
	_timsort(begin, end, comp);
}
template <class It>
void timsort(It begin, It end)
{
	timsort(begin, end, std::less<>());
}

#endif /* TIMSORT_H */
