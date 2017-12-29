#ifndef TIMSORT_STACK_BUFFER_H
#define TIMSORT_STACK_BUFFER_H
#include <cstddef>
#include <limits>
#include <vector>
#include <type_traits>
#include <iostream>
#include <cstring>
#include "memcpy_algos.h"



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


template <class IntType, class ValueType, std::size_t ExtraValueTypeSlots=0>
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
		top{buffer + (buffer_size - 1)},
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
		return (buffer + (buffer_size - 1)) - top;
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
		return std::size_t(num_in_merge_buffer) * sizeof(ValueType);
	}
	static inline constexpr std::size_t total_bytes_in_buffer() noexcept
	{
		return buffer_size * sizeof(IntType);
	}
	inline bool need_to_trim_merge_buffer() const noexcept
	{
		return (std::size_t(num_in_merge_buffer) > 0) and 
			(total_bytes_in_buffer() - bytes_consumed_by_merge_buffer()) < bytes_consumed_by_one_more_run();
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
			// const std::size_t bytes_needed = bytes_consumed_by_one_more_run();
			// std::size_t used = bytes_consumed_by_merge_buffer();
			// while(bytes_needed > total_bytes_in_buffer() - used)
			// {
			// 	--num_in_merge_buffer;
			// 	std::destroy_at(merge_buffer_begin() + num_in_merge_buffer);
			// 	used -= sizeof(ValueType);
			// }
			std::size_t overlap = 
				(total_bytes_in_buffer() - bytes_consumed_by_one_more_run()) 
				- bytes_consumed_by_merge_buffer();
			overlap = overlap / sizeof(ValueType) + ((overlap % sizeof(ValueType)) > 0);
			std::destroy(merge_buffer_end() - overlap, merge_buffer_end());
			num_in_merge_buffer -= overlap;
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
					     std::decay_t<iterator_value_type_t<It>>>, 
			      "If you see this, timsort() is broken.");
		if constexpr (can_forward_memcpy_v<It>)
		{
			std::memcpy(buffer, get_memcpy_iterator(begin), (end - begin) * sizeof(ValueType));
			return merge_buffer_begin();
		}
		else if constexpr (can_reverse_memcpy_v<It>)
		{
			std::memcpy(buffer, get_memcpy_iterator(end - 1), (end - begin) * sizeof(ValueType));
			return std::make_reverse_iterator(merge_buffer_begin() + (end - begin));
		}
		else if constexpr (trivial_destructor)
		{
			for(auto dest = merge_buffer_begin(); begin < end; ++begin, (void)++dest)
				::new(static_cast<ValueType*>(std::addressof(*dest))) ValueType(std::move(*begin));
			// uninitialized_move_wrapper(begin, end, merge_buffer_begin());
			return merge_buffer_begin();
		}
		else
		{
			
			if(const auto len = end - begin; len > num_in_merge_buffer)
			{
				auto dest = merge_buffer_begin();
				COMPILER_ASSUME_(num_in_merge_buffer >= 0);
				dest = std::move(begin, begin + num_in_merge_buffer, dest);
				begin += num_in_merge_buffer;
				for(; begin < end; ++num_in_merge_buffer, (void)++dest, (void)++begin)
					::new(static_cast<ValueType*>(std::addressof(*dest))) ValueType(std::move(*begin));
				
				// uninitialized_move_wrapper(begin + num_in_merge_buffer, end, dest);
				// num_in_merge_buffer = len;
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
		if constexpr(not trivial_destructor)
		{
			if(need_to_trim_merge_buffer())
				destroy_enough_to_fit_one_more_run();
		}
		*top = runlen;
		--top;
	}

	inline void pop() noexcept
	{
		++top;
	}
	

	template <std::size_t I>
	inline const IntType& get_offset() const noexcept
	{
		static_assert(I < 5);
		// assert(offset_count() > I);
		return top[I + 1];
	}
	
	template <std::size_t I>
	inline IntType& get_offset() noexcept
	{
		static_assert(I < 5);
		// assert(offset_count() > I);
		return top[I + 1];
	}
	
	inline bool merge_ABC_case_1() const noexcept
	{
		return get_offset<2>() - get_offset<3>() <= get_offset<0>() - get_offset<2>();
	}

	inline bool merge_ABC_case_2() const noexcept
	{
		return get_offset<3>() - get_offset<4>() <= get_offset<1>() - get_offset<3>();
	}

	inline bool merge_ABC() const noexcept
	{
		return merge_ABC_case_1() or merge_ABC_case_2();
	}

	inline bool merge_AB() const noexcept
	{
		return get_offset<2>() - get_offset<3>() < get_offset<0>() - get_offset<1>();
	}

	inline bool merge_BC() const noexcept
	{
		return get_offset<1>() - get_offset<2>() <= get_offset<0>() - get_offset<1>();
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
		return buffer[buffer_size - 2];
	}

	static constexpr const std::size_t buffer_size = (timsort_max_stack_size<std::size_t>() * sizeof(IntType) + ExtraValueTypeSlots * sizeof(ValueType)) / sizeof(IntType);
	static constexpr const std::size_t required_alignment = alignof(std::aligned_union_t<sizeof(IntType), IntType, ValueType>);
	alignas(required_alignment) IntType buffer[buffer_size];
	IntType* top;
	std::conditional_t<trivial_destructor, const std::ptrdiff_t, std::ptrdiff_t> num_in_merge_buffer;
};




#endif /* TIMSORT_STACK_BUFFER_H */
