#pragma once

#define TRACKING_WITH_OSC
#ifndef TRACKING_WITH_REST

#include <vector>
#include <string>
#include <map>
#include <memory>
#include <iostream>
#include <variant>
#include <sstream>
#include <functional>
#include "PropertyDictionary.h"

// Include the ResolumeOSCListener header to provide the full type definition
#include "OSCListener.h"

// Helper function to split OSC address into path components
inline std::vector<std::string> splitOSCPath(const std::string& address) {
    std::vector<std::string> parts;
    if (address.empty() || address[0] != '/') return parts;
    
    size_t start = 1; // Skip initial slash
    size_t pos = start;
    
    while (pos < address.length()) {
        if (address[pos] == '/') {
            if (pos > start) {
                parts.push_back(address.substr(start, pos - start));
            }
            start = pos + 1;
        }
        pos++;
    }
    
    // Add final part if exists
    if (start < address.length()) {
        parts.push_back(address.substr(start));
    }
    
    return parts;
}

class Effect {
public:
    int id;
    std::string name;
    PropertyDictionary properties;
    
    Effect(int effectId, const std::string& effectName) 
        : id(effectId), name(effectName) {}
    
    void processOSCMessage(const std::vector<std::string>& pathParts, const std::vector<float>& floats, 
                          const std::vector<int>& integers, const std::vector<std::string>& strings) {
        if (pathParts.empty()) {
            // Root effect property
            properties.setFromOSCData("", floats, integers, strings);
        } else {
            // Build endpoint from remaining path
            std::string endpoint = "";
            for (size_t i = 0; i < pathParts.size(); ++i) {
                if (i > 0) endpoint += "/";
                endpoint += pathParts[i];
            }
            properties.setFromOSCData(endpoint, floats, integers, strings);
        }
    }
    
    void clear() {
        properties.clear();
    }
    
    // Print method for trickle-down printing
    void print(const std::string& indent) const {
        std::cout << indent << "Effect: " << name << " (ID: " << id << ")" << std::endl;
        if (!properties.properties.empty()) {
            std::cout << indent << "  Properties:" << std::endl;
            properties.print(indent + "    ");
        }
    }
};

class Clip {
public:
    int id;
    std::string name;
    PropertyDictionary properties;
    std::vector<std::shared_ptr<Effect>> effects;

    Clip(int clipId) : id(clipId), name("") {}

    void setName(const std::string& clipName) { name = clipName; }

    std::shared_ptr<Effect> getOrCreateEffect(const std::string& effectName) {
        // Find existing effect
        for (auto& effect : effects) {
            if (effect->name == effectName) {
                return effect;
            }
        }
        
        // Create new effect
        int newId = effects.size() + 1;
        auto newEffect = std::make_shared<Effect>(newId, effectName);
        effects.push_back(newEffect);
        return newEffect;
    }
    
    void processOSCMessage(const std::vector<std::string>& pathParts, const std::vector<float>& floats, 
                          const std::vector<int>& integers, const std::vector<std::string>& strings) {
        if (pathParts.empty()) {
            // Direct clip property
            properties.setFromOSCData("", floats, integers, strings);
            return;
        }
        
        std::vector<std::string> parts = pathParts; // Make a copy to modify
        
        // Handle clip name specially
        if (parts.size() == 1 && parts[0] == "name" && !strings.empty()) {
            setName(strings[0]);
            return;
        }
        
        if (parts[0] == "video" && parts.size() >= 3 && parts[1] == "effects") {
            // /video/effects/effectname/...
            std::string effectName = parts[2];
            auto effect = getOrCreateEffect(effectName);
            
            // Remove "video", "effects", and effect name from path
            std::vector<std::string> remainingPath(parts.begin() + 3, parts.end());
            effect->processOSCMessage(remainingPath, floats, integers, strings);
            return;
        }
        
        // Store as general property
        std::string endpoint = "";
        for (size_t i = 0; i < parts.size(); ++i) {
            if (i > 0) endpoint += "/";
            endpoint += parts[i];
        }
        properties.setFromOSCData(endpoint, floats, integers, strings);
    }
    
    void clear() {
        name = "";
        properties.clear();
        effects.clear();
    }
    
