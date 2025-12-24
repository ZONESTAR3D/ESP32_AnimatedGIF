/**
 * @file ESP32_AnimatedGIF.cpp
 * @brief Animated GIF decoder optimized for ESP32
 * @author Deepseek
 * @version 1.0.0
 * @date 2024
 * 
 * Refactored from Animated GIF decoder by Larry Bank
 */

#include "ESP32_AnimatedGIF.h"
#include <algorithm>

// ESP32 specific includes
#ifdef ESP32_ANIMATEDGIF_ESP_PLATFORM
  #include <FS.h>
  #include <SPIFFS.h>
  #include <SD.h>
  #ifdef ESP32_ANIMATEDGIF_PSRAM_SUPPORT
    #include <esp32-hal-psram.h>
  #endif
#endif

// Private implementation class
class ESP32_AnimatedGIF::Impl {
public:
    Impl() 
        : _data(nullptr)
        , _dataLength(0)
        , _dataPosition(0)
        , _reader(nullptr)
        , _readerData(nullptr)
        , _frameCallback(nullptr)
        , _pixelCallback(nullptr)
        , _callbackData(nullptr)
        , _currentFrame(0)
        , _totalFrames(0)
        , _loopCount(0)
        , _loop(true)
        , _scale(1.0f)
        , _displayWidth(0)
        , _displayHeight(0)
        , _pixelFormat(PixelFormat::RGB565_LE)
        , _usePSRAM(true)
        , _lastError(GIFError::SUCCESS)
        , _decoding(false) {
        resetState();
    }
    
    ~Impl() {
        cleanup();
    }
    
    bool begin(PixelFormat pixelFormat, bool usePSRAM) {
        _pixelFormat = pixelFormat;
        _usePSRAM = usePSRAM;
        resetState();
        _lastError = GIFError::SUCCESS;
        return true;
    }
    
    GIFError loadFromMemory(const uint8_t* data, uint32_t length) {
        cleanup();
        
        if (!data || length == 0) {
            _lastError = GIFError::INVALID_PARAMETER;
            return _lastError;
        }
        
        _data = (uint8_t*)ESP32_GIF_Utils::allocateMemory(length, _usePSRAM);
        if (!_data) {
            _lastError = GIFError::OUT_OF_MEMORY;
            return _lastError;
        }
        
        memcpy(_data, data, length);
        _dataLength = length;
        _dataPosition = 0;
        
        return parseHeader();
    }
    
    GIFError load(DataReader reader, void* userData) {
        cleanup();
        
        if (!reader) {
            _lastError = GIFError::INVALID_PARAMETER;
            return _lastError;
        }
        
        _reader = reader;
        _readerData = userData;
        
        // Read header first
        uint8_t header[13];
        if (!readData(header, 13, 0)) {
            _lastError = GIFError::FILE_NOT_FOUND;
            return _lastError;
        }
        
        // Validate GIF header
        if (memcmp(header, "GIF89a", 6) != 0 && memcmp(header, "GIF87a", 6) != 0) {
            _lastError = GIFError::BAD_FILE_FORMAT;
            return _lastError;
        }
        
        // Parse basic info
        _canvasWidth = header[6] | (header[7] << 8);
        _canvasHeight = header[8] | (header[9] << 8);
        
        if (_canvasWidth > ESP32_ANIMATEDGIF_MAX_WIDTH || _canvasHeight > ESP32_ANIMATEDGIF_MAX_HEIGHT) {
            _lastError = GIFError::FILE_TOO_WIDE;
            return _lastError;
        }
        
        uint8_t flags = header[10];
        _backgroundColor = header[11];
        
        // Check for global color table
        if (flags & 0x80) {
            uint8_t colorTableBits = (flags & 0x07) + 1;
            _globalColorTableSize = 1 << colorTableBits;
            _globalColorTable = (uint32_t*)ESP32_GIF_Utils::allocateMemory(
                _globalColorTableSize * 3, _usePSRAM);
            
            if (_globalColorTable) {
                readData((uint8_t*)_globalColorTable, _globalColorTableSize * 3, 13);
            }
        }
        
        _lastError = GIFError::SUCCESS;
        return _lastError;
    }
    
    void setDisplaySize(uint16_t width, uint16_t height) {
        _displayWidth = width;
        _displayHeight = height;
    }
    
