cmake_minimum_required(VERSION 3.8)

add_library(timsort INTERFACE)
find_package(Boost COMPONENTS unit_test_framework)
find_package(benchmark)
include_directories(./include/tim)
include_directories(./)
target_include_directories(timsort INTERFACE include/tim)
install(DIRECTORY include/tim INCLUDE DESTINATION include FILES_MATCHING PATTERN "*.h")

set (CMAKE_CXX_STANDARD 17)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# unit tests
add_executable(test-timsort EXCLUDE_FROM_ALL ./src/test.cpp)
target_include_directories(test-timsort PRIVATE ${Boost_INCLUDE_DIRS})

# benchmark executable for timsort()
add_executable(benchmark-timsort EXCLUDE_FROM_ALL ./src/bench.cpp)
target_include_directories(benchmark-timsort PRIVATE ${benchmark_INCLUDE_DIRS})
target_compile_definitions(benchmark-timsort PRIVATE SORT_ALGO=timsort )
target_link_libraries(benchmark-timsort benchmark pthread)

# benchmark executable for std::sort()
add_executable(benchmark-stdsort EXCLUDE_FROM_ALL ./src/bench.cpp)
target_include_directories(benchmark-stdsort PRIVATE ${benchmark_INCLUDE_DIRS})
target_compile_definitions(benchmark-stdsort PRIVATE SORT_ALGO=std::sort )
target_link_libraries(benchmark-stdsort benchmark pthread)

# benchmark executable for std::stable_sort()
add_executable(benchmark-stdstable_sort EXCLUDE_FROM_ALL ./src/bench.cpp)
target_include_directories(benchmark-stdstable_sort PRIVATE ${benchmark_INCLUDE_DIRS})
target_compile_definitions(benchmark-stdstable_sort PRIVATE SORT_ALGO=std::stable_sort )
target_link_libraries(benchmark-stdstable_sort benchmark pthread)


if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|Intel")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
endif()

