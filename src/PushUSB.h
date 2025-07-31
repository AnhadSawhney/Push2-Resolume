#pragma once

#include "RtMidi.h"
#include <vector>
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <iostream>
#include <cstring>
#include <chrono>
#include <algorithm>

// Rename to avoid conflict with RtMidi's MidiMessage
struct PushMidiMessage {
    std::vector<uint8_t> data;
    
    PushMidiMessage(const std::vector<uint8_t>& msg) : data(msg) {}
    PushMidiMessage(const uint8_t* rawData, size_t size) : data(rawData, rawData + size) {}
    
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
    
    uint8_t getNote() const { return (data.size() >= 2) ? data[1] : 0; }
    uint8_t getVelocity() const { return (data.size() >= 3) ? data[2] : 0; }
    uint8_t getController() const { return (data.size() >= 2) ? data[1] : 0; }
    uint8_t getValue() const { return (data.size() >= 3) ? data[2] : 0; }
    uint16_t getPitchBend() const {
        if (data.size() >= 3) {
            // Combine LSB and MSB to get 14-bit pitch bend value
            return static_cast<uint16_t>(data[1]) | (static_cast<uint16_t>(data[2]) << 7);
        }
        return 8192; // Center value
    }
};

class PushUSB {
private:
    // RtMidi objects
    std::unique_ptr<RtMidiIn> midiIn;
    std::unique_ptr<RtMidiOut> midiOut;
    
    std::atomic<bool> isConnected;
    std::function<void(const PushMidiMessage&)> midiCallback;  // Updated type
    
    // Static callback for RtMidi (C-style callback required)
    // This converts RtMidi callbacks to our callback system
    static void midiInputCallback(double timeStamp, std::vector<unsigned char>* message, void* userData) {
        PushUSB* pushUSB = static_cast<PushUSB*>(userData);
        if (pushUSB && pushUSB->midiCallback && !message->empty()) {
            // Convert RtMidi message to our PushMidiMessage
            pushUSB->midiCallback(PushMidiMessage(*message));
        }
    }
    
    // Find Push 2 MIDI ports
    bool findPush2MidiPorts(unsigned int& inputPort, unsigned int& outputPort) {
        // Check input ports
        bool foundInput = false, foundOutput = false;
        
        unsigned int inputPortCount = midiIn->getPortCount();
        for (unsigned int i = 0; i < inputPortCount; i++) {
            std::string portName = midiIn->getPortName(i);
            if (portName.find("Push 2") != std::string::npos || 
                portName.find("Ableton Push 2") != std::string::npos) {
                inputPort = i;
                foundInput = true;
                std::cout << "Found Push 2 input port: " << portName << std::endl;
                break;
            }
        }
        
        unsigned int outputPortCount = midiOut->getPortCount();
        for (unsigned int i = 0; i < outputPortCount; i++) {
            std::string portName = midiOut->getPortName(i);
            if (portName.find("Push 2") != std::string::npos ||
                portName.find("Ableton Push 2") != std::string::npos) {
                outputPort = i;
                foundOutput = true;
                std::cout << "Found Push 2 output port: " << portName << std::endl;
                break;
            }
        }
        
        return foundInput && foundOutput;
    }
    
public:
    PushUSB() : isConnected(false) {
        try {
            midiIn = std::make_unique<RtMidiIn>();
            midiOut = std::make_unique<RtMidiOut>();
        } catch (RtMidiError& error) {
            std::cerr << "RtMidi initialization error: " << error.getMessage() << std::endl;
        }
    }
    
    ~PushUSB() {
        disconnect();
    }
    
    bool initialize() {
        return midiIn && midiOut;
    }
    
    bool connect() {
        if (isConnected.load()) {
            return true;
        }
        
        if (!midiIn || !midiOut) {
            std::cerr << "MIDI objects not initialized" << std::endl;
            return false;
        }
        
        try {
            unsigned int inputPort, outputPort;
            if (!findPush2MidiPorts(inputPort, outputPort)) {
                std::cerr << "Could not find Push 2 MIDI ports" << std::endl;
                std::cout << "\nAvailable MIDI Input Ports:" << std::endl;
                for (unsigned int i = 0; i < midiIn->getPortCount(); i++) {
                    std::cout << "  " << i << ": " << midiIn->getPortName(i) << std::endl;
                }
                std::cout << "\nAvailable MIDI Output Ports:" << std::endl;
                for (unsigned int i = 0; i < midiOut->getPortCount(); i++) {
                    std::cout << "  " << i << ": " << midiOut->getPortName(i) << std::endl;
                }
                return false;
            }
            
            // Open MIDI ports
            midiIn->openPort(inputPort);
            midiOut->openPort(outputPort);
            
            // Set up input callback - THIS IS IMPORTANT FOR RECEIVING INPUT
            midiIn->setCallback(&midiInputCallback, this);
            midiIn->ignoreTypes(false, false, false); // Don't ignore any message types
            
            isConnected.store(true);
            std::cout << "Successfully connected to Push 2 MIDI ports" << std::endl;
            
            // Test the connection
            clearAllPads();
            
            return true;
            
        } catch (RtMidiError& error) {
            std::cerr << "MIDI connection error: " << error.getMessage() << std::endl;
            return false;
        }
    }
    
