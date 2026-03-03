# DVB Subtitle Player

A Qt-based GUI application for displaying DVB subtitles using the libdvbsubdec library.

## Features

- **Graphical Display**: Real DVB subtitle rendering using Qt
- **Multiple Formats**: Supports all DVB subtitle pixel formats
  - 2-bit, 4-bit, 8-bit palettes
  - ARGB32, RGBA32, BGRA32
  - YUV420
- **Playback Controls**: Play, pause, stop, seek, and step frame-by-frame
- **Visual Statistics**: Real-time frame and subtitle counters
- **Adjustable Display**: Scale to any window size
- **Keyboard Shortcuts**: Full keyboard control for playback

## Building

### Prerequisites

- Qt6 (Core, Gui, Widgets modules)
- CMake 3.10+
- C99 compiler

On Ubuntu/Debian:
```bash
sudo apt-get install qt6-base-dev qt6-tools-dev cmake build-essential
```

On Fedora:
```bash
sudo dnf install qt6-qtbase-devel cmake gcc-c++
```

### Build Steps

```bash
cd build
cmake ..
cmake --build .
```

The `dvbplayer` executable will be in `build/bin/`.

## Usage

### Quick Start

```bash
# 1. Generate a test PES file
cd examples
python3 generate_test_pes.py test_subs.pes -n 10

# 2. Run the player
cd ../build/bin
./dvbplayer ../../examples/test_subs.pes
```

### Command Line Options

```
Usage: dvbplayer [options] <pes_file>

Options:
  -v, --verbose     Verbose mode (show decoder events)
  -w, --width WIDTH  Display width (default: 720)
  -h, --height H    Display height (default: 576)
  -l, --loop        Loop playback
  --speed SPEED      Playback speed multiplier (default: 1.0)
```

### Examples

```bash
# Normal playback
./dvbplayer subtitles.pes

# Verbose mode to see decoder internals
./dvbplayer -v subtitles.pes

# HD display size
./dvbplayer -w 1920 -h 1080 subtitles.pes

# Slow motion playback (0.5x speed)
./dvbplayer --speed 0.5 subtitles.pes

# Loop continuously
./dvbplayer -l subtitles.pes
```

## Controls

| Keyboard | Action |
|----------|--------|
| **Space** | Play/Pause |
| **Esc** | Stop and reset |
| **Right/Down** | Next frame |
| **O** | Open file |
| **Q** | Quit |

## Features in Detail

### Pixel Format Support

The player handles all common DVB subtitle pixel formats:

- **Paletted formats** (2/4/8-bit): Converts palette indices to RGB using the CLUT
- **ARGB32**: Direct display
- **RGBA32/BGRA32**: Swaps byte order for Qt display
- **YUV420**: Full YUV to RGB conversion (ITU-R BT.601)

### Display Scaling

Subtitles are automatically scaled to fit the display window while maintaining aspect ratio.

### Playback Features

- **Progress bar**: Shows current position in the file
- **Frame counter**: Displays total frames processed
- **Subtitle counter**: Shows number of subtitles rendered
- **Real-time seeking**: Drag the slider to any position
- **Variable speed**: Slow down or speed up playback

## Screenshots

The application shows:
- Black background (standard for subtitles)
- Subtitle overlay at the correct position
- Border around subtitle area (for debugging)
- Toolbar with playback controls
- Status bar with statistics

## Troubleshooting

### Qt not found
```bash
# Set Qt6 path manually
cmake -DCMAKE_PREFIX_PATH=/usr/lib/x86_64-linux-gnu/cmake/Qt6 ..
```

### No subtitles displayed
- Verify the PES file contains valid DVB subtitle data
- Use the `analyze_samples.py` script to check file format
- Try verbose mode (`-v`) to see decoder messages in the console

### Building fails
```bash
# Clean and rebuild
cd build
rm -rf *
cmake ..
cmake --build .
```

## Requirements for PES Files

The player expects PES files with:
- PES start code: `00 00 01 BD` (private stream 1)
- DVB subtitle segments: `20 00 0F 10...` (page composition)
- Valid CLUT and object data

Use `generate_test_pes.py` to create valid test files.
