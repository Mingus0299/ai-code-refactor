cmake_minimum_required(VERSION 3.22)
project(AICodeRefactor LANGUAGES C CXX)
# or equivalently:
# project(AICodeRefactor)
# enable_language(C)
# enable_language(CXX)


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ENABLE_ONNXRUNTIME "Enable ONNX Runtime integration" OFF)

# --- LLVM + Clang -----------------------------------------------------------
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION} at ${LLVM_INSTALL_PREFIX}")
message(STATUS "Using Clang from ${Clang_DIR}")

# LLVM/Clang require these defs/includes
include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# --- ONNX Runtime (optional) -----------------------------------------------
if(ENABLE_ONNXRUNTIME)
  # Try to locate headers/lib via ONNXRUNTIME_ROOT env or cmake cache var
  if(NOT DEFINED ONNXRUNTIME_ROOT AND DEFINED ENV{ONNXRUNTIME_ROOT})
    set(ONNXRUNTIME_ROOT $ENV{ONNXRUNTIME_ROOT})
  endif()

  find_path(ONNXRUNTIME_INCLUDE_DIR onnxruntime_c_api.h
            HINTS ${ONNXRUNTIME_ROOT} PATH_SUFFIXES include)
  find_library(ONNXRUNTIME_LIB NAMES onnxruntime
               HINTS ${ONNXRUNTIME_ROOT} PATH_SUFFIXES lib lib64)

  if(ONNXRUNTIME_INCLUDE_DIR AND ONNXRUNTIME_LIB)
    message(STATUS "ONNX Runtime found: ${ONNXRUNTIME_LIB}")
    add_compile_definitions(ENABLE_ONNXRUNTIME=1)
    set(HAVE_ONNX_RUNTIME TRUE)
  else()
    message(WARNING "ONNX Runtime not found; disabling ENABLE_ONNXRUNTIME")
    set(HAVE_ONNX_RUNTIME FALSE)
  endif()
endif()

# --- Sources ----------------------------------------------------------------
set(SRC
    src/main.cpp
    src/ai/AiEngine.cpp
    src/analyzers/CppAnalyzer.cpp
    src/refactor/RefactorEngine.cpp
)

if(HAVE_ONNX_RUNTIME)
    list(APPEND SRC src/ai/OnnxAiEngine.cpp)
endif()

add_executable(aicr ${SRC})

target_include_directories(aicr PRIVATE include)

# Link against Clang Tooling libraries
target_link_libraries(aicr
  PRIVATE
    clangTooling
    clangBasic
    clangASTMatchers
)

if(HAVE_ONNX_RUNTIME)
  target_include_directories(aicr PRIVATE ${ONNXRUNTIME_INCLUDE_DIR})
  target_link_libraries(aicr PRIVATE ${ONNXRUNTIME_LIB})
endif()

# On UNIX, prefer libc++ if using Apple Clang
if(APPLE)
  target_compile_options(aicr PRIVATE -Wall -Wextra -Wpedantic)
  target_link_options(aicr PRIVATE)
else()
  target_compile_options(aicr PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Export compile_commands for this tool itself (useful when we analyze our own code)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
