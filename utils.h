#include <algorithm>
#include <iterator>
#include <utility>
#include <cstdint>
static constexpr const std::size_t gallop_index_count = std::numeric_limits<std::size_t>::digits;
using index_array_t = std::array<std::size_t, gallop_index_count>;

template <std::size_t ... I>
static constexpr const index_array_t make_gallop_index_array(std::index_sequence<I...>)
{
	return index_array_t{((std::size_t(0x01) << I) - 1) ...};
} 
static constexpr const index_array_t gallop_indices{
	 make_gallop_index_array(std::make_index_sequence<gallop_index_count>{})
};

template <class It, class Pred>
It gallop_lower_bound_ex(It begin, It end, Pred pred)
{
	auto idx = gallop_indices.begin();
	if((begin >= end) or pred(*begin))
	 	return begin;
	do{
		++idx;
	}while((begin + (*idx)) < end and not pred(begin[*idx]));
	--idx;
	return std::next(begin, *idx);
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