    // Print method for trickle-down printing
    void print(const std::string& indent) const {
        std::cout << indent << "Clip " << id << ": " << name << std::endl;
        
        if (!properties.properties.empty()) {
            std::cout << indent << "  Properties:" << std::endl;
            properties.print(indent + "    ");
        }
        
        if (!effects.empty()) {
            for (const auto& effect : effects) {
                effect->print(indent + "  ");
            }
        }
    }
};

class Layer {
public:
    int id;
    PropertyDictionary properties;
    std::vector<std::shared_ptr<Effect>> effects;
    std::vector<std::shared_ptr<Clip>> clips;

    Layer(int layerId) : id(layerId) {
    }

    std::shared_ptr<Clip> getOrCreateClip(int clipId) {
        if (clipId < 1) return nullptr;
        // Dynamically grow the clips vector as needed
        if (clipId > static_cast<int>(clips.size())) {
            clips.resize(clipId);
            for (int i = 0; i < clipId; ++i) {
                if (!clips[i]) clips[i] = std::make_shared<Clip>(i + 1);
            }
        }
        return clips[clipId - 1];
    }

    std::shared_ptr<Clip> getClip(int clipId) {
        if (clipId < 1) return nullptr;
        if (clipId > static_cast<int>(clips.size())) return nullptr;
        return clips[clipId - 1];
    }

    std::shared_ptr<Effect> getOrCreateEffect(const std::string& effectName) {
        // Find existing effect
        for (auto& effect : effects) {
            if (effect->name == effectName) {
                return effect;
            }
        }
        
        // Create new effect
        int newId = effects.size() + 1;
        auto newEffect = std::make_shared<Effect>(newId, effectName);
        effects.push_back(newEffect);
        return newEffect;
    }
    
    
    
    void processOSCMessage(const std::vector<std::string>& pathParts, const std::vector<float>& floats, 
                          const std::vector<int>& integers, const std::vector<std::string>& strings) {
        if (pathParts.empty()) {
            // Direct layer property
            properties.setFromOSCData("", floats, integers, strings);
            return;
        }
        
        std::vector<std::string> parts = pathParts; // Make a copy to modify
        
        if (parts[0] == "clips" && parts.size() >= 2) {
            // /clips/X/...
            int clipId = std::stoi(parts[1]);
            auto clip = getOrCreateClip(clipId);
            if (clip) {
                // Remove "clips" and clip number from path
                std::vector<std::string> remainingPath(parts.begin() + 2, parts.end());
                clip->processOSCMessage(remainingPath, floats, integers, strings);
            }
            return;
        }
        
        if (parts[0] == "video" && parts.size() >= 3 && parts[1] == "effects") {
            // /video/effects/effectname/...
            std::string effectName = parts[2];
            auto effect = getOrCreateEffect(effectName);
            
            // Remove "video", "effects", and effect name from path
            std::vector<std::string> remainingPath(parts.begin() + 3, parts.end());
            effect->processOSCMessage(remainingPath, floats, integers, strings);
            return;
        }
        
        // Store as general property
        std::string endpoint = "";
        for (size_t i = 0; i < parts.size(); ++i) {
            if (i > 0) endpoint += "/";
            endpoint += parts[i];
        }
        properties.setFromOSCData(endpoint, floats, integers, strings);
    }
    
    void clear() {
        properties.clear();
        effects.clear();
        for (auto& clip : clips) {
            clip->clear();
        }
    }
    
    // Print method for trickle-down printing
    void print(const std::string& indent) const {
        std::cout << indent << "Layer " << id << ":" << std::endl;

        if (!properties.properties.empty()) {
            std::cout << indent << "  Properties:" << std::endl;
            properties.print(indent + "    ");
        }
        
        if (!effects.empty()) {
            for (const auto& effect : effects) {
                effect->print(indent + "  ");
            }
        }
        
        if (!clips.empty()) {
            std::cout << indent << "  Clips:" << std::endl;
            for (const auto& clip : clips) {
                clip->print(indent + "    ");
            }
        }
    }
};

class ResolumeTracker {
private:
    std::vector<std::shared_ptr<Layer>> layers;

