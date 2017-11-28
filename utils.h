#include <algorithm>
#include <iterator>
#include <utility>
#include <cstdint>
#include <iostream>
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
static It gallop_lower_bound(It begin, It end, Pred pred)
{
	// if(begin < end)
	// {
		if(pred(*begin))
			return begin;
		It pos = std::next(begin);
		while(pos < end and not pred(*pos))
		{
			pos = gallop_step(begin, pos);
		}
		return gallop_step_reverse(begin, pos) + 1;
	// }
	// else
	// {
	// 	assert(false);
	// 	return begin;
	// }
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





