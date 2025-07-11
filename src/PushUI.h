#ifndef PUSHUI_H
#define PUSHUI_H

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

// Forward declarations for OSC functionality
// You'll need to implement these based on your OSC library
class OSCSender {
public:
    virtual void sendMessage(const std::string& address, float value) = 0;
    virtual void sendMessage(const std::string& address, int value) = 0;
    virtual void sendMessage(const std::string& address, const std::string& value) = 0;
};

// Navigation button enumeration
enum NavigationButton {
    NAV_UP,
    NAV_DOWN,
    NAV_LEFT,
    NAV_RIGHT
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
        // Ensure hue is in range [0, 360)
        hue = fmod(hue, 360.0f);
        if (hue < 0) hue += 360.0f;
        
        // Clamp saturation and value to [0, 1]
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

// Forward declarations
class PushLights;
class PushDisplay;

// PushLights class - handles all LED lighting
class PushLights {
private:
    PushUSB& pushDevice;
    
    // Current state of all LEDs
    std::map<int, Color> padColors;          // Pad note -> Color
    std::map<int, uint8_t> buttonColors;     // Button CC -> Color index
    
    // Animation state
    std::chrono::steady_clock::time_point animationTime;
    std::map<int, bool> pulsingPads;         // Which pads are pulsing

    // Push 2 pad layout constants
    static const int PAD_ROWS = 8;
    static const int PAD_COLS = 8;
    static const int FIRST_PAD_NOTE = 36;
    
    // Forward declare parent to avoid circular dependency
    PushUI* parentUI;
    
public:
    PushLights(PushUSB& push) : pushDevice(push), parentUI(nullptr) {
        animationTime = std::chrono::steady_clock::now();
    }
    
    // Set parent UI reference after construction
    void setParentUI(PushUI* parent) { parentUI = parent; }
    
    // Set individual pad colors
    void setPadColor(int note, const Color& color) {
        padColors[note] = color;
    }
    
    void setPadColor(int row, int col, const Color& color) {
        int note = FIRST_PAD_NOTE + (row * PAD_COLS + col);
        setPadColor(note, color);
    }
    
    // Set button colors (using Push 2 color palette indices)
    void setButtonColor(int controller, uint8_t colorIndex) {
        buttonColors[controller] = colorIndex;
    }
    
    // Animation control
    void setPadPulsing(int note, bool pulsing, const Color& color) {
        pulsingPads[note] = pulsing;
        if (pulsing) {
            setPadColor(note, color);
        }
    }
    
    void clearAllPulsing() {
        pulsingPads.clear();
    }
    
    // Batch operations
    void clearAllPads() {
        padColors.clear();
        pulsingPads.clear();
    }
    
    void clearAllButtons() {
        buttonColors.clear();
    }
    
    void clearAll() {
        clearAllPads();
        clearAllButtons();
    }
    
    // Update and send to device
    void updateAnimations() {
        auto currentTime = std::chrono::steady_clock::now();
        animationTime = currentTime;
        
        // Update pulsing pads (implement pulsing animation)
        // This is where you'd calculate brightness variations for pulsing effects
    }
    
    void sendToDevice() {
        // Send all pad colors
        for (const auto& [note, color] : padColors) {
            pushDevice.setPadColor(note, color.r, color.g, color.b);
        }
        
        // Send all button colors
        for (const auto& [controller, colorIndex] : buttonColors) {
            pushDevice.setButtonColor(controller, colorIndex);
        }
    }
    
    // Predefined color patterns
    void showClipGrid(const ResolumeTracker& tracker) {
        // Legacy method - kept for compatibility
        clearAllPads();
        
        for (int layer = 1; layer <= 8; layer++) {
            auto layerObj = tracker.getLayer(layer);
            if (!layerObj) continue;
            
            for (int clip = 1; clip <= 8; clip++) {
                auto clipObj = layerObj->getClip(clip);
                if (clipObj && !clipObj->name.empty()) {
                    Color color = Color::DIM_GREEN;  // Default for available clips
                    setPadColor(layer - 1, clip - 1, color);
                }
            }
        }
    }
    
    void showLayerEffects(const ResolumeTracker& tracker, int layerId) {
        // Skeleton: Show effects for selected layer
        clearAllPads();
        // TODO: Implement effect parameter mapping to pads
    }
    
    void showMixerView(const ResolumeTracker& tracker) {
        // Skeleton: Show mixer controls
        clearAllPads();
        // TODO: Implement mixer view lighting
    }
    
    // New methods for Resolume-specific lighting
    void showResolumeGrid(const ResolumeTracker& tracker, int gridOffsetColumn, int gridOffsetLayer) {
        clearAllPads();
        
        for (int gridRow = 0; gridRow < 8; gridRow++) {
            for (int gridCol = 0; gridCol < 8; gridCol++) {
                // Convert grid position to Resolume IDs
                int layerId = (7 - gridRow) + 1 + gridOffsetLayer;  // Invert row (bottom=0 becomes layer 1)
                int clipId = gridCol + 1 + gridOffsetColumn;
                
                // Check if clip exists in ResolumeTracker
                auto layerObj = tracker.getLayer(layerId);
                if (layerObj) {
                    auto clipObj = layerObj->getClip(clipId);
                    if (clipObj && !clipObj->name.empty()) {
                        // Generate color: same hue as column, saturation from layer
                        float hue = (gridCol * 360.0f / 8.0f);
                        float saturation = 0.3f + (gridRow * 0.7f / 7.0f);
                        Color clipColor = Color::fromHSV(hue, saturation, 1.0f);
                        
                        int note = FIRST_PAD_NOTE + (gridRow * PAD_COLS + gridCol);
                        setPadColor(note, clipColor);
                    }
                }
            }
        }
    }
    
