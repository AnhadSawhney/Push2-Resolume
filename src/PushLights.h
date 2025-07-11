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
    static const int PAD_ROWS = 8;
    static const int PAD_COLS = 8;
    static const int FIRST_PAD_NOTE = 36;
    std::map<int, Color> currentPadColors;
    std::map<int, uint8_t> currentButtonColors;
    bool lightsInitialized;

    // Palette management
    // Default palette indices for Push 2 (see Ableton spec)
    static constexpr uint8_t PALETTE_BLACK = 0;
    static constexpr uint8_t PALETTE_WHITE = 122;
    static constexpr uint8_t PALETTE_GREEN = 126;
    static constexpr uint8_t PALETTE_BLUE  = 125;
    static constexpr uint8_t PALETTE_RED   = 127;
    // For buttons, 127 is white for white LEDs

    // Map RGB colors to palette indices (for custom colors)
    std::map<uint32_t, uint8_t> rgbToPaletteIndex;
    uint8_t nextCustomPaletteIndex = 10; // Start at 10, avoid 0 and high reserved values

    // Helper to get palette index for a Color (returns default if not found)
    uint8_t getPaletteIndexForColor(const Color& color) {
        if (color.r == 0 && color.g == 0 && color.b == 0) return PALETTE_BLACK;
        if (color.r == 255 && color.g == 255 && color.b == 255) return PALETTE_WHITE;
        if (color.r == 0 && color.g == 255 && color.b == 0) return PALETTE_GREEN;
        if (color.r == 0 && color.g == 0 && color.b == 255) return PALETTE_BLUE;
        if (color.r == 255 && color.g == 0 && color.b == 0) return PALETTE_RED;
        // Custom color: pack RGB into uint32_t
        uint32_t rgb = (color.r << 16) | (color.g << 8) | color.b;
        auto it = rgbToPaletteIndex.find(rgb);
        if (it != rgbToPaletteIndex.end()) return it->second;
        // Assign new palette index and send sysex
        uint8_t idx = nextCustomPaletteIndex++;
        rgbToPaletteIndex[rgb] = idx;
        pushDevice.setPaletteEntry(idx, color.r, color.g, color.b);
        return idx;
    }
    
public:
    PushLights(PushUSB& push) : pushDevice(push), parentUI(nullptr), lightsInitialized(false) {}
    
    void setParentUI(PushUI* parent) { parentUI = parent; }
    
    // Set pad RGB color using note number (only sends if changed)
    void setPadColor(int note, const Color& color) {
        if (note < FIRST_PAD_NOTE || note > FIRST_PAD_NOTE + 63) return;
        auto it = currentPadColors.find(note);
        if (it != currentPadColors.end() &&
            it->second.r == color.r && it->second.g == color.g && it->second.b == color.b) {
            return;
        }
        uint8_t paletteIdx = getPaletteIndexForColor(color);
        pushDevice.setPadColor(note, paletteIdx, paletteIdx, paletteIdx); // Use palette index for all channels
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
        auto it = currentButtonColors.find(controller);
        if (it != currentButtonColors.end() && it->second == colorIndex) {
            return;
        }
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
            bool connected = false;
            for (int layer = 1; layer <= 32; ++layer) {
                if (parentUI->getResolumeTracker().isClipPlaying(column, layer)) {
                    connected = true;
                    break;
                }
            }
            if (connected) {
                setButtonColor(cc, PALETTE_WHITE); // White (palette index)
            } else {
                // Rainbow: evenly spaced hues, mapped to palette
                float hue = (float)i * 360.0f / 8.0f;
                Color c = Color::fromHSV(hue, 1.0f, 1.0f);
                setButtonColor(cc, getPaletteIndexForColor(c));
            }
        }
        // Layer buttons: cc36-cc43, always white
        for (int i = 0; i < 8; ++i) {
            int cc = 36 + i;
            setButtonColor(cc, PALETTE_WHITE);
        }
    }
    
    // Update grid lighting based on clips
    void updateGridLights() {
        if (!parentUI) return;
        for (int gridRow = 0; gridRow < 8; gridRow++) {
            for (int gridCol = 0; gridCol < 8; gridCol++) {
                int resolumeLayer = gridRow + 1 + parentUI->layerOffset;
                int resolumeColumn = gridCol + 1 + parentUI->columnOffset;
                Color padColor = Color::BLACK;
                if (parentUI->resolumeTracker.hasClip(resolumeColumn, resolumeLayer)) {
                    if (parentUI->resolumeTracker.isClipPlaying(resolumeColumn, resolumeLayer)) {
                        padColor = Color::GREEN;
                    } else {
                        padColor = Color::WHITE;
                    }
                }
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