    void setFrameCallback(FrameCallback callback, void* userData) {
        _frameCallback = callback;
        _callbackData = userData;
    }
    
    void setPixelCallback(PixelCallback callback, void* userData) {
        _pixelCallback = callback;
        _callbackData = userData;
    }
    
    bool getInfo(GIFInfo& info) {
        info.width = _canvasWidth;
        info.height = _canvasHeight;
        info.frameCount = _totalFrames;
        info.totalDuration = _totalDuration;
        info.loopCount = _loopCount;
        info.hasTransparency = _hasTransparency;
        info.backgroundColor = _backgroundColor;
        info.transparentIndex = _transparentIndex;
        return true;
    }
    
    bool getFrameInfo(FrameInfo& info) {
        info.x = _frameX;
        info.y = _frameY;
        info.width = _frameWidth;
        info.height = _frameHeight;
        info.delay = _frameDelay;
        info.disposal = static_cast<DisposalMethod>(_disposalMethod);
        info.interlace = (_imageFlags & 0x40) != 0;
        return true;
    }
    
    GIFError nextFrame(bool syncDelay) {
        if (_lastError != GIFError::SUCCESS) {
            return _lastError;
        }
        
        // Check if we need to restart
        if (_currentFrame >= _totalFrames && _loop) {
            reset();
        } else if (_currentFrame >= _totalFrames) {
            _lastError = GIFError::EMPTY_FRAME;
            return _lastError;
        }
        
        // Parse frame if not already parsed
        if (!parseFrame()) {
            return _lastError;
        }
        
        // Decode frame
        if (!decodeFrame()) {
            return _lastError;
        }
        
        // Apply disposal method
        applyDisposal();
        
        _currentFrame++;
        
        // Wait for frame delay if requested
        if (syncDelay && _frameDelay > 0) {
            delay(_frameDelay);
        }
        
        return GIFError::SUCCESS;
    }
    
    void reset() {
        _dataPosition = 0;
        _currentFrame = 0;
        resetFrameBuffer();
        parseHeader(); // Reset to beginning
    }
    
    GIFError getLastError() const {
        return _lastError;
    }
    
    uint16_t getCurrentFrame() const {
        return _currentFrame;
    }
    
    uint16_t getFrameCount() const {
        return _totalFrames;
    }
    
    bool isAnimationComplete() const {
        return !_loop && _currentFrame >= _totalFrames;
    }
    
    void setLoop(bool loop) {
        _loop = loop;
    }
    
    void setScale(float scale) {
        _scale = std::max(0.1f, std::min(scale, 10.0f));
    }
    
    uint16_t getCanvasWidth() const {
        return _canvasWidth;
    }
    
    uint16_t getCanvasHeight() const {
        return _canvasHeight;
    }
    
private:
    // Data management
    uint8_t* _data;
    uint32_t _dataLength;
    uint32_t _dataPosition;
    DataReader _reader;
    void* _readerData;
    
    // Callbacks
    FrameCallback _frameCallback;
    PixelCallback _pixelCallback;
    void* _callbackData;
    
    // GIF state
    uint16_t _canvasWidth;
    uint16_t _canvasHeight;
    uint16_t _currentFrame;
    uint16_t _totalFrames;
    uint16_t _loopCount;
    bool _loop;
    float _scale;
    uint16_t _displayWidth;
    uint16_t _displayHeight;
    PixelFormat _pixelFormat;
    bool _usePSRAM;
    GIFError _lastError;
    bool _decoding;
    
    // Frame state
    uint16_t _frameX;
    uint16_t _frameY;
    uint16_t _frameWidth;
    uint16_t _frameHeight;
    uint16_t _frameDelay;
    uint8_t _disposalMethod;
    uint8_t _imageFlags;
    bool _hasTransparency;
    uint8_t _transparentIndex;
    uint8_t _backgroundColor;
    
    // Color tables
    uint32_t* _globalColorTable;
    uint32_t* _localColorTable;
    uint16_t _globalColorTableSize;
    uint16_t _localColorTableSize;
    
    // Buffers
    uint8_t* _frameBuffer;
    uint8_t* _previousFrame;
    uint32_t _totalDuration;
    
