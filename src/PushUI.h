#ifndef PUSHUI_H
#define PUSHUI_H

#include "PushUSB.h"
#include "ResolumeTracker.h"
#include <vector>
#include <map>
#include <chrono>
#include <functional>
#include <memory>

// Forward declarations for OSC functionality
// You'll need to implement these based on your OSC library
class OSCSender {
public:
    virtual void sendMessage(const std::string& address, float value) = 0;
    virtual void sendMessage(const std::string& address, int value) = 0;
    virtual void sendMessage(const std::string& address, const std::string& value) = 0;
};

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
    
public:
    PushLights(PushUSB& push);
    
    // Set individual pad colors
    void setPadColor(int note, const Color& color);
    void setPadColor(int row, int col, const Color& color);
    
    // Set button colors (using Push 2 color palette indices)
    void setButtonColor(int controller, uint8_t colorIndex);
    
    // Animation control
    void setPadPulsing(int note, bool pulsing, const Color& color);
    void clearAllPulsing();
    
    // Batch operations
    void clearAllPads();
    void clearAllButtons();
    void clearAll();
    
    // Update and send to device
    void updateAnimations();
    void sendToDevice();
    
    // Predefined color patterns
    void showClipGrid(const ResolumeTracker& tracker);
    void showLayerEffects(const ResolumeTracker& tracker, int layerId);
    void showMixerView(const ResolumeTracker& tracker);
};

// PushDisplay class - handles all display rendering
class PushDisplay {
private:
    PushUSB& pushDevice;
    
    // Display buffer (RGB565 format)
    std::vector<uint16_t> frameBuffer;
    
    // Display constants
    static const int WIDTH = PUSH2_DISPLAY_WIDTH;   // 960
    static const int HEIGHT = PUSH2_DISPLAY_HEIGHT; // 160
    
    // Basic drawing functions
    void setPixel(int x, int y, uint16_t color);
    uint16_t rgb888ToRgb565(uint8_t r, uint8_t g, uint8_t b);
    void drawRect(int x, int y, int width, int height, uint16_t color);
    void fillRect(int x, int y, int width, int height, uint16_t color);
    void drawText(int x, int y, const std::string& text, uint16_t color);
    
public:
    PushDisplay(PushUSB& push);
    
    // Clear display
    void clear();
    void fill(uint8_t r, uint8_t g, uint8_t b);
    
    // High-level UI rendering
    void renderClipView(const ResolumeTracker& tracker);
    void renderLayerEffectsView(const ResolumeTracker& tracker, int layerId);
    void renderMixerView(const ResolumeTracker& tracker);
    void renderBrowserView();
    
    // Send frame to device
    void sendToDevice();
    
    // UI elements
    void drawClipInfo(int x, int y, int layerId, int clipId, const ResolumeTracker& tracker);
    void drawLayerInfo(int x, int y, int layerId, const ResolumeTracker& tracker);
    void drawParameterBar(int x, int y, int width, const std::string& name, float value);
    void drawStatusBar();
};

// Main PushUI class - coordinates everything
class PushUI {
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

    // Timing for animations
    std::chrono::steady_clock::time_point lastUpdateTime;

    // Push 2 pad layout (8x8 grid, MIDI notes 36-99)
    static const int PAD_ROWS = 8;
    static const int PAD_COLS = 8;
    static const int FIRST_PAD_NOTE = 36;
    
    // Push 2 button/encoder CC numbers (you'll need to verify these)
    enum PushControls {
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
    PushUI(PushUSB& push, ResolumeTracker& tracker, std::unique_ptr<OSCSender> osc);
    
    // Destructor
    ~PushUI();

    // Initialization
    bool initialize();

    // Main update loop - call this regularly
    void update();

    // MIDI input handler - set this as callback for PushUSB
    void onMidiMessage(const MidiMessage& msg);

    // View mode control
    void setViewMode(ViewMode mode);
    ViewMode getCurrentViewMode() const;

private:
    // MIDI input handlers
    void handlePadPress(int note, int velocity);
    void handlePadRelease(int note);
    void handleButtonPress(int controller, int value);
    void handleEncoderTurn(int encoder, int delta);
    void handleEncoderPress(int encoder);

    // OSC command senders
    void triggerClip(int layerId, int clipId);
    void selectLayer(int layerId);
    void selectClip(int layerId, int clipId);
    void adjustParameter(const std::string& address, float value);

    // Utility functions
    int padNoteToIndex(int note);
    std::pair<int, int> indexToRowCol(int index);
    int rowColToIndex(int row, int col);
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
const Color Color::DIM_RED(64, 0, 0);

#endif // PUSHUI_H
