#include "timsort.h"
#include <iostream>
#include <random>
#include <vector>
#include <cassert>
#include "datasets/read_data_sets.h"


static std::mt19937_64 mt{std::random_device{}()};

template <class It>
void randarr(It begin, It end, int minm, int maxm)
{
	std::uniform_int_distribution<int> dist(minm, maxm);
	while(begin < end)
	{
		*begin = dist(mt);
		++begin;
	}
}
template <class It>
void randarr_str(It begin, It end, std::size_t min_size, std::size_t max_size, char minm, char maxm)
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

template <class V>
void print(const V & v, const char * delim = ", ")
{
	for(const auto & item:v)
	{
		std::cout << item << delim;
	}
	std::cout << std::endl;
}

template <class V>
void printv(const V & v, const char * delim = ", ")
{
	for(const auto & item:v)
	{
		for(auto c:item)
			std::cout << item;
		std::cout << delim << std::endl;
	}
	std::cout << std::endl;
}

template <class V>
void print_mismatches(const V& left, const V& right)
{
	auto l = left.begin();
	auto r = right.begin();
	auto le = left.end();
	auto re = right.end();
	while(l < le)
	{
		std::tie(l, r) = std::mismatch(l, le, r, re);
		++l;
		++r;
		if(l == le)
			break;
		std::cout << std::distance(left.begin(), l) << ": " << *l << ", " << *r << std::endl;
	}
}

void test_ints()
{
	std::cerr << "test_ints()" << std::endl;
	std::uniform_int_distribution<std::size_t> sz_rng(0, 50);
	std::vector<int> arr;
	arr.resize(1000); 
	auto arr2 = arr;
	for(auto i = 0; i < 1000; ++i)
	{
		arr.resize(sz_rng(mt));
		randarr(arr.begin(), arr.end(), 0, 100);
		arr2 = arr;
		timsort(arr.begin(), arr.end(), std::less<>());
		std::sort(arr2.begin(), arr2.end());
		if((not std::is_sorted(arr.begin(), arr.end(), std::less<>())) or (arr != arr2))
		{
			std::cerr << "Sort Failure." << std::endl;
		}
		// assert(arr == arr2);
	}
}
void test_strs()
{
	std::cerr << "test_strs()" << std::endl;
	std::uniform_int_distribution<std::size_t> sz_rng(0, 50);
	std::vector<std::string> arr;
	arr.resize(100); 
	auto arr2 = arr;
	for(auto i = 0; i < 1000; ++i)
	{
		arr.resize(sz_rng(mt));
		randarr_str(arr.begin(), arr.end(), 10, 100, 'a', 'z' + 1);
		arr2 = arr;
		timsort(arr.begin(), arr.end(), std::less<>());
		std::sort(arr2.begin(), arr2.end());
		if((not std::is_sorted(arr.begin(), arr.end(), std::less<>())) or (arr != arr2))
		{
			std::cerr << "Sort Failure." << std::endl;
		}
		assert(arr == arr2);
	}
}


template <std::size_t I>
void test_census()
{
	static const auto & data_cpy = read_census_data();
	std::cerr << "test_census<" << I << ">()" << std::endl;
	static auto data = data_cpy;
	auto cmp = [](const auto & a, const auto & b) {
		return std::get<I>(a) < std::get<I>(b);
	};
	timsort(data.begin(), data.end(), cmp);
	assert(std::is_sorted(data.begin(), data.end(), cmp));
	assert(std::is_permutation(data.begin(), data.end(), data_cpy.begin(), data_cpy.end()));
	auto data_stable = data_cpy;
	std::stable_sort(data_stable.begin(), data_stable.end(), cmp);
	assert(data_stable == data);

	// stability test
	
}
int main()
{
	test_ints();
	test_strs();
	test_census<0>();
	test_census<1>();
	test_census<2>();
	test_census<3>();
	test_census<4>();
	test_census<5>();
	test_census<6>();
	test_census<7>();
	test_census<8>();
	test_census<9>();
	test_census<10>();
	test_census<11>();
	test_census<12>();
	return 0;
}


