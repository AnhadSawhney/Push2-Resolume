#pragma once

#include "PushUSB.h"
#include "PushUI.h"
#include "ResolumeTrackerREST.h"
#include "Color.h"
#include <cstdint>
#include <memory>

// Display constants
static const int DISPLAY_WIDTH = 960;
static const int DISPLAY_HEIGHT = 160;

// PushDisplay class - handles all display rendering (minimal for now)
class PushDisplay {
private:
    PushUSB& pushDevice;
    PushUI* parentUI;
    uint8_t displayBuffer[DISPLAY_WIDTH * DISPLAY_HEIGHT];
    
public:
    PushDisplay(PushUSB& push) : pushDevice(push), parentUI(nullptr) {
        memset(displayBuffer, 0, sizeof(displayBuffer));
    }
    
    void setParentUI(PushUI* parent) { parentUI = parent; }
    
    void clear() {
        memset(displayBuffer, 0, sizeof(displayBuffer));
    }
    
    void update() {
        // Basic display update - just clear for now
        clear();
    }
    
    void sendToDevice() {
        // Display interface implementation would go here
        // For now, this is a placeholder
    }
};