#include <algorithm>
#include <iterator>
#include <utility>
#include <cstdint>
#include <iostream>
#include "compiler.h"
#include <cassert>


static constexpr const std::size_t gallop_win_dist = 7;

static constexpr inline bool is_pow2(std::size_t num)
{
	return (num & (num - 1)) == 0;
}

template <class LeftIt, class RightIt, class DestIt, class Comp>
DestIt linear_merge_ex(LeftIt lbegin, LeftIt lend, RightIt rbegin, RightIt rend, DestIt dest, Comp comp)
{
	for(;;)
	{
	}
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
	COMPILER_UNREACHABLE_;
}


template <class LeftIt, class RightIt, class DestIt, class Comp>
std::size_t gallop_merge_ex(LeftIt lbegin, LeftIt lend, RightIt rbegin, RightIt rend, DestIt dest, Comp comp, std::ptrdiff_t min_gallop)
{
	// TODO: 
	for(std::ptrdiff_t i=0, stop=0; ; ++min_gallop)
	{
		// LINEAR SEARCH MODE
		for(;;)
		{
			// TODO: have a bool to save whether min_gallop < end - begin
			if((rend - rbegin) <= min_gallop)
			{
				linear_merge(lbegin, lend, rbegin, rend, dest, comp);
				return min_gallop;
			}
			stop = min_gallop;
			// Search the right-hand range
			for(i = 0; (i < stop) and comp(*rbegin, *lbegin); ++i) 
				*dest++ = std::move(*rbegin++);
			if(not (i < min_gallop))
				break;
			// Search the left-hand range
			if((lend - lbegin) <= min_gallop)
			{
				linear_merge(lbegin, lend, rbegin, rend, dest, comp);
				return min_gallop;
			}
			stop = min_gallop;
			for(i = 0; (i < stop) and not comp(*rbegin, *lbegin); ++i) 
				*dest++ = std::move(*lbegin++);
			if(not (i < min_gallop))
				break;
		}
		++min_gallop;
		// GALLOP SEARCH MODE
		for(std::ptrdiff_t g_dist = gallop_win_dist; g_dist >= std::ptrdiff_t(gallop_win_dist); )
		{
			min_gallop -= (min_gallop > 1);
		    gallop_right:
			// gallop through the right range
			stop = (rend - rbegin);
			for(i = 1; i <= stop and comp(rbegin[i - 1], *lbegin); i <<= 1) 
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
					return min_gallop;
				}
			}
		    gallop_left:
			// gallop through the left range
			stop = lend - lbegin;
			for(i = 2; (i <= stop) and (not comp(*rbegin, lbegin[i - 1])); i <<= 1) 
			{
				// LOOP
			}
			--i;
			g_dist = std::max(i, g_dist);
			stop = std::upper_bound(lbegin + (i / 2), lbegin + std::min(i, stop), *rbegin, comp) - lbegin;
			dest = std::move(lbegin, lbegin + stop, dest);
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
                        // for(std::ptrdiff_t i = (end - begin) - 1; i > 0; --i)
                        //	begin[i] = std::move(begin[i - 1]);
			*begin = std::move(temp);
		}
	}
	else
	{
		// for large types, implement as a series of swaps.
		while(begin < --end)
			std::swap(end[-1], *end);
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

