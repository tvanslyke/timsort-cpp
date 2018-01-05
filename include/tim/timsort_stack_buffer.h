#ifndef TIMSORT_STACK_BUFFER_H
#define TIMSORT_STACK_BUFFER_H
#include <cstddef>
#include <limits>
#include <type_traits>
#include "memcpy_algos.h"
#include "compiler.h"
#include "minrun.h"


namespace tim {
namespace internal {




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
	using self_t = timsort_stack_buffer<IntType, ValueType>;
	static constexpr const bool trivial_destructor = std::is_trivially_destructible_v<ValueType>;
	static constexpr const bool nothrow_destructor = std::is_nothrow_destructible_v<ValueType>;
	static constexpr const bool nothrow_move = std::is_nothrow_move_constructible_v<ValueType>;
	static constexpr const bool trivial = std::is_trivial_v<ValueType>;
	
	timsort_stack_buffer() noexcept:
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
	timsort_stack_buffer(const self_t&) = delete; 
	timsort_stack_buffer(self_t&&) = delete; 
	timsort_stack_buffer& operator=(self_t&&) = delete; 
	timsort_stack_buffer& operator=(const self_t&) = delete; 

	/** Returns the number of indices stored in the run stack. */
	inline std::size_t offset_count() const noexcept
	{
		return (buffer + (buffer_size - 1)) - top;
	}

	/** 
	 * Returns the number of runs in the run stack. 
	 * This is always one less than offset_count() because the first
	 * item in the run stack is always SizeType(0).
	 */
	inline std::size_t run_count() const noexcept
	{
		return offset_count() - 1;
	}

	/** 
	 * Returns the number of bytes used by the run stack. 
	 */
	inline std::size_t bytes_consumed_by_run_stack() const noexcept
	{
		return offset_count() * sizeof(IntType);
	}
	
	/** 
	 * Returns the number of bytes that would be used by the run stack 
	 * if an additional run were to be added. 
	 *
	 * Used to determine whether some of the 'ValueType' objects in the 
	 * stack need to be destroyed before pushing an additional run onto 
	 * the run stack.
	 */
	inline std::size_t bytes_consumed_by_one_more_run() const noexcept
	{
		return (offset_count() + 1) * sizeof(IntType);
	}

	/** 
	 * Returns the number of bytes in the stack buffer that are not 
	 * currently used to keep track of pending runs. 
	 * 
	 * Used to determine whether the free space in the stack buffer can be
	 * used as a temporary buffer for the merge algorithm.
	 */
	inline std::size_t bytes_available_for_merge_buffer() const noexcept
	{
		return buffer_size * sizeof(IntType) - bytes_consumed_by_run_stack();
	}

	/** 
	 * Returns the number of 'ValueType' objects that can fit in the 
	 * stack buffer without bleeding in to the space used to keep track
	 * of pending runs.
	 * 
	 * Used to determine whether the free space in the stack buffer can be
	 * used as a temporary buffer for the merge algorithm.
	 */
	inline std::size_t count_available_for_merge_buffer() const noexcept
	{
		return bytes_available_for_merge_buffer() / sizeof(ValueType);
	}
	
	/** 
	 * Returns the number of 'ValueType' objects that currently live in 
	 * the stack buffer and have yet to be destroyed.
	 * 
	 * Used to determine whether some of the 'ValueType' objects in the 
	 * stack need to be destroyed before pushing an additional run onto 
	 * the run stack.
	 */
	inline std::size_t bytes_consumed_by_merge_buffer() const noexcept
	{
		return num_in_merge_buffer * sizeof(ValueType);
	}
	/** Return the total number of bytes in the stack buffer. */
	static inline constexpr std::size_t total_bytes_in_buffer() noexcept
	{
		return buffer_size * sizeof(IntType);
	}
	/** 
	 * Returns a bool indicating whether some of the 'ValueType' objects 
	 * in the stack need to be destroyed before pushing an additional run 
	 * onto the run stack.
	 */
	inline bool need_to_trim_merge_buffer() const noexcept
	{
		return (num_in_merge_buffer > 0) and 
			(total_bytes_in_buffer() - bytes_consumed_by_merge_buffer()) < bytes_consumed_by_one_more_run();
	}
	
