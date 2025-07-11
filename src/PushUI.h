#pragma once

#include "PushUSB.h"
#include "ResolumeTracker.h"
#include <vector>
#include <map>
#include <chrono>
#include <functional>
#include <memory>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <iostream>

// Forward declarations for OSC functionality
class OSCSender {
public:
    virtual void sendMessage(const std::string& address, float value) = 0;
    virtual void sendMessage(const std::string& address, int value) = 0;
    virtual void sendMessage(const std::string& address, const std::string& value) = 0;
};

// Display constants
static const int DISPLAY_WIDTH = 960;
static const int DISPLAY_HEIGHT = 160;

// RGB color structure for LEDs
struct Color {
    uint8_t r, g, b;
    Color(uint8_t red = 0, uint8_t green = 0, uint8_t blue = 0) : r(red), g(green), b(blue) {}
    
    // Predefined colors
    static const Color BLACK;
    static const Color WHITE;
    static const Color RED;
    static const Color GREEN;
    static const Color BLUE;
    static const Color YELLOW;
    static const Color CYAN;
    static const Color MAGENTA;
    static const Color ORANGE;
    static const Color PURPLE;
    static const Color DIM_WHITE;
    static const Color DIM_GREEN;
    static const Color DIM_RED;
    
    // Helper method to create color from HSV
    static Color fromHSV(float hue, float saturation, float value) {
        hue = fmod(hue, 360.0f);
        if (hue < 0) hue += 360.0f;
        
        saturation = (std::max)(0.0f, (std::min)(1.0f, saturation));
        value = (std::max)(0.0f, (std::min)(1.0f, value));
        
        float c = value * saturation;
        float x = c * (1.0f - abs(fmod(hue / 60.0f, 2.0f) - 1.0f));
        float m = value - c;
        
        float r, g, b;
        if (hue < 60) {
            r = c; g = x; b = 0;
        } else if (hue < 120) {
            r = x; g = c; b = 0;
        } else if (hue < 180) {
            r = 0; g = c; b = x;
        } else if (hue < 240) {
            r = 0; g = x; b = c;
        } else if (hue < 300) {
            r = x; g = 0; b = c;
        } else {
            r = c; g = 0; b = x;
        }
        
        return Color(
            static_cast<uint8_t>((r + m) * 255),
            static_cast<uint8_t>((g + m) * 255),
            static_cast<uint8_t>((b + m) * 255)
        );
    }
};

// Forward declaration
class PushUI;

// PushLights class - handles all LED lighting
class PushLights {
private:
    PushUSB& pushDevice;
    PushUI* parentUI;
    
    // Push 2 pad layout constants
    static const int PAD_ROWS = 8;
    static const int PAD_COLS = 8;
    static const int FIRST_PAD_NOTE = 36;
    
public:
    PushLights(PushUSB& push) : pushDevice(push), parentUI(nullptr) {}
    
    void setParentUI(PushUI* parent) { parentUI = parent; }
    
    // Set pad RGB color using note number
    void setPadColor(int note, const Color& color) {
        if (note >= FIRST_PAD_NOTE && note <= FIRST_PAD_NOTE + 63) {
            pushDevice.setPadColor(note, color.r, color.g, color.b);
        }
    }
    
    // Set pad color using row/column (0-based)
    void setPadColor(int row, int col, const Color& color) {
        if (row >= 0 && row < PAD_ROWS && col >= 0 && col < PAD_COLS) {
            int note = FIRST_PAD_NOTE + (row * PAD_COLS + col);
            setPadColor(note, color);
        }
    }
    
    // Set button color using control change
    void setButtonColor(int controller, uint8_t colorIndex) {
        pushDevice.setButtonColor(controller, colorIndex);
    }
    
    // Clear all pads to black
    void clearAllPads() {
        for (int note = FIRST_PAD_NOTE; note <= FIRST_PAD_NOTE + 63; note++) {
            setPadColor(note, Color::BLACK);
        }
    }
    
    // Clear all buttons
    void clearAllButtons() {
        // Clear main control buttons (based on Push 2 MIDI mapping)
        for (int cc = 20; cc <= 63; cc++) {
            setButtonColor(cc, 0);
        }
        for (int cc = 85; cc <= 119; cc++) {
            setButtonColor(cc, 0);
        }
    }
    
    // Update all lights based on current Resolume state
    void updateLights();
    
    // Update grid lighting based on clips
    void updateGridLights();
    
    // Update navigation button lighting
    void updateNavigationLights();
};

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

// Main PushUI class
class PushUI {
    friend class PushLights;
    friend class PushDisplay;
    
private:
    PushUSB& pushDevice;
    ResolumeTracker& resolumeTracker;
    std::unique_ptr<OSCSender> oscSender;