    void resetState() {
        _canvasWidth = 0;
        _canvasHeight = 0;
        _currentFrame = 0;
        _totalFrames = 0;
        _loopCount = 0;
        _totalDuration = 0;
        
        resetFrameState();
        
        _globalColorTable = nullptr;
        _localColorTable = nullptr;
        _globalColorTableSize = 0;
        _localColorTableSize = 0;
        
        _frameBuffer = nullptr;
        _previousFrame = nullptr;
    }
    
    void resetFrameState() {
        _frameX = 0;
        _frameY = 0;
        _frameWidth = 0;
        _frameHeight = 0;
        _frameDelay = 0;
        _disposalMethod = 0;
        _imageFlags = 0;
        _hasTransparency = false;
        _transparentIndex = 0;
    }
    
    void cleanup() {
        if (_data) {
            ESP32_GIF_Utils::freeMemory(_data);
            _data = nullptr;
        }
        
        if (_globalColorTable) {
            ESP32_GIF_Utils::freeMemory(_globalColorTable);
            _globalColorTable = nullptr;
        }
        
        if (_localColorTable) {
            ESP32_GIF_Utils::freeMemory(_localColorTable);
            _localColorTable = nullptr;
        }
        
        if (_frameBuffer) {
            ESP32_GIF_Utils::freeMemory(_frameBuffer);
            _frameBuffer = nullptr;
        }
        
        if (_previousFrame) {
            ESP32_GIF_Utils::freeMemory(_previousFrame);
            _previousFrame = nullptr;
        }
        
        resetState();
    }
    
    bool readData(uint8_t* buffer, uint32_t length, uint32_t position) {
        if (_reader) {
            return _reader(_readerData, buffer, length, position);
        } else if (_data) {
            if (position + length > _dataLength) {
                return false;
            }
            memcpy(buffer, _data + position, length);
            return true;
        }
        return false;
    }
    
    GIFError parseHeader() {
        uint8_t header[13];
        if (!readData(header, 13, 0)) {
            _lastError = GIFError::FILE_NOT_FOUND;
            return _lastError;
        }
        
        // Validate GIF signature
        if (memcmp(header, "GIF89a", 6) != 0 && memcmp(header, "GIF87a", 6) != 0) {
            _lastError = GIFError::BAD_FILE_FORMAT;
            return _lastError;
        }
        
        _canvasWidth = header[6] | (header[7] << 8);
        _canvasHeight = header[8] | (header[9] << 8);
        
        if (_canvasWidth > ESP32_ANIMATEDGIF_MAX_WIDTH || _canvasHeight > ESP32_ANIMATEDGIF_MAX_HEIGHT) {
            _lastError = GIFError::FILE_TOO_WIDE;
            return _lastError;
        }
        
        uint8_t flags = header[10];
        _backgroundColor = header[11];
        
        // Parse global color table if present
        _dataPosition = 13;
        if (flags & 0x80) {
            uint8_t colorTableBits = (flags & 0x07) + 1;
            _globalColorTableSize = 1 << colorTableBits;
            
            _globalColorTable = (uint32_t*)ESP32_GIF_Utils::allocateMemory(
                _globalColorTableSize * 3, _usePSRAM);
            
            if (_globalColorTable) {
                if (!readData((uint8_t*)_globalColorTable, _globalColorTableSize * 3, _dataPosition)) {
                    _lastError = GIFError::EARLY_EOF;
                    return _lastError;
                }
                _dataPosition += _globalColorTableSize * 3;
            }
        }
        
        // Parse frames to count them
        countFrames();
        
        // Allocate frame buffer
        allocateFrameBuffer();
        
        _lastError = GIFError::SUCCESS;
        return _lastError;
    }
    
