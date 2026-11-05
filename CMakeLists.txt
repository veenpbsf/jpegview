cmake_minimum_required(VERSION 3.24)

# The bundled static libraries use the static MSVC CRT, as do the vcxproj files.
if(NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
project(JPEGView LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "JPEGView is a Windows desktop application. Use a Windows toolchain.")
endif()
if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(FATAL_ERROR "Use a separate build directory, for example: cmake -S . -B out/build/release")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/$<CONFIG>")

option(JPEGVIEW_BUILD_APP "Build JPEGView and WICLoader (MSVC + ATL required)" ON)
option(JPEGVIEW_BUILD_EXPLORER_TESTS "Build the standalone Explorer-order tests, when present" ON)

if(JPEGVIEW_BUILD_APP)
    if(NOT MSVC)
        message(FATAL_ERROR
            "The JPEGView application requires the Visual Studio/MSVC toolchain, ATL and a Windows SDK. "
            "In CLion select Settings > Build, Execution, Deployment > Toolchains > Visual Studio. "
            "The bundled MinGW compiler can only build the standalone Explorer tests: "
            "configure with -DJPEGVIEW_BUILD_APP=OFF.")
    endif()
    if(CMAKE_CXX_COMPILER_ARCHITECTURE_ID MATCHES "ARM")
        message(FATAL_ERROR "The bundled codec libraries support x64 and Win32, not ARM/ARM64.")
    endif()
    enable_language(RC)
    include(cmake/JPEGViewDependencies.cmake)
    # Bundled codecs use the dynamic CRT even though JPEGView itself uses /MT.
    # Collect the official redistributable DLLs for app-local deployment.
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
    include(InstallRequiredSystemLibraries)
endif()

# This optional module is also a real CLion target. It has its own Win7 SDK
# settings and must never inherit the application's XP-targeting stdafx.h.
if(EXISTS "${PROJECT_SOURCE_DIR}/src/JPEGView/ExplorerOrder.cpp")
    add_library(ExplorerOrder STATIC
        src/JPEGView/ExplorerOrder.cpp
        src/JPEGView/ExplorerOrder.h
        src/JPEGView/ExplorerOrderPolicy.h)
    target_compile_features(ExplorerOrder PUBLIC cxx_std_17)
    target_include_directories(ExplorerOrder PUBLIC "${PROJECT_SOURCE_DIR}/src/JPEGView")
    target_link_libraries(ExplorerOrder PUBLIC ole32 oleaut32 shell32 shlwapi uuid)
    if(MSVC)
        target_compile_options(ExplorerOrder PRIVATE /W3)
    endif()
endif()

if(JPEGVIEW_BUILD_APP)
    add_subdirectory(src/WICLoader)
    add_subdirectory(src/JPEGView)
endif()

if(JPEGVIEW_BUILD_EXPLORER_TESTS AND TARGET ExplorerOrder
   AND EXISTS "${PROJECT_SOURCE_DIR}/tests/ExplorerOrderTests.cpp")
    enable_testing()
    add_subdirectory(tests)
elseif(NOT JPEGVIEW_BUILD_APP)
    message(FATAL_ERROR "No build target selected: standalone Explorer-order sources/tests were not found.")
endif()
