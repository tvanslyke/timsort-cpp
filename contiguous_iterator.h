#ifndef CONTIGUOUS_ITERATOR_H
#define CONTIGUOUS_ITERATOR_H

#include <iterator>
#include <type_traits>
#include <string>
#include <vector>
#include <valarray>
#include <cstddef>
#include <utility>


#ifdef __has_include
# if __has_include(<string_view>)
#  include <string_view>
# endif
#endif

template <class It>
struct is_valarray_iterator
{
	using _value_type = typename std::iterator_traits<It>::value_type;
	using _va = std::valarray<_value_type>;
	using _begin_result = std::decay_t<decltype(std::begin(std::declval<_va>()))>;
	using _begin_cresult = std::decay_t<decltype(std::begin(std::declval<const _va>()))>;
	using _end_result = std::decay_t<decltype(std::begin(std::declval<_va>()))>;
	using _end_cresult = std::decay_t<decltype(std::begin(std::declval<const _va>()))>;
	
	static constexpr const bool value = std::is_same_v<It, _begin_result>
					    or std::is_same_v<It, _begin_cresult>
					    or std::is_same_v<It, _end_result>
					    or std::is_same_v<It, _end_cresult>;

};

template <class It>
struct is_vector_string_or_valarray_iterator
{
	using _value_type = typename std::iterator_traits<It>::value_type;
	static constexpr const bool value = 
			(std::is_same_v<It, typename std::vector<_value_type>::iterator> and not std::is_same_v<bool, _value_type>)
			or (std::is_same_v<It, typename std::vector<_value_type>::const_iterator> and not std::is_same_v<bool, _value_type>)
			or std::is_same_v<It, typename std::basic_string<_value_type>::iterator>
			or std::is_same_v<It, typename std::basic_string<_value_type>::const_iterator>
#ifdef __has_include
# if __has_include(<string_view>)
			or std::is_same_v<It, typename std::basic_string_view<_value_type>::iterator>
			or std::is_same_v<It, typename std::basic_string_view<_value_type>::const_iterator>
# endif
#endif
			or is_valarray_iterator<It>::value;
};


template <class It>
struct is_contiguous_iterator
{
	static constexpr const bool value = is_vector_string_or_valarray_iterator<It>::value;
};

template <class T>
struct is_contiguous_iterator<T*>: std::true_type{};

template <class T>
struct is_contiguous_iterator<const T*>: std::true_type{};

template <class It>
struct is_contiguous_iterator<const It>: is_contiguous_iterator<It> {};

template <class It>
struct is_contiguous_iterator<std::reverse_iterator<std::reverse_iterator<It>>>: is_contiguous_iterator<It>{};

template <class It>
inline constexpr const bool is_contiguous_iterator_v = is_contiguous_iterator<It>::value;

template <class It>
struct is_reverse_contiguous_iterator: std::false_type{};

template <class It>
struct is_reverse_contiguous_iterator<std::reverse_iterator<It>>: is_contiguous_iterator<It>{};

template <class It>
inline constexpr const bool is_reverse_contiguous_iterator_v = is_reverse_contiguous_iterator<It>::value;



#endif /* CONTIGUOUS_ITERATOR_H */
