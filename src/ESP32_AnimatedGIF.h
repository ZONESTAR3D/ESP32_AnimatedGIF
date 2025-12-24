/**
 * @file ESP32_AnimatedGIF.h
 * @brief Animated GIF decoder optimized for ESP32 with Arduino framework
 * @author Deepseek
 * @version 1.0.0
 * @date 2024
 * 
 * Refactored from Animated GIF decoder by Larry Bank (bitbank@pobox.com)
 * Original Copyright 2020 BitBank Software, Inc.
 * Licensed under Apache License 2.0
 */

#ifndef ESP32_ANIMATED_GIF_H
#define ESP32_ANIMATED_GIF_H

#include <Arduino.h>
#include <stdint.h>
#include <string.h>

// Platform detection
#if defined(ESP32) || defined(ESP8266)
  #define ESP32_ANIMATEDGIF_ESP_PLATFORM
  #if defined(ESP32) && defined(PSRAM_ENABLE)
    #define ESP32_ANIMATEDGIF_PSRAM_SUPPORT
  #endif
#endif

// Configuration
#ifndef ESP32_ANIMATEDGIF_MAX_WIDTH
  #define ESP32_ANIMATEDGIF_MAX_WIDTH 800
#endif

#ifndef ESP32_ANIMATEDGIF_MAX_HEIGHT
  #define ESP32_ANIMATEDGIF_MAX_HEIGHT 600
#endif

// Error codes
enum class GIFError {
    SUCCESS = 0,
    DECODE_ERROR,
    FILE_TOO_WIDE,
    INVALID_PARAMETER,
    UNSUPPORTED_FEATURE,
    FILE_NOT_FOUND,
    EARLY_EOF,
    EMPTY_FRAME,
    BAD_FILE_FORMAT,
    OUT_OF_MEMORY,
    DISPLAY_NOT_SET,
    UNKNOWN_ERROR
};

// Pixel formats
enum class PixelFormat {
    RGB565_LE = 0,      // RGB565 little endian (default for TFT displays)
    RGB565_BE,          // RGB565 big endian
    RGB888,             // 24-bit RGB
    ARGB8888,           // 32-bit ARGB
    GRAYSCALE_8BIT,     // 8-bit grayscale
    MONOCHROME_1BIT     // 1-bit monochrome
};

// Disposal methods
enum class DisposalMethod {
    NONE = 0,           // No disposal specified
    KEEP,               // Do not dispose
    BACKGROUND,         // Restore to background color
    PREVIOUS            // Restore to previous
};

// GIF information structure
struct GIFInfo {
    uint16_t width;             // Canvas width
    uint16_t height;            // Canvas height
    uint16_t frameCount;        // Number of frames
    uint32_t totalDuration;     // Total animation duration (ms)
    uint16_t loopCount;         // Loop count (0 = infinite)
    bool hasTransparency;       // Has transparent color
    uint8_t backgroundColor;    // Background color index
    uint8_t transparentIndex;   // Transparent color index
};

// Frame information structure
struct FrameInfo {
    uint16_t x;                 // X position on canvas
    uint16_t y;                 // Y position on canvas
    uint16_t width;             // Frame width
    uint16_t height;            // Frame height
    uint16_t delay;             // Frame delay (ms)
    DisposalMethod disposal;    // Disposal method
    bool interlace;             // Interlaced image
};

// Callback function types
typedef void (*FrameCallback)(void* userData, uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t* pixels);
typedef void (*PixelCallback)(void* userData, uint16_t x, uint16_t y, uint16_t color);
typedef bool (*DataReader)(void* userData, uint8_t* buffer, uint32_t length, uint32_t position);

// Main GIF decoder class
class ESP32_AnimatedGIF {
public:
    /**
     * @brief Constructor
     */
    ESP32_AnimatedGIF();
    
    /**
     * @brief Destructor
     */
    ~ESP32_AnimatedGIF();
    
    /**
     * @brief Initialize the GIF decoder
     * @param pixelFormat Output pixel format
     * @param usePSRAM Use PSRAM if available (ESP32 only)
     * @return true if successful, false otherwise
     */
    bool begin(PixelFormat pixelFormat = PixelFormat::RGB565_LE, bool usePSRAM = true);
    
