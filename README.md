# ESP32_AnimatedGIF Library

An optimized Animated GIF decoder for ESP32 with Arduino framework, refactored from the original Animated GIF decoder by Larry Bank.

## Features

- **ESP32 Optimized**: Specifically designed for ESP32 with PSRAM support
- **Multiple Sources**: Load GIFs from memory, SPIFFS, or SD card
- **Flexible Rendering**: Direct pixel callbacks or frame buffer modes
- **Scalable Output**: Automatic scaling to fit display
- **Memory Efficient**: Smart memory management with PSRAM support
- **Multiple Formats**: RGB565, RGB888, ARGB8888, Grayscale, Monochrome
- **Full GIF Support**: Transparency, disposal methods, interlacing

## Installation

### Arduino IDE
1. Download the library as ZIP
2. In Arduino IDE: Sketch → Include Library → Add .ZIP Library
3. Restart Arduino IDE

### PlatformIO
Add to `platformio.ini`:
```ini
lib_deps = 
    https://github.com/deepseek-ai/ESP32_AnimatedGIF.git
```

## Dependencies

- **TFT_eSPI** (for display output)
- **SPIFFS** (for file system access)
- **SD** (for SD card support, optional)

## Quick Examples

### Basic Memory Player
```cpp
#include <ESP32_AnimatedGIF.h>
#include <TFT_eSPI.h>

ESP32_AnimatedGIF gif;
TFT_eSPI tft;

const uint8_t gifData[] = { /* GIF data */ };

void setup() {
    tft.begin();
    gif.begin();
    gif.loadFromMemory(gifData, sizeof(gifData));
}

void loop() {
    gif.nextFrame(true);
}
```

### SPIFFS File Player
```cpp
#include <ESP32_AnimatedGIF.h>
#include <SPIFFS.h>

File gifFile;
ESP32_AnimatedGIF gif;

void setup() {
    SPIFFS.begin();
    gifFile = SPIFFS.open("/animation.gif");
    gif.begin();
    gif.load(sdCardReader, &gifFile);
}
```

## API Reference

### Core Functions
- `begin()` - Initialize decoder
- `loadFromMemory()` - Load GIF from array
- `load()` - Load with custom reader
- `nextFrame()` - Decode and display next frame
- `reset()` - Restart animation
- `getInfo()` - Get GIF information

### Configuration
- `setDisplaySize()` - Set output dimensions
- `setPixelCallback()` - Set pixel drawing function
- `setLoop()` - Enable/disable looping
- `setScale()` - Set scaling factor

### Information
- `getCurrentFrame()` - Current frame number
- `getFrameCount()` - Total frames
- `getCanvasWidth/Height()` - GIF dimensions
- `getLastError()` - Last error code
- `getErrorMessage()` - Error description

## Examples Included

1. **BasicGIFPlayer** - Simple memory-based player
2. **SPIFFS_GIFPlayer** - Play from SPIFFS file system
3. **SDCard_GIFPlayer** - Play from SD card with scaling
4. **SDCard_GIFPlayer_Arduino_GFX** - Play from SD card with scaling (use Arduino_GFX lib for display)

## Performance Tips

1. **Use PSRAM**: Enable with `begin(PixelFormat::RGB565_LE, true)`
2. **Match Display**: Scale GIFs to match display resolution
3. **Optimize GIFs**: Reduce colors, frames, and dimensions
4. **Direct Drawing**: Use pixel callbacks for best performance
5. **Disable Features**: Turn off unused features to save memory

## Memory Usage

| Resolution | RGB565 | RGB888 | With PSRAM |
|------------|--------|--------|------------|
| 240x240 | 115KB | 172KB | Yes |
| 320x240 | 153KB | 230KB | Yes |
| 360x360 | 259KB | 388KB | Recommended |
| 480x320 | 307KB | 461KB | Required |

## Error Handling

Check `getLastError()` and use `getErrorMessage()` for debugging:
```cpp
GIFError error = gif.nextFrame();
if (error != GIFError::SUCCESS) {
    Serial.println(gif.getErrorMessage(error));
}
```

## Troubleshooting

**Q: Out of memory errors**  
A: Enable PSRAM or reduce GIF resolution

**Q: Slow playback**  
A: Reduce GIF complexity or disable transparency

**Q: No display output**  
A: Check pixel callback is set and display initialized

**Q: File not found**  
A: Ensure file is uploaded to SPIFFS/SD card

## License

Apache License 2.0

## Credits

Refactored from Animated GIF decoder by Larry Bank (bitbank@pobox.com)  
Maintained by Deepseek