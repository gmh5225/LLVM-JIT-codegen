# CMakeList.txt : CMake project for codegen, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

set(CMAKE_CXX_STANDARD 17)
add_definitions(-D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)
find_package(LLVM REQUIRED CONFIG PATHS ../llvm-lib/lib/cmake/llvm NO_DEFAULT_PATH)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})
llvm_map_components_to_libnames(llvm_libs support core nativecodegen orcjit)

project ("codegen")

# Add source to this project's executable.
set(codegen_sources
    src/ltm.h
    src/ltm.cpp

    src/dom.h
    src/dom.cpp

    src/dom-to-string.cpp
    src/dom-to-string.h

    src/ast.h
    src/ast.cpp

    src/parser.h
    src/parser.cpp

    src/name-resolver.h
    src/name-resolver.cpp

    src/type-checker.h
    src/type-checker.cpp

    src/generator.h
    src/generator.cpp

    src/vmt_util.h
)
add_executable(codegen
    src/main.cpp
)
target_link_libraries(codegen ${llvm_libs})

# TODO: Add tests and install targets if needed.
add_executable(codegen_test
    ${codegen_sources}
    src/fake-gunit.h
    src/fake-gunit.cpp
    src/ltm-test.cpp
    src/dom-test.cpp
    src/dom-to-string-test.cpp
    src/compiler-test.cpp
    src/vmt_util-test.cpp
)
target_link_libraries(codegen_test ${llvm_libs})
