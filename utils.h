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



/**
  * @brief 	  Semantically equivalent to std::upper_bound(), except requires 
  *        	  random access iterators.
  * @param begin  Iterator to the first element in the range to search.
  * @param end    Past-the-end iterator to the range to search.
  * @param value  The value to search for.
  * @param comp   The comparison function to use. 
  *
  * Similar to std::upper_bound, but first finds the k such that 
  * comp(value, begin[2**(k - 1) - 1]) and not comp(begin[2**k - 1], value)
  * and then returns the equivalent of 
  *  	std::upper_bound(begin + 2**(k - 1), begin + 2**k - 1, value, comp)
  *
  */
template <class It, class T, class Comp>
inline It gallop_upper_bound(It begin, It end, const T& value, Comp comp)
{
	std::size_t i = 0;
	std::size_t len = end - begin;
	for(; i < len and not comp(value, begin[i]); i = 2 * i + 1) { /* LOOP */ }
	if(len > i)
		len = i;
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

/**
 * @brief        Performs a leftwards rotation by one.
 * @param begin  Random access iterator to the first element in the range 
 *  	  	 to be rotated. 
 * @param end    Past-the-end random access iterator to the range to be 
 *		 rotated.
 *
 * Semantically equivalent to std::rotate(begin, begin + 1, end).
 */
template <class It>
inline void rotate_left(It begin, It end)
{
	// heuristic for deciding whether to use a temporary + range-move
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


/**
 * @brief        Insertion sort the range [begin, end), where [begin, mid) is 
 * 		 already sorted.
 * @param begin  Random access iterator to the first element in the range.
 * @param begin  Random access iterator to the first out-of-order element 
 * 		 in the range.
 * @param end    Past-the-end random access iterator to the range.
 * @param comp   Comparator to use.
 *
 * Semantically equivalent to std::rotate(begin, begin + 1, end).
 */
template <class It, class Comp>
void finish_insertion_sort(It begin, It mid, It end, Comp comp)
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
			rotate_left(std::upper_bound(begin, mid, *mid, comp), mid + 1);
			++mid;
		}
	}
}


/**
 * @brief 	 Count consecutive number of sorted elements, reversing the sequence 
 * 	  	 if it is descending.
 * @param begin  
 */
template <bool Stable, class It, class Comp>
It count_run(It begin, It end, Comp comp)
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
		// try to extend the run a little further if possible.
		while(begin < end and not comp(*begin, begin[-1]))
			++begin;
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

