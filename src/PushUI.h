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
public:
    PushUI(PushUSB& push, ResolumeTracker& tracker, std::unique_ptr<OSCSender> osc = nullptr);
    ~PushUI();
    bool initialize();
    int getColumnOffset() const;
    int getLayerOffset() const;
    ResolumeTracker& getResolumeTracker();
    void update();
    void onMidiMessage(const PushMidiMessage& msg);
    void forceRefresh();
private:
    void checkForDeckChange();
    void resetNavigation();
    void handlePadPress(int note, int velocity);
    void handleNavigationButtons(int controller, int value);
    bool canMoveLayerUp() const;
    bool canMoveLayerDown() const;
    bool canMoveColumnRight() const;
    bool canMoveColumnLeft() const;
    void triggerClip(int layer, int column);
};

