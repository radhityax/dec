cmake_minimum_required(VERSION 3.16)
project(dec VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)

    set(CMAKE_C_FLAGS_DEBUG
        "-g -O0 -fsanitize=address,undefined -fno-omit-frame-pointer")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG
        "-fsanitize=address,undefined")

    set(CMAKE_C_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")
endif()

add_subdirectory(lib/cmark)

set(DEC_COMMON_SOURCES
    src/util.c
    src/toml.c
    src/template.c
)

set(DEC_SOURCES
${DEC_COMMON_SOURCES}
src/builder.c 
)
add_executable(dec src/main.c ${DEC_SOURCES})
target_include_directories(dec PRIVATE src)
target_link_libraries(dec PRIVATE cmark)

enable_testing()

add_executable(dec_test test/runner.c ${DEC_COMMON_SOURCES})
target_include_directories(dec_test PRIVATE src)
target_compile_definitions(dec_test PRIVATE DEC_TEST)
# target_link_libraries(dec_test PRIVATE cmark)

add_test(NAME dec_unit COMMAND dec_test)
