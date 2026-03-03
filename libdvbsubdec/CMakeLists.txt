cmake_minimum_required(VERSION 3.10)

project(dvbsubdec VERSION 1.0.0 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Enable position independent code for shared library
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Compiler options
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Source files
set(SOURCES
    src/lsptsmgr.c
    src/lslist.c
    src/lspqueue.c
    src/lssubdecoder.c
    src/lssuben300743.c
    src/lssubgfx.c
    src/lssubpes.c
    src/lssubutils.c
    src/lssystem.c
    src/lssubport.c
    src/lssubsegparser.c
    src/lsmemory.c
    src/lssubdec.c
    src/lssubpixmap.c
    src/lssubsegdec.c
    src/lssubconverter.c
    src/lssubdisplay.c
)

# Header files (for IDE support and PCH)
set(HEADERS
    src/lsptsmgr.h
    src/lslist.h
    src/lspqueue.h
    src/lssubdecoder.h
    src/lssuben300743.h
    src/lssubgfx.h
    src/lssubpes.h
    src/lssubutils.h
    src/lssystem.h
    src/lssubport.h
    src/lssubsegparser.h
    src/lsmemory.h
    src/lssubdec.h
    src/lssubpixmap.h
    src/lssubsegdec.h
    src/lssubconverter.h
    src/lssubdisplay.h
    src/lssubmacros.h
    src/lssubver.h
)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# Define a job pool for static library linking to avoid parallel ar race conditions
set_property(GLOBAL PROPERTY JOB_POOLS ar_pool=1)

# Create static library
add_library(dvbsubdec_static STATIC ${SOURCES} ${HEADERS})
set_target_properties(dvbsubdec_static PROPERTIES
    OUTPUT_NAME dvbsubdec
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    JOB_POOL_LINK ar_pool
)

# Create shared library
add_library(dvbsubdec SHARED ${SOURCES} ${HEADERS})
set_target_properties(dvbsubdec PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

# Alias for default library (shared)
add_library(dvbsubdec::dvbsubdec ALIAS dvbsubdec)
add_library(dvbsubdec::dvbsubdec_static ALIAS dvbsubdec_static)

# Installation rules
include(GNUInstallDirs)

install(TARGETS dvbsubdec dvbsubdec_static
    EXPORT dvbsubdecTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install headers
install(FILES ${HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/dvbsubdec
)

# Create and install export targets
install(EXPORT dvbsubdecTargets
    FILE dvbsubdecTargets.cmake
    NAMESPACE dvbsubdec::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dvbsubdec
)

# Create config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/dvbsubdecConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/dvbsubdecConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/dvbsubdecConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dvbsubdec
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/dvbsubdecConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/dvbsubdecConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dvbsubdec
)