    // Centralized selection/connection state
    int selectedColumnId = 0;
    int selectedLayerId = 0;
    int selectedClipLayerId = 0;
    int selectedClipId = 0;
    int selectedDeckId = 0;
    int connectedColumnId = 0;
    std::vector<int> connectedClipIndices; // index: layer-1, value: connected clip index (1-based, 0 if none)

    // Track current deck to detect changes
    int currentDeckId;
    bool deckInitialized;
    
    // Track most recently selected layer and clip
    int lastSelectedLayerId;
    int lastSelectedClipLayerId;  // Which layer the selected clip is in
    int lastSelectedClipId;       // Which clip within that layer
    
    // Track timing to determine which effects bus to use
    enum class LastSelectionType {
        NONE,
        LAYER,
        CLIP
    };
    LastSelectionType lastSelectionType;
    
    // Removed: deckChangedCallback and related members
    // Removed: prevLayerCount, prevColumnCount, checkAndTriggerDeckChanged

    // Helper to ensure connectedClipIndices matches layer count
    void ensureConnectedClipIndices() {
        if (connectedClipIndices.size() != layers.size())
            connectedClipIndices.resize(layers.size(), 0);
    }

    // Add reference to OSC listener for queries
    ResolumeOSCListener* oscListener = nullptr;

public:
    ResolumeTracker(ResolumeOSCListener* listener = nullptr) : currentDeckId(0), deckInitialized(false),
                       lastSelectedLayerId(0), lastSelectedClipLayerId(0), lastSelectedClipId(0),
                       lastSelectionType(LastSelectionType::NONE), oscListener(listener)
    {
        // Start with no layers - they will be created as needed
        ensureConnectedClipIndices();
        
        // Set up the callback if OSCListener is provided
        if (oscListener) {
            oscListener->setMessageCallback([this](const std::string& address, const std::vector<float>& floats, 
                                                   const std::vector<int>& integers, const std::vector<std::string>& strings) {
                this->processOSCMessage(address, floats, integers, strings);
            });
        }
    }
    
    void setOSCListener(ResolumeOSCListener* listener) {
        oscListener = listener;
        if (oscListener) {
            oscListener->setMessageCallback([this](const std::string& address, const std::vector<float>& floats, 
                                                   const std::vector<int>& integers, const std::vector<std::string>& strings) {
                this->processOSCMessage(address, floats, integers, strings);
            });
        }
    }
    
    // Returns the number of layers
    int getLayerCount() const {
        return static_cast<int>(layers.size());
    }

    // Returns the maximum number of clips in any layer (number of columns)
    int getColumnCount() const {
        int maxClips = 0;
        for (const auto& layer : layers) {
            if (!layer) continue;
            int count = 0;
            for (const auto& clip : layer->clips) {
                if (clip && !clip->name.empty()) ++count;
            }
            if (count > maxClips) maxClips = count;
        }
        return maxClips;
    }