    void disconnect() {
        if (!isConnected.load()) {
            return;
        }
        
        try {
            if (midiIn && midiIn->isPortOpen()) {
                midiIn->closePort();
            }
            if (midiOut && midiOut->isPortOpen()) {
                midiOut->closePort();
            }
            
            isConnected.store(false);
            std::cout << "Disconnected from Push 2 MIDI" << std::endl;
            
        } catch (RtMidiError& error) {
            std::cerr << "MIDI disconnect error: " << error.getMessage() << std::endl;
        }
    }
    
    bool isDeviceConnected() const { 
        return isConnected.load(); 
    }
    
    // Set callback for receiving MIDI input from Push 2
    void setMidiCallback(std::function<void(const PushMidiMessage&)> callback) {
        midiCallback = callback;
    }
    
    // Send raw MIDI message
    bool sendMidiMessage(const std::vector<uint8_t>& message) {
        if (!isConnected.load() || !midiOut || !midiOut->isPortOpen()) {
            return false;
        }
        
        try {
            midiOut->sendMessage(&message);
            return true;
        } catch (RtMidiError& error) {
            std::cerr << "MIDI send error: " << error.getMessage() << std::endl;
            return false;
        }
    }
    
    // Send SysEx message
    bool sendSysEx(const std::vector<uint8_t>& sysex) {
        return sendMidiMessage(sysex);
    }

    void reapplyPalette() {
        std::vector<uint8_t> sysex = {
            0xF0, 0x00, 0x21, 0x1D, 0x01, 0x01, 0x05, 0xF7
        };
        sendSysEx(sysex);
    }

    // Send Push 2 palette sysex command
    void setPaletteEntry(uint8_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
        // Split each color into LSB (7 bits) and MSB (1 bit)
        auto split = [](uint8_t v) -> std::pair<uint8_t, uint8_t> {
            return { static_cast<uint8_t>(v & 0x7F), static_cast<uint8_t>((v >> 7) & 0x01) };
        };
        auto [r_lsb, r_msb] = split(r);
        auto [g_lsb, g_msb] = split(g);
        auto [b_lsb, b_msb] = split(b);
        auto [w_lsb, w_msb] = split(w);

        std::vector<uint8_t> sysex = {
            0xF0, 0x00, 0x21, 0x1D, 0x01, 0x01, 0x03, // header + command
            index,
            r_lsb, r_msb,
            g_lsb, g_msb,
            b_lsb, b_msb,
            w_lsb, w_msb,
            0xF7
        };
        sendSysEx(sysex);
        reapplyPalette();
    }
    
    bool setPadColorIndex(int padNumber, uint8_t colorIndex) {
        if (!isConnected.load() || padNumber < 36 || padNumber > 99) {
            return false;
        }

        return sendMidiMessage({0x90, static_cast<uint8_t>(padNumber), colorIndex});
    }

    // Set button color (control change)
    bool setButtonColorIndex(int buttonNumber, uint8_t colorIndex) {
        if (!isConnected.load()) {
            return false;
        }

        return sendMidiMessage({0xB0, static_cast<uint8_t>(buttonNumber), colorIndex});
    }
    
    // Clear all pads
    bool clearAllPads() {
        bool success = true;
        for (int note = 36; note <= 99; note++) {
            success &= sendMidiMessage({0x90, static_cast<uint8_t>(note), 0x00});
        }
        return success;
    }

