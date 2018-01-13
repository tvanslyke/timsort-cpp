# timsort-cpp
[![Language](https://img.shields.io/badge/language-C++-blue.svg)](https://isocpp.org/) [![Standard](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B#Standardization) [![License](https://img.shields.io/badge/license-MIT-blue.svg)](https://opensource.org/licenses/MIT)

An optimized-for-C++ implementation of Tim Peters' Timsort sorting algorithm written in C++17.

[Timsort](https://en.wikipedia.org/wiki/Timsort) is a practical, adaptive, and stable sorting algorithm originally developed for CPython by Tim Peters in 2002.  Timsort is blazingly fast for data which is already partially sorted and it performs well with datatypes where comparisons are relatively expensive to compute.  

This implementation of Timsort is written to be as close to Tim Peter's [original implementation](https://github.com/python/cpython/blob/master/Objects/listsort.txt) as possible, but with the following changes and considerations:

* In CPython, a comparison can be arbitrarily expensive and can have arbitrary side effects.  In C++, we use contracts to enforce certain behavior limitations in these circumstances.
* In CPython, all objects are pointers as thus all moves and swaps are quite cheap.  In C++, while we generally expect moves and swaps to be fairly inexpensive, objects can be much heavier than a pointer.
* In CPython, all type information is determined at runtime.  In C++, we can use compile-time facilities to extract type information about our data and functions.
* This implementation conforms to the corrections described [here](http://www.envisage-project.eu/proving-android-java-and-python-sorting-algorithm-is-broken-and-how-to-fix-it/) (CPython's implementation does as well, however Tim Peters' [documentation](https://github.com/python/cpython/blob/master/Objects/listsort.txt) does not cover this)
* This implementation makes more clever use of stack space than the CPython implementation.  A portion of the stack is used to store information about pending merges.  In this implementation, unused portions of this stack space are used as a temporary buffer when performing merges.  In CPython, an additional buffer is allocated on the stack for this purpose.
    * More precisely, this implementation also allocates extra stack space, but the amount allocated is dependent on the size of the type and is allocated directly in the pending-merge array to allow addional utilization of the unused space in the pending-merge slots.
    * Of course when doing this, we have to be careful with alignment, so we align the stack buffer to accomodate both the type being sorted as well as the integral type used to store the pending-merge information.
* Using a small amount of template metaprogramming, we can, at compile-time, special-case portions of the algorithm for certain data to improve performance.
    * If the data being sorted is cheap to compare and cheap to move, we use a plain (as opposed to binary) insertion sort when forcing runs to 'minrun' length.  This tends to be a win for scalar types like `int` or `double`.
    * The maximum possible value of 'minrun' depends on the size of the type being sorted.  For example, if sorting somewhat heavy objects, minrun is no greater than 32, and for very heavy objects, 16 is the max.
    * As mentioned above, the amount of extra stack space allocated to be used as a merge buffer depends on the size of the type.  This helps minimizing heap usage for small types and keeps us from over-allocating on the stack for large types.
* One other optimization is the usage of compiler intrinsics when possible.  This can be switched off by defining `TIMSORT_NO_USE_COMPILER_INTRINSICS`.  

Overall, the micro-optimizations implemented in this sort result in a sort that is faster than the libstdc++ and (only sometimes) libc++ implementations of `std::stable_sort()`. (with some caveats, see below)

All optimizations were made only when benchmarks showed their effectiveness.  Special care is taken to ensure that no UB is invoked.  The C++17 standard (draft n4567) was heavily consulted while writing this implementation.  Note that all measurements were made using g++-7.2 with an Intel(R) Core(TM) i7-6700 CPU @ 3.40GH CPU at the highest optimization level.

### Usage
`tim::timsort()` has a nearly identical interface to `std::stable_sort()`.  The only difference is that `tim::timsort()` may throw a `std::bad_alloc` exception in the event that there is insufficient memory available to complete the merge routine.  `std::stable_sort()` will, in this case, fall back to using an O(Nlog(N)) merge routine that does not allocate.  A `tim::stable_sort()` routine which implements `std::stable_sort()`'s interface fully will very likely be added in the future.

example.cpp:
```cpp
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <tim/timsort.h>

int main()
{
	std::vector<std::string> words { 
		"Timsort", "is", "pretty", "darn", "awesome", "!"
	};
	tim::timsort(words.begin(), words.end());
	for(const auto& word: words)
		std::cout << word << ' ';
	std::cout << '\n';
	tim::timsort(words.begin(), words.end(), std::greater<>{});
	for(const auto& word: words)
		std::cout << word << ' ';
	std::cout << std::endl;
	return 0;
}
```
Compile and run
```sh
$ g++ -std=c++17 example.cpp -o example
$ ./example
! Timsort awesome darn is pretty 
pretty is darn awesome Timsort ! 
```

### Prerequisites

To use this implementation, a conforming C++17 compiler and STL implementation is required.  The test suite passes on both g++-7.2 and clang++-5.0 with both libstdc++-6.0 and libc++-6.0.

The test suite uses Boost.test and the benchmarks use Google benchmark.  Neither are required for installation.

### Installing
Clone the repo locally:
```sh
git clone https://github.com/tvanslyke/timsort-cpp.git
cd timsort-cpp
```
Run cmake and install
```sh
cmake <optional options> ./
make install
```

### Running the tests
After configuring the build with cmake the tests can be run as follows:
```sh
make test-timsort
./test-timsort
```

### Benchmarks
Benchmarks can be run be run for `std::sort`, `std::stable_sort` and for `timsort` as follows (requires Google benchmark to be installed).

```sh
$ make benchmark-stdsort benchmark-stdstable_sort benchmark-timsort
$ # you can run the benchmarks individually
$ ./benchmark-stdsort
$ # or you can compare them two-at-a-time
$ compare_bench.py ./benchmark-stdstable_sort ./timsort
```

### As a Replacement for `std::stable_sort()`
This implementation of Timsort is *nearly* a drop-in replacement for `std::stable_sort()`, except for the following differences:
* The standard specifies that `std::stable_sort` falls back to an O(Nlog(N)) merge algorithm if insufficient memory is available for merging.  This implementation throws a `std::bad_alloc` exception in this case.  This is a consequence of the fact that the temporary buffer (when the stack cannot be used) is allocated via a `std::vector`.  Attempting to catch this exception and fall back to an inplace merge routine led to measurable pessimizations across all benchmarks.  If allocation fails and `std::bad_alloc` is thrown, the sorted range is valid but may contain a different permutation than it did before the call to `timsort()`.  Note that if `std::bad_alloc` is thrown by any other operation during the sort, some elements in the range may be in a valid, but unspecified (moved-from) state.  
* The standard specifies that `std::stable_sort()` does at most Nlog(N) comparisons if enough memory can be allocated.  Most existing implementations of `std::stable_sort()` don't appear to follow this strictly, instead opting for O(Nlog(N)) asymptotic complexity (rather than as a hard limit).


### Exception Safety and Contract
Aside from the above clarifications, `timsort()` has an identical contract to `std::stable_sort()`.
* If an exception is thrown by a swap, move, or comparison operation, some of the elements in the range may be left in a valid, but unspecified state.  That is, `timsort()` provides only the basic exception guarantee (no resources are leaked).
    * In the case where `std::bad_alloc` is thrown when attempting to allocate memory for the merge routine, then all elements in the range are left in a valid state.  None of the elements will be in a "moved-from" state, and no data loss will have occured.  That is, the range will simply be some valid permutation of the range that was initially passed to `timsort`.
* If the range and values in it are sufficiently small, and no exceptions can be thrown by a swap, move, or comparison then `timsort()` throws no exceptions.

One could transform `timsort()` into a fully-conforming `std::stable_sort()` will the following function:
```cpp
template <class It, class Comp = std::less<>>
void my_stable_sort(It begin, It end, Comp comp = std::less<>{})
{
	try
	{
		tim::timsort(begin, end, comp);
	}
	catch(const std::bad_alloc&)
	{
		// the point is that the bad-alloc is recoverable.  
		std::stable_sort(begin, end, comp);
	}
}
```

### Supported Compilers
The following compilers and STL implementations have compiled and passed the test suite.
* clang-5.0 with libstdc++-6.0 or libc++-6.0
* g++-7.2 with libstdc++-6.0

Earlier versions of any of the above compilers or STL implementations may work as well, but have not been tested.

MSVC and ICC may work but have not been tested.

### Contributing
Contributions and bug reports are welcome!

### Releases
0.1.0 Initial release!

### Authors
* **Timothy VanSlyke** - vanslyke.t@husky.neu.edu

### License
This project is licensed under the MIT License.

### Acknowledgments
* Tim Peters for his awesome sorting algorithm!
* Any C++ STL devs.  Man you guys have it rough!
* The Envisage Project for their formal verification of Timsort.
* Timo Bingmann for creating The Sound of Sorting.  This served as a very useful tool for weeding out some bottlenecks and even helped spot the occasional bug!