    void showColumnButtons(const ResolumeTracker& tracker, int gridOffsetColumn) {
        // Set each column button to rainbow color
        for (int i = 0; i < 8; i++) {
            int columnId = i + 1 + gridOffsetColumn;
            int controller = 20 + i;  // BTN_COL_1 to BTN_COL_8
            
            // Check if this column is connected
            bool isConnected = (tracker.getConnectedColumnId() == columnId);
            
            if (isConnected) {
                setButtonColor(controller, 127);  // White for connected
            } else {
                // Rainbow color based on column index
                float hue = (i * 360.0f / 8.0f);
                Color columnColor = Color::fromHSV(hue, 1.0f, 1.0f);
                // For now, use a simple color index mapping (you may need to adjust this)
                uint8_t colorIndex = 1 + (i * 15);  // Spread colors across palette
                setButtonColor(controller, colorIndex);
            }
        }
    }
    
    void showLayerButtons(const ResolumeTracker& tracker, int gridOffsetLayer) {
        // Set all layer buttons to white
        for (int i = 0; i < 8; i++) {
            int controller = 36 + i;  // BTN_LAYER_1 to BTN_LAYER_8
            setButtonColor(controller, 127);  // White
        }
    }
    
    void showNavigationButtons() {
        // Set all navigation buttons to dim white
        setButtonColor(55, 64);  // BTN_OCTAVE_UP
        setButtonColor(54, 64);  // BTN_OCTAVE_DOWN
        setButtonColor(63, 64);  // BTN_PAGE_LEFT
        setButtonColor(62, 64);  // BTN_PAGE_RIGHT
    }

    // Update all lights based on current state
    void updateLights() {
        // Clear all lights first
        clearAll();
        
        // Update grid (pads)
        updateGridLights();
        
        // Update column buttons (rainbow for columns, white for connected)
        updateColumnLights();
        
        // Update layer buttons (white for available layers)
        updateLayerLights();
        
        // Update navigation buttons (dim white)
        updateNavigationLights();
    }

    // Clear all lights
    void clearAll() {
        // Clear grid pads (64 pads)
        for (int i = 0; i < 64; i++) {
            setGridPadColor(i, Color::BLACK);
        }
        
        // Clear column buttons
        for (int i = 0; i < 8; i++) {
            setColumnButtonColor(i, Color::BLACK);
        }
        
        // Clear layer buttons  
        for (int i = 0; i < 8; i++) {
            setLayerButtonColor(i, Color::BLACK);
        }
        
        // Clear navigation buttons
        setNavigationButtonColor(NAV_UP, Color::BLACK);
        setNavigationButtonColor(NAV_DOWN, Color::BLACK);
        setNavigationButtonColor(NAV_LEFT, Color::BLACK);
        setNavigationButtonColor(NAV_RIGHT, Color::BLACK);
    }

