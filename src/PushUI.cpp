#include "PushUI.h"
#include "PushLights.h"
#include "PushDisplay.h"
#include <iostream>

PushUI::PushUI(PushUSB& push, ResolumeTracker& tracker, std::unique_ptr<OSCSender> osc)
    : pushDevice(push), resolumeTracker(tracker), oscSender(std::move(osc)),
      columnOffset(0), layerOffset(0),
      lastKnownDeck(-1), trackingInitialized(false),
      numLayers(0), numColumns(0), mode(Mode::Triggering) // <-- add members for layer/column count
{
    lights = new PushLights(pushDevice);
    display = new PushDisplay(pushDevice);
    lights->setParentUI(this);
    display->setParentUI(this);
}

PushUI::~PushUI() {
    lights->clearAllPads();
    lights->clearAllButtons();
    delete lights;
    delete display;
}

bool PushUI::initialize() {
    if (!pushDevice.isDeviceConnected()) {
        std::cerr << "Push device not connected" << std::endl;
        return false;
    }
    pushDevice.setMidiCallback([this](const PushMidiMessage& msg) {
        this->onMidiMessage(msg);
    });
    lights->clearAllPads();
    lights->clearAllButtons();
    std::cout << "PushUI initialized successfully" << std::endl;
    return true;
}

int PushUI::getColumnOffset() const { return columnOffset; }
int PushUI::getLayerOffset() const { return layerOffset; }
int PushUI::getNumLayers() const { return resolumeTracker.getLayerCount(); }
int PushUI::getNumColumns() const { return resolumeTracker.getColumnCount(); }

ResolumeTracker& PushUI::getResolumeTracker() { return resolumeTracker; }

void PushUI::update() {
    //resolumeTracker.update();
    lights->updateLights();
    display->update();
    display->sendToDevice();
}

void PushUI::toggleMode() {
    if (mode == Mode::Triggering) {
        mode = Mode::Selecting;
    } else {
        mode = Mode::Triggering;
    }
}

void PushUI::onMidiMessage(const PushMidiMessage& msg) {
    if (msg.isNoteOn()) {
        handlePadPress(msg.getNote(), msg.getVelocity());
    } else if (msg.isControlChange()) {
        int cc = msg.getController();
        int value = msg.getValue();

        // Master button (cc28) toggles mode
        if (cc == 28 && value > 0) {
            toggleMode();
            return;
        }

        // Column buttons
        if (cc >= 20 && cc <= 27 && value > 0) {
            int column = columnOffset + (cc - 20) + 1;
            if (mode == Mode::Selecting) {
                std::string address = "/composition/columns/" + std::to_string(column) + "/select";
                if (oscSender) {
                    oscSender->sendMessage(address, 1.0f);
                } else {
                    std::cout << "Would select: " << address << std::endl;
                }
            } else {
                std::string address = "/composition/columns/" + std::to_string(column) + "/connect";
                if (oscSender) {
                    oscSender->sendMessage(address, 1.0f);
                } else {
                    std::cout << "Would trigger: " << address << std::endl;
                }
            }
            return;
        }

        // Layer buttons
        if (cc >= 36 && cc <= 43 && value > 0) {
            int layer = layerOffset + (cc - 36) + 1;
            std::string address = "/composition/layers/" + std::to_string(layer) + "/select";
            if (oscSender) {
                oscSender->sendMessage(address, 1.0f);
            } else {
                std::cout << "Would select: " << address << std::endl;
            }
            return;
        }

        handleNavigationButtons(cc, value);
    }
}

void PushUI::forceRefresh() {
    lights->forceRefresh();
    lights->updateLights();
}

void PushUI::handlePadPress(int note, int velocity) {
    if (velocity == 0) return;
    if (note >= 36 && note <= 99) {
        int padIndex = note - 36;
        int gridRow = padIndex / 8;
        int gridCol = padIndex % 8;
        int resolumeLayer = gridRow + 1 + layerOffset;
        int resolumeColumn = gridCol + 1 + columnOffset;
        if (mode == Mode::Selecting) {
            // Select the clip
            std::string address = "/composition/layers/" + std::to_string(resolumeLayer) +
                                  "/clips/" + std::to_string(resolumeColumn) + "/select";
            if (oscSender) {
                oscSender->sendMessage(address, 1.0f);
            } else {
                std::cout << "Would select: " << address << std::endl;
            }
        } else {
            // Trigger the clip
            std::string address = "/composition/layers/" + std::to_string(resolumeLayer) +
                             "/clips/" + std::to_string(resolumeColumn) + "/connect";
            if (oscSender) {
                oscSender->sendMessage(address, 1.0f);
            } else {
                std::cout << "Would trigger: " << address << std::endl;
            }
        }
    }
}

void PushUI::handleNavigationButtons(int controller, int value) {
    int columns = resolumeTracker.getColumnCount();
    int layers = resolumeTracker.getLayerCount();
    
    if (value == 0) return;
    if (controller == BTN_OCTAVE_UP && layerOffset + 8 < layers) {
        layerOffset++;
    } else if (controller == BTN_OCTAVE_DOWN && layerOffset > 0) {
        layerOffset--;
    } else if (controller == BTN_PAGE_RIGHT && columnOffset + 8 < columns) {
        columnOffset++;
    } else if (controller == BTN_PAGE_LEFT && columnOffset > 0) {
        columnOffset--;
    }
}

/*
inline bool canMoveLayerUp() { return layerOffset + 8 < numLayers; };
inline bool canMoveLayerDown() { return layerOffset > 0; };
inline bool canMoveColumnRight() { return columnOffset + 8 < numColumns; };
inline bool canMoveColumnLeft() { return columnOffset > 0; };
*/