    void processOSCMessage(const std::string& address, const std::vector<float>& floats,
                           const std::vector<int>& integers, const std::vector<std::string>& strings) {
        // Only process /composition messages
        if (address.find("/composition") != 0) return;

        // Split the address into components
        std::vector<std::string> pathParts = splitOSCPath(address);
        
        // Remove "composition" from the beginning
        if (pathParts.empty() || pathParts[0] != "composition") return;
        pathParts.erase(pathParts.begin());

        if (pathParts.empty()) return;

        // --- 1. Handle deck selection and deck change ---
        if (pathParts[0] == "decks" && pathParts.size() >= 3) {
            int deckId = std::stoi(pathParts[1]);
            if (pathParts[2] == "select" && !integers.empty() && integers[0] == 1) {
                if (deckId != currentDeckId) {
                    clear();
                    currentDeckId = deckId;
                }
            }
            return;
        }

        // Check for select/connect messages
        std::string endpoint = pathParts.back();
        bool isSelect = (endpoint == "select");
        bool isConnect = (endpoint == "connect");

        if ((isSelect || isConnect) && !integers.empty() && integers[0] == 1) {
            std::cout << "Select/Connect Message: " << address << std::endl;

            if (pathParts[0] == "columns" && pathParts.size() >= 2) {
                int columnId = std::stoi(pathParts[1]);
                if (isSelect) {
                    selectedColumnId = columnId;
                } else if (isConnect) {
                    connectedColumnId = columnId;
                    ensureConnectedClipIndices();
                    for (size_t i = 0; i < layers.size(); ++i) {
                        connectedClipIndices[i] = columnId;
                    }
                }
                return;
            }

            if (pathParts[0] == "layers" && pathParts.size() >= 2) {
                int layerId = std::stoi(pathParts[1]);
                if (pathParts.size() == 3 && isSelect) {
                    // /layers/X/select
                    selectedLayerId = layerId;
                    return;
                }
                if (pathParts.size() >= 4 && pathParts[2] == "clips") {
                    // /layers/X/clips/Y/select or /connect
                    int clipId = std::stoi(pathParts[3]);
                    if (isSelect) {
                        selectedClipLayerId = layerId;
                        selectedClipId = clipId;
                    } else if (isConnect) {
                        ensureConnectedClipIndices();
                        if (layerId >= 1 && layerId <= static_cast<int>(connectedClipIndices.size())) {
                            connectedClipIndices[layerId - 1] = clipId;
                        }
                    }
                    return;
                }
            }
        }

        // Skip certain endpoints
        if (endpoint == "selected" || endpoint == "connected") {
            return;
        }

        // --- 3. Trickledown: pass to appropriate layer/clip/effect ---
        if (pathParts[0] == "layers" && pathParts.size() >= 2) {
            int layerId = std::stoi(pathParts[1]);
            auto layer = getOrCreateLayer(layerId); // Use getOrCreateLayer instead of getLayer
            if (layer) {
                // Remove "layers" and layer number from path
                std::vector<std::string> remainingPath(pathParts.begin() + 2, pathParts.end());
                layer->processOSCMessage(remainingPath, floats, integers, strings);
            }
            return;
        }

        // Ignore other top-level paths
        if (pathParts[0] == "columns" || pathParts[0] == "decks" || 
            pathParts[0] == "selectedlayer" || pathParts[0] == "selectedclip" || 
            pathParts[0] == "selectedcolumn") {
            return;
        }
    }
    
    std::shared_ptr<Layer> getOrCreateLayer(int layerId) {
        if (layerId < 1) return nullptr;
        
        // Dynamically grow the layers vector as needed
        if (layerId > static_cast<int>(layers.size())) {
            layers.resize(layerId);
            for (int i = 0; i < layerId; ++i) {
                if (!layers[i]) layers[i] = std::make_shared<Layer>(i + 1);
            }
            ensureConnectedClipIndices(); // Update connected clip indices when layers change
        }
        return layers[layerId - 1];
    }
    
    std::shared_ptr<Layer> getLayer(int layerId) {
        if (layerId >= 1 && layerId <= static_cast<int>(layers.size())) {
            return layers[layerId - 1];
        }
        return nullptr;
    }
    
    std::shared_ptr<const Layer> getLayer(int layerId) const {
        if (layerId >= 1 && layerId <= static_cast<int>(layers.size())) {
            return layers[layerId - 1];
        }
        return nullptr;
    }
    
    // Convenience getters for commonly used values
    //bool isTempoControllerPlaying() const { 
    //    return deckProperties.getInt("tempocontroller/play", 0) == 1;
    //}
    int getSelectedLayer() const { return selectedLayerId; }
    int getSelectedColumn() const { return selectedColumnId; }
    int getConnectedColumn() const { return connectedColumnId; }
    int getCurrentDeckId() const { return currentDeckId; }
    bool isDeckInitialized() const { return deckInitialized; }
    
    
    std::pair<int, int> getSelectedClip() const {
        return std::make_pair(selectedClipLayerId, selectedClipId);
    }
    
