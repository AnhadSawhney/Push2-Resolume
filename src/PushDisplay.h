#pragma once

#include "PushUSB.h"
#include "PushUI.h"
#include "ResolumeTrackerOSC.h"
#include "Color.h"
#define CANVAS_ITY_IMPLEMENTATION
#include "canvas_ity.hpp"
#include <cstdint>
#include <memory>

// Display constants
static const int DISPLAY_WIDTH = 960;
static const int DISPLAY_HEIGHT = 160;

// Forward declaration
class PushUI;

// PushDisplay class - handles all display rendering
class PushDisplay {
private:
    PushUSB& pushDevice;
    PushUI* parentUI;
    std::unique_ptr<canvas_ity::canvas> canvas;
    uint8_t displayBuffer[DISPLAY_WIDTH * DISPLAY_HEIGHT * 4]; // RGBA
    
public:
    PushDisplay(PushUSB& push) : pushDevice(push), parentUI(nullptr) {
        canvas = std::make_unique<canvas_ity::canvas>(DISPLAY_WIDTH, DISPLAY_HEIGHT);
    }
    
    void setParentUI(PushUI* parent) { parentUI = parent; }
    
    void clear() {
        // Clear the canvas to black
        canvas->set_color(canvas_ity::fill_style, 0.0f, 0.0f, 0.0f, 1.0f);
        canvas->clear_rectangle(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    }
    
    void update() {
        if (!parentUI) {
            clear();
            return;
        }
        
        // Clear to black background
        clear();
        
        // Check if we're in selecting mode - need to check the mode from parentUI
        // Assuming PushUI has a getMode() method that returns the current mode
        if (parentUI->getMode() == PushUI::Mode::Selecting) {
            // Draw 2-pixel green border around entire screen
            canvas->set_color(canvas_ity::stroke_style, 0.0f, 1.0f, 0.0f, 1.0f);
            canvas->set_line_width(2.0f);
            
            // Draw rectangle border (stroke a rectangle that covers the screen)
            canvas->stroke_rectangle(1.0f, 1.0f, 
                                   static_cast<float>(DISPLAY_WIDTH - 2), 
                                   static_cast<float>(DISPLAY_HEIGHT - 2));
        }
    }
    
    void sendToDevice() {
        // Get the rendered image data from canvas
        canvas->get_image_data(displayBuffer, DISPLAY_WIDTH, DISPLAY_HEIGHT, 
                             DISPLAY_WIDTH * 4, 0, 0);
                             
        if (pushDevice.isDeviceConnected()) {
            pushDevice.sendDisplayFrameBlocking(displayBuffer);
        }
    }
};