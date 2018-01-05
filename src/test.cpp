#define BOOST_TEST_MODULE test timsort
#include <boost/test/included/unit_test.hpp>
#include <boost/test/parameterized_test.hpp>
#include "timsort.h"
#include <iostream>
#include <random>
#include <vector>
#include <cassert>
#include "datasets/read_data_sets.h"
#include <list>

using namespace tim;
static std::mt19937_64 mt{std::random_device{}()};


static const auto & census_data()
{
	static const auto data = read_census_data("./datasets/" + census_fname);
	return data;
}


template <class It, class Cmp, class EqualTo>
void test_stable_sort(It data_begin, It data_end, Cmp cmp, EqualTo equal_to = std::equal_to<>{})
{
	using value_t = typename std::iterator_traits<It>::value_type;
	std::vector<value_t> data_copy(data_begin, data_end);

	// checks range equivalence
	auto ranges_are_equal = [&]() {
 		return std::equal(data_begin, data_end, data_copy.begin(), data_copy.end(), equal_to);
	};
	
	// under test
	timsort(data_begin, data_end, cmp);

	// test that the sort did its job
	bool sorted = std::is_sorted(data_begin, data_end, cmp);
	BOOST_TEST_REQUIRE(sorted, "Resultant range is NOT sorted with respect to the given comparator.");
	
	// make a copy of the original data
	std::vector<value_t> original(data_begin, data_end);

	// ground-truth result 
	std::stable_sort(data_copy.begin(), data_copy.end(), cmp);
	// check that the sort was stable
	bool stable = ranges_are_equal();
	BOOST_TEST_CHECK(stable, "Resultant range is sorted but NOT stable according to the given equality comparator.");
	// if not, ensure the resultant range is a permutation of the original.
	// if this fails, the sort is probably overwriting data it shouldn't be
	if(not stable)
	{
		bool value_safe = std::is_permutation(data_begin, data_end, original.begin(), original.end(), equal_to);
		BOOST_TEST_REQUIRE(value_safe, "Resultant range is not a permutation of the original data.  " 
					       "The sort is not value-safe."
		);
	}
}

template <class It>
void random_ints(It begin, It end, int minm, int maxm)
{
	std::uniform_int_distribution<int> dist(minm, maxm);
	while(begin < end)
	{
		*begin++ = dist(mt);
	}
}

template <class It>
void random_strs(It begin, It end, std::size_t min_size, std::size_t max_size, char minm, char maxm)
{
	std::uniform_int_distribution<char> randchr(minm, maxm);
	std::uniform_int_distribution<std::size_t> randsize(min_size, max_size);
	while(begin < end)
	{
		begin->resize(randsize(mt));
		for(auto& chr: *begin)
			chr = randchr(mt);
		++begin;
	}
}


template <class Comp>
using int_params_t = std::tuple<int, int, Comp, std::equal_to<>, std::vector<std::size_t>>;


template <class Params>
void int_test(const Params & params)
{
	int minm = std::get<0>(params);
	int maxm = std::get<1>(params);
	auto cmp = std::get<2>(params);
	auto eq = std::get<3>(params);
	std::vector<std::size_t> lengths = std::get<4>(params);
	std::vector<int> data;
	for(auto size: lengths)
	{
		data.resize(size);
		random_ints(data.begin(), data.end(), minm, maxm);
		test_stable_sort(data.begin(), data.end(), cmp, eq);
	}
}

template <class Comp>
using str_params_t = std::tuple<char, char, std::size_t, std::size_t, Comp, std::equal_to<>, std::vector<std::size_t>>;

template <class Params>
void str_test(const Params & params)
{
	char minm = std::get<0>(params);
	char maxm = std::get<1>(params);
	auto minlen = std::get<2>(params);
	auto maxlen = std::get<3>(params);
	auto cmp = std::get<4>(params);
	auto eq = std::get<5>(params);
	std::vector<std::size_t> lengths = std::get<6>(params);
	std::vector<std::string> data;
	for(auto size: lengths)
	{
		data.resize(size);
		random_strs(data.begin(), data.end(), minlen, maxlen, minm, maxm);
		test_stable_sort(data.begin(), data.end(), cmp, eq);
		// sorted ascending
		random_strs(data.begin(), data.end(), minlen, maxlen, minm, maxm);
		std::sort(data.begin(), data.end(), std::less<>());
		test_stable_sort(data.begin(), data.end(), cmp, eq);
		// sorted descending
		random_strs(data.begin(), data.end(), minlen, maxlen, minm, maxm);
		std::sort(data.begin(), data.end(), std::greater<>());
		test_stable_sort(data.begin(), data.end(), cmp, eq);
	}
}



template <std::size_t I>
bool census_comparator(const naics_t& left, const naics_t& right)
{
	return std::get<I>(left) < std::get<I>(right);
}