    PushLights lights;
    PushDisplay display;

    // Navigation state for 8x8 grid
    int columnOffset;    // 0-based offset for which column appears at left edge (column 1 = offset 0)
    int layerOffset;     // 0-based offset for which layer appears at bottom edge (layer 1 = offset 0)

    // Push 2 control constants (from MIDI documentation)
    enum PushControls {
        // Navigation buttons
        BTN_OCTAVE_UP = 55,
        BTN_OCTAVE_DOWN = 54,
        BTN_PAGE_LEFT = 63,
        BTN_PAGE_RIGHT = 62,
        
        // Other buttons
        BTN_PLAY = 85,
        BTN_RECORD = 86,
        BTN_STOP = 87
    };

    // Track deck changes to reset navigation
    int lastKnownDeck;
    bool trackingInitialized;

public:
    PushUI(PushUSB& push, ResolumeTracker& tracker, std::unique_ptr<OSCSender> osc = nullptr)
        : pushDevice(push), resolumeTracker(tracker), oscSender(std::move(osc)),
          lights(pushDevice), display(pushDevice),
          columnOffset(0), layerOffset(0),
          lastKnownDeck(-1), trackingInitialized(false) {
        
        lights.setParentUI(this);
        display.setParentUI(this);
    }
    
    ~PushUI() {
        lights.clearAllPads();
        lights.clearAllButtons();
    }

    bool initialize() {
        if (!pushDevice.isDeviceConnected()) {
            std::cerr << "Push device not connected" << std::endl;
            return false;
        }
        
        // Set up MIDI callback to handle input from Push 2
        pushDevice.setMidiCallback([this](const PushMidiMessage& msg) {
            this->onMidiMessage(msg);
        });
        
        // Clear everything and set initial state
        lights.clearAllPads();
        lights.clearAllButtons();
        resetNavigation();
        
        std::cout << "PushUI initialized successfully" << std::endl;
        return true;
    }

    void update() {
        // Check for deck changes and reset navigation if needed
        checkForDeckChange();
        
        // Update lighting
        lights.updateLights();
        
        // Update display
        display.update();
        display.sendToDevice();
    }

    // MIDI input handler
    void onMidiMessage(const PushMidiMessage& msg) {
        if (msg.isNoteOn()) {
            handlePadPress(msg.getNote(), msg.getVelocity());
        } else if (msg.isControlChange()) {
            handleButtonPress(msg.getController(), msg.getValue());
        }
    }

    // Get current navigation offsets
    int getColumnOffset() const { return columnOffset; }
    int getLayerOffset() const { return layerOffset; }

private:
    void checkForDeckChange() {
        int currentDeck = resolumeTracker.getCurrentDeckId();
        
        if (!trackingInitialized) {
            lastKnownDeck = currentDeck;
            trackingInitialized = true;
        } else if (resolumeTracker.isDeckInitialized() && currentDeck != lastKnownDeck) {
            std::cout << "Deck changed from " << lastKnownDeck << " to " << currentDeck << " - resetting navigation" << std::endl;
            resetNavigation();
            lastKnownDeck = currentDeck;
        }
    }

    void resetNavigation() {
        columnOffset = 0;
        layerOffset = 0;
        std::cout << "Navigation reset to origin (Column 1, Layer 1)" << std::endl;
    }

    void handlePadPress(int note, int velocity) {
        if (velocity == 0) return; // Ignore note off messages
        
        // Check if it's a grid pad (notes 36-99)
        if (note >= 36 && note <= 99) {
            int padIndex = note - 36;
            int gridRow = padIndex / 8;      // 0-7 (bottom to top)
            int gridCol = padIndex % 8;      // 0-7 (left to right)
            
            // Convert grid position to Resolume layer/clip IDs
            int resolumeLayer = (7 - gridRow) + 1 + layerOffset;  // Invert: bottom row = layer 1
            int resolumeColumn = gridCol + 1 + columnOffset;       // Left col = column 1
            
            std::cout << "Pad pressed: Grid(" << gridRow << "," << gridCol << ") -> "
                      << "Resolume Layer " << resolumeLayer << ", Column " << resolumeColumn << std::endl;
            
            // Send OSC command to trigger clip
            triggerClip(resolumeLayer, resolumeColumn);
        }
    }

