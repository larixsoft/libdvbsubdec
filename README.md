# libdvbsubdec - DVB Subtitle Decoder Library

A C library for decoding DVB (Digital Video Broadcasting) subtitles according to ETSI EN 300 743.

## Overview

Complete implementation of DVB subtitle decoding with pixel-level rendering and multiple color format support.

## Quick Start

```bash
mkdir build && cd build
cmake -DBUILD_APP=ON ..
make
```

## Build Options

```bash
# Build SDL demo app (default)
cmake -DBUILD_APP=ON ..

# Debug build
cmake -DBUILD_APP=ON -DCMAKE_BUILD_TYPE=Debug ..
```

### Memory Allocator Selection

The library supports three memory allocator modes via CMake options:

| Option | Description | Best For |
| :--- | :--- | :--- |
| Default (no flags) | OS malloc with size tracking wrapper | General use |
| `-DUSE_SYSTEM_MALLOC=ON` | Direct system malloc/free (fastest) | Desktop/server applications |
| `-DUSE_INTERNAL_HEAP=ON` | Custom internal heap with bounded memory | Embedded/constrained systems |

```bash
# Recommended for desktop/server (fastest performance)
cmake -DBUILD_APP=ON -DUSE_SYSTEM_MALLOC=ON ..

# For embedded systems with memory constraints
cmake -DBUILD_APP=ON -DUSE_INTERNAL_HEAP=ON ..
```

**Memory Allocator Comparison:**

| Feature | System Malloc | Internal Heap |
| :--- | :--- | :--- |
| **Performance** | Best (SIMD, thread-local caching) | Slower (custom implementation) |
| **Memory limit** | No hard limit | Fixed heap size |
| **Fragmentation** | System-managed | Custom algorithm |
| **Overhead** | ~8-16 bytes per allocation | ~4-8 bytes per allocation |
| **Use case** | Desktop/server apps | Set-top boxes, embedded devices |

## Demo Applications

### SDL Player

```bash
# From build directory
./bin/dvbplayer_sdl ../demo_app/samples/490000000_subtitle_pid_205.pes
```

- Hardware-accelerated SDL2 rendering
- Manual timer system (thread-safe)
- Reference implementation for integration

### Qt Player

```bash
# From build directory
./bin/dvbplayer ../demo_app/samples/490000000_subtitle_pid_205.pes
```

- Qt6-based GUI with playback controls
- QTimer-based decoder integration (thread-safe)
- Requires Qt6 (Core, Gui, Widgets modules)

## Project Structure

```text
libdvbsubdec/
├── CMakeLists.txt
├── libdvbsubdec/
│   └── src/                      # Core library source
└── demo_app/
    ├── demo_app_sdl/             # SDL2 demo player
    ├── demo_app_qt/              # Qt6 demo player
    └── samples/                  # Test PES files
```

## Thread Safety

This library is **thread-safe** with the following guarantees:

- Multiple service instances can be used concurrently from different threads
- All API functions are protected by internal mutexes
- Different services operate independently without blocking each other

**Important:**

- Application callbacks (drawPixmapFunc, cleanRegionFunc, etc.) are invoked with the service mutex held
- Callbacks **MUST NOT** call back into the library to avoid deadlock
- Callbacks should be non-blocking to avoid holding locks for extended periods

**Integration recommendations:**

- **SDL: DON'T use SDL_AddTimer** - callbacks run in separate thread with mutex held
- **SDL: DO use manual timer system** - check timers in main event loop
- **Qt: Use QTimer** - callbacks run in main thread via Qt event loop (safe)

## Critical Integration Notes

**Shutdown order is critical:**

1. Stop any timers
2. Stop decoder service (`LS_DVBSubDecServiceStop`)
3. Delete decoder service (`LS_DVBSubDecServiceDelete`)
4. Finalize library (`LS_DVBSubDecFinalize`)

## API Usage Example

