# SDL DVB Subtitle Player
cmake_minimum_required(VERSION 3.10)

# Find SDL2
find_package(SDL2 REQUIRED)

# Get SDL2 include directories from the imported target
get_target_property(SDL2_INCLUDE_DIRS SDL2::SDL2 INTERFACE_INCLUDE_DIRECTORIES)

# Handle different SDL2 CMake configurations
if(SDL2_INCLUDE_DIRS)
    # Some SDL2 configs point to include/SDL2, we need include/
    foreach(SDL_INC ${SDL2_INCLUDE_DIRS})
        if(SDL_INC MATCHES "/SDL2/?$")
            # Get parent directory (the actual include dir)
            get_filename_component(SDL_INC_PARENT ${SDL_INC} DIRECTORY)
            list(APPEND SDL2_INCLUDE_DIRS_FIXED ${SDL_INC_PARENT})
        else()
            list(APPEND SDL2_INCLUDE_DIRS_FIXED ${SDL_INC})
        endif()
    endforeach()
    set(SDL2_INCLUDE_DIRS ${SDL2_INCLUDE_DIRS_FIXED})
endif()

# Fallback to manual search if the imported target doesn't provide includes
if(NOT SDL2_INCLUDE_DIRS)
    find_path(SDL2_INCLUDE_DIR NAMES SDL2/SDL.h
        PATH_SUFFIXES include
    )
    if(SDL2_INCLUDE_DIR)
        get_filename_component(SDL2_INCLUDE_DIRS ${SDL2_INCLUDE_DIR} DIRECTORY)
    endif()
endif()

message(STATUS "Found SDL2 includes: ${SDL2_INCLUDE_DIRS}")

# Link against the dvbsubdec library (built by parent CMake)
# The library target is defined in the parent directory's CMakeLists.txt
if(TARGET dvbsubdec::dvbsubdec)
    # Use the imported target from the parent project
    set(DVBSUBDEC_TARGET dvbsubdec::dvbsubdec)
elseif(TARGET dvbsubdec)
    # Fallback to the direct target
    set(DVBSUBDEC_TARGET dvbsubdec)
else()
    message(FATAL_ERROR "Cannot find libdvbsubdec target - please ensure the library is built first")
endif()

# Source files
set(SOURCES
    src/main.c
    src/sdl_player.c
    src/subtitle_renderer.c
    src/pes_queue.c
)

# Header files
set(HEADERS
    src/sdl_player.h
    src/subtitle_renderer.h
    src/pes_queue.h
)

# Create executable
add_executable(dvbplayer_sdl ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(dvbplayer_sdl
    PRIVATE
    SDL2::SDL2
    ${DVBSUBDEC_TARGET}
    pthread
)

# Include the library headers (from libdvbsubdec/src directory)
target_include_directories(dvbplayer_sdl
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libdvbsubdec/src
    ${SDL2_INCLUDE_DIRS}
)

# Set output directory
set_target_properties(dvbplayer_sdl PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Installation
install(TARGETS dvbplayer_sdl
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