    void handleButtonPress(int controller, int value) {
        if (value == 0) return; // Only handle button presses, not releases
        
        switch (controller) {
            case BTN_OCTAVE_UP:
                if (canMoveLayerUp()) {
                    layerOffset++;
                    std::cout << "Layer offset increased to " << layerOffset << " (showing layers " 
                              << (layerOffset + 1) << "-" << (layerOffset + 8) << ")" << std::endl;
                }
                break;
                
            case BTN_OCTAVE_DOWN:
                if (canMoveLayerDown()) {
                    layerOffset--;
                    std::cout << "Layer offset decreased to " << layerOffset << " (showing layers " 
                              << (layerOffset + 1) << "-" << (layerOffset + 8) << ")" << std::endl;
                }
                break;
                
            case BTN_PAGE_RIGHT:
                if (canMoveColumnRight()) {
                    columnOffset++;
                    std::cout << "Column offset increased to " << columnOffset << " (showing columns " 
                              << (columnOffset + 1) << "-" << (columnOffset + 8) << ")" << std::endl;
                }
                break;
                
            case BTN_PAGE_LEFT:
                if (canMoveColumnLeft()) {
                    columnOffset--;
                    std::cout << "Column offset decreased to " << columnOffset << " (showing columns " 
                              << (columnOffset + 1) << "-" << (columnOffset + 8) << ")" << std::endl;
                }
                break;
        }
    }

    // Navigation boundary checks
    bool canMoveLayerUp() const {
        // Check if there are layers beyond the current view
        for (int i = 1; i <= 8; i++) {
            int checkLayer = layerOffset + 8 + i; // Check layers beyond current top
            if (resolumeTracker.hasLayerContent(checkLayer)) {
                return true;
            }
        }
        return false;
    }

    bool canMoveLayerDown() const {
        return layerOffset > 0;
    }

    bool canMoveColumnRight() const {
        // Check if there are columns beyond the current view
        for (int i = 1; i <= 8; i++) {
            int checkColumn = columnOffset + 8 + i; // Check columns beyond current right edge
            for (int layer = 1; layer <= 32; layer++) { // Check a reasonable range of layers
                if (resolumeTracker.hasClip(checkColumn, layer)) {
                    return true;
                }
            }
        }
        return false;
    }

    bool canMoveColumnLeft() const {
        return columnOffset > 0;
    }

    void triggerClip(int layer, int column) {
        std::string address = "/composition/layers/" + std::to_string(layer) + 
                                 "/clips/" + std::to_string(column) + "/connect";
        if (oscSender) {
            oscSender->sendMessage(address, 1.0f);
        } else {
            std::cout << "Would trigger: " << address << std::endl;
        }
    }
};

// Implementation of PushLights methods
inline void PushLights::updateLights() {
    updateGridLights();
    updateNavigationLights();
}

inline void PushLights::updateGridLights() {
    if (!parentUI) return;
    
    for (int gridRow = 0; gridRow < 8; gridRow++) {
        for (int gridCol = 0; gridCol < 8; gridCol++) {
            // Convert grid position to Resolume layer/column
            int resolumeLayer = (7 - gridRow) + 1 + parentUI->layerOffset;  // Bottom row = layer 1
            int resolumeColumn = gridCol + 1 + parentUI->columnOffset;       // Left col = column 1
            
            Color padColor = Color::BLACK; // Default: no clip
            
            // Check if clip exists
            if (parentUI->resolumeTracker.hasClip(resolumeColumn, resolumeLayer)) {
                // Check if clip is connected (playing)
                if (parentUI->resolumeTracker.isClipPlaying(resolumeColumn, resolumeLayer)) {
                    padColor = Color::GREEN; // Connected/playing clip
                } else {
                    padColor = Color::WHITE; // Available but not playing clip
                }
            }
            
            setPadColor(gridRow, gridCol, padColor);
        }
    }
}

inline void PushLights::updateNavigationLights() {
    if (!parentUI) return;
    
    // Octave up/down (layer navigation)
    setButtonColor(55, parentUI->canMoveLayerUp() ? 127 : 0);     // BTN_OCTAVE_UP
    setButtonColor(54, parentUI->canMoveLayerDown() ? 127 : 0);   // BTN_OCTAVE_DOWN
    
    // Page left/right (column navigation)  
    setButtonColor(63, parentUI->canMoveColumnRight() ? 127 : 0); // BTN_PAGE_RIGHT
    setButtonColor(62, parentUI->canMoveColumnLeft() ? 127 : 0);  // BTN_PAGE_LEFT
}

// Static color definitions
const Color Color::BLACK(0, 0, 0);
const Color Color::WHITE(255, 255, 255);
const Color Color::RED(255, 0, 0);
const Color Color::GREEN(0, 255, 0);
const Color Color::BLUE(0, 0, 255);
const Color Color::YELLOW(255, 255, 0);
const Color Color::CYAN(0, 255, 255);
const Color Color::MAGENTA(255, 0, 255);
const Color Color::ORANGE(255, 128, 0);
const Color Color::PURPLE(128, 0, 255);
const Color Color::DIM_WHITE(64, 64, 64);
const Color Color::DIM_GREEN(0, 64, 0);
const Color Color::DIM_RED(64, 0, 0);