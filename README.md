# GStreamer NvImageSrc Plugin

A high-performance GStreamer plugin for screen capture using NVIDIA Frame Buffer Capture (NvFBC) API with hardware H.264 encoding via NVIDIA Video Encode API (NVENC).

## Features

- **Ultra-low latency screen capture** using NVIDIA NvFBC Direct Capture
- **Hardware H.264 encoding** with NVENC for maximum performance
- **Optimized for real-time streaming** with minimal CPU overhead
- **GStreamer integration** for easy pipeline creation
- **Multi-threaded architecture** for optimal performance
- **Automatic resolution and format handling**

## Requirements

### Hardware
- NVIDIA GPU with Kepler architecture or newer
- GPU driver version 470.x or newer
- Display connected to NVIDIA GPU

### Software
- Linux operating system
- GStreamer 1.0+ development libraries
- NVIDIA CUDA Toolkit
- X11 development libraries
- OpenGL development libraries

### NVIDIA Libraries
- `libnvidia-fbc` - Frame Buffer Capture library
- `libnvidia-encode` - Video Encode library
- `libcuda` - CUDA runtime library

## Build Instructions

### Prerequisites
```bash
# Ubuntu/Debian
sudo apt update
sudo apt install build-essential pkg-config
sudo apt install libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev
sudo apt install libx11-dev libgl1-mesa-dev
sudo apt install nvidia-cuda-toolkit

# Install GStreamer to /opt/gstreamer (expected by build script)
# Or modify build.sh with your GStreamer installation path
```

### Compilation
```bash
cd nvimage/
chmod +x build.sh
./build.sh
```

This will create `libgstnvimagesrc.so` plugin file.

### Installation
```bash
# Copy plugin to GStreamer plugins directory
sudo cp libgstnvimagesrc.so /usr/lib/x86_64-linux-gnu/gstreamer-1.0/

# Or use GST_PLUGIN_PATH environment variable
export GST_PLUGIN_PATH=$PWD:$GST_PLUGIN_PATH
```

## Usage

### Basic Pipeline
```bash
# Simple screen capture to file
gst-launch-1.0 nvimagesrc ! filesink location=screen.h264

# Screen capture with custom bitrate and framerate
gst-launch-1.0 nvimagesrc bitrate=5000000 fps=60 ! filesink location=screen.h264

# Live streaming via UDP
gst-launch-1.0 nvimagesrc fps=30 bitrate=2000000 ! \
    udpsink host=192.168.1.100 port=5000
```

### Advanced Pipeline Examples
```bash
# RTMP streaming
gst-launch-1.0 nvimagesrc fps=60 bitrate=6000000 ! \
    h264parse ! flvmux ! \
    rtmpsink location=rtmp://live.twitch.tv/live/YOUR_STREAM_KEY

# WebRTC streaming preparation
gst-launch-1.0 nvimagesrc fps=30 bitrate=1500000 ! \
    h264parse ! rtph264pay ! \
    udpsink host=127.0.0.1 port=5004

# Multiple outputs
gst-launch-1.0 nvimagesrc fps=60 bitrate=8000000 ! tee name=t \
    t. ! queue ! filesink location=recording.h264 \
    t. ! queue ! udpsink host=192.168.1.100 port=5000
```

## Plugin Properties

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `display-name` | string | NULL | X11 display name (e.g., ":0") |
| `bitrate` | uint | 2000000 | Video bitrate in bits per second |
| `fps` | double | 25.0 | Target framerate |
| `show-pointer` | boolean | TRUE | Include mouse cursor in capture |

### Property Examples
```bash
# High quality capture
gst-launch-1.0 nvimagesrc bitrate=10000000 fps=60 show-pointer=false ! \
    filesink location=high_quality.h264

# Low latency streaming
gst-launch-1.0 nvimagesrc bitrate=1000000 fps=30 ! \
    udpsink host=127.0.0.1 port=5000
```

## Performance Optimization

### Direct Capture Mode
The plugin automatically enables NVIDIA Direct Capture when possible for minimal latency:
- Requires fullscreen unoccluded applications
- Cursor must be disabled (`show-pointer=false`)
- Push model is automatically enabled
- Monitor logs for "Direct Capture ACTIVE" messages

### Optimal Settings
```bash
# Gaming/Desktop capture (high quality)
nvimagesrc fps=60 bitrate=8000000 show-pointer=false

# Remote desktop (balanced)
nvimagesrc fps=30 bitrate=2000000 show-pointer=true

# Streaming (low latency)
nvimagesrc fps=30 bitrate=1500000 show-pointer=false
```

## Troubleshooting

### Common Issues

1. **Plugin not found**
   ```bash
   # Check plugin installation
   gst-inspect-1.0 nvimagesrc
   
   # If not found, check GST_PLUGIN_PATH
   export GST_PLUGIN_PATH=/path/to/plugin:$GST_PLUGIN_PATH
   ```

2. **Permission denied**
   ```bash
   # User needs access to GPU and X display
   sudo usermod -a -G video $USER
   # Re-login required
   ```

3. **Black screen capture**
   - Check if display is connected to NVIDIA GPU
   - Verify X11 is running on NVIDIA GPU
   - Check compositor settings

4. **High CPU usage**
   - Ensure hardware encoding is working
   - Check that Direct Capture is active
   - Reduce framerate or bitrate

### Debug Information
```bash
# Enable debug output
GST_DEBUG=nvimagesrc:5 gst-launch-1.0 nvimagesrc ! fakesink

# Check Direct Capture status
GST_DEBUG=nvimagesrc:4 gst-launch-1.0 nvimagesrc show-pointer=false ! fakesink
```

## Architecture

### Components
- **gstnvimagesrc.c**: Main GStreamer plugin implementation
- **nvimageutil.c**: Core capture and encoding utilities
- **NvFBC integration**: Screen capture using NVIDIA Frame Buffer Capture
- **NVENC integration**: Hardware H.264 encoding
- **Multi-threading**: Separate worker thread for GPU operations

### Data Flow
1. NvFBC captures screen framebuffer to OpenGL texture
2. Texture registered with NVENC as input resource
3. NVENC encodes frame to H.264 bitstream
4. Encoded data wrapped in GStreamer buffer
5. Buffer pushed to downstream elements

## License

This library is free software; you can redistribute it and/or modify it under the terms of the GNU Library General Public License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.

## Contributing

1. Fork the repository
2. Create feature branch
3. Make changes with English comments
4. Test thoroughly
5. Submit pull request

## Authors

- Original GStreamer integration: Luca Ognibene
- Screen capture implementation: Zaheer Merali  
- NVIDIA integration and optimizations: Lukas Hejtmanek

## See Also

- [GStreamer Documentation](https://gstreamer.freedesktop.org/documentation/)
- [NVIDIA Video Codec SDK](https://developer.nvidia.com/nvidia-video-codec-sdk)
- [NVIDIA Frame Buffer Capture](https://docs.nvidia.com/capture-sdk/)