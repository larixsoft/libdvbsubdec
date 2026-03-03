cmake_minimum_required(VERSION 3.10)

project(dvbsubdec VERSION 1.0.0 LANGUAGES C CXX)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Set CXX standard for Qt
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable position independent code for shared library
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Compiler options
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Options for building components
option(BUILD_APP "Build DVB subtitle player applications" ON)

# Memory allocator selection
option(USE_SYSTEM_MALLOC "Use system malloc/free instead of custom heap (recommended for desktop/server)" OFF)
option(USE_INTERNAL_HEAP "Use custom internal heap (for embedded/constrained systems)" OFF)

# Apply memory allocator selection
if(USE_SYSTEM_MALLOC)
    add_compile_definitions(LS_USE_SYSTEM_MALLOC)
    message(STATUS "Memory allocator: SYSTEM malloc (fastest, no heap limit)")
elseif(USE_INTERNAL_HEAP)
    add_compile_definitions(LS_INTERNAL_HEAP)
    message(STATUS "Memory allocator: CUSTOM internal heap (bounded, for embedded)")
else()
    # Default: OS malloc with size tracking wrapper
    message(STATUS "Memory allocator: OS malloc with tracking wrapper (default)")
endif()

# Set output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Include subdirectories
add_subdirectory(libdvbsubdec)

if(BUILD_APP)
    add_subdirectory(demo_app/demo_app_sdl)
    # Qt app requires Qt6 - only add if Qt6 is found
    find_package(Qt6 QUIET COMPONENTS Core Gui Widgets)
    if(Qt6_FOUND)
        add_subdirectory(demo_app/demo_app_qt)
    endif()
endif()

# Installation
include(GNUInstallDirs)

# Print build configuration
message(STATUS "")
message(STATUS "DVB Subtitle Decoder Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Build apps: ${BUILD_APP}")
message(STATUS "  Qt6 found: ${Qt6_FOUND}")
message(STATUS "")
