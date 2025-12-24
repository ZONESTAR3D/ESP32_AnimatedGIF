/**
 * @file BasicGIFPlayer.ino
 * @brief Basic GIF player example for ESP32_AnimatedGIF library
 * @author Deepseek
 */

#include <ESP32_AnimatedGIF.h>
#include <TFT_eSPI.h>

// Display setup
TFT_eSPI tft = TFT_eSPI();
#define SCREEN_WIDTH 360
#define SCREEN_HEIGHT 360

// GIF decoder
ESP32_AnimatedGIF gif;

// Frame buffer for double buffering
uint16_t* frameBuffer = nullptr;

// Example GIF data (small 16x16 animation)
const uint8_t exampleGIF[] PROGMEM = {
    // GIF89a header
    0x47, 0x49, 0x46, 0x38, 0x39, 0x61,
    // Width (16) and Height (16)
    0x10, 0x00, 0x10, 0x00,
    // Flags: Global color table, 2 colors
    0x91, 0x00, 0x00,
    // Color table: Black and White
    0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF,
    // Graphics Control Extension
    0x21, 0xF9, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
    // Image descriptor
    0x2C, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x10, 0x00, 0x00,
    // LZW minimum code size
    0x02,
    // Image data
    0x02, 0x16, 0x8C, 0x2D, 0x99, 0x87, 0x2A, 0x1C, 0xDC, 0x33,
    0xA0, 0x02, 0x00, 0x3B
};

/**
 * @brief Pixel callback function for direct drawing
 */
void pixelCallback(void* userData, uint16_t x, uint16_t y, uint16_t color) {
    TFT_eSPI* display = (TFT_eSPI*)userData;
    // Scale coordinates if needed
    uint16_t scaledX = x * SCREEN_WIDTH / gif.getCanvasWidth();
    uint16_t scaledY = y * SCREEN_HEIGHT / gif.getCanvasHeight();
    display->drawPixel(scaledX, scaledY, color);
}

/**
 * @brief Frame callback function for partial updates
 */
void frameCallback(void* userData, uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t* pixels) {
    // This would be called when a complete frame is decoded
    // Useful for partial display updates
}

/**
 * @brief Setup function
 */
void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("ESP32_AnimatedGIF Basic Example");
    
    // Initialize display
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1);
    
    // Allocate frame buffer
    frameBuffer = (uint16_t*)ps_malloc(SCREEN_WIDTH * SCREEN_HEIGHT * 2);
    if (!frameBuffer) {
        Serial.println("Failed to allocate frame buffer!");
        while (1);
    }
    
    // Initialize GIF decoder
    if (!gif.begin(ESP32_AnimatedGIF::PixelFormat::RGB565_LE, true)) {
        Serial.println("Failed to initialize GIF decoder!");
        return;
    }
    
    // Set display size for scaling
    gif.setDisplaySize(SCREEN_WIDTH, SCREEN_HEIGHT);
    
    // Set pixel callback for direct drawing
    gif.setPixelCallback(pixelCallback, &tft);
    
    // Load example GIF from memory
    GIFError error = gif.loadFromMemory(exampleGIF, sizeof(exampleGIF));
    if (error != GIFError::SUCCESS) {
        Serial.print("Failed to load GIF: ");
        Serial.println(ESP32_AnimatedGIF::getErrorMessage(error));
        return;
    }
    
    // Get GIF information
    GIFInfo info;
    if (gif.getInfo(info)) {
        Serial.println("GIF Information:");
        Serial.printf("  Size: %dx%d\n", info.width, info.height);
        Serial.printf("  Frames: %d\n", info.frameCount);
        Serial.printf("  Duration: %dms\n", info.totalDuration);
        Serial.printf("  Loop: %s\n", info.loopCount == 0 ? "Infinite" : String(info.loopCount).c_str());
    }
    
    Serial.println("Ready to play GIF...");
}

/**
 * @brief Main loop
 */
void loop() {
    static uint32_t lastFrameTime = 0;
    static uint16_t frameCount = 0;
    
    // Play next frame
    GIFError error = gif.nextFrame(true);
    
    if (error == GIFError::SUCCESS) {
        frameCount++;
        
        // Display frame info every 10 frames
        if (frameCount % 10 == 0) {
            FrameInfo frameInfo;
            if (gif.getFrameInfo(frameInfo)) {
                tft.setCursor(0, 0);
                tft.fillRect(0, 0, 100, 16, TFT_BLACK);
                tft.printf("Frame: %d/%d", gif.getCurrentFrame(), gif.getFrameCount());
            }
        }
        
    } else if (error == GIFError::EMPTY_FRAME) {
        // End of animation, restart
        Serial.println("Animation complete, restarting...");
        gif.reset();
        delay(1000);
    } else {
        // Error occurred
        Serial.print("Frame error: ");
        Serial.println(ESP32_AnimatedGIF::getErrorMessage(error));
        delay(1000);
    }
    
    // Small delay to prevent watchdog issues
    delay(5);
}