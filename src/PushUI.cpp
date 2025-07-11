#include "PushUI.h"
#include "PushLights.h"
#include "PushDisplay.h"
#include <iostream>

PushUI::PushUI(PushUSB& push, ResolumeTracker& tracker, std::unique_ptr<OSCSender> osc)
    : pushDevice(push), resolumeTracker(tracker), oscSender(std::move(osc)),
      columnOffset(0), layerOffset(0),
      lastKnownDeck(-1), trackingInitialized(false)
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
    resetNavigation();
    std::cout << "PushUI initialized successfully" << std::endl;
    return true;
}

int PushUI::getColumnOffset() const { return columnOffset; }
int PushUI::getLayerOffset() const { return layerOffset; }
ResolumeTracker& PushUI::getResolumeTracker() { return resolumeTracker; }

void PushUI::update() {
    checkForDeckChange();
    lights->updateLights();
    display->update();
    display->sendToDevice();
}

void PushUI::onMidiMessage(const PushMidiMessage& msg) {
    if (msg.isNoteOn()) {
        handlePadPress(msg.getNote(), msg.getVelocity());
    } else if (msg.isControlChange()) {
        int cc = msg.getController();
        int value = msg.getValue();
        if (cc >= 20 && cc <= 27 && value > 0) {
            int column = columnOffset + (cc - 20) + 1;
            std::string address = "/composition/columns/" + std::to_string(column) + "/connect";
            if (oscSender) {
                oscSender->sendMessage(address, 1.0f);
            } else {
                std::cout << "Would trigger: " << address << std::endl;
            }
            return;
        }
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

void PushUI::checkForDeckChange() {
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

void PushUI::resetNavigation() {
    columnOffset = 0;
    layerOffset = 0;
    std::cout << "Navigation reset to origin (Column 1, Layer 1)" << std::endl;
}

void PushUI::handlePadPress(int note, int velocity) {
    if (velocity == 0) return;
    if (note >= 36 && note <= 99) {
        int padIndex = note - 36;
        int gridRow = padIndex / 8;
        int gridCol = padIndex % 8;
        int resolumeLayer = gridRow + 1 + layerOffset;
        int resolumeColumn = gridCol + 1 + columnOffset;
        //std::cout << "Pad pressed: Grid(" << gridRow << "," << gridCol << ") -> "
        //          << "Resolume Layer " << resolumeLayer << ", Column " << resolumeColumn << std::endl;
        triggerClip(resolumeLayer, resolumeColumn);
    }
}

void PushUI::handleNavigationButtons(int controller, int value) {
    if (value == 0) return;
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

bool PushUI::canMoveLayerUp() const {
    for (int i = 1; i <= 8; i++) {
        int checkLayer = layerOffset + 8 + i;
        if (resolumeTracker.hasLayerContent(checkLayer)) {
            return true;
        }
    }
    return false;
}

bool PushUI::canMoveLayerDown() const {
    return layerOffset > 0;
}

bool PushUI::canMoveColumnRight() const {
    for (int i = 1; i <= 8; i++) {
        int checkColumn = columnOffset + 8 + i;
        for (int layer = 1; layer <= 32; layer++) {
            if (resolumeTracker.hasClip(checkColumn, layer)) {
                return true;
            }
        }
    }
    return false;
}

bool PushUI::canMoveColumnLeft() const {
    return columnOffset > 0;
}

void PushUI::triggerClip(int layer, int column) {
    std::string address = "/composition/layers/" + std::to_string(layer) +
                             "/clips/" + std::to_string(column) + "/connect";
    if (oscSender) {
        oscSender->sendMessage(address, 1.0f);
    } else {
        std::cout << "Would trigger: " << address << std::endl;
    }
}
