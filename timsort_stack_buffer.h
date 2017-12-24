#ifndef TIMSORT_STACK_BUFFER_H
#define TIMSORT_STACK_BUFFER_H
#include <cstddef>
#include <limits>
#include <vector>
#include <type_traits>
#include <iostream>
#include <cstring>
#include "contiguous_iterator.h"



template <class It>
inline constexpr const bool has_memcpy_safe_value_type_v = 
		std::is_trivially_copyable_v<typename std::iterator_traits<It>::value_type>;

template <class It>
static constexpr const bool can_forward_memcpy_v = is_contiguous_iterator_v<It> and has_memcpy_safe_value_type_v<It>;

template <class It>
static constexpr const bool can_reverse_memcpy_v = is_reverse_contiguous_iterator_v<It> and has_memcpy_safe_value_type_v<It>;


template <class It>
struct GetMemcpyIterator
{
	static typename std::iterator_traits<It>::value_type* get(It iter) noexcept
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



static constexpr std::size_t div_by_log2phi(std::size_t num) noexcept
{
	constexpr const double log2phi = 0.69424191363061737991557720306445844471454620361328125;
	return std::size_t(num / log2phi) + 1;
}

template <class IntType>
static constexpr std::size_t timsort_max_stack_size() noexcept
{
	return div_by_log2phi(std::numeric_limits<IntType>::digits) + 1;
}


template <class IntType, class ValueType>
struct timsort_stack_buffer
{
	using buffer_pointer_t = ValueType*;
	template <class It>
	using buffer_iter_t = std::conditional_t<(not can_forward_memcpy_v<It>) and can_reverse_memcpy_v<It>, 
					         std::reverse_iterator<buffer_pointer_t>, 
					         buffer_pointer_t>;
	static constexpr const bool trivial_destructor = std::is_trivially_destructible_v<ValueType>;
	static constexpr const bool nothrow_destructor = std::is_nothrow_destructible_v<ValueType>;
	static constexpr const bool nothrow_move = std::is_nothrow_move_constructible_v<ValueType>;
	static constexpr const bool trivial = std::is_trivial_v<ValueType>;

	timsort_stack_buffer():
		buffer{},
		top{std::make_reverse_iterator(buffer + buffer_size)},
		bottom{std::make_reverse_iterator(buffer + buffer_size)},
		num_in_merge_buffer{0}
	{
		push(0);
	}	

	~timsort_stack_buffer() noexcept(nothrow_destructor)
	{
		destroy_merge_buffer();
	}

	inline std::size_t offset_count() const noexcept
	{
		return top - bottom;
	}

	inline std::size_t run_count() const noexcept
	{
		return offset_count() - 1;
	}

	inline std::size_t bytes_consumed_by_run_stack() const noexcept
	{
		return offset_count() * sizeof(IntType);
	}
	
	inline std::size_t bytes_consumed_by_one_more_run() const noexcept
	{
		return (offset_count() + 1) * sizeof(IntType);
	}

	inline std::size_t bytes_available_for_merge_buffer() const noexcept
	{
		return buffer_size * sizeof(IntType) - bytes_consumed_by_run_stack();
	}

	inline std::size_t count_available_for_merge_buffer() const noexcept
	{
		return bytes_available_for_merge_buffer() / sizeof(ValueType);
	}
	
	inline std::size_t bytes_consumed_by_merge_buffer() const noexcept
	{
		return num_in_merge_buffer * sizeof(ValueType);
	}
	static inline constexpr std::size_t total_bytes_in_buffer() noexcept
	{
		return buffer_size * sizeof(IntType);
	}
	inline bool need_to_trim_merge_buffer() const noexcept
	{
		if constexpr(not trivial_destructor)
			return (num_in_merge_buffer > 0) and 
				(total_bytes_in_buffer() - bytes_consumed_by_merge_buffer()) < bytes_consumed_by_one_more_run();
		else
			return false;
	}
	inline void destroy_enough_to_fit_one_more_run() noexcept
	{
		if constexpr(sizeof(ValueType) >= sizeof(IntType))
		{
			--num_in_merge_buffer;
			std::destroy_at(merge_buffer_begin() + num_in_merge_buffer);
		}
		else
		{
			const std::size_t bytes_needed = bytes_consumed_by_one_more_run();
			std::size_t used = bytes_consumed_by_merge_buffer();
			while(bytes_needed > total_bytes_in_buffer() - used)
			{
				--num_in_merge_buffer;
				std::destroy_at(merge_buffer_begin() + num_in_merge_buffer);
				used -= sizeof(ValueType);
			}
		}
	}

	void destroy_merge_buffer() noexcept(nothrow_destructor)
	{
		if constexpr(not trivial_destructor)
		{
			std::destroy(merge_buffer_begin(), merge_buffer_end());
			num_in_merge_buffer = 0;
		}
	}
	
	inline void destroy_merge_buffer_top_half() noexcept(nothrow_destructor)
	{
		if constexpr(not trivial_destructor)
		{
			auto destroy_count = (num_in_merge_buffer + 1) / 2;
			std::destroy(merge_buffer_end() - destroy_count, merge_buffer_end());
			num_in_merge_buffer -= destroy_count;
		}
	}


