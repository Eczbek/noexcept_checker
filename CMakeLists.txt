cmake_minimum_required(VERSION 3.25)
project(noexcept_checker)

include(FetchContent)

find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

add_library(noexcept_checker MODULE
	noexcept_checker.cpp
)

target_include_directories(noexcept_checker PRIVATE
	${LLVM_INCLUDE_DIRS}
	${CLANG_INCLUDE_DIRS}
)

target_compile_definitions(noexcept_checker PRIVATE
	${LLVM_DEFINITIONS}
)

target_compile_features(noexcept_checker PRIVATE cxx_std_23)

target_compile_options(noexcept_checker PRIVATE
	-fno-rtti # grep LLVM_ENABLE_RTTI /path/to/llvm-project/build/CMakeCache.txt
	-Wall
	-Wextra
	-Wpedantic
	-Wconversion
	-Wsign-conversion
	-fconcepts-diagnostics-depth=999
)

target_link_options(noexcept_checker PRIVATE
	"-Wl,--allow-shlib-undefined"
)

set_target_properties(noexcept_checker PROPERTIES
	PREFIX ""
)

FetchContent_Declare(xieite GIT_REPOSITORY https://github.com/Eczbek/xieite GIT_TAG main)
FetchContent_MakeAvailable(xieite)
target_link_libraries(noexcept_checker PRIVATE xieite)
