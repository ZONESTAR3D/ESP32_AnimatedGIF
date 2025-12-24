/**
 * @file SPIFFS_GIFPlayer.ino
 * @brief Play GIF from SPIFFS file system
 */

#include <ESP32_AnimatedGIF.h>
#include <TFT_eSPI.h>
#include <SPIFFS.h>

TFT_eSPI tft;
ESP32_AnimatedGIF gif;

// Display settings
#define SCREEN_WIDTH  360
#define SCREEN_HEIGHT 360
#define GIF_FILENAME  "/animation.gif"

// Frame buffer
uint16_t* frameBuffer = nullptr;

/**
 * @brief SPIFFS data reader callback
 */
bool spiffsReader(void* userData, uint8_t* buffer, uint32_t length, uint32_t position) {
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
 * @brief Pixel drawing callback
 */
void drawPixelCallback(void* userData, uint16_t x, uint16_t y, uint16_t color) {
    TFT_eSPI* display = (TFT_eSPI*)userData;
    display->drawPixel(x, y, color);
}

/**
 * @brief Setup function
 */
void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("ESP32_AnimatedGIF SPIFFS Player");
    Serial.println("===============================");
    
    // Initialize SPIFFS
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS Mount Failed");
        while (1);
    }
    
    // List files
    Serial.println("SPIFFS Contents:");
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    while (file) {
        Serial.printf("  %s (%d bytes)\n", file.name(), file.size());
        file = root.openNextFile();
    }
    root.close();
    
    // Initialize display
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    
    // Check if GIF file exists
    if (!SPIFFS.exists(GIF_FILENAME)) {
        Serial.printf("GIF file not found: %s\n", GIF_FILENAME);
        tft.setCursor(10, 10);
        tft.print("GIF file not found!");
        while (1);
    }
    
    // Open GIF file
    File gifFile = SPIFFS.open(GIF_FILENAME, "r");
    if (!gifFile) {
        Serial.println("Failed to open GIF file");
        while (1);
    }
    
    // Initialize GIF decoder
    if (!gif.begin(ESP32_AnimatedGIF::PixelFormat::RGB565_LE, true)) {
        Serial.println("Failed to initialize GIF decoder");
        return;
    }
    
    // Set display size
    gif.setDisplaySize(SCREEN_WIDTH, SCREEN_HEIGHT);
    
    // Set pixel callback
    gif.setPixelCallback(drawPixelCallback, &tft);
    
    // Load GIF using SPIFFS reader
    GIFError error = gif.load(spiffsReader, &gifFile);
    if (error != ESP32_AnimatedGIF::GIFError::SUCCESS) {
        Serial.printf("Failed to load GIF: %s\n", 
                     ESP32_AnimatedGIF::getErrorMessage(error));
        tft.setCursor(10, 30);
        tft.printf("Error: %s", ESP32_AnimatedGIF::getErrorMessage(error));
        while (1);
    }
    
    // Display GIF info
    ESP32_AnimatedGIF::GIFInfo info;
    if (gif.getInfo(info)) {
        Serial.println("GIF Info:");
        Serial.printf("  Size: %dx%d\n", info.width, info.height);
        Serial.printf("  Frames: %d\n", info.frameCount);
        Serial.printf("  Duration: %dms\n", info.totalDuration);
        Serial.printf("  Loop: %s\n", info.loopCount == 0 ? "Infinite" : String(info.loopCount).c_str());
        
        tft.setCursor(10, 10);
        tft.printf("Size: %dx%d", info.width, info.height);
        tft.setCursor(10, 30);
        tft.printf("Frames: %d", info.frameCount);
    }
    
    Serial.println("Playing GIF from SPIFFS...");
    delay(2000); // Show info for 2 seconds
    tft.fillScreen(TFT_BLACK);
}

/**
 * @brief Main loop
 */
void loop() {
    static uint32_t frameCounter = 0;
    static uint32_t lastFPS = 0;
    
    // Play next frame
    GIFError error = gif.nextFrame(true); // true = sync to frame delay
    
    if (error == ESP32_AnimatedGIF::GIFError::SUCCESS) {
        frameCounter++;
        
        // Calculate FPS every second
        uint32_t now = millis();
        if (now - lastFPS >= 1000) {
            float fps = frameCounter * 1000.0 / (now - lastFPS);
            Serial.printf("FPS: %.1f, Frame: %d/%d\n", 
                         fps, gif.getCurrentFrame(), gif.getFrameCount());
            
            // Show FPS on display corner
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.setCursor(SCREEN_WIDTH - 50, 10);
            tft.printf("%.1f FPS", fps);
            
            frameCounter = 0;
            lastFPS = now;
        }
        
    } else if (error == ESP32_AnimatedGIF::GIFError::EMPTY_FRAME) {
        // End of animation
        Serial.println("Animation complete, restarting...");
        gif.reset();
        delay(100);
        
    } else {
        Serial.printf("Error: %s\n", ESP32_AnimatedGIF::getErrorMessage(error));
        delay(1000);
    }
    
    // Small delay to prevent watchdog issues
    delay(5);
}