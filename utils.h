#include <algorithm>
#include <iterator>
#include <utility>
static constexpr const std::size_t gallop_index_count = std::numeric_limits<std::size_t>::digits();
using index_array_t = std::array<std::size_t, gallop_index_count>;

template <std::size_t ... I>
static constexpr const index_array_t make_gallop_index_array(std::index_sequence<I...>)
{
	return index_array_t{((std::size_t(0x01) << I) - 1) ...};
} 
static constexpr const index_array_t gallop_indices{
	 make_gallop_index_array(std::index_sequence_for<gallop_index_count>{})
};

template <class It, class Pred>
It gallop_lower_bound_ex(It begin, It end, Pred pred)
{
	std::size_t index = 0;
	if(begin < end)
	{
		
	}
	else
		return begin;
}