BOOST_AUTO_TEST_CASE(census_data_0)
{
	auto data = census_data();
	test_stable_sort(data.begin(), data.end(), census_comparator<0>, std::equal_to<>{});
}
BOOST_AUTO_TEST_CASE(census_data_1)
{
	auto data = census_data();
	test_stable_sort(data.begin(), data.end(), census_comparator<1>, std::equal_to<>{});
}
BOOST_AUTO_TEST_CASE(census_data_2)
{
	auto data = census_data();
	test_stable_sort(data.begin(), data.end(), census_comparator<2>, std::equal_to<>{});
}
BOOST_AUTO_TEST_CASE(census_data_3)
{
	auto data = census_data();
	test_stable_sort(data.begin(), data.end(), census_comparator<3>, std::equal_to<>{});
}
BOOST_AUTO_TEST_CASE(census_data_4)
{
	auto data = census_data();
	test_stable_sort(data.begin(), data.end(), census_comparator<4>, std::equal_to<>{});
}
BOOST_AUTO_TEST_CASE(census_data_5)
{
	auto data = census_data();
	test_stable_sort(data.begin(), data.end(), census_comparator<5>, std::equal_to<>{});
}
BOOST_AUTO_TEST_CASE(census_data_6)
{
	auto data = census_data();
	test_stable_sort(data.begin(), data.end(), census_comparator<6>, std::equal_to<>{});
}
BOOST_AUTO_TEST_CASE(census_data_7)
{
	auto data = census_data();
	test_stable_sort(data.begin(), data.end(), census_comparator<7>, std::equal_to<>{});
}
BOOST_AUTO_TEST_CASE(census_data_8)
{
	auto data = census_data();
	test_stable_sort(data.begin(), data.end(), census_comparator<8>, std::equal_to<>{});
}
BOOST_AUTO_TEST_CASE(census_data_9)
{
	auto data = census_data();
	test_stable_sort(data.begin(), data.end(), census_comparator<9>, std::equal_to<>{});
}
BOOST_AUTO_TEST_CASE(census_data_10)
{
	auto data = census_data();
	test_stable_sort(data.begin(), data.end(), census_comparator<10>, std::equal_to<>{});
}
BOOST_AUTO_TEST_CASE(census_data_11)
{
	auto data = census_data();
	test_stable_sort(data.begin(), data.end(), census_comparator<11>, std::equal_to<>{});
}
BOOST_AUTO_TEST_CASE(census_data_12)
{
	auto data = census_data();
	test_stable_sort(data.begin(), data.end(), census_comparator<12>, std::equal_to<>{});
}


static const std::vector<int_params_t<std::less<>>> int_params_less{
	{0,        1, std::less<>(), std::equal_to<>(), {0, 1, 2, 3, 10, 1000}}, 
	{0,       10, std::less<>(), std::equal_to<>(), {0, 1, 2, 3, 10, 1000, 100000}}, 
	{0, 10000000, std::less<>(), std::equal_to<>(), {0, 1, 2, 3, 10, 1000, 100000}}, 
};


static const std::vector<int_params_t<std::greater<>>> int_params_greater{
	{0,        1, std::greater<>(), std::equal_to<>(), {0, 1, 2, 3, 10, 1000}}, 
	{0,       10, std::greater<>(), std::equal_to<>(), {0, 1, 2, 3, 10, 1000, 100000}}, 
	{0, 10000000, std::greater<>(), std::equal_to<>(), {0, 1, 2, 3, 10, 1000, 100000}}, 
};

static const std::vector<str_params_t<std::less<>>> str_params_less{
	{char(32),  char(128), 0,   0, std::less<>(), std::equal_to<>(), {0, 1, 2, 3, 10, 1000, 100000}}, 
	{char(32),  char(128), 0,   1, std::less<>(), std::equal_to<>(), {0, 1, 2, 3, 10, 1000, 100000}},
	{char(32),  char(128), 0,   2, std::less<>(), std::equal_to<>(), {0, 1, 2, 3, 10, 1000, 100000}},
	{char(32),  char(128), 0,   8, std::less<>(), std::equal_to<>(), {0, 1, 2, 3, 10, 1000, 100000}}, 
	{char(32),  char(128), 0,  32, std::less<>(), std::equal_to<>(), {0, 1, 2, 3, 10, 1000, 100000}}, 
	{char(32),  char(128), 0, 128, std::less<>(), std::equal_to<>(), {0, 1, 2, 3, 10, 1000, 100000}}, 
};

static const std::vector<str_params_t<std::greater<>>> str_params_greater{
	{char(32),  char(128), 0,   0, std::greater<>(), std::equal_to<>(), {0, 1, 2, 3, 10, 1000, 100000}}, 
	{char(32),  char(128), 0,   1, std::greater<>(), std::equal_to<>(), {0, 1, 2, 3, 10, 1000, 100000}},
	{char(32),  char(128), 0,   2, std::greater<>(), std::equal_to<>(), {0, 1, 2, 3, 10, 1000, 100000}},
	{char(32),  char(128), 0,   8, std::greater<>(), std::equal_to<>(), {0, 1, 2, 3, 10, 1000, 100000}},
	{char(32),  char(128), 0,  32, std::greater<>(), std::equal_to<>(), {0, 1, 2, 3, 10, 1000, 100000}}, 
	{char(32),  char(128), 0, 128, std::greater<>(), std::equal_to<>(), {0, 1, 2, 3, 10, 1000, 100000}}, 
};

static auto* int_test_less = int_test<int_params_t<std::less<>>>;
static auto* int_test_greater = int_test<int_params_t<std::greater<>>>;
static auto* str_test_less = str_test<str_params_t<std::less<>>>;
static auto* str_test_greater = str_test<str_params_t<std::greater<>>>;


///// HACK BECAUSE I ONLY SUPERFICIALLY KNOW HOW TO USE Boost.test

int add_param_tests();
static int _hack = add_param_tests();

int add_param_tests()
{

	boost::unit_test::framework::master_test_suite().add( 
		BOOST_PARAM_TEST_CASE(int_test_less,    int_params_less.begin(),    int_params_less.end())
	);
	boost::unit_test::framework::master_test_suite().add( 
		BOOST_PARAM_TEST_CASE(int_test_greater, int_params_greater.begin(), int_params_greater.end())
	);
	boost::unit_test::framework::master_test_suite().add( 
		BOOST_PARAM_TEST_CASE(str_test_less,    str_params_less.begin(),    str_params_less.end())
	);
	boost::unit_test::framework::master_test_suite().add( 
		BOOST_PARAM_TEST_CASE(str_test_greater, str_params_greater.begin(), str_params_greater.end())
	);
	return 0;
}

