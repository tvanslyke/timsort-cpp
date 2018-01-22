#ifndef TIMSORT_MEMCPY_ALGOS_H
#define TIMSORT_MEMCPY_ALGOS_H
#include "contiguous_iterator.h"
#include "iter.h"
#include <algorithm>
#include <cstring>

namespace tim {
namespace internal {

template <class It>
inline constexpr const bool has_memcpy_safe_value_type_v = 
		std::is_trivially_copyable_v<iterator_value_type_t<It>>;

template <class It>
static constexpr const bool can_forward_memcpy_v = is_contiguous_iterator_v<It> and has_memcpy_safe_value_type_v<It>;

template <class It>
static constexpr const bool can_reverse_memcpy_v = is_reverse_contiguous_iterator_v<It> and has_memcpy_safe_value_type_v<It>;


template <class It>
struct GetMemcpyIterator
{
	static iterator_value_type_t<It>* get(It iter) noexcept
	{
		return &(*iter);
	}
};

template <class T>
struct GetMemcpyIterator<T*>
{
	static T* get(T* iter) noexcept
	{
		return iter;
	}
};

template <class T>
struct GetMemcpyIterator<const T*>
{
	static const T* get(const T* iter) noexcept
	{
		return iter;
	}
};

template <class It>
auto get_memcpy_iterator(It iter) noexcept
{
	return GetMemcpyIterator<It>::get(iter);
}

/**
 * Try to memcpy to the destination range, otherwise use std::move.
 */
template <class SrcIt, class DestIt>
auto move_or_memcpy(SrcIt begin, SrcIt end, DestIt dest)
{
	using value_type = iterator_value_type_t<SrcIt>;
	if constexpr(can_forward_memcpy_v<SrcIt> and can_forward_memcpy_v<DestIt>)
	{
		std::memcpy(get_memcpy_iterator(dest), get_memcpy_iterator(begin), (end - begin) * sizeof(value_type));
		return dest + (end - begin);
	}
	else if constexpr(can_reverse_memcpy_v<SrcIt> and can_reverse_memcpy_v<DestIt>)
	{
		std::memcpy(get_memcpy_iterator(dest + (end - begin) - 1), get_memcpy_iterator(end - 1), (end - begin) * sizeof(value_type));
		return dest + (end - begin);
	}
	else
	{
		return std::move(begin, end, dest);
	}
}
} /* namespace internal */
} /* namespace tim */


#endif /* TIMSORT_MEMCPY_ALGOS_H */
