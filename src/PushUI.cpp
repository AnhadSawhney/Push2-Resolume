#include "PushUI.h"
#include "PushLights.h"
#include "PushDisplay.h"
#include <iostream>

PushUI::PushUI(PushUSB& push, ResolumeTracker& tracker, std::shared_ptr<OSCSender> osc)
    : pushDevice(push), resolumeTracker(tracker), oscSender(osc), // Changed to shared_ptr
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

    std::cout << "Mode toggled to: " << (mode == Mode::Triggering ? "Triggering" : "Selecting") << std::endl;
}

void PushUI::onMidiMessage(const PushMidiMessage& msg) {
    if (msg.isNoteOn()) {
        handlePadPress(msg.getNote(), msg.getVelocity());
    } else if (msg.isPitchBend()) {
        handleTouchStripPitchBend(msg.getPitchBend());
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
                    oscSender->sendMessage(address, 1);
                } else {
                    std::cout << "Would select: " << address << std::endl;
                }
            } else {
                std::string address = "/composition/columns/" + std::to_string(column) + "/connect";
                if (oscSender) {
                    oscSender->sendMessage(address, 1);
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
                oscSender->sendMessage(address, 1);
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
                oscSender->sendMessage(address, velocity ? 1 : 0);
            } else {
                std::cout << "Would select: " << address << std::endl;
            }
        } else {
            // Trigger the clip
            std::string address = "/composition/layers/" + std::to_string(resolumeLayer) +
                             "/clips/" + std::to_string(resolumeColumn) + "/connect";
            if (oscSender) {
                oscSender->sendMessage(address, velocity ? 1 : 0);
            } else {
                std::cout << "Would trigger: " << address << std::endl;
            }

            // After triggering the clip, timeout all other clips in the same layer. If done before resolume may still send messages for the clip we tried to stop
            auto layer = resolumeTracker.getLayer(resolumeLayer);
            if (layer) {
                layer->timeoutAllExcept(resolumeColumn);
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

    int d = resolumeTracker.getCurrentDeck();

    std::string address;

    switch (controller) {
        case 49:
            // don't clear, resolumeTracker will automatically clear on deck change. 
            //std::cout << "Deck: " << d << std::endl;
            //resolumeTracker.clear();
            // send the osc message to change the deck
            if(d <= 1) break; // don't go below deck 1
            address = "/composition/decks/" + std::to_string(d-1) + "/select";
            if (oscSender) {
                oscSender->sendMessage(address, 1);
            }
            break;
        case 48:
            //std::cout << "Deck: " << d << std::endl;
            //resolumeTracker.clear();
            // send the osc message to change the deck
            address = "/composition/decks/" + std::to_string(d+1) + "/select";
            if (oscSender) {
                oscSender->sendMessage(address, 1);
            }
            break;
    }
}

void PushUI::handleTouchStripPitchBend(uint16_t pitchBendValue) {
    // Check if there's a selected layer
    int selectedLayer = resolumeTracker.getSelectedLayer();
    if (selectedLayer <= 0) {
        return; // No layer selected
    }
    
    // Convert 14-bit pitch bend value (0-16383) to normalized position (0.0-1.0)
    float normalizedPosition = static_cast<float>(pitchBendValue) / 16383.0f;
    
    float opacity;
    
    // Create sensitive middle region with clipped top and bottom fourths
    if (normalizedPosition <= 0.25f) {
        // Bottom fourth - clamp to 0
        opacity = 0.0f;
    } else if (normalizedPosition >= 0.75f) {
        // Top fourth - clamp to 1
        opacity = 1.0f;
    } else {
        // Middle half (0.25 to 0.75) - map to full range (0.0 to 1.0)
        opacity = (normalizedPosition - 0.25f) / 0.5f;
    }
    
    // Clamp to valid range (should already be in range, but safety check)
    opacity = std::max(0.0f, std::min(1.0f, opacity));
    
    // Send OSC message to set layer opacity
    std::string address = "/composition/selectedlayer/video/opacity";
    if (oscSender) {
        oscSender->sendMessage(address, opacity);
    } else {
        std::cout << "Would set layer " << selectedLayer << " opacity to: " << opacity << std::endl;
    }
}

/*
inline bool canMoveLayerUp() { return layerOffset + 8 < numLayers; };
inline bool canMoveLayerDown() { return layerOffset > 0; };
inline bool canMoveColumnRight() { return columnOffset + 8 < numColumns; };
inline bool canMoveColumnLeft() { return columnOffset > 0; };
*/