```c
#include "lssubdec.h"

// 1. Initialize library with system functions
LS_SystemFuncs_t sysFuncs = {
    .mutexCreateFunc = my_mutex_create,
    .mutexDeleteFunc = my_mutex_delete,
    .mutexWaitFunc = my_mutex_wait,
    .mutexSignalFunc = my_mutex_signal,
    .timerCreateFunc = my_timer_create,
    .timerDeleteFunc = my_timer_delete,
    .timerStartFunc = my_timer_start,
    .timerStopFunc = my_timer_stop,
    .getTimeStampFunc = my_get_timestamp
};
LS_DVBSubDecInit(1024 * 1024, sysFuncs);

// 2. Create a subtitle service with adequate memory
LS_ServiceMemCfg_t memCfg = {
    .codedDataBufferSize = 512 * 1024,
    .pixelBufferSize = 16 * 1024 * 1024,
    .compositionBufferSize = 2 * 1024 * 1024
};
LS_ServiceID_t service = LS_DVBSubDecServiceNew(memCfg);

// 3. Setup rendering callbacks
LS_OSDRender_t osdRender = {
    .cleanRegionFunc = my_clean_region_callback,
    .cleanRegionFuncData = my_user_data,
    .drawPixmapFunc = my_draw_pixmap_callback,
    .drawPixmapFuncData = my_user_data,
    .ddsNotifyFunc = my_dds_notify_callback,
    .ddsNotifyFuncData = my_user_data,
    .getCurrentPCRFunc = my_get_pcr_callback,
    .getCurrentPCRFuncData = my_user_data,
    .OSDPixmapFormat = LS_PIXFMT_ARGB32,
    .alphaValueFullTransparent = 0,
    .alphaValueFullOpaque = 255
};
LS_DVBSubDecServiceStart(service, osdRender);

// 4. Process PES data
LS_CodedData_t pesData = { .data = pes_buffer, .dataSize = pes_size };
LS_PageId_t pageId = { .compositionPageId = 0, .ancillaryPageId = 0 };  // 0 = wildcard
LS_DVBSubDecServicePlay(service, &pesData, pageId);  // Note: pageId is by value, pesData is by pointer

// 5. Cleanup (in correct order!)
LS_DVBSubDecServiceStop(service);
LS_DVBSubDecServiceDelete(service);
LS_DVBSubDecFinalize();
```

## Installing with CMake

After installation, you can use `find_package()` in your CMake project:

```cmake
find_package(dvbsubdec REQUIRED)

target_link_libraries(your_target PRIVATE dvbsubdec::dvbsubdec)

target_include_directories(your_target PRIVATE dvbsubdec::dvbsubdec)
```

## Integration Guides

- **[demo_app/demo_app_sdl/src/main.c](demo_app/demo_app_sdl/src/main.c)** - SDL integration guide
- **[demo_app/demo_app_qt/src/main.cpp](demo_app/demo_app_qt/src/main.cpp)** - Qt integration guide

## API Reference

### Main Functions

- `LS_DVBSubDecInit(bufferSize, sysFuncs)` - Initialize library (REQUIRED)
- `LS_DVBSubDecFinalize()` - Clean up library resources
- `LS_DVBSubDecServiceNew(memCfg)` - Create a new subtitle service
- `LS_DVBSubDecServiceDelete(service)` - Delete a subtitle service
- `LS_DVBSubDecServiceStart(service, osdRender)` - Start with callbacks
- `LS_DVBSubDecServicePlay(service, &pesData, pageId)` - Process PES data (note: &pesData, pageId)
- `LS_DVBSubDecServiceStop(service)` - Stop the service

### Required Callbacks

You must provide these callbacks in `LS_OSDRender_t`:

- `cleanRegionFunc` - Clear rectangular area before drawing new content
- `drawPixmapFunc` - Draw subtitle pixmap (ARGB32 or PALETTE8BIT format)

### Optional Callbacks

- `ddsNotifyFunc` - Receive display dimensions (Display Definition Segment)
- `getCurrentPCRFunc` - Provide current PCR timestamp

## Requirements

- CMake 3.10+
- C99 compiler (gcc, clang)
- SDL2 (for SDL demo app)
- Qt6 (for Qt demo app, optional)

## Standards Compliance

- **ETSI EN 300 743**: DVB Subtitling systems
- **ISO/IEC 13818-1**: MPEG-2 Systems (PES format)

## Supported Features

- All segment types (PCS, RCS, CLUT, ODS, DDS)
- 2/4/8-bit palettes, ARGB32, RGBA32
- Multiple regions, scrolling, clipping
- Transparency and alpha blending

## License

See source file headers for LGPL 2.1 license information.

## Resources

- [ETSI EN 300 743](https://www.etsi.org/) specification
- [DVB Subtitles Wikipedia](https://en.wikipedia.org/wiki/DVB_subtitling)