    // Get the most recently selected effects bus
    std::vector<std::shared_ptr<Effect>>* getSelectedEffectsBus() {
        if (lastSelectionType == LastSelectionType::CLIP && selectedClipLayerId > 0 && selectedClipId > 0) {
            auto layer = getLayer(selectedClipLayerId);
            if (layer) {
                auto clip = layer->getClip(selectedClipId);
                if (clip) {
                    return &clip->effects;
                }
            }
        } else if (lastSelectionType == LastSelectionType::LAYER && selectedLayerId > 0) {
            auto layer = getLayer(selectedLayerId);
            if (layer) {
                return &layer->effects;
            }
        }
        
        // Fallback: if we have a selected clip, return its effects
        if (selectedClipLayerId > 0 && selectedClipId > 0) {
            auto layer = getLayer(selectedClipLayerId);
            if (layer) {
                auto clip = layer->getClip(selectedClipId);
                if (clip) {
                    return &clip->effects;
                }
            }
        }
        
        // Fallback: if we have a selected layer, return its effects
        if (selectedLayerId > 0) {
            auto layer = getLayer(selectedLayerId);
            if (layer) {
                return &layer->effects;
            }
        }
        
        return nullptr;
    }
    
    //PropertyDictionary& getDeckProperties() { return deckProperties; }
    //const PropertyDictionary& getDeckProperties() const { return deckProperties; }
    
    // Method to manually set/change deck (useful for testing)
    void setCurrentDeck(int deckId) {
        if (deckInitialized && deckId != currentDeckId) {
            std::cout << "Manually changing deck from " << currentDeckId << " to " << deckId << " - clearing all data" << std::endl;
            clear();
        }
        currentDeckId = deckId;
        deckInitialized = true;
    }
    
    void clear() {
        selectedColumnId = 0;
        connectedColumnId = 0;
        selectedLayerId = 0;
        selectedClipLayerId = 0;
        selectedClipId = 0;
        lastSelectionType = LastSelectionType::NONE;
        //deckProperties.clear();
        
        for (auto& layer : layers) {
            layer->clear();
        }
        // Removed: prevLayerCount and prevColumnCount reset
    }
    
    // Additional convenience methods for PushUI integration
    bool doesClipExist(int column, int layer) {
        if (!oscListener) {
            // Fallback to old method if no OSC listener available
            auto layerObj = getLayer(layer);
            if (!layerObj) return false;
            auto clipObj = layerObj->getClip(column);
            return clipObj && !clipObj->name.empty();
        }

        //std::cout << "Querying clip existence: Layer " << layer << ", Column " << column << std::endl;
        
        try {
            std::string address = "/composition/layers/" + std::to_string(layer) + "/clips/" + std::to_string(column) + "/name";
            std::string clipName = oscListener->QueryString(address, 15); // 15 ms timeout
            return !clipName.empty();
        } catch (const std::exception& ex) {
            // Query failed or timed out, assume clip doesn't exist
            //std::cout << "Error querying: " << ex.what() << std::endl;
            return false;
        }
    }
    
    bool isColumnConnected(int column) {
        return getConnectedColumn() == column;
    }
    
    bool isClipConnected(int column, int layer) {
        // Check if the given clip (column) is the connected clip for the given layer
        return connectedClipIndices.size() >= static_cast<size_t>(layer) && connectedClipIndices[layer - 1] == column;
    }
    
    bool doesLayerExist(int layer) {
        auto layerObj = getLayer(layer);
        if (!layerObj) return false;

        // Only check existing clips, do not create new ones
        for (const auto& clip : layerObj->clips) {
            if (clip && !clip->name.empty()) {
                return true;
            }
        }
        return false;
    }

    // Print method for trickle-down printing
    void print(const std::string& indent = "") const {
        std::cout << indent << "ResolumeTracker:" << std::endl;
        std::cout << indent << "  Current Deck: " << currentDeckId << " (Initialized: " << (deckInitialized ? "Yes" : "No") << ")" << std::endl;
        std::cout << indent << "  Selected Column: " << selectedColumnId << ", Connected Column: " << connectedColumnId << std::endl;
        std::cout << indent << "  Selected Layer: " << selectedLayerId << ", Selected Clip: " << selectedClipId << " (Layer " << selectedClipLayerId << ")" << std::endl;
        
        //if (!deckProperties.properties.empty()) {
        //    std::cout << indent << "  Deck Properties:" << std::endl;
        //    deckProperties.print(indent + "    ");
        //}
        
        if (!layers.empty()) {
            std::cout << indent << "  Layers:" << std::endl;
            for (const auto& layer : layers) {
                layer->print(indent + "    ");
            }
        }
    }
};

#endif // TRACKING_WITH_REST