cmake_minimum_required(VERSION 3.25)
project(noexcept_checker)

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

target_link_options(noexcept_checker PRIVATE
	"-Wl,--allow-shlib-undefined"
)

set_target_properties(noexcept_checker PROPERTIES
	PREFIX ""
)

target_compile_options(noexcept_checker PRIVATE
	-fno-rtti # grep LLVM_ENABLE_RTTI /path/to/llvm-project/build/CMakeCache.txt
	-std=c++23
	-Wall
	-Wextra
	-Wpedantic
	-Wconversion
	-Wsign-conversion
	-fconcepts-diagnostics-depth=999
)