    // Update grid pad lighting based on clip states
    void updateGridLights() {
        if (!parentUI) return;
        
        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 8; col++) {
                int globalCol = col + parentUI->gridOffsetColumn;
                int globalLayer = row + parentUI->gridOffsetLayer;
                
                // Get clip state from Resolume tracker
                bool hasClip = parentUI->resolumeTracker.hasClip(globalCol, globalLayer);
                bool isConnected = parentUI->resolumeTracker.isColumnConnected(globalCol);
                bool isPlaying = parentUI->resolumeTracker.isClipPlaying(globalCol, globalLayer);
                
                Color color = Color::BLACK;
                
                if (hasClip) {
                    if (isPlaying) {
                        // Playing clip: bright color
                        color = getClipColor(globalCol, globalLayer, 1.0f);
                    } else {
                        // Stopped clip: dim color
                        color = getClipColor(globalCol, globalLayer, 0.3f);
                    }
                } else if (isConnected) {
                    // No clip but column connected: very dim white
                    color = Color(20, 20, 20);
                }
                // else: no clip, not connected = black (already set)
                
                int padIndex = row * 8 + col;
                setGridPadColor(padIndex, color);
            }
        }
    }

    // Update column button lighting (rainbow for available columns)
    void updateColumnLights() {
        if (!parentUI) return;
        
        for (int i = 0; i < 8; i++) {
            int globalCol = i + parentUI->gridOffsetColumn;
            bool isConnected = parentUI->resolumeTracker.isColumnConnected(globalCol);
            
            Color color;
            if (isConnected) {
                // Rainbow color based on column position
                float hue = (globalCol * 360.0f / 16.0f); // Distribute over 16 columns
                color = Color::fromHSV(hue, 1.0f, 0.8f);
            } else {
                // Dim white for disconnected columns
                color = Color(30, 30, 30);
            }
            
            setColumnButtonColor(i, color);
        }
    }

    // Update layer button lighting (white for available layers)
    void updateLayerLights() {
        if (!parentUI) return;
        
        for (int i = 0; i < 8; i++) {
            int globalLayer = i + parentUI->gridOffsetLayer;
            bool hasContent = parentUI->resolumeTracker.hasLayerContent(globalLayer);
            
            Color color;
            if (hasContent) {
                // White for layers with content, saturation based on layer position
                float saturation = 1.0f - (globalLayer * 0.8f / 16.0f); // Fade over 16 layers
                saturation = (std::max)(0.2f, saturation);
                color = Color::fromHSV(0, 0, saturation); // White with varying brightness
            } else {
                // Very dim for empty layers
                color = Color(15, 15, 15);
            }
            
            setLayerButtonColor(i, color);
        }
    }

    // Update navigation button lighting
    void updateNavigationLights() {
        Color navColor = Color(40, 40, 40); // Dim white
        
        setNavigationButtonColor(NAV_UP, navColor);
        setNavigationButtonColor(NAV_DOWN, navColor);
        setNavigationButtonColor(NAV_LEFT, navColor);
        setNavigationButtonColor(NAV_RIGHT, navColor);
    }

    // Get color for a specific clip based on its position
    Color getClipColor(int column, int layer, float brightness = 1.0f) {
        // Generate color based on column and layer position
        float hue = (column * 360.0f / 16.0f); // Hue based on column (16 columns cycle)
        float saturation = 0.4f + (layer * 0.6f / 16.0f); // Saturation based on layer
        saturation = (std::min)(1.0f, saturation);
        
        return Color::fromHSV(hue, saturation, brightness);
    }

    // Set color for a grid pad (0-63, row-major order)
    void setGridPadColor(int padIndex, const Color& color) {
        if (padIndex < 0 || padIndex >= 64) return;
        
        // Send MIDI message to set pad color
        // Note: Implementation depends on PushUSB interface
        int row = padIndex / 8;
        int col = padIndex % 8;
        int midiNote = 36 + row * 8 + col; // Standard Push 2 pad mapping
        
        // Convert color to Push 2 velocity value
        int velocity = colorToVelocity(color);
        // pushDevice.sendMIDI(0x90, midiNote, velocity);
        // Placeholder for now
    }

    // Set color for column button (0-7)
    void setColumnButtonColor(int columnIndex, const Color& color) {
        if (columnIndex < 0 || columnIndex >= 8) return;
        
        int midiNote = 20 + columnIndex; // Push 2 column button mapping
        int velocity = colorToVelocity(color);
        // pushDevice.sendMIDI(0x90, midiNote, velocity);
        // Placeholder for now
    }

    // Set color for layer button (0-7)  
    void setLayerButtonColor(int layerIndex, const Color& color) {
        if (layerIndex < 0 || layerIndex >= 8) return;
        
        int midiNote = 102 + layerIndex; // Push 2 layer button mapping (side buttons)
        int velocity = colorToVelocity(color);
        // pushDevice.sendMIDI(0x90, midiNote, velocity);
        // Placeholder for now
    }

    // Set color for navigation button
    void setNavigationButtonColor(NavigationButton button, const Color& color) {
        int midiNote;
        switch (button) {
            case NAV_UP: midiNote = 46; break;
            case NAV_DOWN: midiNote = 47; break;
            case NAV_LEFT: midiNote = 44; break;
            case NAV_RIGHT: midiNote = 45; break;
            default: return;
        }
        
        int velocity = colorToVelocity(color);
        // pushDevice.sendMIDI(0x90, midiNote, velocity);
        // Placeholder for now
    }
};

// PushDisplay class - handles all display rendering
class PushDisplay {
private:
    PushUSB& pushDevice;
    PushUI* parentUI;  // Reference to parent for accessing state
    
    // Display buffer (simple grayscale for now)
    uint8_t displayBuffer[DISPLAY_WIDTH * DISPLAY_HEIGHT];
    
public:
    PushDisplay(PushUSB& push) : pushDevice(push), parentUI(nullptr) {
        memset(displayBuffer, 0, sizeof(displayBuffer));
    }
    
    // Set parent UI reference after construction
    void setParentUI(PushUI* parent) { parentUI = parent; }
    
    // Clear display
    void clear() {
        memset(displayBuffer, 0, sizeof(displayBuffer));
    }
    
    void fill(uint8_t brightness) {
        memset(displayBuffer, brightness, sizeof(displayBuffer));
    }
    
    // High-level UI rendering
    void renderClipView(const ResolumeTracker& tracker) {
        clear();
        drawText(10, 10, "CLIP VIEW");
    }
    
    void renderLayerEffectsView(const ResolumeTracker& tracker, int layerId) {
        clear();
        char title[64];
        snprintf(title, sizeof(title), "LAYER %d EFFECTS", layerId);
        drawText(10, 10, title);
    }
    
    void renderMixerView(const ResolumeTracker& tracker) {
        clear();
        drawText(10, 10, "MIXER VIEW");
    }
    
    void renderBrowserView() {
        clear();
        drawText(10, 10, "BROWSER");
    }
    
    // Send frame to device  
    void sendToDevice() {
        sendToDisplay();
    }
    
    // UI elements
    void drawClipInfo(int x, int y, int layerId, int clipId, const ResolumeTracker& tracker) {
        // Skeleton: Draw clip information
    }
    