    /**
     * @brief Load GIF from memory buffer
     * @param data Pointer to GIF data
     * @param length Length of GIF data
     * @return GIFError code
     */
    GIFError loadFromMemory(const uint8_t* data, uint32_t length);
    
    /**
     * @brief Load GIF using custom data reader
     * @param reader Data reader callback
     * @param userData User data for callback
     * @return GIFError code
     */
    GIFError load(DataReader reader, void* userData = nullptr);
    
    /**
     * @brief Set display dimensions for scaling
     * @param width Display width
     * @param height Display height
     */
    void setDisplaySize(uint16_t width, uint16_t height);
    
    /**
     * @brief Set frame callback for partial updates
     * @param callback Frame callback function
     * @param userData User data for callback
     */
    void setFrameCallback(FrameCallback callback, void* userData = nullptr);
    
    /**
     * @brief Set pixel callback for pixel-by-pixel rendering
     * @param callback Pixel callback function
     * @param userData User data for callback
     */
    void setPixelCallback(PixelCallback callback, void* userData = nullptr);
    
    /**
     * @brief Get GIF information
     * @param info Reference to GIFInfo structure
     * @return true if successful, false otherwise
     */
    bool getInfo(GIFInfo& info);
    
    /**
     * @brief Get current frame information
     * @param info Reference to FrameInfo structure
     * @return true if successful, false otherwise
     */
    bool getFrameInfo(FrameInfo& info);
    
    /**
     * @brief Decode and render next frame
     * @param syncDelay Wait for frame delay if true
     * @return GIFError code
     */
    GIFError nextFrame(bool syncDelay = true);
    
    /**
     * @brief Reset to first frame
     */
    void reset();
    
    /**
     * @brief Get last error code
     * @return Last GIFError
     */
    GIFError getLastError() const;
    
    /**
     * @brief Get error message string
     * @param error Error code
     * @return Error message
     */
    static const char* getErrorMessage(GIFError error);
    
    /**
     * @brief Get current frame number (0-based)
     * @return Current frame number
     */
    uint16_t getCurrentFrame() const;
    
    /**
     * @brief Get total number of frames
     * @return Total frame count
     */
    uint16_t getFrameCount() const;
    
    /**
     * @brief Check if animation is complete
     * @return true if animation complete, false otherwise
     */
    bool isAnimationComplete() const;
    
    /**
     * @brief Set loop mode
     * @param loop true to loop animation, false to play once
     */
    void setLoop(bool loop);
    
    /**
     * @brief Set scaling factor
     * @param scale Scaling factor (1.0 = no scaling)
     */
    void setScale(float scale);
    
    /**
     * @brief Get canvas width
     * @return Canvas width in pixels
     */
    uint16_t getCanvasWidth() const;
    
    /**
     * @brief Get canvas height
     * @return Canvas height in pixels
     */
    uint16_t getCanvasHeight() const;
    
private:
    // Private implementation
    class Impl;
    Impl* _impl;
    
    // Disable copy constructor and assignment operator
    ESP32_AnimatedGIF(const ESP32_AnimatedGIF&) = delete;
    ESP32_AnimatedGIF& operator=(const ESP32_AnimatedGIF&) = delete;
};

// Utility functions for ESP32
namespace ESP32_GIF_Utils {
    
    /**
     * @brief Allocate memory with PSRAM preference
     * @param size Size to allocate
     * @param usePSRAM Use PSRAM if available
     * @return Pointer to allocated memory or nullptr
     */
    void* allocateMemory(size_t size, bool usePSRAM = true);
    
    /**
     * @brief Free allocated memory
     * @param ptr Pointer to memory
     */
    void freeMemory(void* ptr);
    
    /**
     * @brief Convert RGB888 to RGB565
     * @param r Red component (0-255)
     * @param g Green component (0-255)
     * @param b Blue component (0-255)
     * @return RGB565 color
     */
    uint16_t rgb888To565(uint8_t r, uint8_t g, uint8_t b);
    
    /**
     * @brief Convert RGB888 to grayscale
     * @param r Red component (0-255)
     * @param g Green component (0-255)
     * @param b Blue component (0-255)
     * @return Grayscale value (0-255)
     */
    uint8_t rgb888ToGrayscale(uint8_t r, uint8_t g, uint8_t b);
}

#endif // ESP32_ANIMATED_GIF_H