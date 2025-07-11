#pragma once

#include "PushUSB.h"
#include "Color.h"
#include <map>

class PushUI; // Forward declaration

// PushLights class - handles all LED lighting
class PushLights {
private:
    PushUSB& pushDevice;
    PushUI* parentUI;
    
    // Push 2 pad layout constants
    static const int PAD_ROWS = 8;
    static const int PAD_COLS = 8;
    static const int FIRST_PAD_NOTE = 36;
    // State tracking to avoid unnecessary MIDI messages
        // Make sure Color is declared or included before this line
        std::map<int, Color> currentPadColors;     // Note -> current color
        std::map<int, uint8_t> currentButtonColors; // Controller -> current color index
        bool lightsInitialized;
    //bool lightsInitialized;
    
public:
    PushLights(PushUSB& push) : pushDevice(push), parentUI(nullptr), lightsInitialized(false) {}
    
    void setParentUI(PushUI* parent) { parentUI = parent; }
    
    // Set pad RGB color using note number (only sends if changed)
    void setPadColor(int note, const Color& color) {
        if (note < FIRST_PAD_NOTE || note > FIRST_PAD_NOTE + 63) return;
        
        // Check if color has actually changed
        auto it = currentPadColors.find(note);
        if (it != currentPadColors.end() && 
            it->second.r == color.r && it->second.g == color.g && it->second.b == color.b) {
            return; // Color hasn't changed, skip MIDI message
        }
        
        // Color has changed, send MIDI and update state
        pushDevice.setPadColor(note, color.r, color.g, color.b);
        currentPadColors[note] = color;
    }
    
    // Set pad color using row/column (0-based)
    void setPadColor(int row, int col, const Color& color) {
        if (row >= 0 && row < PAD_ROWS && col >= 0 && col < PAD_COLS) {
            int note = FIRST_PAD_NOTE + (row * PAD_COLS + col);
            setPadColor(note, color);
        }
    }
    
    // Set button color using control change (only sends if changed)
    void setButtonColor(int controller, uint8_t colorIndex) {
        // Check if color has actually changed
        auto it = currentButtonColors.find(controller);
        if (it != currentButtonColors.end() && it->second == colorIndex) {
            return; // Color hasn't changed, skip MIDI message
        }
        
        // Color has changed, send MIDI and update state
        pushDevice.setButtonColor(controller, colorIndex);
        currentButtonColors[controller] = colorIndex;
    }
    
    // Clear all pads to black (forces update)
    void clearAllPads() {
        for (int note = FIRST_PAD_NOTE; note <= FIRST_PAD_NOTE + 63; note++) {
            currentPadColors[note] = Color::BLACK;
            pushDevice.setPadColor(note, 0, 0, 0);
        }
    }
    
    // Clear all buttons (forces update)
    void clearAllButtons() {
        // Clear main control buttons (based on Push 2 MIDI mapping)
        for (int cc = 20; cc <= 63; cc++) {
            currentButtonColors[cc] = 0;
            pushDevice.setButtonColor(cc, 0);
        }
        for (int cc = 85; cc <= 119; cc++) {
            currentButtonColors[cc] = 0;
            pushDevice.setButtonColor(cc, 0);
        }
    }
    
    // Force complete refresh (useful after reconnection or initialization)
    void forceRefresh() {
        currentPadColors.clear();
        currentButtonColors.clear();
        lightsInitialized = false;
    }
    
    // Update all lights based on current Resolume state
    void updateLights() {
        if (!lightsInitialized) {
            // First time setup - clear everything to ensure known state
            clearAllPads();
            clearAllButtons();
            lightsInitialized = true;
        }

        updateGridLights();
        updateNavigationLights();
        updateColumnAndLayerButtons();
    }

    // New: update column (cc20-27) and layer (cc36-43) button lights
    void updateColumnAndLayerButtons() {
        if (!parentUI) return;
        // Column buttons: cc20-cc27
        for (int i = 0; i < 8; ++i) {
            int cc = 20 + i;
            int column = parentUI->getColumnOffset() + i + 1; // 1-based column
            // If any clip in this column is playing, light white, else rainbow
            bool connected = false;
            for (int layer = 1; layer <= 32; ++layer) {
                if (parentUI->getResolumeTracker().isClipPlaying(column, layer)) {
                    connected = true;
                    break;
                }
            }
            if (connected) {
                setButtonColor(cc, 127); // White (full brightness)
            } else {
                // Rainbow: evenly spaced hues
                float hue = (float)i * 360.0f / 8.0f;
                Color c = Color::fromHSV(hue, 1.0f, 1.0f);
                // Map to MIDI color index: use red/green/blue channels, pick the brightest
                // But Push 2 supports RGB, so if your setButtonColor supports RGB, use it. Otherwise, use green for demo.
                // Here, we use green for demo (MIDI index 60 = green, 5 = red, 9 = yellow, etc.)
                // But for now, just use 5 + i*10 for a color gradient, or always 60 (green)
                // If you have RGB support, you could add setButtonColorRGB.
                setButtonColor(cc, 5 + i * 10); // crude color ramp
            }
        }
        // Layer buttons: cc36-cc43, always white
        for (int i = 0; i < 8; ++i) {
            int cc = 36 + i;
            setButtonColor(cc, 127); // White (full brightness)
        }
    }
    
    // Update grid lighting based on clips
    void updateGridLights() {
        if (!parentUI) return;
        
        for (int gridRow = 0; gridRow < 8; gridRow++) {
            for (int gridCol = 0; gridCol < 8; gridCol++) {
                // Convert grid position to Resolume layer/column
                int resolumeLayer = gridRow + 1 + parentUI->layerOffset;  // Bottom row = layer 1
                int resolumeColumn = gridCol + 1 + parentUI->columnOffset;       // Left col = column 1
                
                Color padColor = Color::BLACK; // Default: no clip
                
                // Check if clip exists
                if (parentUI->resolumeTracker.hasClip(resolumeColumn, resolumeLayer)) {
                    //std::cout << "Clip exists at Layer " << resolumeLayer << ", Column " << resolumeColumn << std::endl;
                    // Check if clip is connected (playing)
                    if (parentUI->resolumeTracker.isClipPlaying(resolumeColumn, resolumeLayer)) {
                        padColor = Color::GREEN; // Connected/playing clip
                    } else {
                        padColor = Color::WHITE; // Available but not playing clip
                    }
                }
                
                // setPadColor will only send MIDI if the color has changed
                setPadColor(gridRow, gridCol, padColor);
            }
        }
    }
    
    // Update navigation button lighting
    void updateNavigationLights() {
        if (!parentUI) return;
        
        // Only send MIDI if the button state has actually changed
        setButtonColor(55, parentUI->canMoveLayerUp() ? 127 : 0);     // BTN_OCTAVE_UP
        setButtonColor(54, parentUI->canMoveLayerDown() ? 127 : 0);   // BTN_OCTAVE_DOWN
        setButtonColor(63, parentUI->canMoveColumnRight() ? 127 : 0); // BTN_PAGE_RIGHT
        setButtonColor(62, parentUI->canMoveColumnLeft() ? 127 : 0);  // BTN_PAGE_LEFT
    }
};