    void drawLayerInfo(int x, int y, int layerId, const ResolumeTracker& tracker) {
        // Skeleton: Draw layer information
    }
    
    void drawParameterBar(int x, int y, int width, const std::string& name, float value) {
        // Skeleton: Draw parameter bar
    }
    
    void drawStatusBar() {
        // Skeleton: Draw status information
    }

    // Update display content
    void update() {
        // Clear display buffer
        clearDisplay();
        
        // Draw main grid view
        drawGridView();
        
        // Draw status information
        drawStatusBar();
        
        // Send display data to Push 2
        sendToDisplay();
    }

    // Clear the display buffer
    void clearDisplay() {
        // Fill display buffer with black
        memset(displayBuffer, 0, sizeof(displayBuffer));
    }

    // Draw the main grid view showing current clips
    void drawGridView() {
        if (!parentUI) return;
        
        // Draw grid representation of visible clips
        int startX = 10, startY = 30;
        int cellWidth = 15, cellHeight = 10;
        
        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 8; col++) {
                int globalCol = col + parentUI->gridOffsetColumn;
                int globalLayer = row + parentUI->gridOffsetLayer;
                
                int x = startX + col * cellWidth;
                int y = startY + row * cellHeight;
                
                bool hasClip = parentUI->resolumeTracker.hasClip(globalCol, globalLayer);
                bool isPlaying = parentUI->resolumeTracker.isClipPlaying(globalCol, globalLayer);
                
                if (hasClip) {
                    if (isPlaying) {
                        drawRect(x, y, cellWidth-2, cellHeight-2, 255); // Bright white
                    } else {
                        drawRect(x, y, cellWidth-2, cellHeight-2, 128); // Dim white
                    }
                } else {
                    drawRect(x, y, cellWidth-2, cellHeight-2, 32); // Very dim outline
                }
            }
        }
    }

    // Draw status information
    void drawStatusBar() {
        if (!parentUI) return;
        
        // Draw current grid position
        char statusText[64];
        snprintf(statusText, sizeof(statusText), "Col:%d-%d Layer:%d-%d", 
                 parentUI->gridOffsetColumn, parentUI->gridOffsetColumn + 7,
                 parentUI->gridOffsetLayer, parentUI->gridOffsetLayer + 7);
        
        drawText(10, 10, statusText);
        
        // Draw deck information
        snprintf(statusText, sizeof(statusText), "Deck: %d", parentUI->resolumeTracker.getCurrentDeck());
        drawText(10, 140, statusText);
    }

    // Send display buffer to Push 2
    void sendToDisplay() {
        // Note: Implementation depends on PushUSB interface
        // pushDevice.sendDisplayData(displayBuffer, sizeof(displayBuffer));
        // For now, just a placeholder
    }
    
private:
    // Draw a filled rectangle on the display
    void drawRect(int x, int y, int width, int height, uint8_t brightness) {
        for (int dy = 0; dy < height; dy++) {
            for (int dx = 0; dx < width; dx++) {
                int px = x + dx;
                int py = y + dy;
                if (px >= 0 && px < DISPLAY_WIDTH && py >= 0 && py < DISPLAY_HEIGHT) {
                    displayBuffer[py * DISPLAY_WIDTH + px] = brightness;
                }
            }
        }
    }

    // Draw text on the display (simple bitmap font)
    void drawText(int x, int y, const char* text) {
        // Simple 5x7 character rendering
        // This is a placeholder - real implementation would use actual font data
        int charWidth = 6;
        int charHeight = 8;
        
        for (int i = 0; text[i] != '\0'; i++) {
            char c = text[i];
            int charX = x + i * charWidth;
            
            // Draw simple character representation
            if (c >= ' ' && c <= '~') {
                // Draw a simple rectangle for each character as placeholder
                drawRect(charX, y, charWidth-1, charHeight, 200);
            }
        }
    }
};

// Main PushUI class - coordinates everything
class PushUI {
    // Allow PushLights and PushDisplay to access private members
    friend class PushLights;
    friend class PushDisplay;
    
private:
    // Core dependencies
    PushUSB& pushDevice;
    ResolumeTracker& resolumeTracker;
    std::unique_ptr<OSCSender> oscSender;

    // UI components
    PushLights lights;
    PushDisplay display;

    // Current view mode
    enum class ViewMode {
        CLIPS,          // 8x8 clip grid view
        LAYER_EFFECTS,  // Effects for selected layer
        MIXER,          // Layer mixing controls
        BROWSER         // File browser mode
    };
    ViewMode currentViewMode;

    // Navigation state for 8x8 grid
    int gridOffsetColumn;  // 0-based offset for which column appears at left edge
    int gridOffsetLayer;   // 0-based offset for which layer appears at bottom edge

    // Timing for animations
    std::chrono::steady_clock::time_point lastUpdateTime;

    // Push 2 pad layout (8x8 grid, MIDI notes 36-99)
    static const int PAD_ROWS = 8;
    static const int PAD_COLS = 8;
    static const int FIRST_PAD_NOTE = 36;
    
    // Push 2 button/encoder CC numbers (based on documentation)
    enum PushControls {
        // Navigation buttons
        BTN_OCTAVE_UP = 55,
        BTN_OCTAVE_DOWN = 54,
        BTN_PAGE_LEFT = 63,
        BTN_PAGE_RIGHT = 62,
        
