#pragma once

#include "PushUSB.h"
#include "Color.h"
#include <map>

#define PALETTE_BLACK 0
#define PALETTE_RGB_WHITE 122
#define PALETTE_BW_WHITE 127

class PushUI; // Forward declaration

// PushLights class - handles all LED lighting
class PushLights {
private:
    PushUSB& pushDevice;
    PushUI* parentUI;
    static const int PAD_ROWS = 8;
    static const int PAD_COLS = 8;
    static const int FIRST_PAD_NOTE = 36;
    // Use a fixed array for 64 pads
    uint8_t currentPadPaletteIndices[64] = {0};
    // Use a fixed array for all buttons (cc0-cc119)
    uint8_t currentButtonPaletteIndices[120] = {0};
    bool lightsInitialized;

    // Pre-populate rgbPalette with standard colors
    std::map<uint32_t, uint8_t> rgbPalette = {
        {0x000000, PALETTE_BLACK},                  // black
        {0xFFFFFF, PALETTE_RGB_WHITE},                  // white
        {0x00FF00, 126},                  // green
        {0x0000FF, 125},                   // blue
        {0xFF0000, 127}                     // red
    };
    uint8_t nextCustomPaletteIndex = 10; // Start at 10, avoid 0 and high reserved values
    static constexpr uint8_t MAX_CUSTOM_PALETTE_INDEX = 121; // 122+ reserved

    // BW palette: map brightness (0-128) to palette index
    std::map<uint8_t, uint8_t> bwPalette = {
        {0, PALETTE_BLACK},        // black
        {32, 16},      // dark gray
        {84, 48},      // light gray
        {128, PALETTE_BW_WHITE}     // white
    };

    // Helper to get palette index for a Color (returns default if not found)
    uint8_t getRGBPaletteIndex(const Color& color) {
        uint32_t rgb = (color.r << 16) | (color.g << 8) | color.b;
        auto it = rgbPalette.find(rgb);
        if (it != rgbPalette.end()) return it->second;
        if (nextCustomPaletteIndex > MAX_CUSTOM_PALETTE_INDEX) {
            std::cerr << "PushLights: Out of palette indices for custom colors!" << std::endl;
            return 0;
        }
        uint8_t idx = nextCustomPaletteIndex++;
        rgbPalette[rgb] = idx;
        pushDevice.setPaletteEntry(idx, color.r, color.g, color.b);
        return idx;
    }

    // Helper: is this button RGB?
    static inline bool isRGBButton(int cc) {
        return
            (cc >= 102 && cc <= 109) ||
            (cc >= 20 && cc <= 27) ||
            (cc >= 36 && cc <= 43) ||
            cc == 60 || cc == 61 || cc == 29 ||
            cc == 85 || cc == 86 || cc == 89;
    }

    // Helper: get BW palette index for brightness (0-128)
    uint8_t getBWPaletteIndex(uint8_t brightness) {
        // Use nearest match from bwPalette
        uint8_t bestIdx = 0;
        int bestDist = 999;
        for (const auto& kv : bwPalette) {
            int dist = std::abs((int)kv.first - (int)brightness);
            if (dist < bestDist) {
                bestDist = dist;
                bestIdx = kv.second;
            }
        }
        return bestIdx;
    }

public:
    PushLights(PushUSB& push) : pushDevice(push), parentUI(nullptr), lightsInitialized(false) {
        // Initialize all pad palette indices to black
        for (int i = 0; i < 64; ++i) currentPadPaletteIndices[i] = PALETTE_BLACK;
        for (int i = 0; i < 120; ++i) currentButtonPaletteIndices[i] = 0;
    }

    void setParentUI(PushUI* parent) { parentUI = parent; }

    // Set pad color using note number (only sends if palette index changed)
    void setPadColor(int note, const Color& color) {
        if (note < FIRST_PAD_NOTE || note > FIRST_PAD_NOTE + 63) return;
        uint8_t paletteIdx = getRGBPaletteIndex(color);
        int idx = note - FIRST_PAD_NOTE;
        if (currentPadPaletteIndices[idx] == paletteIdx) {
            return;
        }
        pushDevice.setPadColorIndex(note, paletteIdx);
        currentPadPaletteIndices[idx] = paletteIdx;
    }