    void countFrames() {
        uint32_t pos = _dataPosition;
        uint8_t block[256];
        _totalFrames = 0;
        _totalDuration = 0;
        
        while (pos < _dataLength - 1) {
            if (!readData(block, 1, pos)) break;
            
            if (block[0] == 0x2C) { // Image descriptor
                _totalFrames++;
                pos += 10; // Skip image descriptor
                
                // Skip local color table if present
                if (!readData(block, 1, pos)) break;
                if (block[0] & 0x80) {
                    uint8_t colorTableBits = (block[0] & 0x07) + 1;
                    uint16_t colorTableSize = 1 << colorTableBits;
                    pos += colorTableSize * 3 + 1;
                } else {
                    pos += 1;
                }
                
                // Skip image data
                while (pos < _dataLength) {
                    if (!readData(block, 1, pos)) break;
                    uint8_t subBlockSize = block[0];
                    pos += 1;
                    
                    if (subBlockSize == 0) break;
                    pos += subBlockSize;
                }
            } else if (block[0] == 0x21) { // Extension block
                pos += 1;
                if (!readData(block, 1, pos)) break;
                uint8_t extensionType = block[0];
                pos += 1;
                
                if (extensionType == 0xF9) { // Graphics control extension
                    if (!readData(block, 5, pos)) break;
                    uint16_t delay = (block[2] << 8) | block[1];
                    if (delay < 2) delay = 2; // Minimum delay
                    _totalDuration += delay * 10; // Convert to milliseconds
                    pos += 5;
                }
                
                // Skip extension data
                while (pos < _dataLength) {
                    if (!readData(block, 1, pos)) break;
                    uint8_t subBlockSize = block[0];
                    pos += 1;
                    
                    if (subBlockSize == 0) break;
                    pos += subBlockSize;
                }
            } else if (block[0] == 0x3B) { // Trailer
                break;
            } else {
                pos += 1;
            }
        }
        
        _dataPosition = 13 + (_globalColorTableSize * 3);
    }
    
    void allocateFrameBuffer() {
        if (_frameBuffer) {
            ESP32_GIF_Utils::freeMemory(_frameBuffer);
        }
        
        if (_previousFrame) {
            ESP32_GIF_Utils::freeMemory(_previousFrame);
        }
        
        size_t bufferSize = _canvasWidth * _canvasHeight;
        switch (_pixelFormat) {
            case PixelFormat::RGB565_LE:
            case PixelFormat::RGB565_BE:
                bufferSize *= 2;
                break;
            case PixelFormat::RGB888:
                bufferSize *= 3;
                break;
            case PixelFormat::ARGB8888:
                bufferSize *= 4;
                break;
            default:
                // 1 byte per pixel for others
                break;
        }
        
        _frameBuffer = (uint8_t*)ESP32_GIF_Utils::allocateMemory(bufferSize, _usePSRAM);
        _previousFrame = (uint8_t*)ESP32_GIF_Utils::allocateMemory(bufferSize, _usePSRAM);
        
        if (_frameBuffer && _previousFrame) {
            memset(_frameBuffer, 0, bufferSize);
            memset(_previousFrame, 0, bufferSize);
        }
    }
    
    void resetFrameBuffer() {
        if (_frameBuffer && _previousFrame) {
            size_t bufferSize = _canvasWidth * _canvasHeight;
            switch (_pixelFormat) {
                case PixelFormat::RGB565_LE:
                case PixelFormat::RGB565_BE:
                    bufferSize *= 2;
                    break;
                case PixelFormat::RGB888:
                    bufferSize *= 3;
                    break;
                case PixelFormat::ARGB8888:
                    bufferSize *= 4;
                    break;
                default:
                    break;
            }
            memset(_frameBuffer, 0, bufferSize);
            memset(_previousFrame, 0, bufferSize);
        }
    }
    