        // Column buttons (CC 20-27)
        BTN_COL_1 = 20,
        BTN_COL_2 = 21,
        BTN_COL_3 = 22,
        BTN_COL_4 = 23,
        BTN_COL_5 = 24,
        BTN_COL_6 = 25,
        BTN_COL_7 = 26,
        BTN_COL_8 = 27,
        
        // Note length buttons (CC 36-43) - used as layer select
        BTN_LAYER_1 = 36,
        BTN_LAYER_2 = 37,
        BTN_LAYER_3 = 38,
        BTN_LAYER_4 = 39,
        BTN_LAYER_5 = 40,
        BTN_LAYER_6 = 41,
        BTN_LAYER_7 = 42,
        BTN_LAYER_8 = 43,
        
        // Top row buttons
        BTN_TRACK = 102,
        BTN_CLIP = 103,
        BTN_DEVICE = 104,
        BTN_BROWSE = 105,
        BTN_MIX = 106,
        BTN_EDIT = 107,
        BTN_CREATE = 108,
        BTN_QUANTIZE = 109,
        
        // Transport
        BTN_PLAY = 85,
        BTN_RECORD = 86,
        BTN_STOP = 87,
        
        // Encoders (8 rotary encoders)
        ENC_1 = 71,
        ENC_2 = 72,
        ENC_3 = 73,
        ENC_4 = 74,
        ENC_5 = 75,
        ENC_6 = 76,
        ENC_7 = 77,
        ENC_8 = 78,
        
        // Encoder buttons
        ENC_BTN_1 = 20,
        ENC_BTN_2 = 21,
        ENC_BTN_3 = 22,
        ENC_BTN_4 = 23,
        ENC_BTN_5 = 24,
        ENC_BTN_6 = 25,
        ENC_BTN_7 = 26,
        ENC_BTN_8 = 27
    };

public:
    // Constructor
    PushUI(PushUSB& push, ResolumeTracker& tracker, std::unique_ptr<OSCSender> osc)
        : pushDevice(push), resolumeTracker(tracker), oscSender(std::move(osc)),
          lights(pushDevice), display(pushDevice),
          currentViewMode(ViewMode::CLIPS), gridOffsetColumn(0), gridOffsetLayer(0),
          lastUpdateTime(std::chrono::steady_clock::now()) {
        
        // Set parent references
        lights.setParentUI(this);
        display.setParentUI(this);
    }
    
    // Destructor
    ~PushUI() {
        // Cleanup - turn off all lights
        lights.clearAll();
        lights.sendToDevice();
    }

    // Initialization
    bool initialize() {
        if (!pushDevice.isDeviceConnected()) {
            std::cerr << "Push device not connected" << std::endl;
            return false;
        }
        
        // Clear everything
        lights.clearAll();
        display.clear();
        
        // Set initial view
        setViewMode(ViewMode::CLIPS);
        
        return true;
    }

    // Main update loop - call this regularly
    void update() {
        auto currentTime = std::chrono::steady_clock::now();
        lastUpdateTime = currentTime;
        
        // Update animations
        lights.updateAnimations();
        
        // Update lights based on current Resolume state
        switch (currentViewMode) {
            case ViewMode::CLIPS:
                lights.showResolumeGrid(resolumeTracker, gridOffsetColumn, gridOffsetLayer);
                lights.showColumnButtons(resolumeTracker, gridOffsetColumn);
                lights.showLayerButtons(resolumeTracker, gridOffsetLayer);
                lights.showNavigationButtons();
                display.renderClipView(resolumeTracker);
                break;
            case ViewMode::LAYER_EFFECTS:
                lights.showLayerEffects(resolumeTracker, resolumeTracker.getSelectedLayerId());
                display.renderLayerEffectsView(resolumeTracker, resolumeTracker.getSelectedLayerId());
                break;
            case ViewMode::MIXER:
                lights.showMixerView(resolumeTracker);
                display.renderMixerView(resolumeTracker);
                break;
            case ViewMode::BROWSER:
                display.renderBrowserView();
                break;
        }
        
        // Send updates to device
        lights.sendToDevice();
        display.sendToDevice();
    }

    // MIDI input handler - set this as callback for PushUSB
    void onMidiMessage(const MidiMessage& msg) {
        if (msg.isNoteOn()) {
            handlePadPress(msg.getNote(), msg.getVelocity());
        } else if (msg.isNoteOff()) {
            handlePadRelease(msg.getNote());
        } else if (msg.isControlChange()) {
            int controller = msg.getController();
            int value = msg.getValue();
            
            // Check if it's an encoder
            if (controller >= ENC_1 && controller <= ENC_8) {
                int delta = (value < 64) ? value : value - 128;  // Convert to signed delta
                handleEncoderTurn(controller - ENC_1, delta);
            } else if (controller >= ENC_BTN_1 && controller <= ENC_BTN_8) {
                if (value > 0) {
                    handleEncoderPress(controller - ENC_BTN_1);
                }
            } else {
                handleButtonPress(controller, value);
            }
        }
    }

    // Handle MIDI input from Push 2
    void handleMIDI(uint8_t status, uint8_t data1, uint8_t data2) {
        uint8_t channel = status & 0x0F;
        uint8_t command = status & 0xF0;
        
        if (command == 0x90 && data2 > 0) { // Note On
            handleButtonPress(data1);
        } else if (command == 0x80 || (command == 0x90 && data2 == 0)) { // Note Off
            handleButtonRelease(data1);
        }
    }

