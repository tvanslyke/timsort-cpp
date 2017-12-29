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

template <class LeftIt, class RightIt, class DestIt, class Comp>
bool linear_search_mode_new(LeftIt& lbegin, LeftIt lend, RightIt& rbegin, RightIt rend, DestIt& dest, Comp comp, std::ptrdiff_t min_gallop)
{
	for(std::ptrdiff_t lcount = 0, rcount = 0; ;)
	{
		if(comp(*rbegin, *lbegin))
		{
			*dest = std::move(*rbegin);
			++rbegin;
			++rcount;
			lcount = 0;
			if(rbegin == rend)
				break;
		}
		else
		{
			// TODO: do while?
			*dest = std::move(*lbegin);
			++lbegin;
			++lcount;
			rcount = 0;
		}
		++dest;
		if(rcount >= min_gallop or lcount >= min_gallop)
			return false;
	}
	++dest;
	move_or_memcpy(lbegin, lend, dest);
	return true;
}

template <class It, class T, class Comp>
inline It gallop_upper_bound(It begin, It end, const T& value, Comp comp)
{
	using index_t = typename std::iterator_traits<It>::difference_type;
	index_t i = 1;
	
	for(const index_t stop = end - begin; i <= stop; i *= 2)
	{ 
		if(comp(value, begin[i - 1]))
		{
			end = begin + (i - 1);
			break;
		}
	}
	// hand-rolled upper bound.  
	begin += (i / 2);
	for(index_t len = end - begin; len > 0;)
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

template <class It, class T, class Comp>
inline It gallop_lower_bound(It begin, It end, const T& value, Comp comp)
{
	using index_t = typename std::iterator_traits<It>::difference_type;
	index_t i = 1;
	
	for(const index_t stop = end - begin; i <= stop; i *= 2)
	{ 
		if(not comp(begin[i - 1], value))
		{
			end = begin + (i - 1);
			break;
		}
	}
	// hand-rolled lower bound.  
	begin += (i / 2);
	for(index_t len = end - begin; len > 0;)
	{
		i = len / 2;
		if (not comp(begin[i], value))
			len = i;
		else
		{
			begin += (i + 1);
			len -= (i + 1);
		}
	}
	return begin;
}


template <class It, class T, class Comp>
inline It my_upper_bound(It begin, It end, const T& value, Comp comp)
{
	using index_t = typename std::iterator_traits<It>::difference_type;
	for(index_t len = end - begin; len > 0;)
	{
		const auto half = len / 2;
		if(comp(value, begin[half]))
			len = half;
		else
		{
			begin += (half + 1);
			len -= (half + 1);
		}
	}
	return begin;
}

template <class It, class T, class Comp>
inline It my_lower_bound(It begin, It end, const T& value, Comp comp)
{
	using index_t = typename std::iterator_traits<It>::difference_type;
	for(index_t len = end - begin; len > 0;)
	{
		const auto half = len / 2;
		if(not comp(value, begin[half]))	
		{
			begin += (half + 1);
			len -= (half + 1);
		}
		else
			len = half;
	}
	return begin;
}

template <class It, class T, class Comp>
inline std::ptrdiff_t linear_upper_bound(It begin, std::ptrdiff_t len, const T& value, Comp comp)
{
	for(std::ptrdiff_t i = 0; i < len; ++i)
	{
		if(comp(value, begin[i]))
			return i;
	}
	return len;
}

template <class It, class T, class Comp>
inline std::ptrdiff_t linear_lower_bound(It begin, std::ptrdiff_t len, const T& value, Comp comp)
{
	for(std::ptrdiff_t i = 0; i < len; ++i)
	{
		if(not comp(begin[i], value))
			return i;
	}
	return len;
}




template <class LeftIt, class RightIt, class DestIt, class Comp>
std::size_t gallop_merge_ex_old(LeftIt lbegin, LeftIt lend, RightIt rbegin, RightIt rend, DestIt dest, Comp comp, std::ptrdiff_t min_gallop)
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
			--i;
			g_dist = i;
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
			for(i = 2; (i <= stop) and not comp(*rbegin, lbegin[i - 1]); i *= 2) 
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

template <class LeftIt, class RightIt, class DestIt, class Comp>
std::size_t gallop_merge_ex(LeftIt lbegin, LeftIt lend, RightIt rbegin, RightIt rend, DestIt dest, Comp comp, std::ptrdiff_t min_gallop)
{
	// TODO: 
	for(std::ptrdiff_t i=0, stop=0; ;)
	{
		// LINEAR SEARCH MODE
		for(std::ptrdiff_t lcount = 0, rcount = 0; ;)
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
					break;
			}
			else
			{
				*dest = std::move(*lbegin);
				++dest;
				++lbegin;
				++lcount;
				rcount = 0;
				if(lcount >= min_gallop)
					break;
				// don't need to check if we reached the end.  that will happen on the right-hand-side 
			}
		}
		++min_gallop;
		// GALLOP SEARCH MODE
		for(std::ptrdiff_t g_dist = gallop_win_dist; g_dist >= std::ptrdiff_t(gallop_win_dist); )
		{
			min_gallop -= (min_gallop > 1);
			// gallop through the left range
			stop = lend - lbegin;
			for(i = 1; (i <= stop) and not comp(*rbegin, lbegin[i - 1]); i *= 2) 
			{
				// LOOP
			}
			--i;
			g_dist = i;
			if(i > 0)
			{
				stop = std::upper_bound(lbegin + (i / 2), lbegin + std::min(i, stop), *rbegin, comp) - lbegin;
				move_or_memcpy(lbegin, lbegin + stop, dest);
				dest += stop;
				lbegin += stop;
				// don't need to check if we reached the end.  that will happen on the right-hand-side 
			}
			
			stop = rend - rbegin;
			for(i = 1; i <= stop and comp(rbegin[i - 1], *lbegin); i *= 2) 
			{
				// LOOP
			}
			i -= 1;
			if(g_dist < i)
				g_dist = i;
			stop = std::lower_bound(rbegin + (i / 2), rbegin + std::min(i, stop), *lbegin, comp) - rbegin;
			dest = std::move(rbegin, rbegin + stop, dest);
			rbegin += stop;
			if(not (rbegin < rend))
			{
				move_or_memcpy(lbegin, lend, dest);
				return min_gallop;
			}
		}
		++min_gallop;
	}
	COMPILER_UNREACHABLE_;
}


template <class It>
inline void rotate_right_1(It begin, It end)
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
			if(not comp(*begin, begin[-1]))
				break;
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


template <class It, class Comp>
inline It get_existing_run(It begin, It end, Comp comp)
{
	// contract: begin < end
	using index_t = typename std::iterator_traits<It>::difference_type;
	if(index_t len = end - begin; len < 2)
		return end;
	else
	{
		index_t i = 1;
		for(; i < len; ++i)
			if(comp(begin[i], begin[i - 1]))
				break;
		if(i > 1)
			return begin + i;
		for(++i; i < len; ++i)
			if(not comp(begin[i], begin[i - 1]))
				break;
		std::reverse(begin, begin + i);
		return begin + i;
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