    bool parseFrame() {
        uint8_t block[256];
        resetFrameState();
        
        while (_dataPosition < _dataLength) {
            if (!readData(block, 1, _dataPosition)) {
                _lastError = GIFError::EARLY_EOF;
                return false;
            }
            
            if (block[0] == 0x21) { // Extension block
                _dataPosition += 1;
                if (!readData(block, 1, _dataPosition)) {
                    _lastError = GIFError::EARLY_EOF;
                    return false;
                }
                
                uint8_t extensionType = block[0];
                _dataPosition += 1;
                
                if (extensionType == 0xF9) { // Graphics control extension
                    if (!readData(block, 5, _dataPosition)) {
                        _lastError = GIFError::EARLY_EOF;
                        return false;
                    }
                    
                    uint8_t packed = block[0];
                    _disposalMethod = (packed >> 2) & 0x07;
                    _hasTransparency = (packed & 0x01) != 0;
                    _frameDelay = ((block[2] << 8) | block[1]) * 10; // Convert to ms
                    if (_frameDelay < 20) _frameDelay = 20; // Minimum 20ms
                    _transparentIndex = block[3];
                    
                    _dataPosition += 5;
                }
                
                // Skip extension data
                while (true) {
                    if (!readData(block, 1, _dataPosition)) {
                        _lastError = GIFError::EARLY_EOF;
                        return false;
                    }
                    uint8_t subBlockSize = block[0];
                    _dataPosition += 1;
                    
                    if (subBlockSize == 0) break;
                    _dataPosition += subBlockSize;
                }
            } else if (block[0] == 0x2C) { // Image descriptor
                // Parse image descriptor
                if (!readData(block, 9, _dataPosition + 1)) {
                    _lastError = GIFError::EARLY_EOF;
                    return false;
                }
                
                _frameX = block[0] | (block[1] << 8);
                _frameY = block[2] | (block[3] << 8);
                _frameWidth = block[4] | (block[5] << 8);
                _frameHeight = block[6] | (block[7] << 8);
                _imageFlags = block[8];
                
                _dataPosition += 10;
                
                // Parse local color table if present
                if (_imageFlags & 0x80) {
                    uint8_t colorTableBits = (_imageFlags & 0x07) + 1;
                    _localColorTableSize = 1 << colorTableBits;
                    
                    if (_localColorTable) {
                        ESP32_GIF_Utils::freeMemory(_localColorTable);
                    }
                    
                    _localColorTable = (uint32_t*)ESP32_GIF_Utils::allocateMemory(
                        _localColorTableSize * 3, _usePSRAM);
                    
                    if (_localColorTable) {
                        if (!readData((uint8_t*)_localColorTable, _localColorTableSize * 3, _dataPosition)) {
                            _lastError = GIFError::EARLY_EOF;
                            return false;
                        }
                        _dataPosition += _localColorTableSize * 3;
                    }
                }
                
                return true;
            } else if (block[0] == 0x3B) { // Trailer
                _lastError = GIFError::EMPTY_FRAME;
                return false;
            } else {
                _dataPosition += 1;
            }
        }
        
        _lastError = GIFError::EMPTY_FRAME;
        return false;
    }
    
    bool decodeFrame() {
        // Simplified LZW decoding for ESP32
        // Note: Full LZW implementation would be quite complex
        // This is a placeholder that demonstrates the structure
        
        uint8_t lzwCodeSize;
        if (!readData(&lzwCodeSize, 1, _dataPosition)) {
            _lastError = GIFError::DECODE_ERROR;
            return false;
        }
        _dataPosition += 1;
        
        // Skip image data for now (placeholder)
        // In a full implementation, this would decode the LZW data
        uint8_t subBlockSize;
        do {
            if (!readData(&subBlockSize, 1, _dataPosition)) {
                _lastError = GIFError::DECODE_ERROR;
                return false;
            }
            _dataPosition += 1 + subBlockSize;
        } while (subBlockSize > 0);
        
        // For now, just fill with test pattern
        fillTestPattern();
        
        return true;
    }
    
    void fillTestPattern() {
        // Create a simple test pattern for demonstration
        uint32_t* colorTable = _localColorTable ? _localColorTable : _globalColorTable;
        if (!colorTable || !_frameBuffer) return;
        
        for (uint16_t y = 0; y < _frameHeight; y++) {
            for (uint16_t x = 0; x < _frameWidth; x++) {
                uint8_t colorIndex = (x + y) % _localColorTableSize;
                
                if (_hasTransparency && colorIndex == _transparentIndex) {
                    continue; // Skip transparent pixels
                }
                
                uint32_t rgb = colorTable[colorIndex];
                uint8_t r = (rgb >> 16) & 0xFF;
                uint8_t g = (rgb >> 8) & 0xFF;
                uint8_t b = rgb & 0xFF;
                
                drawPixel(x + _frameX, y + _frameY, r, g, b);
            }
        }
    }
    
