#include <algorithm>
#include <iterator>
#include <utility>
#include <cstdint>
#include <iostream>
#include "compiler.h"
#include "memcpy_algos.h"
#include <cassert>


inline constexpr const std::size_t gallop_win_dist = 7;




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
	COMPILER_UNREACHABLE_;
}

template <class LeftIt, class RightIt, class DestIt, class Comp>
bool linear_search_mode(LeftIt& lbegin, LeftIt lend, RightIt& rbegin, RightIt rend, DestIt& dest, Comp comp, std::ptrdiff_t min_gallop)
{
	for(std::ptrdiff_t i = 0;;)
	{
		if((rend - rbegin) <= min_gallop or (lend - lbegin) <= min_gallop)
		{
			linear_merge(lbegin, lend, rbegin, rend, dest, comp);
			return true;
		}
		// Search the right-hand range
		for(i = 0; (i < min_gallop) and comp(*rbegin, *lbegin); ++i) 
			*dest++ = std::move(*rbegin++);
		if(not (i < min_gallop))
			break;
		// Search the left-hand range
		for(i = 0; (i < min_gallop) and not comp(*rbegin, *lbegin); ++i) 
			*dest++ = std::move(*lbegin++);
		if(not (i < min_gallop))
			break;
	}
	return false;
}

template <class It, class T, class Comp>
It gallop_upper_bound(It begin, It end, const T& value, Comp comp)
{
	using index_t = typename std::iterator_traits<It>::difference_type;
	index_t i = 1;
	const index_t stop = end - begin;
	for(; i <= stop; i *= 2)
	{ 
		if(not comp(begin[i - 1], value))
		{
			return std::upper_bound(begin + (i / 2), begin + (i - 1), value, comp);
		}
	} 
	
	return std::upper_bound(begin + (i / 2), end, value, comp);
}

template <class LeftIt, class RightIt, class DestIt, class Comp>
bool gallop_search_mode(LeftIt& lbegin, LeftIt lend, RightIt& rbegin, RightIt rend, DestIt& dest, Comp comp, std::ptrdiff_t& min_gallop)
{
	for(std::ptrdiff_t g_dist = gallop_win_dist, i = 1, stop=1; g_dist >= std::ptrdiff_t(gallop_win_dist); )
	{
		min_gallop -= (min_gallop > 1);
		// gallop through the right range
		stop = rend - rbegin;
		for(i = 1; i <= stop and comp(rbegin[i - 1], *lbegin); i *= 2) 
		{
			// LOOP
		}
		g_dist = --i;
		if(i > 0)
		{
			stop = std::lower_bound(rbegin + (i / 2), rbegin + std::min(i, stop), *lbegin, comp) - rbegin;
			dest = std::move(rbegin, rbegin + stop, dest);
			rbegin += stop;
			if(not (rbegin < rend))
			{
				std::move(lbegin, lend, dest);
				return true;
			}
		}
		// gallop through the left range
		stop = lend - lbegin;
		for(i = 2; (i <= stop) and (not comp(*rbegin, lbegin[i - 1])); i *= 2) 
		{
			// LOOP
		}
		--i;
		g_dist = std::max(i, g_dist);
		stop = std::upper_bound(lbegin + (i / 2), lbegin + std::min(i, stop), *rbegin, comp) - lbegin;
		dest = std::move(lbegin, lbegin + stop, dest);
		lbegin += stop;
		if(not (lbegin < lend)) 
			return true;
	}
	return false;
}

template <class LeftIt, class RightIt, class DestIt, class Comp>
std::size_t gallop_merge_ex(LeftIt lbegin, LeftIt lend, RightIt rbegin, RightIt rend, DestIt dest, Comp comp, std::ptrdiff_t min_gallop)
{
	// TODO: 
	for(std::ptrdiff_t i=0, stop=0; ; ++min_gallop)
	{
		// LINEAR SEARCH MODE
		if(linear_search_mode(lbegin, lend, rbegin, rend, dest, comp, min_gallop))
			return min_gallop;
		++min_gallop;
		// GALLOP SEARCH MODE
		for(std::ptrdiff_t g_dist = gallop_win_dist; g_dist >= std::ptrdiff_t(gallop_win_dist); )
		{
			min_gallop -= (min_gallop > 1);
			// gallop through the right range
			stop = rend - rbegin;
			for(i = 1; i <= stop and comp(rbegin[i - 1], *lbegin); i *= 2) 
			{
				// LOOP
			}
			g_dist = --i;
			if(i > 0)
			{
				stop = std::lower_bound(rbegin + (i / 2), rbegin + std::min(i, stop), *lbegin, comp) - rbegin;
				dest = std::move(rbegin, rbegin + stop, dest);
				rbegin += stop;
				if(not (rbegin < rend))
				{
					move_or_memcpy(lbegin, lend, dest);
					return min_gallop;
				}
			}
			// gallop through the left range
			stop = lend - lbegin;
			for(i = 2; (i <= stop) and (not comp(*rbegin, lbegin[i - 1])); i *= 2) 
			{
				// LOOP
			}
			--i;
			g_dist = std::max(i, g_dist);
			stop = std::upper_bound(lbegin + (i / 2), lbegin + std::min(i, stop), *rbegin, comp) - lbegin;
			move_or_memcpy(lbegin, lbegin + stop, dest);
			dest += stop;
			lbegin += stop;
			if(not (lbegin < lend)) 
				return min_gallop;
		}
	}
	COMPILER_UNREACHABLE_;
	return min_gallop;
}



template <class It>
inline void rotate_right_1(It begin, It end)
{
	// heuristic for deciding whether to use a temporary + range-copy (move)
	// if a type is smaller than a 'pointer-size-capacity' type like std::vector or std::string,
	// then implement the rotate as a:
	//	 move-to-temporary -> std::move_backward -> move-from-temporary
	// otherwise implement as a series of swaps.
	// 
	// this heuristic is, of course, evaluated at compile-time
	// 
	// benchmarking across a number of different cases shows that this usually wins over 
	// a call to std::rotate() or using a value like '64' (likely cache line size)
	constexpr std::size_t upper_limit = 3 * std::max(sizeof(void*), sizeof(std::size_t));
	using value_type = typename std::iterator_traits<It>::value_type;
	if constexpr(sizeof(value_type) < upper_limit)
	{
		// for small types, implement using a temporary.
		if(end - begin > 1) 
		{
			value_type temp = std::move(end[-1]);
			std::move_backward(begin, end - 1, end);
			*begin = std::move(temp);
		}
	}
	else
	{
		// for large types, implement as a series of swaps.
		if(end - begin > 1)
		{
			--end;
			do{
				std::swap(end[-1], *end);
			} while(begin < --end);
		}
	}
}

template <class It, class Comp>
void partial_insertion_sort(It begin, It mid, It end, Comp comp)
{
	while(mid < end)
	{
		rotate_right_1(std::upper_bound(begin, mid, *mid, comp), mid + 1);
		++mid;
	}
}



template <class It, class Comp>
It extend_and_reverse_descending_run(It begin, It mid, It end, Comp comp)
{
	if(mid == end)
		return mid;
	auto stop = mid + 1;
	while(stop < end)
	{
		while(stop < end and comp(*stop, stop[-1]))
		{
			++stop;
		}
		mid = stop;
		while(stop < end and (not (comp(*stop, stop[-1]) or comp(stop[-1], *stop))))
		{
			++stop;
		}
		if(stop - mid == 0)
			return stop;
		std::reverse(mid, stop);
	}
	std::reverse(begin, stop);
	assert(std::is_sorted(begin, stop, comp));
	return stop;
}
