#include <algorithm>
#include <iterator>
#include <utility>
#include <cstdint>
#include <iostream>
#include "compiler.h"



template <class It, class Pred>
It gallop_lower_bound_ex__unchecked(It begin, It end, Pred pred)
{
	const std::size_t stopindex = std::distance(begin, end) + 1;
	std::size_t index = 1;

	if(pred(*begin))
	 	return begin;
	while(index < stopindex and not pred(begin[index - 1]))
	{
		index *= 2;
	}
	return begin + (index / 2);
}
static constexpr const std::size_t gallop_win_dist = 7;



template <class It>
static It gallop_step(It begin, It it)
{
	return begin + (2 * (std::distance(begin, it) + 1) - 1);
}
template <class It>
static It gallop_step_reverse(It begin, It it)
{
	return begin + ((std::distance(begin, it) + 1) / 2 - 1);
}

template <class It, class Pred>
It gallop_lower_bound(It begin, It end, Pred pred)
{
	if(pred(*begin))
		return begin;
	It pos = std::next(begin);
	while(pos < end and not pred(*pos))
	{
		pos = gallop_step(begin, pos);
	}
	return gallop_step_reverse(begin, pos) + 1;
}


template <bool UpperBound, class It, class Pred>
inline std::pair<It, It> gallop_reduce_search_region(It begin, It end, Pred pred)
{
	auto pos = begin;
	while(pos < end and not pred(*pos))
		pos = gallop_step(begin, pos);
	return {gallop_step_reverse(begin, pos), std::min(pos, end)};
}

template <class It>
using iter_value_t = typename std::iterator_traits<It>::value_type;

template <class It, class Comp>
std::pair<It, It> gallop_reduce_search_region_lb(It begin, It end, const iter_value_t<It>& bound, Comp comp)
{
	return gallop_reduce_search_region(begin, 
					   end, 
					   [&](auto && v) { return not comp(std::forward<decltype(v)>(v), bound); }
	);
}

template <class It, class Comp>
std::pair<It, It> gallop_reduce_search_region_ub(It begin, It end, const iter_value_t<It>& bound, Comp comp)
{
	return gallop_reduce_search_region(begin, 
					   end, 
					   [&](auto && v) { return comp(bound, std::forward<decltype(v)>(v)); }
	);
}


// TODO: implement eager copy behavior and test efficiency
template <class It, class T, class Comp>
It _gallop_upper_bound(const It begin, const It end, const T & value, Comp comp)
{
	It pos = begin;
	while(pos < end and not comp(value, *pos))
	{
		pos = gallop_step(begin, pos);
	}
	return pos;
}
template <class It, class T, class Comp>
It _gallop_lower_bound(It begin, It end, const T & value, Comp comp)
{
	It pos = begin;
	while(pos < end and comp(*pos, value))
	{
		pos = gallop_step(begin, pos);
	}
	return pos;
}
template <class It, class Pred>
It _gallop_while(It begin, It end, Pred pred)
{
	It pos = begin;
	while(pos < end and pred(*pos))
	{
		pos = gallop_step(begin, pos);
	}
	return pos;
}

template <bool UseUpperBound, class It, class T, class Comp>
std::pair<It, It> _get_gallop_limits(It begin, It end, const T& value, Comp comp)
{
	if constexpr(UseUpperBound) 
	{
		It pos = _gallop_upper_bound(begin, end, value, comp);
		return {gallop_step_reverse(begin, pos), std::min(pos, end)};
	}
	else
	{
		It pos = _gallop_lower_bound(begin, end, value, comp);
		return {gallop_step_reverse(begin, pos), std::min(pos, end)};
	}
}

template <class It, class T, class Comp>
It _timsort_lower_bound(It begin, It end, const T& value, Comp comp)
{
	auto [b, e] = _get_gallop_limits<false>(begin, end, value, comp);
	return std::lower_bound(b, e, value, comp);
}
template <class It, class T, class Comp>
It _timsort_upper_bound(It begin, It end, const T& value, Comp comp)
{
	auto [b, e] = _get_gallop_limits<true>(begin, end, value, comp);
	return std::upper_bound(b, e, value, comp);
}



// TODO: see if optimized versions of these helper algorithms are any faster
template <class It, class DestIt, class Pred>
std::pair<It, DestIt> copy_until(It begin, It end, DestIt dest, Pred pred)
{
	auto pos = std::find_if(begin, end, pred);
	return {pos, std::copy(begin, pos, dest)};
}

template <class It, class DestIt, class Pred>
inline std::pair<It, DestIt> copy_while(It begin, It end, DestIt dest, Pred pred)
{
	return copy_until(begin, end, dest, std::not_fn(pred));
}

