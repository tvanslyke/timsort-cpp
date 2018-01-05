#include "timsort.h"
#include <string>
#include <utility>
#include <random>
#include <iterator>
#include <benchmark/benchmark.h>
#include <cmath>
#include "datasets/read_data_sets.h"

using namespace tim;
#ifndef SORT_ALGO
	#error "Must define macro 'SORT_ALGO'."
#endif


constexpr volatile std::size_t seed = 10219073871745632017ull;
static std::mt19937_64 mt(seed);

static constexpr const double two_pi = 6.28318530717958623199592693708837032318115234375;

using integral_t = int;
using real_t = double;

static const std::vector<naics_t>& census_data()
{
	static const std::vector<naics_t> data = 
		read_census_data("./datasets/us_state_naics_detailedsizes_2015.txt");
	return data;
}

static const std::vector<int>& mnist_data()
{
	static std::vector<int> data = read_mnist_data("./datasets/" + mnist_fname);
	return data;
}

template <class It>
void rand_small_ints(It begin, It end)
{
	static std::uniform_int_distribution<int> dist(0, 10);
	while(begin < end)
	{
		*begin = dist(mt);
		++begin;
	}
}
template <class It>
void rand_large_ints(It begin, It end)
{
	static std::uniform_int_distribution<int> dist(std::numeric_limits<int>::min(), 
						       std::numeric_limits<int>::max());
	while(begin < end)
	{
		*begin = dist(mt);
		++begin;
	}
}

template <std::size_t Minm, std::size_t Maxm, class It>
void rand_strings(It begin, It end)
{
	static std::uniform_int_distribution<char> chrval(char(32), char(126));
	static std::uniform_int_distribution<std::size_t> strsize(Minm, Maxm);
	while(begin < end)
	{
		begin->resize(strsize(mt));
		for(auto& chr: *begin)
			chr = chrval(mt);
		++begin;
	}
}


template <class It>
void sinusoid(It begin, It end, std::size_t period)
{
	double scale = two_pi / period;
	for(auto it = begin; it < end; ++it)
	{
		auto x = it - begin;
		*it = std::sin(x * scale); 
	}
}


template <class It>
void sorted_rand_ints(It begin, It end)
{
	rand_large_ints(begin, end);
	std::sort(begin, end);
}



static void BM_sort_random_uniform_ints(benchmark::State& state)
{
	std::vector<integral_t> vec;
	for(auto _: state)
	{
		state.PauseTiming();
		vec.resize(state.range(0));
		rand_large_ints(vec.begin(), vec.end());
		benchmark::DoNotOptimize(vec.data());
		state.ResumeTiming();
		SORT_ALGO(vec.begin(), vec.end());
	}
}





static void BM_sort_small_random_uniform_ints(benchmark::State& state)
{
	std::vector<integral_t> vec;
	for(auto _: state)
	{
		state.PauseTiming();
		vec.resize(state.range(0));
		rand_small_ints(vec.begin(), vec.end());
		benchmark::DoNotOptimize(vec.data());
		state.ResumeTiming();
		SORT_ALGO(vec.begin(), vec.end());
	}

}

template <std::size_t Minm, std::size_t Maxm>
static void BM_sort_random_strings(benchmark::State& state)
{
	std::vector<std::string> vec;
	for(auto _: state)
	{
		state.PauseTiming();
		vec.resize(state.range(0));
		rand_strings<Minm, Maxm>(vec.begin(), vec.end());
		benchmark::DoNotOptimize(vec.data());
		state.ResumeTiming();
		SORT_ALGO(vec.begin(), vec.end());
	}
}

static void BM_sort_mnist_train_labels(benchmark::State& state)
{
	std::vector<int> vec;
	for(auto _: state)
	{
		state.PauseTiming();
		vec = mnist_data();
		benchmark::DoNotOptimize(vec.data());
		state.ResumeTiming();
		SORT_ALGO(vec.begin(), vec.end());
	}
}


using census_data_category_t = enum {
	STATE        = 0,
	NAICS        = 1,
	ENTRSIZE     = 2,
	FIRM         = 3,
	ESTB         = 4,
	EMPL_N       = 5,
	EMPLFL_R     = 6,
	EMPLFL_N     = 7,
	PAYR_N       = 8,
	PAYRFL_N     = 9,
	STATEDSCR    = 10,
	NAICSDSCR    = 11,
	entrsizedscr = 12,
};

template <census_data_category_t C>
static void BM_sort_census_naics_data(benchmark::State& state)
{
	std::vector<naics_t> vec;
	auto compare = [](const naics_t& left, const naics_t& right) { 
		return std::get<std::size_t(C)>(left) < std::get<std::size_t(C)>(right);
	};
	for(auto _: state)
	{
		state.PauseTiming();
		vec = census_data();
		benchmark::DoNotOptimize(vec.data());
		state.ResumeTiming();
		SORT_ALGO(vec.begin(), vec.end(), compare);
	}
}





BENCHMARK(BM_sort_random_uniform_ints)->RangeMultiplier(8)->Range(8, 262144);
BENCHMARK(BM_sort_small_random_uniform_ints)->RangeMultiplier(8)->Range(8, 262144);
BENCHMARK_TEMPLATE(BM_sort_random_strings,  0,  8 /* ALL SSO */)->RangeMultiplier(8)->Range(8, 262144);
BENCHMARK_TEMPLATE(BM_sort_random_strings,  0, 64 /* SOME SSO */)->RangeMultiplier(8)->Range(8, 262144);
BENCHMARK_TEMPLATE(BM_sort_random_strings, 32, 64 /* NO SSO */ )->RangeMultiplier(8)->Range(8, 262144);
BENCHMARK(BM_sort_mnist_train_labels);
BENCHMARK_TEMPLATE(BM_sort_census_naics_data, STATE);
BENCHMARK_TEMPLATE(BM_sort_census_naics_data, NAICS);
BENCHMARK_TEMPLATE(BM_sort_census_naics_data, ENTRSIZE);
BENCHMARK_TEMPLATE(BM_sort_census_naics_data, FIRM);
BENCHMARK_TEMPLATE(BM_sort_census_naics_data, ESTB);
BENCHMARK_TEMPLATE(BM_sort_census_naics_data, EMPL_N);
BENCHMARK_TEMPLATE(BM_sort_census_naics_data, EMPLFL_R);
BENCHMARK_TEMPLATE(BM_sort_census_naics_data, EMPLFL_N);
BENCHMARK_TEMPLATE(BM_sort_census_naics_data, PAYR_N);
BENCHMARK_TEMPLATE(BM_sort_census_naics_data, PAYRFL_N);
BENCHMARK_TEMPLATE(BM_sort_census_naics_data, STATEDSCR);
BENCHMARK_TEMPLATE(BM_sort_census_naics_data, NAICSDSCR);
BENCHMARK_TEMPLATE(BM_sort_census_naics_data, entrsizedscr);

BENCHMARK_MAIN()


