/**
 * @file SDCard_GIFPlayer_Arduino_GFX.ino
 * @brief Play GIF from SD card using Arduino_GFX display library
 * 
 * Note: Requires SD card module and Arduino_GFX compatible display
 */

#include <ESP32_AnimatedGIF.h>
#include <Arduino_GFX_Library.h>
#include <SD.h>
#include <SPI.h>

// Display configuration for ESP32-S3 360x360
#define TFT_CS   10
#define TFT_DC   7
#define TFT_RST  6
#define TFT_BL   45

// Pin definitions for SD card (adjust for your board)
#define SD_CS_PIN     5
#define SD_MOSI_PIN   23
#define SD_MISO_PIN   19
#define SD_SCK_PIN    18

// Display settings
#define SCREEN_WIDTH  360
#define SCREEN_HEIGHT 360
#define GIF_FILENAME  "/animation.gif"

// Create display instance for GC9A01 360x360 round display
Arduino_ESP32SPI* bus = new Arduino_ESP32SPI(
    TFT_DC, TFT_CS, 
    18 /* SCK */, 23 /* MOSI */, -1 /* MISO */);
    
Arduino_GC9A01* tft = new Arduino_GC9A01(
    bus, TFT_RST, 
    0 /* rotation */, 
    true /* IPS */);

// GIF decoder
ESP32_AnimatedGIF gif;

// Performance tracking
uint32_t totalDecodeTime = 0;
uint32_t totalFrames = 0;

/**
 * @brief SD card data reader callback
 */
bool sdCardReader(void* userData, uint8_t* buffer, uint32_t length, uint32_t position) {
    File* file = (File*)userData;
    
    if (!file || !*file) {
        return false;
    }
    
    if (file->position() != position) {
        file->seek(position);
    }
    
    return file->read(buffer, length) == length;
}

/**
 * @brief Pixel drawing callback for Arduino_GFX
 */
void drawPixelCallback(void* userData, uint16_t x, uint16_t y, uint16_t color) {
    Arduino_GC9A01* display = (Arduino_GC9A01*)userData;
    
    // Apply gamma correction if needed
    // color = gammaCorrect(color);
    
    display->drawPixel(x, y, color);
}

/**
 * @brief Frame callback for partial updates (optional optimization)
 */
void frameCallback(void* userData, uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t* pixels) {
    // This can be used for more efficient block transfers
    // Currently using pixel-by-pixel for simplicity
}

/**
 * @brief Apply gamma correction to RGB565 color
 */
uint16_t gammaCorrect(uint16_t color) {
    // Simple gamma correction for better color display
    uint8_t r = (color >> 11) & 0x1F;
    uint8_t g = (color >> 5) & 0x3F;
    uint8_t b = color & 0x1F;
    
    // Gamma correction table (approximate)
    static const uint8_t gammaTable[32] = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
        16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31
    };
    
    r = gammaTable[r];
    g = gammaTable[g >> 1] << 1; // Adjust for 6-bit green
    b = gammaTable[b];
    
    return (r << 11) | (g << 5) | b;
}

/**
 * @brief Setup function
 */
