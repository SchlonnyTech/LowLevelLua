cmake_minimum_required(VERSION 3.10)
project(LLLCompiler VERSION 1.0.2 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

find_library(TCC_LIB tcc)
if(TCC_LIB)
  message(STATUS "Found libtcc: ${TCC_LIB} - enabling legacy JIT")
  add_definitions(-DLLL_JIT)
else()
  message(STATUS "libtcc not found - legacy JIT disabled")
endif()

if(WIN32)
  set(PLATFORM "windows")
  add_definitions(-D_WIN32 -DLLL_WINDOWS)
elseif(APPLE)
  set(PLATFORM "macos")
else()
  set(PLATFORM "linux")
endif()

string(TIMESTAMP BUILD_YEAR "%Y")
string(TIMESTAMP BUILD_MONTH "%m")
string(TIMESTAMP BUILD_DAY "%d")
string(TIMESTAMP BUILD_HOUR "%H")
string(TIMESTAMP BUILD_MINUTE "%M")
string(TIMESTAMP BUILD_SECOND "%S")
string(RANDOM LENGTH 6 ALPHABET 0123456789ABCDEF BUILD_RANDOM)
set(BUILD_NUM "${BUILD_YEAR}:${BUILD_MONTH}:${BUILD_DAY}:${BUILD_HOUR}:${BUILD_MINUTE}:${BUILD_SECOND}-${BUILD_RANDOM}")
add_definitions(-DBUILD_NUMBER="${BUILD_NUM}")

file(GLOB_RECURSE LLLC_SOURCES
  "src/*.c"
  "src/keywords/*.c"
)

add_executable(lllc ${LLLC_SOURCES})

target_include_directories(lllc PRIVATE include src src/keywords)

target_link_libraries(lllc LLVM)

if(WIN32)
  target_link_libraries(lllc ws2_32)
else()
  target_link_libraries(lllc dl m pthread)
endif()

install(TARGETS lllc DESTINATION bin)