	/*
	Move elements from [begin, end) onto the unused memory in the run stack.
	It is assumed that the caller has ensured that the stack buffer has sufficient
	unused space to hold the provided range without overwriting any of the run counts.

	This member function tries suuuuper hard to safely use a memcpy().  std::copy() and
	friends cannot
	Using compile-time type information, this copy
	*/
	
	template <class It>
	buffer_iter_t<It> move_to_merge_buffer(It begin, It end) 
		noexcept(trivial or nothrow_move)
	{
		
		static_assert(std::is_same_v<std::decay_t<ValueType>, 
					     std::decay_t<typename std::iterator_traits<It>::value_type>>, 
			      "If you see this, timsort() is broken.");
		if constexpr(can_forward_memcpy_v<It>)
		{
			std::memcpy(buffer, get_memcpy_iterator(begin), (end - begin) * sizeof(ValueType));
			return merge_buffer_begin();
		}
		else if constexpr (can_reverse_memcpy_v<It>)
		{
			std::memcpy(buffer, get_memcpy_iterator(end - 1), (end - begin) * sizeof(ValueType));
			return std::make_reverse_iterator(merge_buffer_begin() + (end - begin));
		}
		else if constexpr(trivial_destructor)
		{
			std::uninitialized_move(begin, end, merge_buffer_begin());
			return merge_buffer_begin();
		}
		else
		{
			std::size_t len = end - begin;
			if(len > num_in_merge_buffer)
			{
				auto dest = merge_buffer_begin();
				dest = std::move(begin, begin + num_in_merge_buffer, dest);
				std::uninitialized_move(begin + num_in_merge_buffer, end, dest);
				num_in_merge_buffer = len;
			}
			else
			{
				std::move(begin, end, merge_buffer_begin());
			}
			return merge_buffer_begin();
		}
	}

	template <class It>
	inline bool can_acquire_merge_buffer([[maybe_unused]] It begin, [[maybe_unused]] It end) const noexcept
	{
		if constexpr(sizeof(ValueType) > (buffer_size * sizeof(IntType)))
			return false;
		else
			return (end - begin) <= std::ptrdiff_t(count_available_for_merge_buffer());
	}
		
	inline void push(IntType runlen) noexcept(nothrow_destructor)
	{
		// TODO: assume(i > *(top + 1))
		if(need_to_trim_merge_buffer())
			destroy_enough_to_fit_one_more_run();
		*top = runlen;
		++top;
	}

	inline void pop() noexcept
	{
		--top;
	}
	
	inline IntType stack_top() const noexcept
	{
		return top[-1];
	}

	inline IntType* stack_top() noexcept
	{
		return top[-1];
	}

	template <std::size_t I>
	inline std::size_t get_offset() const noexcept
	{
		static_assert(I < 5);
		assert(offset_count() > I);
		return *(top - (I + 1));
	}
	
	template <std::size_t I>
	inline IntType& get_offset() noexcept
	{
		static_assert(I < 5);
		assert(offset_count() > I);
		return *(top - (I + 1));
	}

	template <std::size_t I>
	inline IntType get_run_length() const noexcept
	{
		static_assert(I < 4);
		return get_offset<I>() - get_offset<I + 1>();
	}
	
	template <std::size_t I>
	inline void remove_run() noexcept
	{
		static_assert(I < 4);
		if constexpr(I == 0)
		{
			pop();
		}
		else
		{
			get_offset<I>() = get_offset<I - 1>();
			remove_run<I - 1>();
		}
	}

	inline const ValueType* merge_buffer_begin() const noexcept
	{
		return reinterpret_cast<const ValueType*>(buffer);
	}
	
	inline ValueType* merge_buffer_begin() noexcept
	{
		return reinterpret_cast<ValueType*>(buffer);
	}

	inline const ValueType* merge_buffer_end() const noexcept
	{
		return reinterpret_cast<const ValueType*>(buffer) + num_in_merge_buffer;
	}

	inline ValueType* merge_buffer_end() noexcept
	{
		return reinterpret_cast<ValueType*>(buffer) + num_in_merge_buffer;
	}

	inline std::size_t get_bottom_run_size() const noexcept
	{
		return bottom[1];
	}
	/// DEBUGGING
	void print() const
	{
		std::copy(bottom, top, std::ostream_iterator<std::ptrdiff_t>(std::cerr, ", "));
		std::cerr << std::endl;
	}

	void print_lens() const
	{
		std::vector<std::ptrdiff_t> v;
		std::adjacent_difference(bottom, top, std::back_inserter(v));
		std::copy(v.begin() + bool(v.size()), v.end(), std::ostream_iterator<std::ptrdiff_t>(std::cerr, ", "));
		std::cerr << std::endl;
	}
	// TODO: add function to compute bytes needed after run stack is topped out
	static constexpr const std::size_t buffer_size = timsort_max_stack_size<IntType>();
	static constexpr const std::size_t required_alignment = alignof(std::aligned_union_t<sizeof(IntType), IntType, ValueType>);
	alignas(required_alignment) IntType buffer[buffer_size];
	std::reverse_iterator<IntType*> top;
	std::reverse_iterator<IntType*> bottom;
	std::size_t num_in_merge_buffer;
};




#endif /* TIMSORT_STACK_BUFFER_H */