    // Handle button press events
    void handleButtonPress(uint8_t midiNote) {
        // Grid pads (36-99, 8x8 grid)
        if (midiNote >= 36 && midiNote <= 99) {
            int padIndex = midiNote - 36;
            int row = padIndex / 8;
            int col = padIndex % 8;
            
            if (row < 8 && col < 8) { // Valid grid position
                handleGridPress(col, row);
            }
            return;
        }
        
        // Column buttons (20-27)
        if (midiNote >= 20 && midiNote <= 27) {
            int columnIndex = midiNote - 20;
            handleColumnPress(columnIndex);
            return;
        }
        
        // Layer buttons (102-109, side buttons)
        if (midiNote >= 102 && midiNote <= 109) {
            int layerIndex = midiNote - 102;
            handleLayerPress(layerIndex);
            return;
        }
        
        // Navigation buttons
        switch (midiNote) {
            case 44: handleNavigationPress(NAV_LEFT); break;
            case 45: handleNavigationPress(NAV_RIGHT); break;
            case 46: handleNavigationPress(NAV_UP); break;
            case 47: handleNavigationPress(NAV_DOWN); break;
        }
    }

    // Handle button release events
    void handleButtonRelease(uint8_t midiNote) {
        // Currently no special handling for button releases
        // Could be used for things like velocity-sensitive actions
    }

    // Update the UI (called periodically)
    void update() {
        // Update Resolume state
        resolumeTracker.update();
        
        // Check for deck changes
        if (resolumeTracker.hasDeckChanged()) {
            // Reset navigation to origin on deck change
            gridOffsetColumn = 0;
            gridOffsetLayer = 0;
        }
        
        // Update lighting
        lights.updateLights();
        
        // Update display
        display.update();
    }

    // View mode control
    void setViewMode(ViewMode mode) {
        currentViewMode = mode;
        
        // Update button lights to reflect current mode
        lights.clearAllButtons();
        
        switch (mode) {
            case ViewMode::CLIPS:
                lights.setButtonColor(BTN_CLIP, 127);  // Bright color
                break;
            case ViewMode::LAYER_EFFECTS:
                lights.setButtonColor(BTN_DEVICE, 127);
                break;
            case ViewMode::MIXER:
                lights.setButtonColor(BTN_MIX, 127);
                break;
            case ViewMode::BROWSER:
                lights.setButtonColor(BTN_BROWSE, 127);
                break;
        }
    }
    
    ViewMode getCurrentViewMode() const {
        return currentViewMode;
    }

private:
    // MIDI input handlers
    void handlePadPress(int note, int velocity) {
        if (note < FIRST_PAD_NOTE || note > FIRST_PAD_NOTE + 63) return;
        
        int padIndex = note - FIRST_PAD_NOTE;
        auto [row, col] = indexToRowCol(padIndex);
        
        switch (currentViewMode) {
            case ViewMode::CLIPS: {
                // Convert grid position to Resolume layer/clip IDs
                auto [layerId, clipId] = gridPosToResolumeIds(row, col);
                triggerClip(layerId, clipId);
                break;
            }
            case ViewMode::LAYER_EFFECTS:
                // Handle effect parameter selection
                break;
            case ViewMode::MIXER:
                // Handle mixer controls
                break;
            case ViewMode::BROWSER:
                // Handle browser navigation
                break;
        }
    }
    
    void handlePadRelease(int note) {
        // Handle pad release if needed
    }
    
    void handleButtonPress(int controller, int value) {
        if (value == 0) return;  // Only handle button presses, not releases
        
        switch (controller) {
            // Navigation buttons
            case BTN_OCTAVE_UP:
                moveGridOffset(0, 1);
                break;
            case BTN_OCTAVE_DOWN:
                moveGridOffset(0, -1);
                break;
            case BTN_PAGE_RIGHT:
                moveGridOffset(1, 0);
                break;
            case BTN_PAGE_LEFT:
                moveGridOffset(-1, 0);
                break;
                
            // Column buttons
            case BTN_COL_1: case BTN_COL_2: case BTN_COL_3: case BTN_COL_4:
            case BTN_COL_5: case BTN_COL_6: case BTN_COL_7: case BTN_COL_8: {
                int columnIndex = controller - BTN_COL_1;
                int columnId = columnIndex + 1 + gridOffsetColumn;
                connectColumn(columnId);
                break;
            }
            
            // Layer buttons
            case BTN_LAYER_1: case BTN_LAYER_2: case BTN_LAYER_3: case BTN_LAYER_4:
            case BTN_LAYER_5: case BTN_LAYER_6: case BTN_LAYER_7: case BTN_LAYER_8: {
                int layerIndex = controller - BTN_LAYER_1;
                int layerId = layerIndex + 1 + gridOffsetLayer;
                selectLayer(layerId);
                break;
            }
            
            // View mode buttons
            case BTN_CLIP:
                setViewMode(ViewMode::CLIPS);
                break;
            case BTN_DEVICE:
                setViewMode(ViewMode::LAYER_EFFECTS);
                break;
            case BTN_MIX:
                setViewMode(ViewMode::MIXER);
                break;
            case BTN_BROWSE:
                setViewMode(ViewMode::BROWSER);
                break;
                
            // Transport
            case BTN_PLAY:
                // Handle play/pause
                break;
            case BTN_STOP:
                // Handle stop
                break;
        }
    }
    
