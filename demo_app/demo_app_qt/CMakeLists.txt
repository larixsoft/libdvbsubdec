cmake_minimum_required(VERSION 3.10)

# DVB Subtitle Player Application (Qt-based)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

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

# Add executable
add_executable(dvbplayer
    src/main.cpp
    src/MainWindow.cpp
    src/SubtitleWidget.cpp
)

# Link against Qt and the library
target_link_libraries(dvbplayer
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    ${DVBSUBDEC_TARGET}
)

# Include the library headers (from libdvbsubdec/src directory)
target_include_directories(dvbplayer
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libdvbsubdec/src
)

# Enable Qt's MOC, UIC and RCC processors
set_target_properties(dvbplayer PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)

# Set output directory
set_target_properties(dvbplayer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Installation
install(TARGETS dvbplayer
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