    // Get palette entry from Push 2 (blocking, returns true if reply received)
    bool getPaletteEntry(uint8_t index, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& w) {
        // Send sysex request: F0 00 21 1D 01 01 04 00 <index> F7
        std::vector<uint8_t> sysex = {
            0xF0, 0x00, 0x21, 0x1D, 0x01, 0x01, 0x04, 0x00, index, 0xF7
        };

        // We'll need to capture the reply. This is a blocking call and not thread safe.
        // In a real implementation, this should be async/event-driven.
        std::atomic<bool> gotReply{false};
        uint8_t replyR = 0, replyG = 0, replyB = 0, replyW = 0;

        auto oldCallback = midiCallback;
        midiCallback = [&](const PushMidiMessage& msg) {
            // Expect: F0 00 21 1D 01 01 04 00 <index> r g b w F7
            if (msg.data.size() >= 13 &&
                msg.data[0] == 0xF0 && msg.data[1] == 0x00 && msg.data[2] == 0x21 &&
                msg.data[3] == 0x1D && msg.data[4] == 0x01 && msg.data[5] == 0x01 &&
                msg.data[6] == 0x04 && msg.data[7] == 0x00 && msg.data[8] == index &&
                msg.data[12] == 0xF7) {
                replyR = msg.data[9];
                replyG = msg.data[10];
                replyB = msg.data[11];
                replyW = msg.data[12 - 1]; // msg.data[11] is b, msg.data[12] is w
                gotReply = true;
            }
        };

        sendSysEx(sysex);

        // Wait for reply (timeout after 100ms)
        for (int i = 0; i < 100; ++i) {
            if (gotReply) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        midiCallback = oldCallback;

        if (gotReply) {
            r = replyR;
            g = replyG;
            b = replyB;
            w = replyW;
            return true;
        }
        return false;
    }

    // Set just the RGB part of a palette entry (preserve W)
    void setPaletteEntryRGB(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
        uint8_t oldR = 0, oldG = 0, oldB = 0, oldW = 0;
        if (getPaletteEntry(index, oldR, oldG, oldB, oldW)) {
            setPaletteEntry(index, r, g, b, oldW);
        } else {
            setPaletteEntry(index, r, g, b, 0);
        }
    }

    // Set just the BW part of a palette entry (preserve RGB)
    void setPaletteEntryBW(uint8_t index, uint8_t w) {
        uint8_t oldR = 0, oldG = 0, oldB = 0, oldW = 0;
        if (getPaletteEntry(index, oldR, oldG, oldB, oldW)) {
            setPaletteEntry(index, oldR, oldG, oldB, w);
        } else {
            setPaletteEntry(index, 0, 0, 0, w);
        }
    }

    // Set touch strip LEDs (31 LEDs, values 0-7)
    bool setTouchStripLEDs(const uint8_t ledValues[31]) {
        if (!isConnected.load()) {
            return false;
        }

        // Validate LED values are in range 0-7
        for (int i = 0; i < 31; ++i) {
            if (ledValues[i] > 7) {
                return false;
            }
        }

        // Pack LED values according to specification:
        // Each byte packs two 3-bit LED values, with LED 30 getting special treatment
        std::vector<uint8_t> sysex = {
            0xF0, 0x00, 0x21, 0x1D, 0x01, 0x01, 0x19 // header + command ID
        };

        // Pack LEDs 0-29 into 15 bytes (2 LEDs per byte)
        for (int i = 0; i < 15; i++) {
            uint8_t led_low = ledValues[i * 2];     // even index LED
            uint8_t led_high = ledValues[i * 2 + 1]; // odd index LED
            uint8_t packed = (led_high << 3) | led_low;
            sysex.push_back(packed);
        }

        // Pack LED 30 into the last byte (bits 2-0, with bits 7-3 as zero)
        uint8_t last_byte = ledValues[30] & 0x07;
        sysex.push_back(last_byte);

        sysex.push_back(0xF7); // end of sysex

        return sendSysEx(sysex);
    }

    // Configure touch strip for host control via sysex commands
    bool configureTouchStrip() {
        if (!isConnected.load()) {
            return false;
        }

        // Configuration flags:
        // bit 0: LEDs Controlled By (1 = Host)
        // bit 1: Host Sends (1 = Sysex)
        // bit 2: Values Sent As (0 = Pitch Bend)
        // bit 3: LEDs Show (1 = Point)
        // bit 4: Bar Starts At (0 = Bottom)
        // bit 5: Do Autoreturn (0 = No)
        // bit 6: Autoreturn To (0 = Bottom)
        //
        // Setting to 0x0B (00001011) = host control, sysex, pitch bend, point, center, autoreturn, bottom
        uint8_t config = 0x0B;

        std::vector<uint8_t> sysex = {
            0xF0, 0x00, 0x21, 0x1D, 0x01, 0x01, 0x17, // header + command ID
            config,
            0xF7
        };

        return sendSysEx(sysex);
    }
};