void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("ESP32_AnimatedGIF SD Card Player with Arduino_GFX");
    Serial.println("==================================================");
    
    // Initialize display
    tft->begin();
    tft->fillScreen(BLACK);
    tft->setTextColor(WHITE);
    tft->setTextSize(2);
    tft->setCursor(50, 150);
    tft->print("Initializing...");
    
    // Initialize SD card
    SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
    
    if (!SD.begin(SD_CS_PIN)) {
        Serial.println("SD Card Mount Failed");
        tft->setCursor(50, 180);
        tft->print("SD Card Error!");
        while (1);
    }
    
    Serial.println("SD Card initialized.");
    
    // Check if GIF file exists
    if (!SD.exists(GIF_FILENAME)) {
        Serial.printf("GIF file not found: %s\n", GIF_FILENAME);
        tft->fillScreen(BLACK);
        tft->setCursor(50, 150);
        tft->print("GIF not found!");
        
        // List available files
        Serial.println("Available files:");
        File root = SD.open("/");
        File file = root.openNextFile();
        while (file) {
            if (strstr(file.name(), ".gif") || strstr(file.name(), ".GIF")) {
                Serial.printf("  %s (%d bytes)\n", file.name(), file.size());
            }
            file = root.openNextFile();
        }
        root.close();
        
        delay(5000);
        return;
    }
    
    // Open GIF file
    File gifFile = SD.open(GIF_FILENAME);
    if (!gifFile) {
        Serial.println("Failed to open GIF file");
        tft->setCursor(50, 180);
        tft->print("File open error");
        while (1);
    }
    
    Serial.printf("GIF file opened: %s (%d bytes)\n", 
                  GIF_FILENAME, gifFile.size());
    
    // Show loading info
    tft->fillScreen(BLACK);
    tft->setTextSize(1);
    tft->setCursor(50, 100);
    tft->printf("Loading: %s", GIF_FILENAME);
    tft->setCursor(50, 130);
    tft->printf("Size: %d KB", gifFile.size() / 1024);
    
    // Initialize GIF decoder with PSRAM support
    if (!gif.begin(ESP32_AnimatedGIF::PixelFormat::RGB565_LE, true)) {
        Serial.println("Failed to initialize GIF decoder");
        tft->setCursor(50, 160);
        tft->print("GIF init error");
        while (1);
    }
    
    // Set display size
    gif.setDisplaySize(SCREEN_WIDTH, SCREEN_HEIGHT);
    
    // Set pixel callback for Arduino_GFX
    gif.setPixelCallback(drawPixelCallback, tft);
    
    // Load GIF
    uint32_t loadStart = millis();
    ESP32_AnimatedGIF::GIFError error = gif.load(sdCardReader, &gifFile);
    uint32_t loadTime = millis() - loadStart;
    
    if (error != ESP32_AnimatedGIF::GIFError::SUCCESS) {
        Serial.printf("Failed to load GIF: %s\n", 
                     ESP32_AnimatedGIF::getErrorMessage(error));
        tft->setCursor(50, 190);
        tft->printf("Error: %s", ESP32_AnimatedGIF::getErrorMessage(error));
        while (1);
    }
    
    // Display GIF info
    ESP32_AnimatedGIF::GIFInfo info;
    if (gif.getInfo(info)) {
        Serial.println("GIF Information:");
        Serial.printf("  Size: %dx%d\n", info.width, info.height);
        Serial.printf("  Frames: %d\n", info.frameCount);
        Serial.printf("  Duration: %dms\n", info.totalDuration);
        Serial.printf("  Loop: %s\n", info.loopCount == 0 ? "Infinite" : String(info.loopCount).c_str());
        Serial.printf("  Load time: %dms\n", loadTime);
        
        tft->fillScreen(BLACK);
        tft->setCursor(50, 100);
        tft->printf("Size: %dx%d", info.width, info.height);
        tft->setCursor(50, 120);
        tft->printf("Frames: %d", info.frameCount);
        tft->setCursor(50, 140);
        tft->printf("Load: %dms", loadTime);
    }
    
    // Set loop mode
    gif.setLoop(true);
    
    Serial.println("Ready to play GIF from SD card...");
    delay(2000); // Show info for 2 seconds
    tft->fillScreen(BLACK);
    
    // Backlight control (if available)
    #ifdef TFT_BL
        pinMode(TFT_BL, OUTPUT);
        digitalWrite(TFT_BL, HIGH); // Turn on backlight
    #endif
}

/**
 * @brief Main loop
 */
void loop() {
    static uint32_t lastFrameTime = 0;
    static uint32_t frameStartTime = 0;
    static uint16_t currentFrame = 0;
    static float fps = 0;
    
    // Start timing
    frameStartTime = millis();
    
    // Play next frame
    ESP32_AnimatedGIF::GIFError error = gif.nextFrame(true); // Sync to frame delay
    
    if (error == ESP32_AnimatedGIF::GIFError::SUCCESS) {
        uint32_t decodeTime = millis() - frameStartTime;
        totalDecodeTime += decodeTime;
        totalFrames++;
        
        currentFrame = gif.getCurrentFrame();
        
        // Calculate FPS every second
        uint32_t now = millis();
        if (now - lastFrameTime >= 1000) {
            fps = totalFrames * 1000.0 / (now - lastFrameTime);
            float avgDecodeTime = (float)totalDecodeTime / totalFrames;
            
            Serial.printf("Frame: %d/%d | FPS: %.1f | Decode: %.1fms\n", 
                         currentFrame, gif.getFrameCount(), fps, avgDecodeTime);
            
            // Reset counters
            totalDecodeTime = 0;
            totalFrames = 0;
            lastFrameTime = now;
            
            // Display FPS in corner
            tft->setTextColor(WHITE, BLACK);
            tft->setTextSize(1);
            tft->setCursor(SCREEN_WIDTH - 80, 10);
            tft->printf("%.1f FPS", fps);
        }
        
    } else if (error == ESP32_AnimatedGIF::GIFError::EMPTY_FRAME) {
        // End of animation
        Serial.println("Animation complete, restarting...");
        
        // Show completion briefly
        tft->fillScreen(BLACK);
        tft->setTextColor(GREEN, BLACK);
        tft->setTextSize(2);
        tft->setCursor(SCREEN_WIDTH/2 - 60, SCREEN_HEIGHT/2 - 20);
        tft->print("Complete!");
        
        delay(1000);
        
        // Restart animation
        gif.reset();
        tft->fillScreen(BLACK);
        tft->setTextSize(1);
        
    } else {
        Serial.printf("Play error: %s\n", 
                     ESP32_AnimatedGIF::getErrorMessage(error));
        delay(1000);
    }
    
    // Small delay to prevent watchdog issues
    delay(1);
}

/**
 * @brief Performance monitoring function (optional)
 */
void printPerformanceStats() {
    if (totalFrames > 0) {
        float avgDecodeTime = (float)totalDecodeTime / totalFrames;
        Serial.println("\nPerformance Summary:");
        Serial.printf("  Average decode time: %.2f ms\n", avgDecodeTime);
        Serial.printf("  Maximum FPS: %.1f\n", 1000.0 / avgDecodeTime);
    }
}