    void handleEncoderTurn(int encoder, int delta) {
        // Handle encoder rotation based on current view mode
        switch (currentViewMode) {
            case ViewMode::LAYER_EFFECTS:
                // Adjust effect parameters
                break;
            case ViewMode::MIXER:
                // Adjust mixer parameters
                break;
        }
    }
    
    void handleEncoderPress(int encoder) {
        // Handle encoder button presses
    }

    // OSC command senders
    void triggerClip(int layerId, int clipId) {
        if (oscSender) {
            std::string address = "/composition/layers/" + std::to_string(layerId) + 
                                 "/clips/" + std::to_string(clipId) + "/connect";
            oscSender->sendMessage(address, 1);
        }
    }
    
    void connectColumn(int columnId) {
        if (oscSender) {
            std::string address = "/composition/columns/" + std::to_string(columnId) + "/connect";
            oscSender->sendMessage(address, 1);
        }
    }
    
    void selectLayer(int layerId) {
        if (oscSender) {
            std::string address = "/composition/layers/" + std::to_string(layerId) + "/select";
            oscSender->sendMessage(address, 1);
        }
    }
    
    void selectClip(int layerId, int clipId) {
        if (oscSender) {
            std::string address = "/composition/layers/" + std::to_string(layerId) + 
                                 "/clips/" + std::to_string(clipId) + "/select";
            oscSender->sendMessage(address, 1);
        }
    }
    
    void adjustParameter(const std::string& address, float value) {
        if (oscSender) {
            oscSender->sendMessage(address, value);
        }
    }

    // Navigation helpers
    void moveGridOffset(int deltaColumn, int deltaLayer) {
        gridOffsetColumn = (std::max)(0, gridOffsetColumn + deltaColumn);
        gridOffsetLayer = (std::max)(0, gridOffsetLayer + deltaLayer);
    }
    
    void resetGridOffset() {
        gridOffsetColumn = 0;
        gridOffsetLayer = 0;
    }
    
    // Color generation helpers
    Color generateColumnColor(int columnIndex) {
        float hue = (columnIndex * 360.0f / 8.0f);
        return Color::fromHSV(hue, 1.0f, 1.0f);
    }
    
    Color generateClipColor(int columnIndex, int layerIndex) {
        float hue = (columnIndex * 360.0f / 8.0f);
        float saturation = 0.3f + (layerIndex * 0.7f / 7.0f);
        return Color::fromHSV(hue, saturation, 1.0f);
    }

    // Utility functions
    int padNoteToIndex(int note) {
        return note - FIRST_PAD_NOTE;
    }
    
    std::pair<int, int> indexToRowCol(int index) {
        return {index / PAD_COLS, index % PAD_COLS};
    }
    
    int rowColToIndex(int row, int col) {
        return row * PAD_COLS + col;
    }
    
    // Convert grid position to Resolume layer/clip IDs
    std::pair<int, int> gridPosToResolumeIds(int gridRow, int gridCol) {
        // Convert 0-based grid position to 1-based Resolume IDs
        int layerId = (7 - gridRow) + 1 + gridOffsetLayer;  // Invert row (bottom=0 becomes layer 1)
        int clipId = gridCol + 1 + gridOffsetColumn;
        return {layerId, clipId};
    }

    // Handle grid pad press (clip triggering)
    void handleGridPress(int col, int row) {
        int globalCol = col + gridOffsetColumn;
        int globalLayer = row + gridOffsetLayer;
        
        // Send OSC command to trigger clip in Resolume
        sendClipTrigger(globalCol, globalLayer);
        
        // Update lighting immediately for visual feedback
        lights.updateGridLights();
    }

    // Handle column button press (column selection)
    void handleColumnPress(int columnIndex) {
        int globalCol = columnIndex + gridOffsetColumn;
        
        // Send OSC command to select column in Resolume
        sendColumnSelect(globalCol);
        
        // Update lighting
        lights.updateColumnLights();
    }

    // Handle layer button press (layer selection)
    void handleLayerPress(int layerIndex) {
        int globalLayer = layerIndex + gridOffsetLayer;
        
        // Send OSC command to select layer in Resolume
        sendLayerSelect(globalLayer);
        
        // Update lighting
        lights.updateLayerLights();
    }

    // Handle navigation button press (grid movement)
    void handleNavigationPress(NavigationButton navButton) {
        switch (navButton) {
            case NAV_LEFT:
                if (gridOffsetColumn > 0) {
                    gridOffsetColumn--;
                }
                break;
                
            case NAV_RIGHT:
                // Allow unlimited rightward navigation
                gridOffsetColumn++;
                break;
                
            case NAV_UP:
                if (gridOffsetLayer > 0) {
                    gridOffsetLayer--;
                }
                break;
                
            case NAV_DOWN:
                // Allow unlimited downward navigation
                gridOffsetLayer++;
                break;
        }
        
        // Update all lighting after navigation
        lights.updateLights();
        
        // Update display to show new grid position
        display.update();
    }

