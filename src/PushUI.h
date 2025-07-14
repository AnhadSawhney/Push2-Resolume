#pragma once

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

#include "OSCSender.h"

#include "PushUSB.h"
//#include "ResolumeTrackerREST.h"
#include "ResolumeTrackerOSC.h"

// Forward declarations for OSC functionality

class PushLights;
class PushDisplay;

// Main PushUI class
class PushUI {
    friend class PushLights;
    friend class PushDisplay;
private:
    PushUSB& pushDevice;
    ResolumeTracker& resolumeTracker;
    std::unique_ptr<OSCSender> oscSender;
    PushLights* lights;
    PushDisplay* display;
    int columnOffset;
    int layerOffset;
    int numLayers;  // Total number of layers in the current deck
    int numColumns; // Total number of columns in the current deck
    enum PushControls {
        BTN_OCTAVE_UP = 55,
        BTN_OCTAVE_DOWN = 54,
        BTN_PAGE_LEFT = 62,
        BTN_PAGE_RIGHT = 63,
        BTN_PLAY = 85,
        BTN_RECORD = 86,
        BTN_STOP = 87
    };
    int lastKnownDeck;
    bool trackingInitialized;

    // Add mode enum and member
    enum class Mode {
        Triggering,
        Selecting
    };
    Mode mode = Mode::Triggering;

public:
    PushUI(PushUSB& push, ResolumeTracker& tracker, std::unique_ptr<OSCSender> osc = nullptr);
    ~PushUI();
    bool initialize();
    int getColumnOffset() const;
    int getLayerOffset() const;
    int getNumLayers() const;
    int getNumColumns() const;
    ResolumeTracker& getResolumeTracker();
    void update();
    void onMidiMessage(const PushMidiMessage& msg);
    void forceRefresh();
    OSCSender* getOSCSender() { return oscSender.get(); }

    // Mode accessors
    Mode getMode() const { return mode; }
    void setMode(Mode m) { mode = m; }
    void toggleMode();

private:
    void handlePadPress(int note, int velocity);
    void handleNavigationButtons(int controller, int value);
};