    // Set pad color using row/column (0-based)
    void setPadColor(int row, int col, const Color& color) {
        if (row >= 0 && row < PAD_ROWS && col >= 0 && col < PAD_COLS) {
            int note = FIRST_PAD_NOTE + (row * PAD_COLS + col);
            setPadColor(note, color);
        }
    }

    // Set button color for BW button (brightness 0-128)
    void setButtonColorBW(int cc, uint8_t brightness) {
        if (cc < 0 || cc >= 120) return;
        if (isRGBButton(cc)) {
            std::cerr << "setButtonColorBW: cc" << cc << " is not a BW button!" << std::endl;
            return;
        }
        uint8_t paletteIdx = getBWPaletteIndex(brightness);
        if (currentButtonPaletteIndices[cc] == paletteIdx) return;
        pushDevice.setButtonColorIndex(cc, paletteIdx);
        currentButtonPaletteIndices[cc] = paletteIdx;
    }

    // Set button color for RGB button
    void setButtonColorRGB(int cc, const Color& color) {
        if (cc < 0 || cc >= 120) return;
        if (!isRGBButton(cc)) {
            std::cerr << "setButtonColorRGB: cc" << cc << " is not an RGB button!" << std::endl;
            return;
        }
        uint8_t paletteIdx = getRGBPaletteIndex(color);
        if (currentButtonPaletteIndices[cc] == paletteIdx) return;
        pushDevice.setButtonColorIndex(cc, paletteIdx);
        currentButtonPaletteIndices[cc] = paletteIdx;
    }

    // Clear all pads to black (forces update)
    void clearAllPads() {
        for (int i = 0; i < 64; ++i) {
            currentPadPaletteIndices[i] = PALETTE_BLACK;
            pushDevice.setPadColorIndex(FIRST_PAD_NOTE + i, PALETTE_BLACK);
        }
    }

    // Clear all buttons (forces update)
    void clearAllButtons() {
        for (int cc = 0; cc < 120; ++cc) {
            if(isRGBButton(cc)) {
                setButtonColorRGB(cc, Color::BLACK);
            } else {
                setButtonColorBW(cc, 0);
            }
        }
    }

    // Force complete refresh (useful after reconnection or initialization)
    void forceRefresh() {
        for (int i = 0; i < 64; ++i) currentPadPaletteIndices[i] = PALETTE_BLACK;
        for (int i = 0; i < 120; ++i) currentButtonPaletteIndices[i] = PALETTE_BLACK;
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
        int connectedColumn = parentUI->getResolumeTracker().getConnectedColumnId();

        // Column buttons: cc20-cc27
        for (int i = 0; i < 8; ++i) {
            int cc = 20 + i;
            int column = parentUI->getColumnOffset() + i + 1; // 1-based column
            if (column == connectedColumn) {
                setButtonColorRGB(cc, Color::WHITE); // White (palette index)
            } else {
                // Rainbow: evenly spaced hues, mapped to palette
                float hue = (float)i * 360.0f / 8.0f;
                Color c = Color::fromHSV(hue, 1.0f, 1.0f);
                setButtonColorRGB(cc, c);
            }
        }
        // Layer buttons: cc36-cc43, always white
        for (int cc = 36; cc < 44; ++cc) {
            setButtonColorRGB(cc, Color::WHITE);
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
        setButtonColorBW(55, parentUI->canMoveLayerUp() ? 255 : 0);     // BTN_OCTAVE_UP
        setButtonColorBW(54, parentUI->canMoveLayerDown() ? 255 : 0);   // BTN_OCTAVE_DOWN
        setButtonColorBW(63, parentUI->canMoveColumnRight() ? 255 : 0); // BTN_PAGE_RIGHT
        setButtonColorBW(62, parentUI->canMoveColumnLeft() ? 255 : 0);  // BTN_PAGE_LEFT
    }
};