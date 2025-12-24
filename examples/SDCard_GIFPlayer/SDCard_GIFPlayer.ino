/**
 * @file SDCard_GIFPlayer.ino
 * @brief Play GIF from SD card
 * 
 * Note: Requires SD card module connected to ESP32
 */

#include <ESP32_AnimatedGIF.h>
#include <TFT_eSPI.h>
#include <SD.h>
#include <SPI.h>

TFT_eSPI tft;
ESP32_AnimatedGIF gif;

// Pin definitions for SD card
#define SD_CS_PIN     5
#define SD_MOSI_PIN   23
#define SD_MISO_PIN   19
#define SD_SCK_PIN    18

// Display settings
#define SCREEN_WIDTH  360
#define SCREEN_HEIGHT 360
#define GIF_FILENAME  "/animation.gif"

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
 * @brief Pixel drawing callback with scaling
 */
void drawPixelScaled(void* userData, uint16_t x, uint16_t y, uint16_t color) {
    TFT_eSPI* display = (TFT_eSPI*)userData;
    
    // Scale coordinates to fit screen
    uint16_t scaledX = map(x, 0, gif.getCanvasWidth(), 0, SCREEN_WIDTH);
    uint16_t scaledY = map(y, 0, gif.getCanvasHeight(), 0, SCREEN_HEIGHT);
    
    display->drawPixel(scaledX, scaledY, color);
}

/**
 * @brief Setup function
 */
void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("ESP32_AnimatedGIF SD Card Player");
    Serial.println("================================");
    
    // Initialize display first
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(10, 10);
    tft.print("Initializing SD card...");
    
    // Initialize SD card
    SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
    
    if (!SD.begin(SD_CS_PIN)) {
        Serial.println("SD Card Mount Failed");
        tft.setCursor(10, 30);
        tft.print("SD Card Error!");
        while (1);
    }
    
    Serial.println("SD Card initialized.");
    tft.setCursor(10, 30);
    tft.print("SD Card OK");
    
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("No SD card attached");
        tft.setCursor(10, 50);
        tft.print("No SD Card");
        while (1);
    }
    
    // Print SD card info
    Serial.printf("SD Card Type: ");
    if (cardType == CARD_MMC) {
        Serial.println("MMC");
    } else if (cardType == CARD_SD) {
        Serial.println("SDSC");
    } else if (cardType == CARD_SDHC) {
        Serial.println("SDHC");
    } else {
        Serial.println("UNKNOWN");
    }
    
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("SD Card Size: %lluMB\n", cardSize);
    
    // Check if GIF file exists
    if (!SD.exists(GIF_FILENAME)) {
        Serial.printf("GIF file not found: %s\n", GIF_FILENAME);
        tft.setCursor(10, 70);
        tft.print("GIF file not found!");
        
        // List available files
        Serial.println("Available files:");
        File root = SD.open("/");
        File file = root.openNextFile();
        while (file) {
            Serial.printf("  %s\n", file.name());
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
        tft.setCursor(10, 90);
        tft.print("File open error");
        while (1);
    }
    
    Serial.printf("GIF file opened: %s (%d bytes)\n", 
                  GIF_FILENAME, gifFile.size());
    
    tft.setCursor(10, 50);
    tft.printf("File: %s", GIF_FILENAME);
    tft.setCursor(10, 70);
    tft.printf("Size: %d KB", gifFile.size() / 1024);
    
    // Initialize GIF decoder
    if (!gif.begin(ESP32_AnimatedGIF::PixelFormat::RGB565_LE, true)) {
        Serial.println("Failed to initialize GIF decoder");
        tft.setCursor(10, 110);
        tft.print("GIF init error");
        while (1);
    }
    
    // Set display size for scaling
    gif.setDisplaySize(SCREEN_WIDTH, SCREEN_HEIGHT);
    
    // Set pixel callback with scaling
    gif.setPixelCallback(drawPixelScaled, &tft);
    
    // Load GIF
    GIFError error = gif.load(sdCardReader, &gifFile);
    if (error != ESP32_AnimatedGIF::GIFError::SUCCESS) {
        Serial.printf("Failed to load GIF: %s\n", 
                     ESP32_AnimatedGIF::getErrorMessage(error));
        tft.setCursor(10, 130);
        tft.printf("Error: %s", ESP32_AnimatedGIF::getErrorMessage(error));
        while (1);
    }
    
    // Display GIF info
    ESP32_AnimatedGIF::GIFInfo info;
    if (gif.getInfo(info)) {
        Serial.println("GIF Information:");
        Serial.printf("  Original: %dx%d\n", info.width, info.height);
        Serial.printf("  Display: %dx%d\n", SCREEN_WIDTH, SCREEN_HEIGHT);
        Serial.printf("  Frames: %d\n", info.frameCount);
        Serial.printf("  Duration: %dms\n", info.totalDuration);
        
        tft.setCursor(10, 90);
        tft.printf("Frames: %d", info.frameCount);
        tft.setCursor(10, 110);
        tft.printf("Duration: %ds", info.totalDuration / 1000);
    }
    
    Serial.println("Ready to play GIF from SD card...");
    delay(3000); // Show info for 3 seconds
    tft.fillScreen(TFT_BLACK);
}

/**
 * @brief Main loop
 */
void loop() {
    static uint32_t startTime = millis();
    static uint32_t framesPlayed = 0;
    
    // Play next frame
    GIFError error = gif.nextFrame(true);
    
    if (error == ESP32_AnimatedGIF::GIFError::SUCCESS) {
        framesPlayed++;
        
        // Show progress every 30 frames
        if (framesPlayed % 30 == 0) {
            uint32_t elapsed = millis() - startTime;
            float fps = framesPlayed * 1000.0 / elapsed;
            
            // Display FPS in corner
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.setCursor(SCREEN_WIDTH - 60, 10);
            tft.printf("%.1f FPS", fps);
        }
        
    } else if (error == ESP32_AnimatedGIF::GIFError::EMPTY_FRAME) {
        // Animation complete
        Serial.println("Animation complete!");
        
        // Show completion message
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setTextSize(2);
        tft.setCursor(SCREEN_WIDTH/2 - 80, SCREEN_HEIGHT/2 - 20);
        tft.print("Complete!");
        
        delay(2000);
        
        // Restart
        gif.reset();
        startTime = millis();
        framesPlayed = 0;
        tft.fillScreen(TFT_BLACK);
        tft.setTextSize(1);
        
    } else {
        Serial.printf("Play error: %s\n", 
                     ESP32_AnimatedGIF::getErrorMessage(error));
        delay(1000);
    }
    
    // Prevent watchdog timeout
    delay(1);
}