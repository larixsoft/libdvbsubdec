# SDL DVB Subtitle Player
cmake_minimum_required(VERSION 3.10)

# Find SDL2
find_package(SDL2 REQUIRED)

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
)

# Set output directory
set_target_properties(dvbplayer_sdl PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Installation
install(TARGETS dvbplayer_sdl
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
