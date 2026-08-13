cmake_minimum_required(VERSION 3.10)
project(LLLCompiler VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Find libtcc for JIT support
find_library(TCC_LIB tcc)
if(TCC_LIB)
  message(STATUS "Found libtcc: ${TCC_LIB} - enabling JIT")
  add_definitions(-DLLL_JIT)
else()
  message(STATUS "libtcc not found - JIT disabled")
endif()

# Platform detection
if(WIN32)
  set(PLATFORM "windows")
elseif(APPLE)
  set(PLATFORM "macos")
else()
  set(PLATFORM "linux")
endif()

# Build number from git or default
execute_process(COMMAND git rev-list --count HEAD
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE BUILD_NUM
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)
if(NOT BUILD_NUM)
  set(BUILD_NUM 0)
endif()
add_definitions(-DBUILD_NUMBER=${BUILD_NUM})

file(GLOB_RECURSE LLLC_SOURCES
  "src/*.c"
  "src/keywords/*.c"
)

add_executable(lllc ${LLLC_SOURCES})

target_include_directories(lllc PRIVATE include src src/keywords)

if(TCC_LIB)
  target_link_libraries(lllc ${TCC_LIB} dl m)
else()
  target_link_libraries(lllc m)
endif()

install(TARGETS lllc DESTINATION bin)

option(BUILD_TESTS "Build tests" OFF)
if(BUILD_TESTS)
  enable_testing()
  file(GLOB TEST_LEXER_SOURCES "tests/test_lexer.c" "src/lexer.c" "src/utils.c")
  add_executable(test_lexer ${TEST_LEXER_SOURCES})
  file(GLOB TEST_PARSER_SOURCES "tests/test_parser.c" "src/lexer.c" "src/parser.c" "src/utils.c" "src/keywords/*.c")
  add_executable(test_parser ${TEST_PARSER_SOURCES})
  target_include_directories(test_lexer PRIVATE include src src/keywords)
  target_include_directories(test_parser PRIVATE include src src/keywords)
  add_test(NAME test_lexer COMMAND test_lexer)
  add_test(NAME test_parser COMMAND test_parser)
endif()
