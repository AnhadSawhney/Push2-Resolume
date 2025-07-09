#ifndef PUSH_USB_H
#define PUSH_USB_H

#include <libusb.h>
#include <vector>
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <iostream>
#include <cstring>
#include <chrono>

// Push 2 USB identifiers from documentation
const uint16_t PUSH2_VENDOR_ID = 0x2982;   // Ableton
const uint16_t PUSH2_PRODUCT_ID = 0x1967;  // Push 2

// Push 2 USB endpoints
const uint8_t PUSH2_BULK_EP_OUT = 0x01;      // Display data out
const uint8_t PUSH2_INTERRUPT_EP_IN = 0x81;  // MIDI data in
const uint8_t PUSH2_INTERRUPT_EP_OUT = 0x02; // MIDI data out

// Push 2 display constants from documentation
const int PUSH2_DISPLAY_WIDTH = 960;
const int PUSH2_DISPLAY_HEIGHT = 160;
const int PUSH2_DISPLAY_BYTES_PER_PIXEL = 2; // RGB565 format
const int PUSH2_DISPLAY_LINE_SIZE = 2048;    // 1920 pixel data + 128 filler bytes
const int PUSH2_DISPLAY_BUFFER_SIZE = PUSH2_DISPLAY_HEIGHT * PUSH2_DISPLAY_LINE_SIZE;
const int PUSH2_DISPLAY_FRAME_HEADER_SIZE = 16;
const int PUSH2_DISPLAY_TRANSFER_SIZE = 16384; // Transfer chunk size

// MIDI constants
const int PUSH2_MIDI_BUFFER_SIZE = 64;
const int PUSH2_MIDI_TIMEOUT_MS = 100;
const int PUSH2_DISPLAY_TIMEOUT_MS = 1000;

// XOR pattern for display data from documentation
const uint32_t PUSH2_XOR_PATTERN = 0xFFE7F3E7;

// MIDI message structure
struct MidiMessage {
    std::vector<uint8_t> data;
    
    MidiMessage(const std::vector<uint8_t>& msg) : data(msg) {}
    MidiMessage(const uint8_t* rawData, size_t size) : data(rawData, rawData + size) {}
    
    bool isNoteOn() const { 
        return !data.empty() && (data[0] & 0xF0) == 0x90 && data.size() >= 3 && data[2] > 0; 
    }
    bool isNoteOff() const { 
        return !data.empty() && ((data[0] & 0xF0) == 0x80 || 
               ((data[0] & 0xF0) == 0x90 && data.size() >= 3 && data[2] == 0)); 
    }
    bool isControlChange() const { 
        return !data.empty() && (data[0] & 0xF0) == 0xB0; 
    }
    bool isPitchBend() const {
        return !data.empty() && (data[0] & 0xF0) == 0xE0;
    }
    bool isChannelPressure() const {
        return !data.empty() && (data[0] & 0xF0) == 0xD0;
    }
    bool isPolyPressure() const {
        return !data.empty() && (data[0] & 0xF0) == 0xA0;
    }
    
    uint8_t getNote() const { return (data.size() >= 2) ? data[1] : 0; }
    uint8_t getVelocity() const { return (data.size() >= 3) ? data[2] : 0; }
    uint8_t getController() const { return (data.size() >= 2) ? data[1] : 0; }
    uint8_t getValue() const { return (data.size() >= 3) ? data[2] : 0; }
    uint16_t getPitchBend() const {
        if (data.size() >= 3) {
            return (uint16_t(data[2]) << 7) | data[1];
        }
        return 8192; // center value
    }
};

class PushUSB {
private:
    libusb_context* usbContext;
    libusb_device_handle* deviceHandle;
    
    std::atomic<bool> isConnected;
    std::atomic<bool> shouldStop;
    
    std::thread midiInThread;
    std::mutex midiMutex;
    std::mutex displayMutex;
    
    // Callback for incoming MIDI messages
    std::function<void(const MidiMessage&)> midiCallback;
    
    // Display frame buffer
    std::vector<uint8_t> displayBuffer;
    
