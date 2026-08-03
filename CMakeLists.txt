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

target_compile_features(noexcept_checker PRIVATE cxx_std_23)

target_compile_options(noexcept_checker PRIVATE
	-fno-rtti # grep LLVM_ENABLE_RTTI /path/to/llvm-project/build/CMakeCache.txt
	-Wall
	-Wextra
	-Wpedantic
	-Wconversion
	-Wsign-conversion
	-fsanitize=address
	-static-libasan
)

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	target_link_options(noexcept_checker PRIVATE
		"-Wl,-undefined,dynamic_lookup"
	)
else()
	target_link_options(noexcept_checker PRIVATE
		"-Wl,--allow-shlib-undefined"
	)
endif()

set_target_properties(noexcept_checker PROPERTIES
	PREFIX ""
)
