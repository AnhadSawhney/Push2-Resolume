#pragma once

#include <vector>
#include <string>
#include <map>
#include <memory>
#include <iostream>
#include <variant>
#include <sstream>
#include <functional>
#include "PropertyDictionary.h"

inline int extractNumber(const std::string& str) {
    std::string numStr = "";
    for (char c : str) {
        if (c >= '0' && c <= '9') {
            numStr += c;
        }
    }
    return numStr.empty() ? 0 : std::stoi(numStr);
}

class Effect {
public:
    int id;
    std::string name;
    PropertyDictionary properties;
    
    Effect(int effectId, const std::string& effectName) 
        : id(effectId), name(effectName) {}
    
    void processOSCMessage(const std::string& address, const std::vector<float>& floats, 
                          const std::vector<int>& integers, const std::vector<std::string>& strings) {
        std::string endpoint = address.substr(1); // Remove leading slash
        properties.setFromOSCData(endpoint, floats, integers, strings);
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
    
    void processOSCMessage(const std::string& address, const std::vector<float>& floats, 
                          const std::vector<int>& integers, const std::vector<std::string>& strings) {
        // Parse the address to find what we're dealing with
        size_t firstSlash = address.find('/', 1);
        if (firstSlash == std::string::npos) {
            // This is a direct property of the clip
            std::string endpoint = address.substr(1); // Remove leading slash
            
            // Handle clip name specially
            if (endpoint == "name" && !strings.empty()) {
                setName(strings[0]);
                return;
            }
            
            // Store other properties in dictionary
            properties.setFromOSCData(endpoint, floats, integers, strings);
            return;
        }
        
        std::string firstPart = address.substr(1, firstSlash - 1);
        std::string remainder = address.substr(firstSlash);
        
        if (firstPart == "video") {
            // Look for /video/effects/effectname
            size_t effectsPos = remainder.find("/effects/");
            if (effectsPos != std::string::npos) {
                size_t effectNameStart = effectsPos + 9; // Length of "/effects/"
                size_t effectNameEnd = remainder.find('/', effectNameStart);
                
                if (effectNameEnd == std::string::npos) {
                    // Effect name is at the end
                    std::string effectName = remainder.substr(effectNameStart);
                    auto effect = getOrCreateEffect(effectName);
                    effect->processOSCMessage("/", floats, integers, strings);
                } else {
                    // There's more after the effect name
                    std::string effectName = remainder.substr(effectNameStart, effectNameEnd - effectNameStart);
                    std::string effectRemainder = remainder.substr(effectNameEnd);
                    auto effect = getOrCreateEffect(effectName);
                    effect->processOSCMessage(effectRemainder, floats, integers, strings);
                }
                return;
            }
        }
        
        // If we get here, store as a general property
        std::string endpoint = address.substr(1); // Remove leading slash
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

    std::shared_ptr<Clip> getClip(int clipId) {
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

    //std::shared_ptr<const Clip> getClip(int clipId) const {
    //    if (clipId < 1) return nullptr;
    //    if (clipId > static_cast<int>(clips.size())) return nullptr;
    //    return clips[clipId - 1];
    //}

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
    
    
    
    void processOSCMessage(const std::string& address, const std::vector<float>& floats, 
                          const std::vector<int>& integers, const std::vector<std::string>& strings) {
        // Parse the address to find what we're dealing with
        size_t firstSlash = address.find('/', 1);
        if (firstSlash == std::string::npos) {
            // This is a direct property of the layer
            std::string endpoint = address.substr(1); // Remove leading slash
            properties.setFromOSCData(endpoint, floats, integers, strings);
            return;
        }
        
        std::string firstPart = address.substr(1, firstSlash - 1);
        std::string remainder = address.substr(firstSlash);
        
        if (firstPart == "clips") {
            // Extract clip number and pass remainder to clip
            size_t secondSlash = remainder.find('/', 1);
            if (secondSlash == std::string::npos) {
                // Just /clips/X
                int clipId = extractNumber(remainder);
                auto clip = getClip(clipId);
                if (clip) {
                    clip->processOSCMessage("/", floats, integers, strings);
                }
            } else {
                // /clips/X/something
                std::string clipPart = remainder.substr(1, secondSlash - 1);
                int clipId = extractNumber(clipPart);
                std::string clipRemainder = remainder.substr(secondSlash);
                auto clip = getClip(clipId);
                if (clip) {
                    clip->processOSCMessage(clipRemainder, floats, integers, strings);
                }
            }
            return;
        }
        
        if (firstPart == "video") {
            // Look for /video/effects/effectname
            size_t effectsPos = remainder.find("/effects/");
            if (effectsPos != std::string::npos) {
                size_t effectNameStart = effectsPos + 9; // Length of "/effects/"
                size_t effectNameEnd = remainder.find('/', effectNameStart);
                
                if (effectNameEnd == std::string::npos) {
                    // Effect name is at the end
                    std::string effectName = remainder.substr(effectNameStart);
                    auto effect = getOrCreateEffect(effectName);
                    effect->processOSCMessage("/", floats, integers, strings);
                } else {
                    // There's more after the effect name
                    std::string effectName = remainder.substr(effectNameStart, effectNameEnd - effectNameStart);
                    std::string effectRemainder = remainder.substr(effectNameEnd);
                    auto effect = getOrCreateEffect(effectName);
                    effect->processOSCMessage(effectRemainder, floats, integers, strings);
                }
                return;
            }
        }
        
        // If we get here, store as a general property
        std::string endpoint = address.substr(1); // Remove leading slash
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
    
    // Deck change callback
    std::function<void(int, int)> deckChangedCallback;

    // Track previous layer/column counts for change detection
    int prevLayerCount = 0;
    int prevColumnCount = 0;
    
    // Helper function to print properties nicely
    void printProperties(const PropertyDictionary& props, const std::string& indent) const {
        if (props.empty()) return;

        size_t count = 0;
        size_t total = props.size();
        for (const auto& pair : props) {
            bool isLast = (++count == total);
            std::cout << indent << (isLast ? "`-- " : "|-- ") << pair.first << " = "
                      << props.getPropertyAsString(pair.first)
                      << " (" << props.getPropertyType(pair.first) << ")" << std::endl;
        }
    }

    // Helper to count columns (max number of clips in any layer)
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

    // Helper to check and trigger callback if needed
    void checkAndTriggerDeckChanged() {
        int layerCount = static_cast<int>(layers.size());
        int columnCount = getColumnCount();
        if (deckChangedCallback) {
            std::cout << "Triggering callback" << std::endl;
            deckChangedCallback(layerCount, columnCount);
        }
        prevLayerCount = layerCount;
        prevColumnCount = columnCount;
    }

    // Helper to ensure connectedClipIndices matches layer count
    void ensureConnectedClipIndices() {
        if (connectedClipIndices.size() != layers.size())
            connectedClipIndices.resize(layers.size(), 0);
    }

public:
    ResolumeTracker() : currentDeckId(0), deckInitialized(false),
                       lastSelectedLayerId(0), lastSelectedClipLayerId(0), lastSelectedClipId(0),
                       lastSelectionType(LastSelectionType::NONE),
                       prevLayerCount(0), prevColumnCount(0) {
        for (int i = 1; i <= 10; i++) {
            layers.push_back(std::make_shared<Layer>(i));
        }
        prevLayerCount = static_cast<int>(layers.size());
        prevColumnCount = getColumnCount();
        ensureConnectedClipIndices();
    }
    
    // Setter for deckChangedCallback
    void setDeckChangedCallback(const std::function<void(int, int)>& cb) {
        deckChangedCallback = cb;
    }

    void processOSCMessage(const std::string& address, const std::vector<float>& floats,
                           const std::vector<int>& integers, const std::vector<std::string>& strings) {
        // Only process /composition messages
        if (address.find("/composition") != 0) return;

        // --- 1. Handle deck selection and deck change ---
        if (address.find("/composition/decks/") == 0) {
            size_t deckStart = 19; // "/composition/decks/" length
            size_t deckEnd = address.find('/', deckStart);
            if (deckEnd != std::string::npos) {
                int deckId = extractNumber(address.substr(deckStart, deckEnd - deckStart));
                std::string remainder = address.substr(deckEnd + 1);
                if ((remainder == "select" || remainder == "selected") && !integers.empty() && integers[0] == 1) {
                    if (deckId != currentDeckId) {
                        clearAll();
                        currentDeckId = deckId;
                        if (deckChangedCallback) deckChangedCallback(static_cast<int>(layers.size()), getColumnCount());
                    }
                }
            }
            return;
        }

        // Remove "/composition" prefix
        std::string path = address.substr(12);
        if (path.empty()) return;

        // Find the endpoint (last part after '/')
        size_t lastSlash = path.rfind('/');
        std::string endpoint = (lastSlash != std::string::npos) ? path.substr(lastSlash + 1) : path.substr(1);

        if (endpoint == "selected" || endpoint == "connected") {
            // Ignore these
            return;
        }

        bool isSelect = (endpoint == "select");
        bool isConnect = (endpoint == "connect");

        if ((isSelect || isConnect) && !integers.empty() && integers[0] == 1) {
            // Determine what is being accessed
            // /layers/X/clips/Y/select
            // /layers/X/select
            // /columns/X/select
            // /columns/X/connect
            // /layers/X/clips/Y/connect

            // Parse path parts
            std::vector<std::string> parts;
            size_t start = 0, slash;
            while ((slash = path.find('/', start)) != std::string::npos) {
                if (slash > start)
                    parts.push_back(path.substr(start + (start == 0 ? 1 : 0), slash - start - (start == 0 ? 1 : 0)));
                start = slash;
            }
            if (start < path.size())
                parts.push_back(path.substr(start + 1));

            // Handle columns
            if (parts.size() >= 2 && parts[0] == "columns") {
                int columnId = extractNumber(parts[1]);
                if (isSelect) {
                    selectedColumnId = columnId;
                } else if (isConnect) {
                    connectedColumnId = columnId;
                    ensureConnectedClipIndices();
                    // Connect all clips with this column index across all layers
                    for (size_t i = 0; i < layers.size(); ++i) {
                        connectedClipIndices[i] = columnId;
                    }
                }
                return;
            }

            // Handle layers
            if (parts.size() >= 2 && parts[0] == "layers") {
                int layerId = extractNumber(parts[1]);
                if (parts.size() == 2 && isSelect) {
                    selectedLayerId = layerId;
                    return;
                }
                // /layers/X/clips/Y/select or /connect
                if (parts.size() >= 4 && parts[2] == "clips") {
                    int clipId = extractNumber(parts[3]);
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

        // --- 3. Trickledown: pass to appropriate layer/clip/effect ---
        // Parse first path part
        size_t firstSlash = path.find('/', 1);
        std::string firstPart = (firstSlash == std::string::npos) ? path.substr(1) : path.substr(1, firstSlash - 1);
        std::string nextRemainder = (firstSlash == std::string::npos) ? "" : path.substr(firstSlash);

        if (firstPart == "layers") {
            if (nextRemainder.empty()) return;
            size_t secondSlash = nextRemainder.find('/', 1);
            if (secondSlash == std::string::npos) {
                int layerId = extractNumber(nextRemainder);
                auto layer = getLayer(layerId);
                if (layer) layer->processOSCMessage("/", floats, integers, strings);
            } else {
                std::string layerPart = nextRemainder.substr(1, secondSlash - 1);
                int layerId = extractNumber(layerPart);
                std::string layerRemainder = nextRemainder.substr(secondSlash);
                auto layer = getLayer(layerId);
                if (layer) layer->processOSCMessage(layerRemainder, floats, integers, strings);
            }
            return;
        }
        if (firstPart == "columns") {
            // No trickle-down for columns, handled above
            return;
        }
        if (firstPart == "decks" || firstPart == "selectedlayer" || firstPart == "selectedclip" || firstPart == "selectedcolumn") {
            // Ignore these
            return;
        }
    }
    
    std::shared_ptr<Layer> getLayer(int layerId) {
        if (layerId >= 1 && layerId <= layers.size()) {
            return layers[layerId - 1];
        }
        return nullptr;
    }
    
    std::shared_ptr<const Layer> getLayer(int layerId) const {
        if (layerId >= 1 && layerId <= layers.size()) {
            return layers[layerId - 1];
        }
        return nullptr;
    }
    
    // Convenience getters for commonly used values
    //bool isTempoControllerPlaying() const { 
    //    return deckProperties.getInt("tempocontroller/play", 0) == 1;
    //}
    
    int getSelectedColumnId() const { return selectedColumnId; }
    int getConnectedColumnId() const { return connectedColumnId; }
    int getCurrentDeckId() const { return currentDeckId; }
    bool isDeckInitialized() const { return deckInitialized; }
    
    // New getters for selected layer and clip
    int getSelectedLayerId() const { return selectedLayerId; }
    
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
            clearAll();
        }
        currentDeckId = deckId;
        deckInitialized = true;
    }
    
    void clearAll() {
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
        // Reset previous counts after clearing
        prevLayerCount = static_cast<int>(layers.size());
        prevColumnCount = getColumnCount();
    }
    
    // Additional convenience methods for PushUI integration
    bool hasClip(int column, int layer) {
        auto layerObj = getLayer(layer);
        if (!layerObj) return false;
        auto clipObj = layerObj->getClip(column);
        return clipObj && !clipObj->name.empty();
    }
    
    bool isColumnConnected(int column) {
        return getConnectedColumnId() == column;
    }
    
    bool isClipPlaying(int column, int layer) {
        // Check if the given clip (column) is the connected clip for the given layer
        return connectedClipIndices.size() >= static_cast<size_t>(layer) && connectedClipIndices[layer - 1] == column;
    }
    
    bool hasLayerContent(int layer) {
        auto layerObj = getLayer(layer);
        if (!layerObj) return false;
        
        // Check if layer has any clips
        for (int i = 1; i <= 32; i++) {  // Check reasonable number of clips
            auto clipObj = layerObj->getClip(i);
            if (clipObj && !clipObj->name.empty()) {
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