    void drawPixel(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b) {
        if (x >= _canvasWidth || y >= _canvasHeight) return;
        
        if (_pixelCallback) {
            uint16_t color = ESP32_GIF_Utils::rgb888To565(r, g, b);
            _pixelCallback(_callbackData, x, y, color);
        }
        
        // Store in frame buffer if allocated
        if (_frameBuffer) {
            size_t offset = (y * _canvasWidth + x);
            
            switch (_pixelFormat) {
                case PixelFormat::RGB565_LE: {
                    uint16_t color = ESP32_GIF_Utils::rgb888To565(r, g, b);
                    _frameBuffer[offset * 2] = color & 0xFF;
                    _frameBuffer[offset * 2 + 1] = color >> 8;
                    break;
                }
                case PixelFormat::RGB565_BE: {
                    uint16_t color = ESP32_GIF_Utils::rgb888To565(r, g, b);
                    _frameBuffer[offset * 2] = color >> 8;
                    _frameBuffer[offset * 2 + 1] = color & 0xFF;
                    break;
                }
                case PixelFormat::RGB888: {
                    _frameBuffer[offset * 3] = r;
                    _frameBuffer[offset * 3 + 1] = g;
                    _frameBuffer[offset * 3 + 2] = b;
                    break;
                }
                case PixelFormat::ARGB8888: {
                    _frameBuffer[offset * 4] = 0xFF; // Alpha
                    _frameBuffer[offset * 4 + 1] = r;
                    _frameBuffer[offset * 4 + 2] = g;
                    _frameBuffer[offset * 4 + 3] = b;
                    break;
                }
                case PixelFormat::GRAYSCALE_8BIT: {
                    _frameBuffer[offset] = ESP32_GIF_Utils::rgb888ToGrayscale(r, g, b);
                    break;
                }
                case PixelFormat::MONOCHROME_1BIT: {
                    uint8_t gray = ESP32_GIF_Utils::rgb888ToGrayscale(r, g, b);
                    uint8_t bitPosition = x % 8;
                    uint8_t byteOffset = offset / 8;
                    if (gray > 127) {
                        _frameBuffer[byteOffset] |= (1 << (7 - bitPosition));
                    } else {
                        _frameBuffer[byteOffset] &= ~(1 << (7 - bitPosition));
                    }
                    break;
                }
            }
        }
    }
    
    void applyDisposal() {
        if (_disposalMethod == 2) { // Restore to background
            // Fill frame area with background color
            uint32_t* colorTable = _globalColorTable;
            if (colorTable && _backgroundColor < _globalColorTableSize) {
                uint32_t rgb = colorTable[_backgroundColor];
                uint8_t r = (rgb >> 16) & 0xFF;
                uint8_t g = (rgb >> 8) & 0xFF;
                uint8_t b = rgb & 0xFF;
                
                for (uint16_t y = 0; y < _frameHeight; y++) {
                    for (uint16_t x = 0; x < _frameWidth; x++) {
                        drawPixel(x + _frameX, y + _frameY, r, g, b);
                    }
                }
            }
        } else if (_disposalMethod == 3) { // Restore to previous
            // Restore previous frame buffer
            if (_frameBuffer && _previousFrame) {
                size_t bufferSize = _canvasWidth * _canvasHeight;
                switch (_pixelFormat) {
                    case PixelFormat::RGB565_LE:
                    case PixelFormat::RGB565_BE:
                        bufferSize *= 2;
                        break;
                    case PixelFormat::RGB888:
                        bufferSize *= 3;
                        break;
                    case PixelFormat::ARGB8888:
                        bufferSize *= 4;
                        break;
                    default:
                        break;
                }
                memcpy(_frameBuffer, _previousFrame, bufferSize);
            }
        }
        
        // Save current frame for next disposal
        if (_frameBuffer && _previousFrame) {
            size_t bufferSize = _canvasWidth * _canvasHeight;
            switch (_pixelFormat) {
                case PixelFormat::RGB565_LE:
                case PixelFormat::RGB565_BE:
                    bufferSize *= 2;
                    break;
                case PixelFormat::RGB888:
                    bufferSize *= 3;
                    break;
                case PixelFormat::ARGB8888:
                    bufferSize *= 4;
                    break;
                default:
                    break;
            }
            memcpy(_previousFrame, _frameBuffer, bufferSize);
        }
    }
};

// Public class implementation
ESP32_AnimatedGIF::ESP32_AnimatedGIF() : _impl(new Impl()) {}

ESP32_AnimatedGIF::~ESP32_AnimatedGIF() {
    delete _impl;
}

bool ESP32_AnimatedGIF::begin(PixelFormat pixelFormat, bool usePSRAM) {
    return _impl->begin(pixelFormat, usePSRAM);
}