    // Send OSC command to trigger a clip
    void sendClipTrigger(int column, int layer) {
        char oscAddress[64];
        snprintf(oscAddress, sizeof(oscAddress), "/composition/layers/%d/clips/%d/connect", layer + 1, column + 1);
        
        // Send OSC message to Resolume (value 1 to trigger)
        sendOSCMessage(oscAddress, 1.0f);
    }

    // Send OSC command to select a column
    void sendColumnSelect(int column) {
        char oscAddress[64];
        snprintf(oscAddress, sizeof(oscAddress), "/composition/columns/%d/select", column + 1);
        
        // Send OSC message to select column
        sendOSCMessage(oscAddress, 1.0f);
    }

    // Send OSC command to select a layer
    void sendLayerSelect(int layer) {
        char oscAddress[64];
        snprintf(oscAddress, sizeof(oscAddress), "/composition/layers/%d/select", layer + 1);
        
        // Send OSC message to select layer
        sendOSCMessage(oscAddress, 1.0f);
    }

    // Send OSC message (to be implemented based on OSC library)
    void sendOSCMessage(const char* address, float value) {
        // Implementation depends on OSC library being used
        // This is a placeholder for the actual OSC sending logic
        
        // Example using oscpack (if available):
        // char buffer[1024];
        // osc::OutboundPacketStream p(buffer, 1024);
        // p << osc::BeginMessage(address) << value << osc::EndMessage;
        // 
        // // Send via UDP to Resolume (typically localhost:7000)
        // udpSocket.SendTo(IpEndpointName("127.0.0.1", 7000), p.Data(), p.Size());
        
        // For now, just print what would be sent
        printf("OSC: %s %.2f\n", address, value);
    }
};

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
const Color Color::DIM_RED(64, 0, 0);    // Convert Color to Push 2 velocity value for MIDI
    int colorToVelocity(const Color& color) {
        // Push 2 uses velocity values 1-127 for colors
        // This is a simplified mapping - real Push 2 has specific color palette
        if (color.r == 0 && color.g == 0 && color.b == 0) {
            return 0; // Black/off
        }
        
        // Map RGB to closest Push 2 palette color
        // For now, use a simple brightness-based mapping
        int brightness = (color.r + color.g + color.b) / 3;
        return (std::max)(1, (std::min)(127, brightness / 2 + 1));
    }

/*
Implementation notes for key methods:
PushUI Constructor:
- Initialize gridOffsetColumn = 0, gridOffsetLayer = 0
- Set currentViewMode to CLIPS

Color::fromHSV(float hue, float saturation, float value):
- Convert HSV (0-360, 0-1, 0-1) to RGB (0-255, 0-255, 0-255)
- Standard HSV to RGB conversion algorithm

PushUI::generateColumnColor(int columnIndex):
- Generate rainbow colors with even hue spacing
- Hue = (columnIndex * 360.0f / 8.0f) % 360.0f
- Full saturation and value for vivid colors
- Return Color::fromHSV(hue, 1.0f, 1.0f)

PushUI::generateClipColor(int columnIndex, int layerIndex):
- Use same hue as column: hue = (columnIndex * 360.0f / 8.0f) % 360.0f
- Saturation increases from bottom to top: saturation = 0.3f + (layerIndex * 0.7f / 7.0f)
- Return Color::fromHSV(hue, saturation, 1.0f)

PushUI::gridPosToResolumeIds(int gridRow, int gridCol):
- Convert 0-based grid position to 1-based Resolume IDs
- layerId = (7 - gridRow) + 1 + gridOffsetLayer  // Invert row (bottom=0 becomes layer 1)
- clipId = gridCol + 1 + gridOffsetColumn
- Return {layerId, clipId}

PushUI::handlePadPress(int note, int velocity):
- Convert note to grid position
- Get Resolume layer/clip IDs using gridPosToResolumeIds
- Call triggerClip(layerId, clipId)

PushUI::handleButtonPress(int controller, int value):
- Handle navigation buttons (BTN_OCTAVE_UP/DOWN, BTN_PAGE_LEFT/RIGHT)
- Handle column buttons (BTN_COL_1 to BTN_COL_8): call connectColumn()
- Handle layer buttons (BTN_LAYER_1 to BTN_LAYER_8): call selectLayer()

PushUI::triggerClip(int layerId, int clipId):
- Send OSC: "/composition/layers/{layerId}/clips/{clipId}/connect" with value 1

PushUI::connectColumn(int columnId):
- Send OSC: "/composition/columns/{columnId}/connect" with value 1

PushLights::showResolumeGrid():
- Clear all pads
- For each grid position (0-7, 0-7):
  - Convert to Resolume IDs
  - Check if clip exists in ResolumeTracker
  - If exists, set pad color using generateClipColor()

PushLights::showColumnButtons():
- Set each column button (BTN_COL_1 to BTN_COL_8) to rainbow color
- If column is connected, set to WHITE instead

PushLights::showLayerButtons():
- Set all layer buttons (BTN_LAYER_1 to BTN_LAYER_8) to WHITE

PushLights::showNavigationButtons():
- Set all navigation buttons to DIM_WHITE

PushUI::update() in CLIPS mode:
- Call lights.showResolumeGrid()
- Call lights.showColumnButtons()
- Call lights.showLayerButtons()
- Call lights.showNavigationButtons()
*/

#endif // PUSHUI_H
