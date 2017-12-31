#include <algorithm>
#include <iterator>
#include <utility>
#include <cstdint>
#include <iostream>
#include "compiler.h"
#include "memcpy_algos.h"
#include <cassert>
#include "iter.h"

inline constexpr const std::size_t gallop_win_dist = 7;


template <class It, class T, class Comp>
inline It gallop_upper_bound(It begin, It end, const T& value, Comp comp)
{
	using index_t = iterator_difference_type_t<It>;
	std::size_t i = 1;
	std::size_t len = end - begin;
	for(; i <= len and not comp(value, begin[i - 1]); i *= 2)
	{
		// if()
		// {
		// 	end = begin + (i - 1);
		// 	break;
		// }
	}
	if(len >= i)
		len = i - 1;
	len -= (i / 2);
	begin += (i / 2);
	// hand-rolled std::upper_bound.  
	for(; len > 0;)
	{
		i = len / 2;
		if (comp(value, begin[i]))
			len = i;
		else
		{
			begin += (i + 1);
			len -= (i + 1);
		}
	}
	return begin;
}




template <class LeftIt, class RightIt, class DestIt, class Comp>
std::size_t gallop_merge_ex(LeftIt lbegin, LeftIt lend, RightIt rbegin, RightIt rend, DestIt dest, Comp comp, std::size_t min_gallop)
{
	// TODO: reuse lcount and rcount instead of using g_dist
	for(std::size_t i=0, lcount=0, rcount=0; ;)
	{
		// LINEAR SEARCH MODE
		for(lcount=0, rcount=0;;)
		{
			if(comp(*rbegin, *lbegin))
			{
				*dest = std::move(*rbegin);
				++dest;
				++rbegin;
				++rcount;
				lcount = 0;
				if(rbegin == rend)
				{
					move_or_memcpy(lbegin, lend, dest);
					return min_gallop;
				}
				else if(rcount >= min_gallop)
					goto gallop_right;
			}
			else
			{
			    //linear_left:
				*dest = std::move(*lbegin);
				++dest;
				++lbegin;
				++lcount;
				rcount = 0;
				if(lcount >= min_gallop) 
					goto gallop_left;
				// don't need to check if we reached the end.  that will happen on the right-hand-side 
			}
		}
		COMPILER_UNREACHABLE_;
		// GALLOP SEARCH MODE
		for(; lcount >= gallop_win_dist or rcount >= gallop_win_dist;)
		{
			min_gallop -= (min_gallop > 1);
			// gallop through the left range
		    gallop_left:
			lcount = lend - lbegin;
			for(i = 1; (i <= lcount) and not comp(*rbegin, lbegin[i - 1]); i *= 2) 
			{
				
			}
			if(lcount >= i)
				lcount = i - 1;
			lcount = std::upper_bound(lbegin + (i / 2), lbegin + lcount, *rbegin, comp) - lbegin;
			move_or_memcpy(lbegin, lbegin + lcount, dest);
			dest += lcount;
			lbegin += lcount;
			// don't need to check if we reached the end.  that will happen on the right-hand-side 
		    gallop_right:	
			rcount = rend - rbegin;
			for(i = 1; i <= rcount and comp(rbegin[i - 1], *lbegin); i *= 2) 
			{
				
			}
			if(rcount >= i)
				rcount = i - 1;
			rcount = std::lower_bound(rbegin + (i / 2), rbegin + rcount, *lbegin, comp) - rbegin;
			dest = std::move(rbegin, rbegin + rcount, dest);
			rbegin += rcount;
			if(not (rbegin < rend))
			{
				move_or_memcpy(lbegin, lend, dest);
				return min_gallop;
			}
		}
		++min_gallop;
	}
	COMPILER_UNREACHABLE_;
	return min_gallop;
}


template <class It>
inline void rotate_right(It begin, It end)
{
	// heuristic for deciding whether to use a temporary + range-copy (move)
	// if a type is smaller than a 'pointer-size-capacity' type like std::vector or std::string,
	// then implement the rotate as:
	//	 move-to-temporary -> std::move_backward -> move-from-temporary
	// otherwise implement as a series of swaps.
	// 
	// this heuristic is, of course, evaluated at compile-time
	// 
	// benchmarking across a number of different cases shows that this usually wins over 
	// a call to std::rotate() or using a value like '64' (likely cache line size)
	constexpr std::size_t upper_limit = 3 * std::max(sizeof(void*), sizeof(std::size_t));
	using value_type = iterator_value_type_t<It>;
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
	using value_type = iterator_value_type_t<It>;
	
	if constexpr(std::is_scalar_v<value_type>
		     and (   std::is_same_v<Comp, std::less<>> 
		          or std::is_same_v<Comp, std::less<value_type>>
		          or std::is_same_v<Comp, std::greater<>>
		          or std::is_same_v<Comp, std::greater<value_type>>)
	)
	{
		while(mid < end)
		{
			for(auto pos = mid; pos > begin and comp(*pos, pos[-1]); --pos)
				std::swap(pos[-1], *pos);
			++mid;
		}
	}
	else
	{
		while(mid < end)
		{
			rotate_right(std::upper_bound(begin, mid, *mid, comp), mid + 1);
			++mid;
		}
	}
}


template <bool Stable, class It, class Comp>
inline It count_run(It begin, It end, Comp comp)
{
	if(COMPILER_UNLIKELY_(end - begin < 2))
	{
		begin = end;
	}
	else if(comp(begin[1], *begin))
	{
		auto save = begin;
		for(begin += 2; begin < end; ++begin)
		{
			if constexpr(Stable)
			{
				if(not comp(*begin, begin[-1]))
					break;
			}
			else
			{
				if(comp(begin[-1], *begin))
					break;
			}
		}
		std::reverse(save, begin);
	}
	else
	{
		for(begin += 2; begin < end; ++begin)
		{
			if(comp(*begin, begin[-1]))
				break;
		}
	}
	return begin;
}