	/** 
	 * Destroy some of the 'ValueType' objects in the stack buffer to 
	 * make room for an additional run to be pushed onto the stack.
	 */
	inline void destroy_enough_to_fit_one_more_run() noexcept
	{
		if constexpr(sizeof(ValueType) >= sizeof(IntType))
		{
			--num_in_merge_buffer;
			std::destroy_at(merge_buffer_begin() + num_in_merge_buffer);
		}
		else
		{
			std::size_t overlap = 
				(total_bytes_in_buffer() - bytes_consumed_by_one_more_run()) 
				- bytes_consumed_by_merge_buffer();
			overlap = overlap / sizeof(ValueType) + ((overlap % sizeof(ValueType)) > 0);
			std::destroy(merge_buffer_end() - overlap, merge_buffer_end());
			num_in_merge_buffer -= overlap;
		}
	}

	/** 
	 * Destroy all of the 'ValueType' objects in the stack buffer. 
	 */
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
			// both contiguous iterators to trivially copyable type.  do a memcpy
			std::memcpy(buffer, get_memcpy_iterator(begin), (end - begin) * sizeof(ValueType));
			return merge_buffer_begin();
		}
		else if constexpr (can_reverse_memcpy_v<It>)
		{
			// It is a reverse contiguous iterators to trivially copyable type.  do a memcpy
			// and return a reverse iterator 
			std::memcpy(buffer, get_memcpy_iterator(end - 1), (end - begin) * sizeof(ValueType));
			return std::make_reverse_iterator(merge_buffer_begin() + (end - begin));
		}
		else
		{
			// not trivially copyable.  provide at least basic exception guarantee
			fill_merge_buffer(begin, end);
			return merge_buffer_begin();
		}
	}

	/**
	 * Move the range [begin, end) on to the stack buffer.
	 * If an exception is thrown while moving one of the objects to the 
	 * stack buffer, some of the objects in [begin, end) will be left in
	 * a valid, but unspecified state.  No resources are leaked.
	 */
	template <class It>
	void fill_merge_buffer(It begin, It end) noexcept(nothrow_move)
	{
		auto dest = merge_buffer_begin();
		if constexpr (not trivial_destructor)
		{
			if(num_in_merge_buffer < std::size_t(end - begin))
			{
				dest = std::move(begin, begin + num_in_merge_buffer, dest);
				begin += num_in_merge_buffer;
			}
			else
			{
				dest = std::move(begin, end, dest);
				begin = end;
				return;
			}
		}
		// if the destructor is trivial we don't need to increment 'num_in_merge_buffer'
		for(; begin < end; (void)++dest, (void)++begin)
		{
			::new(static_cast<ValueType*>(std::addressof(*dest))) ValueType(std::move(*begin));
			if constexpr(not trivial_destructor)
				++num_in_merge_buffer;
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
	/**
	 * @brief 	       Push a run onto the run stack.
	 * @param run_end_pos  The index of the run being pushed onto the stack.
	 */ 
	inline void push(IntType run_end_pos) noexcept(nothrow_destructor)
	{
		// TODO: assume(i > *(top + 1))
		if constexpr(not trivial_destructor)
		{
			if(need_to_trim_merge_buffer())
				destroy_enough_to_fit_one_more_run();
		}
		*top = run_end_pos;
		--top;
	}

	/**
	 * @brief 	  Erase the index at the top of the stack.
	 * @param runlen  The index of the run being pushed onto the stack.
	 */ 
	inline void pop() noexcept
	{
		++top;
	}
	

	template <std::size_t I>
	inline const IntType& get_offset() const noexcept
	{
		static_assert(I < 5);
		return top[I + 1];
	}
	
	template <std::size_t I>
	inline IntType& get_offset() noexcept
	{
		static_assert(I < 5);
		return top[I + 1];
	}
	
	inline const IntType& operator[](std::ptrdiff_t i) const noexcept
	{
		return top[i + 1];
	}
	
	/** 
	 * Return a bool indicating whether a merge needs to occur between
	 * two of the three runs at the top of the stack.
	 */
	inline bool merge_ABC_case_1() const noexcept
	{
		return get_offset<2>() - get_offset<3>() <= get_offset<0>() - get_offset<2>();
	}

	/** 
	 * Return a bool indicating whether a merge needs to occur between
	 * two of the three runs at the top of the stack.
	 */
	inline bool merge_ABC_case_2() const noexcept
	{
		return get_offset<3>() - get_offset<4>() <= get_offset<1>() - get_offset<3>();
	}

	/** 
	 * Return a bool indicating whether a merge needs to occur between
	 * two of the three runs at the top of the stack.  
	 */
	inline bool merge_ABC() const noexcept
	{
		return merge_ABC_case_1() or merge_ABC_case_2();
	}

	/** 
	 * Given that a merge needs to occur between two of the three runs 
	 * at the top of the stack, returns true if the merge should occur 
	 * between the third-from-the-top and second-from-the-top runs 
	 * on the stack.  If false is returned, the two topmost runs should
	 * be merged instead.
	 */
	inline bool merge_AB() const noexcept
	{
		return get_offset<2>() - get_offset<3>() < get_offset<0>() - get_offset<1>();
	}

	/** 
	 * Return a bool indicating whether a merge needs to occur between
	 * the two runs at the top of the stack.  
	 */
	inline bool merge_BC() const noexcept
	{
		return get_offset<1>() - get_offset<2>() <= get_offset<0>() - get_offset<1>();
	}
	
	/** 
	 * Modifies the run stack to reflect the fact that the run ending at
	 * index 'I' was merged with the run beginning at index 'I'.
	 */
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

	/** 
	 * Obtain an iterator to the beginning of the merge buffer.
	 */
	inline const ValueType* merge_buffer_begin() const noexcept
	{
		return reinterpret_cast<const ValueType*>(buffer);
	}
	
	/** 
	 * Obtain an iterator to the beginning of the merge buffer.
	 */
	inline ValueType* merge_buffer_begin() noexcept
	{
		return reinterpret_cast<ValueType*>(buffer);
	}

	/** 
	 * Obtain an iterator to the end of the merge buffer.
	 */
	inline const ValueType* merge_buffer_end() const noexcept
	{
		return reinterpret_cast<const ValueType*>(buffer) + num_in_merge_buffer;
	}

	/** 
	 * Obtain an iterator to the end of the merge buffer.
	 */
	inline ValueType* merge_buffer_end() noexcept
	{
		return reinterpret_cast<ValueType*>(buffer) + num_in_merge_buffer;
	}
	
	static constexpr std::size_t extra_stack_alloc() noexcept
	{
		constexpr std::size_t minrun_bytes = max_minrun<ValueType>() * sizeof(ValueType);
		constexpr std::size_t max_bytes = 128 * sizeof(void*);
		// a merge only happens after at least two runs are on the 
		// stack so subtract off 2 from the stack size to simulate
		constexpr std::size_t stack_max_bytes = (timsort_max_stack_size<IntType>() - 2) * sizeof(IntType);
		
		if constexpr(minrun_bytes < max_bytes)
		{
			std::size_t nbytes = minrun_bytes;
			// see how many '(2**k) * minrun's we can fit on the stack
			while(nbytes < max_bytes)
				nbytes += nbytes;
			return (nbytes / sizeof(IntType)) / 2;
		}
		else if constexpr(max_bytes + stack_max_bytes >= minrun_bytes and not (stack_max_bytes >= minrun_bytes))
			// if adding 'max_bytes' bytes to the run stack allows fitting 
			// a run of length 'minrun' on the stack, then go ahead and allocate
			return max_bytes / sizeof(IntType);
		else
			// otherwise don't bother allocating extra, we'll probably be on the 
			// heap the whole time anyway
			return 0;

	}
	

	/** Size of the buffer measured in 'IntType' objects. */
	static constexpr const std::size_t buffer_size = timsort_max_stack_size<IntType>();// + extra_stack_alloc();
	/** Alignment of the buffer. */
	static constexpr const std::size_t required_alignment = alignof(std::aligned_union_t<sizeof(IntType), IntType, ValueType>);
	alignas(required_alignment) IntType buffer[buffer_size];
	IntType* top;
	/** 
	 * Number of 'ValueType' objects in the merge buffer.  
	 * If 'ValueType' is trivially destructible, this is always zero.
	 */
	std::conditional_t<trivial_destructor, const std::size_t, std::size_t> num_in_merge_buffer = 0;
};


} /* namespace internal */
} /* namespace tim */



#endif /* TIMSORT_STACK_BUFFER_H */
