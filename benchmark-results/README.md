# Benchmarks
All benchmarks were done using Google benchmark.

Most of the benchmark names are self-explainatory.
* The `BM_sort_random_strings<X, Y>` benchmarks are sorting random strings with lengths in `[X, Y)`.
* The `BM_sort_census_naics_data<Z>` benchmarks are sorting the 2015 US Census Bureau data on the [North American Industry Classification System](https://www.census.gov/eos/www/naics/).
    * Each of these corresponds to sorting a vector of tuples of strings and `std::size_t`s by each tuple entry. This is meant to simulate expensive-to-move objects.  Additionally, the tuples are already sorted or partially sorted with respect to some of the entries in each tuple. 
    * This data is also used in the test suite to test that the implementation is stable.