template <class It, class DestIt, class Pred>
std::pair<It, DestIt> move_until(It begin, It end, DestIt dest, Pred pred)
{
	auto pos = std::find_if(begin, end, pred);
	return {pos, std::move(begin, pos, dest)};
}

template <class It, class DestIt, class Pred>
inline std::pair<It, DestIt> move_while(It begin, It end, DestIt dest, Pred pred)
{
	return move_until(begin, end, dest, std::not_fn(pred));
}


template <class T>
static constexpr const T& const_ref(const T & value)
{
	return value;
}




template <class LeftIt, class RightIt, class DestIt, class Comp>
std::size_t gallop_merge(LeftIt lbegin, LeftIt lend, RightIt rbegin, RightIt rend, DestIt dest, Comp comp, std::size_t min_gallop)
{
	constexpr bool upper_bound_flag = true;
	constexpr bool lower_bound_flag = false;
	const auto right_range_pred = [=](auto && val) { 
		return comp(std::forward<decltype(val)>(val), *lbegin); 
	};
	const auto left_range_pred = [=](auto && val) { 
		return not comp(*rbegin, std::forward<decltype(val)>(val)); 
	};
	LeftIt left_limit;
	RightIt right_limit;
	for(; lbegin < lend and rbegin < rend; ++min_gallop)
	{
		left_limit = lend;
		right_limit = rend;
		// LINEAR SEARCH MODE
		while(lbegin < left_limit and rbegin < right_limit)
		{
			if(comp(*rbegin, *lbegin))
			{
				// Search the right-hand range	
				right_limit = std::min(rbegin + min_gallop, rend);
				auto pos = std::find_if_not(rbegin, right_limit, right_range_pred);
				dest = std::move(rbegin, pos, dest);
				rbegin = pos;
			}
			else
			{
				// Search the left-hand range	
				left_limit = std::min(lbegin + min_gallop, lend);
				auto pos = std::find_if_not(lbegin, left_limit, left_range_pred);
				dest = std::move(lbegin, pos, dest);
				lbegin = pos;
			}
		}
		// GALLOP SEARCH MODE
		for(std::size_t g_dist = gallop_win_dist; 
		    (g_dist >= gallop_win_dist) and (lbegin < lend) and (rbegin < rend);
		    min_gallop -= min_gallop > 1)
		{
			if(comp(*rbegin, *lbegin))
			{
				right_limit = _gallop_while(rbegin, rend, right_range_pred);
				right_limit = std::lower_bound(gallop_step_reverse(rbegin, right_limit) + 1,
							       std::min(right_limit, rend),
							       *lbegin, 
							       comp);
				dest = std::move(rbegin, right_limit, dest);
				g_dist = right_limit - rbegin;
				rbegin = right_limit;
			}
			else
			{
				left_limit = _gallop_while(lbegin, lend, left_range_pred);
				left_limit = std::lower_bound(gallop_step_reverse(lbegin, left_limit) + 1,
							      std::min(left_limit, lend),
							      *rbegin, 
							      comp);
				dest = std::move(lbegin, left_limit, dest);
				g_dist = left_limit - lbegin;
				lbegin = left_limit;
			}
		}
	}
	std::move(lbegin, lend, dest);
	return min_gallop;
}