    void midiInputThreadFunc() {
        uint8_t buffer[PUSH2_MIDI_BUFFER_SIZE];
        
        while (!shouldStop.load() && isConnected.load()) {
            int transferred = 0;
            int result = libusb_interrupt_transfer(deviceHandle, PUSH2_INTERRUPT_EP_IN, 
                                                 buffer, PUSH2_MIDI_BUFFER_SIZE, 
                                                 &transferred, PUSH2_MIDI_TIMEOUT_MS);
            
            if (result == 0 && transferred > 0) {
                // Parse MIDI data - Push 2 sends 4-byte USB MIDI packets
                for (int i = 0; i < transferred; i += 4) {
                    if (i + 3 < transferred) {
                        uint8_t cable = buffer[i] >> 4;
                        uint8_t code = buffer[i] & 0x0F;
                        
                        if (code != 0) { // Valid MIDI message
                            std::vector<uint8_t> midiData;
                            
                            // Determine message length based on code
                            int messageLength = 0;
                            switch (code) {
                                case 0x8: case 0x9: case 0xA: case 0xB: case 0xE: // 3-byte messages
                                    messageLength = 3;
                                    break;
                                case 0xC: case 0xD: // 2-byte messages
                                    messageLength = 2;
                                    break;
                                case 0xF: // System messages
                                    if (buffer[i + 1] == 0xF0) { // SysEx start
                                        messageLength = 3;
                                    } else {
                                        messageLength = 1;
                                    }
                                    break;
                            }
                            
                            for (int j = 1; j <= messageLength && (i + j) < transferred; j++) {
                                if (buffer[i + j] != 0 || j == messageLength) {
                                    midiData.push_back(buffer[i + j]);
                                }
                            }
                            
                            if (!midiData.empty() && midiCallback) {
                                std::lock_guard<std::mutex> lock(midiMutex);
                                midiCallback(MidiMessage(midiData));
                            }
                        }
                    }
                }
            } else if (result != LIBUSB_ERROR_TIMEOUT) {
                if (result != LIBUSB_ERROR_NO_DEVICE && result != LIBUSB_ERROR_IO) {
                    std::cerr << "MIDI input error: " << libusb_error_name(result) << std::endl;
                }
                if (result == LIBUSB_ERROR_NO_DEVICE) {
                    isConnected.store(false);
                }
                break;
            }
        }
    }
    
    // Convert RGB888 to RGB565 (as per documentation)
    uint16_t rgbToRgb565(uint8_t r, uint8_t g, uint8_t b) {
        return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    }
    
    // XOR pixel data with shaping pattern (as per documentation)
    void xorPixelData(uint8_t* data, size_t size) {
        uint8_t pattern[4] = {0xE7, 0xF3, 0xE7, 0xFF};
        for (size_t i = 0; i < size; i++) {
            data[i] ^= pattern[i % 4];
        }
    }
    
    // Send frame header (as per documentation)
    bool sendFrameHeader() {
        uint8_t header[PUSH2_DISPLAY_FRAME_HEADER_SIZE] = {
            0xFF, 0xCC, 0xAA, 0x88,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00
        };
        
        int transferred = 0;
        int result = libusb_bulk_transfer(deviceHandle, PUSH2_BULK_EP_OUT,
                                        header, PUSH2_DISPLAY_FRAME_HEADER_SIZE,
                                        &transferred, PUSH2_DISPLAY_TIMEOUT_MS);
        
        return result == 0 && transferred == PUSH2_DISPLAY_FRAME_HEADER_SIZE;
    }
    
public:
    PushUSB() : usbContext(nullptr), deviceHandle(nullptr), 
                isConnected(false), shouldStop(false) {
        displayBuffer.resize(PUSH2_DISPLAY_BUFFER_SIZE, 0);
    }
    
    ~PushUSB() {
        disconnect();
        if (usbContext) {
            libusb_exit(usbContext);
        }
    }
    
    bool initialize() {
        int result = libusb_init(&usbContext);
        if (result < 0) {
            std::cerr << "Failed to initialize libusb: " << libusb_error_name(result) << std::endl;
            return false;
        }
        
        // Set debug level to warnings only
        libusb_set_option(usbContext, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_WARNING);
        return true;
    }
    
    bool connect() {
        if (isConnected.load()) {
            return true;
        }
        
        if (!usbContext) {
            std::cerr << "USB context not initialized" << std::endl;
            return false;
        }
        
        std::cout << "Looking for Ableton Push 2..." << std::endl;
        
        // Find Push 2 device
        deviceHandle = libusb_open_device_with_vid_pid(usbContext, PUSH2_VENDOR_ID, PUSH2_PRODUCT_ID);
        if (!deviceHandle) {
            std::cerr << "Push 2 device not found" << std::endl;
            return false;
        }
        
        // Claim interface 0 (MIDI and display interface)
        int result = libusb_claim_interface(deviceHandle, 0);
        if (result < 0) {
            std::cerr << "Failed to claim interface: " << libusb_error_name(result) << std::endl;
            libusb_close(deviceHandle);
            deviceHandle = nullptr;
            return false;
        }
        
        isConnected.store(true);
        shouldStop.store(false);
        
        // Start MIDI input thread
        midiInThread = std::thread(&PushUSB::midiInputThreadFunc, this);
        
        std::cout << "Successfully connected to Push 2" << std::endl;
        
        // Initialize device
        clearDisplay();
        clearAllPads();
        setDisplayBrightness(128); // Medium brightness
        
        return true;
    }
    
