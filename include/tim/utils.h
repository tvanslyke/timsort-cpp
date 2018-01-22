#ifndef UTILS_H
#define UTILS_H
#include <algorithm>
#include "compiler.h"
#include "memcpy_algos.h"
#include "iter.h"
#include "minrun.h"


namespace tim {
namespace internal {


/**
 * Number of consecutive elements for which galloping would be a win over
 * either linear or binary search.
 */
inline constexpr const std::size_t gallop_win_dist = 7;

/**
 * Function object type implementing a generic operator<.  
 * This is used instead of std::less<> to stay consistent with std::sort() and 
 * std::stable_sort() when std::less<>::operator() is specialized.  This isn't 
 * necessary in general, because specializing member functions of types defined
 * in namespace std is undefined behavior, but it doesn't hurt either.
 */
struct DefaultComparator
{
	template <class Left, class Right>
	inline bool operator()(Left&& left, Right&& right) const
	{
		return std::forward<Left>(left) < std::forward<Right>(right);
	}
};

/**
 * @brief 	 Semantically equivalent to std::upper_bound(), except requires 
 *        	 random access iterators.
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
 * Semantically equivalent to std::rotate(begin, begin + 1, end) except 
 * random access iterators are required.
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
	// a call to std::rotate()
	using value_type = iterator_value_type_t<It>;
	constexpr std::size_t use_temporary_upper_limit = 3 * sizeof(void*);
	constexpr bool use_temporary = sizeof(value_type) < use_temporary_upper_limit;
	if constexpr(use_temporary)
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
		// for large types, implement as a series of adjacent swaps
		for(end -= (end > begin); end > begin; --end)
			std::swap(end[-1], *end);
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
 */
template <class It, class Comp>
void finish_insertion_sort(It begin, It mid, It end, Comp comp)
{
	using value_type = iterator_value_type_t<It>;
	if constexpr(std::is_scalar_v<value_type>
		     and (   std::is_same_v<Comp, std::less<>> 
		          or std::is_same_v<Comp, std::less<value_type>>
		          or std::is_same_v<Comp, std::greater<>>
		          or std::is_same_v<Comp, std::greater<value_type>>
		          or std::is_same_v<Comp, DefaultComparator>)
	)
	{
		// if types are cheap to compare and cheap to copy, do a linear search
		// instead of a binary search
		while(mid < end)
		{
			for(auto pos = mid; pos > begin and comp(*pos, pos[-1]); --pos)
				std::swap(pos[-1], *pos);
			++mid;
		}
	}
	else
	{
		// do the canonical 'swap it with the one before it' insertion sort up
		// to max_minrun<value_type>() / 4 elements
		for(const auto stop = begin + std::min(max_minrun<value_type>() / 4, std::size_t(end - begin)); mid < stop; ++mid)
			for(auto pos = mid; pos > begin and comp(*pos, pos[-1]); --pos)
				std::swap(pos[-1], *pos);
		// finish it off with a binary insertion sort
		for(; mid < end; ++mid)
			rotate_left(std::upper_bound(begin, mid, *mid, comp), mid + 1);
	}
	
}

} /* namespace internal */
} /* namespace tim */


#endif /* UTILS_H */
