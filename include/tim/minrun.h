#ifndef TIMSORT_MINRUN_H
#define TIMSORT_MINRUN_H


namespace tim {
namespace internal {



/*
 * Modified variant of the compute_minrun() function used in CPython's 
 * list_sort().  
 * 
 * The CPython version of this function chooses a value in [32, 65) for 
 * minrun.  Unlike in CPython, C++ objects aren't guaranteed to be the 
 * size of a pointer.  A heuristic is used here under the assumption 
 * that std::move(some_arbitrary_cpp_object) is basically a bit-blit.  
 * If the type is larger that 4 pointers then minrun maxes out at 32 
 * instead of 64.  Similarly, if the type is larger than 8 pointers, 
 * it maxes out at 16.  This is a major win for large objects 
 * (think tuple-of-strings).
 * Four pointers is used as the cut-off because libstdc++'s std::string
 * implementation was slightly, but measurably worse in the benchmarks
 * when the max minrun was 32 instead of 64 (and their std::string 
 * is 4 pointers large).
 */
template <class T>
static constexpr std::size_t max_minrun() noexcept
{
	if constexpr(sizeof(T) > (sizeof(void*) * 8))
		return 16;
	else if constexpr(sizeof(T) > (sizeof(void*) * 4))
		return 32;
	else 
		return 64;
}

template <class T>
static constexpr std::size_t compute_minrun(std::size_t n) noexcept
{
	constexpr std::size_t minrun_max = max_minrun<T>();
	std::size_t r = 0;
	while (n >= minrun_max) 
	{
		r |= (n & 1);
		n >>= 1;
	}
	return (n + r);
}


} /* namespace internal */
} /* namespace tim */


#endif /* TIMSORT_MINRUN_H */