    void disconnect() {
        if (!isConnected.load()) {
            return;
        }
        
        std::cout << "Disconnecting from Push 2..." << std::endl;
        
        shouldStop.store(true);
        
        if (midiInThread.joinable()) {
            midiInThread.join();
        }
        
        if (deviceHandle) {
            libusb_release_interface(deviceHandle, 0);
            libusb_close(deviceHandle);
            deviceHandle = nullptr;
        }
        
        isConnected.store(false);
        std::cout << "Disconnected from Push 2" << std::endl;
    }
    
    bool isDeviceConnected() const { 
        return isConnected.load(); 
    }
    
    void setMidiCallback(std::function<void(const MidiMessage&)> callback) {
        std::lock_guard<std::mutex> lock(midiMutex);
        midiCallback = callback;
    }
    
    // Send raw MIDI message
    bool sendMidiMessage(const std::vector<uint8_t>& message) {
        if (!isConnected.load() || !deviceHandle || message.empty()) {
            return false;
        }
        
        // Convert to USB MIDI packet format
        uint8_t packet[4] = {0, 0, 0, 0};
        
        if (message.size() <= 3) {
            // Determine cable and code based on message type
            uint8_t status = message[0];
            uint8_t cable = 0; // Use cable 0
            uint8_t code = 0;
            
            if ((status & 0xF0) == 0x80) code = 0x8; // Note off
            else if ((status & 0xF0) == 0x90) code = 0x9; // Note on
            else if ((status & 0xF0) == 0xA0) code = 0xA; // Poly pressure
            else if ((status & 0xF0) == 0xB0) code = 0xB; // Control change
            else if ((status & 0xF0) == 0xC0) code = 0xC; // Program change
            else if ((status & 0xF0) == 0xD0) code = 0xD; // Channel pressure
            else if ((status & 0xF0) == 0xE0) code = 0xE; // Pitch bend
            else if (status == 0xF0) code = 0x4; // SysEx start
            else code = 0xF; // Other system messages
            
            packet[0] = (cable << 4) | code;
            for (size_t i = 0; i < message.size() && i < 3; i++) {
                packet[i + 1] = message[i];
            }
        }
        
        int transferred = 0;
        int result = libusb_interrupt_transfer(deviceHandle, PUSH2_INTERRUPT_EP_OUT,
                                             packet, 4, &transferred, PUSH2_MIDI_TIMEOUT_MS);
        
        return result == 0 && transferred == 4;
    }
    
    // Send SysEx message
    bool sendSysEx(const std::vector<uint8_t>& sysex) {
        if (!isConnected.load() || !deviceHandle || sysex.empty()) {
            return false;
        }
        
        // For SysEx messages, we need to send multiple USB MIDI packets
        bool success = true;
        size_t pos = 0;
        
        while (pos < sysex.size() && success) {
            uint8_t packet[4] = {0, 0, 0, 0};
            size_t remaining = sysex.size() - pos;
            
            if (pos == 0) {
                // First packet - SysEx start
                packet[0] = 0x04; // Cable 0, SysEx start
                packet[1] = sysex[pos++];
                if (pos < sysex.size()) packet[2] = sysex[pos++];
                if (pos < sysex.size()) packet[3] = sysex[pos++];
            } else if (remaining <= 3) {
                // Last packet
                if (remaining == 1) packet[0] = 0x05; // SysEx end 1 byte
                else if (remaining == 2) packet[0] = 0x06; // SysEx end 2 bytes
                else packet[0] = 0x07; // SysEx end 3 bytes
                
                for (size_t i = 0; i < remaining; i++) {
                    packet[i + 1] = sysex[pos++];
                }
            } else {
                // Continue packet
                packet[0] = 0x04; // Cable 0, SysEx continue
                packet[1] = sysex[pos++];
                packet[2] = sysex[pos++];
                packet[3] = sysex[pos++];
            }
            
            int transferred = 0;
            int result = libusb_interrupt_transfer(deviceHandle, PUSH2_INTERRUPT_EP_OUT,
                                                 packet, 4, &transferred, PUSH2_MIDI_TIMEOUT_MS);
            success = (result == 0 && transferred == 4);
        }
        
        return success;
    }
    