template <class LeftIt, class RightIt, class DestIt, class Comp>
DestIt binary_merge(LeftIt lbegin, LeftIt lend, RightIt rbegin, RightIt rend, DestIt dest, Comp comp)
{
	for(std::size_t dist = 0, lsize = lend - lbegin, rsize = rend - rbegin; lsize > 0;)
	{
		dist = std::lower_bound(rbegin, rend, *lbegin, comp) - rbegin;
		std::move(rbegin, rbegin + dist, dest);
		dest += dist;
		rbegin += dist;
		rsize -= dist;
		if(not (rsize > 0))
		{
			std::move(lbegin, lend, dest);
			return dest;
		}
		dist = std::upper_bound(lbegin, lend, *rbegin, comp) - lbegin;
		std::move(lbegin, lbegin + dist, dest);
		dest += dist;
		lbegin += dist;
		lsize -= dist;
		if(not (lsize > 0))
			return dest;
	}
	// TODO: UNREACHABLE
	return dest;
}
template <class LeftIt, class RightIt, class DestIt, class Comp>
DestIt linear_merge(LeftIt lbegin, LeftIt lend, RightIt rbegin, RightIt rend, DestIt dest, Comp comp)
{
	for(;;)
	{
		for(;;)
		{
			if(rbegin == rend)
				return std::move(lbegin, lend, dest);
			else if(comp(*rbegin, *lbegin))
				*dest++ = std::move(*rbegin++);
			else
				break;
		}
		for(;;)
		{
			if(lbegin == lend)
				return dest;
			else if(not comp(*rbegin, *lbegin))
				*dest++ = std::move(*lbegin++);
			else
				break;
		}
		
	}
}
template <class LeftIt, class RightIt, class DestIt, class Comp>
std::size_t gallop_merge_ex(LeftIt lbegin, LeftIt lend, RightIt rbegin, RightIt rend, DestIt dest, Comp comp, std::size_t min_gallop)
{
	// TODO: switch to a pure linear-search merge after one or both ranges are shorter than min-gallop
	for(std::size_t i=0, stop=0; ; ++min_gallop)
	{
		// LINEAR SEARCH MODE
		for(;;)
		{
			// TODO: just use iterators?  
			// TODO: have a bool to save whether min_gallop < end - begin
			// Search the right-hand range
			stop = std::min(min_gallop, std::size_t(rend - rbegin));
			for(i = 0; (i < stop) and comp(*rbegin, *lbegin); ++i) 
				*dest++ = std::move(*rbegin++);

			if(rbegin == rend)
			{
				std::move(lbegin, lend, dest);
				return min_gallop;
			}
			else if(not (i < min_gallop))
				break;

			// Search the left-hand range
			stop = std::min(min_gallop, std::size_t(lend - lbegin));
			for(i = 0; (i < stop) and not comp(*rbegin, *lbegin); ++i) 
				*dest++ = std::move(*lbegin++);

			if(lbegin == lend)
				return min_gallop;
			else if(not (i < min_gallop))
				break;
		}
		++min_gallop;
		// GALLOP SEARCH MODE
		for(std::size_t g_dist = gallop_win_dist; (g_dist >= gallop_win_dist); )
		{
			min_gallop -= (min_gallop > 1);
			i = 0;
			for(i = 0, stop = (rend - rbegin); i < stop and comp(rbegin[i], *lbegin);) 
				i = (i + 1) * 2 - 1;
			g_dist = i;
			if(i > 0)
			{
				stop = std::lower_bound(rbegin + ((i + 1) / 2), rbegin + std::min(i, size_t(rend - rbegin)), *lbegin, comp) - rbegin;
				dest = std::move(rbegin, rbegin + stop, dest);
				rbegin += stop;
				if(not (rbegin < rend))
				{
					std::move(lbegin, lend, dest);
					return min_gallop;
				}
				i = 0;
			}
			
			i = 1;
			for(stop = lend - lbegin; (i < stop) and (not comp(*rbegin, lbegin[i]));) 
				i = (i + 1) * 2 - 1;
			g_dist = std::max(i, g_dist);
			stop = std::upper_bound(lbegin + ((i + 1) / 2), lbegin + std::min(i, size_t(lend - lbegin)), *rbegin, comp) - lbegin;
			dest = std::move(lbegin, lbegin + stop, dest);
			lbegin += stop;
			if(not (lbegin < lend)) 
				return min_gallop;
		}
	}
	return min_gallop;
}

template <std::size_t ... I>
static constexpr const std::array<unsigned char, sizeof...(I)> make_small_array(std::index_sequence<I...>)
{
	return {I...};
}


template <class It>
void rotate_right_1__unchecked(It begin, It end)
{
	// heuristic for deciding whether to use a temporary + range-copy (move)
	// if a type is smaller than a 'pointer-size-capacity' type like std::vector or std::string,
	// then implement the rotate as a:
	//	 move-to-temporary -> std::move_backward (basically memcpy) -> move-from-temporary
	// otherwise implement as a series of swaps.
	// 
	// this heuristic is, of course, enacted at compile-time
	// 
	// benchmarking across a number of different cases shows that this usually wins over 
	// a call to std::rotate() or using a value like '64' (cache-line size)
	constexpr std::size_t upper_bound = 3 * std::max(sizeof(void*), sizeof(std::size_t));
	using value_type = typename std::iterator_traits<It>::value_type;
	if constexpr(sizeof(value_type) < upper_bound)
	{
		// for small types, implement using a temporary.
		value_type temp = std::move(end[-1]);
		std::move_backward(begin, end - 1, end);
		*begin = std::move(temp);
	}
	else
	{
		// for large types, implement as a series of swaps.
		value_type& last_value = end[-1];
		auto last_valid = end - 1;
		while(begin < last_valid)
			std::swap(*begin++, *last_valid);
	}
}
template <class It, class Comp>
void partial_insertion_sort(It begin, It mid, It end, Comp comp)
{
	for(It pos = mid; pos < end; ++pos)
	{
		mid = std::upper_bound(begin, pos, *pos, comp);
		if(std::distance(mid, pos) > 0)
			rotate_right_1__unchecked(mid, pos + 1);
	}
}


// template <class It>
// void large_partial_insertion_sort(It begin, It mid, It end)
// {
// 	// should fit in a cache line, though we still need to touch the data
// 	std::array<unsigned char, 64> proxy_array{make_small_array(std::make_index_sequence<64>{})};
// 	
// }





