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
    
    uint8_t getNote() const { return (data.size() >= 2) ? data[1] : 0; }
    uint8_t getVelocity() const { return (data.size() >= 3) ? data[2] : 0; }
    uint8_t getController() const { return (data.size() >= 2) ? data[1] : 0; }
    uint8_t getValue() const { return (data.size() >= 3) ? data[2] : 0; }
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

    // Send Push 2 palette sysex command
    void setPaletteEntry(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
        // Ableton Push 2 palette sysex format:
        // F0 00 21 1D 01 01 03 00 <index> <r> <g> <b> F7
        std::vector<uint8_t> sysex = {
            0xF0, 0x00, 0x21, 0x1D, 0x01, 0x01, 0x03, 0x00,
            index, r, g, b, 0xF7
        };
        sendSysEx(sysex);
    }
    
    // Set pad RGB color (note numbers 36-99 as per documentation)
    bool setPadColor(int padNote, uint8_t red, uint8_t green, uint8_t blue) {
        if (!isConnected.load() || padNote < 36 || padNote > 99) {
            return false;
        }
        
        // Use SysEx to set RGB color (from Ableton documentation)
        std::vector<uint8_t> sysex = {
            0xF0, 0x00, 0x21, 0x1D, 0x01, 0x01, 0x03, // Set LED Color Palette Entry
            0x7F, // Color index 127 (temporary)
            static_cast<uint8_t>(red >> 1), 0x00,     // Red (7+1 bits)
            static_cast<uint8_t>(green >> 1), 0x00,   // Green (7+1 bits) 
            static_cast<uint8_t>(blue >> 1), 0x00,    // Blue (7+1 bits)
            0x00, 0x00,           // White (unused)
            0xF7
        };
        
        if (!sendSysEx(sysex)) {
            return false;
        }
        
        // Send note on with color index 127
        return sendMidiMessage({0x90, static_cast<uint8_t>(padNote), 0x7F});
    }
    
    // Set button color (control change)
    bool setButtonColor(int controller, uint8_t colorIndex) {
        if (!isConnected.load()) {
            return false;
        }
        
        return sendMidiMessage({0xB0, static_cast<uint8_t>(controller), colorIndex});
    }
    
    // Clear all pads
    bool clearAllPads() {
        bool success = true;
        for (int note = 36; note <= 99; note++) {
            success &= sendMidiMessage({0x90, static_cast<uint8_t>(note), 0x00});
        }
        return success;
    }
    
    // Test function to light up some pads
    bool testLighting() {
        std::cout << "Testing Push 2 lighting..." << std::endl;
        
        // Light up corner pads in different colors
        setPadColor(36, 255, 0, 0);    // Bottom-left: Red
        setPadColor(43, 0, 255, 0);    // Bottom-right: Green  
        setPadColor(92, 0, 0, 255);    // Top-left: Blue
        setPadColor(99, 255, 255, 0);  // Top-right: Yellow
        
        // Light up some buttons
        setButtonColor(85, 127);  // Play button
        setButtonColor(86, 64);   // Record button
        
        return true;
    }
};