    // Set pad RGB color (note numbers 36-99 as per documentation)
    bool setPadColor(int padNote, uint8_t red, uint8_t green, uint8_t blue) {
        if (!isConnected.load() || padNote < 36 || padNote > 99) {
            return false;
        }
        
        // Use SysEx to set RGB color
        std::vector<uint8_t> sysex = {
            0xF0, 0x00, 0x21, 0x1D, 0x01, 0x01, 0x03, // Set LED Color Palette Entry
            0x7F, // Color index 127 (temporary)
            (red >> 1), 0x00,     // Red (7+1 bits)
            (green >> 1), 0x00,   // Green (7+1 bits) 
            (blue >> 1), 0x00,    // Blue (7+1 bits)
            0x00, 0x00,           // White (unused)
            0xF7
        };
        
        if (!sendSysEx(sysex)) {
            return false;
        }
        
        // Send note on with color index 127
        return sendMidiMessage({0x90, (uint8_t)padNote, 0x7F});
    }
    
    // Set button color (control change)
    bool setButtonColor(int controller, uint8_t colorIndex) {
        if (!isConnected.load()) {
            return false;
        }
        
        return sendMidiMessage({0xB0, (uint8_t)controller, colorIndex});
    }
    
    // Clear all pads
    bool clearAllPads() {
        bool success = true;
        for (int note = 36; note <= 99; note++) {
            success &= sendMidiMessage({0x90, (uint8_t)note, 0x00});
        }
        return success;
    }
    
    // Set display brightness (0-255)
    bool setDisplayBrightness(uint8_t brightness) {
        std::vector<uint8_t> sysex = {
            0xF0, 0x00, 0x21, 0x1D, 0x01, 0x01, 0x08, // Set Display Brightness
            (uint8_t)(brightness & 0x7F),              // Lower 7 bits
            (uint8_t)((brightness >> 7) & 0x01),       // Upper 1 bit
            0xF7
        };
        
        return sendSysEx(sysex);
    }
    
    // Send display frame (RGB565 format, 960x160 pixels)
    bool sendDisplayFrame(const uint16_t* pixels) {
        if (!isConnected.load() || !deviceHandle || !pixels) {
            return false;
        }
        
        std::lock_guard<std::mutex> lock(displayMutex);
        
        // Send frame header
        if (!sendFrameHeader()) {
            return false;
        }
        
        // Prepare display buffer with XOR and line formatting
        for (int line = 0; line < PUSH2_DISPLAY_HEIGHT; line++) {
            uint8_t* lineStart = displayBuffer.data() + (line * PUSH2_DISPLAY_LINE_SIZE);
            
            // Copy pixel data (960 pixels = 1920 bytes)
            const uint16_t* linePixels = pixels + (line * PUSH2_DISPLAY_WIDTH);
            memcpy(lineStart, linePixels, PUSH2_DISPLAY_WIDTH * 2);
            
            // Add 128 filler bytes (already zeroed)
            memset(lineStart + (PUSH2_DISPLAY_WIDTH * 2), 0, 128);
            
            // Apply XOR pattern to the line
            xorPixelData(lineStart, PUSH2_DISPLAY_LINE_SIZE);
        }
        
        // Send pixel data in chunks
        size_t totalSent = 0;
        while (totalSent < PUSH2_DISPLAY_BUFFER_SIZE) {
            size_t chunkSize = std::min((size_t)PUSH2_DISPLAY_TRANSFER_SIZE, 
                                       PUSH2_DISPLAY_BUFFER_SIZE - totalSent);
            
            int transferred = 0;
            int result = libusb_bulk_transfer(deviceHandle, PUSH2_BULK_EP_OUT,
                                            displayBuffer.data() + totalSent, chunkSize,
                                            &transferred, PUSH2_DISPLAY_TIMEOUT_MS);
            
            if (result != 0 || transferred != chunkSize) {
                std::cerr << "Display transfer failed: " << libusb_error_name(result) << std::endl;
                return false;
            }
            
            totalSent += transferred;
        }
        
        return true;
    }
    
    // Clear display (all black)
    bool clearDisplay() {
        std::vector<uint16_t> blackFrame(PUSH2_DISPLAY_WIDTH * PUSH2_DISPLAY_HEIGHT, 0);
        return sendDisplayFrame(blackFrame.data());
    }
    
    // Fill display with solid color
    bool fillDisplay(uint8_t red, uint8_t green, uint8_t blue) {
        uint16_t color = rgbToRgb565(red, green, blue);
        std::vector<uint16_t> colorFrame(PUSH2_DISPLAY_WIDTH * PUSH2_DISPLAY_HEIGHT, color);
        return sendDisplayFrame(colorFrame.data());
    }
    
    // Device inquiry
    bool sendDeviceInquiry() {
        std::vector<uint8_t> inquiry = {0xF0, 0x7E, 0x01, 0x06, 0x01, 0xF7};
        return sendSysEx(inquiry);
    }
};

#endif // PUSH_USB_H