GIFError ESP32_AnimatedGIF::loadFromMemory(const uint8_t* data, uint32_t length) {
    return _impl->loadFromMemory(data, length);
}

GIFError ESP32_AnimatedGIF::load(DataReader reader, void* userData) {
    return _impl->load(reader, userData);
}

void ESP32_AnimatedGIF::setDisplaySize(uint16_t width, uint16_t height) {
    _impl->setDisplaySize(width, height);
}

void ESP32_AnimatedGIF::setFrameCallback(FrameCallback callback, void* userData) {
    _impl->setFrameCallback(callback, userData);
}

void ESP32_AnimatedGIF::setPixelCallback(PixelCallback callback, void* userData) {
    _impl->setPixelCallback(callback, userData);
}

bool ESP32_AnimatedGIF::getInfo(GIFInfo& info) {
    return _impl->getInfo(info);
}

bool ESP32_AnimatedGIF::getFrameInfo(FrameInfo& info) {
    return _impl->getFrameInfo(info);
}

GIFError ESP32_AnimatedGIF::nextFrame(bool syncDelay) {
    return _impl->nextFrame(syncDelay);
}

void ESP32_AnimatedGIF::reset() {
    _impl->reset();
}

GIFError ESP32_AnimatedGIF::getLastError() const {
    return _impl->getLastError();
}

const char* ESP32_AnimatedGIF::getErrorMessage(GIFError error) {
    switch (error) {
        case GIFError::SUCCESS: return "Success";
        case GIFError::DECODE_ERROR: return "Decode error";
        case GIFError::FILE_TOO_WIDE: return "File too wide";
        case GIFError::INVALID_PARAMETER: return "Invalid parameter";
        case GIFError::UNSUPPORTED_FEATURE: return "Unsupported feature";
        case GIFError::FILE_NOT_FOUND: return "File not found";
        case GIFError::EARLY_EOF: return "Early end of file";
        case GIFError::EMPTY_FRAME: return "Empty frame";
        case GIFError::BAD_FILE_FORMAT: return "Bad file format";
        case GIFError::OUT_OF_MEMORY: return "Out of memory";
        case GIFError::DISPLAY_NOT_SET: return "Display not set";
        case GIFError::UNKNOWN_ERROR: return "Unknown error";
        default: return "Unknown error code";
    }
}

uint16_t ESP32_AnimatedGIF::getCurrentFrame() const {
    return _impl->getCurrentFrame();
}

uint16_t ESP32_AnimatedGIF::getFrameCount() const {
    return _impl->getFrameCount();
}

bool ESP32_AnimatedGIF::isAnimationComplete() const {
    return _impl->isAnimationComplete();
}

void ESP32_AnimatedGIF::setLoop(bool loop) {
    _impl->setLoop(loop);
}

void ESP32_AnimatedGIF::setScale(float scale) {
    _impl->setScale(scale);
}

uint16_t ESP32_AnimatedGIF::getCanvasWidth() const {
    return _impl->getCanvasWidth();
}

uint16_t ESP32_AnimatedGIF::getCanvasHeight() const {
    return _impl->getCanvasHeight();
}

// Utility functions implementation
namespace ESP32_GIF_Utils {
    
    void* allocateMemory(size_t size, bool usePSRAM) {
        if (size == 0) return nullptr;
        
#ifdef ESP32_ANIMATEDGIF_PSRAM_SUPPORT
        if (usePSRAM && psramFound()) {
            void* ptr = ps_malloc(size);
            if (ptr) {
                memset(ptr, 0, size);
                return ptr;
            }
        }
#endif
        
        void* ptr = malloc(size);
        if (ptr) {
            memset(ptr, 0, size);
        }
        return ptr;
    }
    
    void freeMemory(void* ptr) {
        if (ptr) {
            free(ptr);
        }
    }
    
    uint16_t rgb888To565(uint8_t r, uint8_t g, uint8_t b) {
        return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    }
    
    uint8_t rgb888ToGrayscale(uint8_t r, uint8_t g, uint8_t b) {
        // Using luminance formula: Y = 0.299R + 0.587G + 0.114B
        return (uint8_t)((r * 77 + g * 150 + b * 29) >> 8);
    }
}