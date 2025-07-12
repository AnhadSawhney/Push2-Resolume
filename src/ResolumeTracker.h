#pragma once

#include <vector>
#include <string>
#include <map>
#include <memory>
#include <iostream>
#include <variant>
#include <sstream>

// Unified property value type
using PropertyValue = std::variant<float, int, std::string>;

class PropertyDictionary {
public:
    std::map<std::string, PropertyValue> properties;
    
    // Print method for trickle-down printing
    void print(const std::string& indent) const {
        if (properties.empty()) return;
        
        for (const auto& pair : properties) {
            std::cout << indent << pair.first << " = "
                      << getPropertyAsString(pair.first)
                      << " (" << getPropertyType(pair.first) << ")" << std::endl;
        }
       return;
    }
    
    void setFloat(const std::string& key, float value) {
        properties[key] = value;
    }
    
    void setInt(const std::string& key, int value) {
        properties[key] = value;
    }
    
    void setString(const std::string& key, const std::string& value) {
        properties[key] = value;
    }
    
    // Generic setter
    void setValue(const std::string& key, const PropertyValue& value) {
        properties[key] = value;
    }
    
    float getFloat(const std::string& key, float defaultValue = 0.0f) const {
        auto it = properties.find(key);
        if (it != properties.end()) {
            if (std::holds_alternative<float>(it->second)) {
                return std::get<float>(it->second);
            } else if (std::holds_alternative<int>(it->second)) {
                return static_cast<float>(std::get<int>(it->second));
            }
        }
        return defaultValue;
    }
    
    int getInt(const std::string& key, int defaultValue = 0) const {
        auto it = properties.find(key);
        if (it != properties.end()) {
            if (std::holds_alternative<int>(it->second)) {
                return std::get<int>(it->second);
            } else if (std::holds_alternative<float>(it->second)) {
                return static_cast<int>(std::get<float>(it->second));
            }
        }
        return defaultValue;
    }
    
    std::string getString(const std::string& key, const std::string& defaultValue = "") const {
        auto it = properties.find(key);
        if (it != properties.end()) {
            if (std::holds_alternative<std::string>(it->second)) {
                return std::get<std::string>(it->second);
            }
        }
        return defaultValue;
    }
    
    // Generic getter
    PropertyValue getValue(const std::string& key, const PropertyValue& defaultValue = PropertyValue{}) const {
        auto it = properties.find(key);
        return (it != properties.end()) ? it->second : defaultValue;
    }
    
    // Check if property exists
    bool hasProperty(const std::string& key) const {
        return properties.find(key) != properties.end();
    }
    
    // Get property type as string
    std::string getPropertyType(const std::string& key) const {
        auto it = properties.find(key);
        if (it != properties.end()) {
            if (std::holds_alternative<float>(it->second)) return "float";
            if (std::holds_alternative<int>(it->second)) return "int";
            if (std::holds_alternative<std::string>(it->second)) return "string";
        }
        return "unknown";
    }
    
    // Convert property to string for display
    std::string getPropertyAsString(const std::string& key) const {
        auto it = properties.find(key);
        if (it != properties.end()) {
            std::ostringstream oss;
            std::visit([&oss](const auto& value) {
                if constexpr (std::is_same_v<std::decay_t<decltype(value)>, std::string>) {
                    oss << "\"" << value << "\"";
                } else {
                    oss << value;
                }
            }, it->second);
            return oss.str();
        }
        return "";
    }
    
    void clear() {
        properties.clear();
    }
    
    // Helper function to set property from OSC data
    void setFromOSCData(const std::string& endpoint, 
                       const std::vector<float>& floats, 
                       const std::vector<int>& integers, 
                       const std::vector<std::string>& strings) {
        if (!floats.empty()) {
            setFloat(endpoint, floats[0]);
        } else if (!integers.empty()) {
            setInt(endpoint, integers[0]);
        } else if (!strings.empty()) {
            setString(endpoint, strings[0]);
        }
    }
    
    // Iterator support for range-based loops
    auto begin() const { return properties.begin(); }
    auto end() const { return properties.end(); }
    auto begin() { return properties.begin(); }
    auto end() { return properties.end(); }
    
    size_t size() const { return properties.size(); }
    bool empty() const { return properties.empty(); }
};

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
    std::string name; // Hardcoded as critically important
    bool connected = false; // Connection status moved from PropertyDictionary
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
            
            // Handle connected status specially
            if (endpoint == "connect" && !integers.empty()) {
                connected = (integers[0] == 1);
                //std::cout << "Clip " << id << " endpoint:" << endpoint << " value: " << integers[0] << std::endl;
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
        connected = false;
        properties.clear();
        effects.clear();
    }
    
    // Print method for trickle-down printing
    void print(const std::string& indent) const {
        std::cout << indent << "Clip " << id << ": " << name << (connected ? " [Connected]" : "") << std::endl;
        
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
        // Do not pre-initialize clips to a fixed size.
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

    std::shared_ptr<const Clip> getClip(int clipId) const {
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
    
    int extractNumber(const std::string& str) {
        std::string numStr = "";
        for (char c : str) {
            if (c >= '0' && c <= '9') {
                numStr += c;
            }
        }
        return numStr.empty() ? 0 : std::stoi(numStr);
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
        
        // If we get here, store as a general layer property
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
    PropertyDictionary deckProperties;
    int selectedColumnId;
    int connectedColumnId;
    
    // Track current deck to detect changes
    int currentDeckId;
    bool deckInitialized;
    
    // Track most recently selected layer and clip
    int selectedLayerId;
    int selectedClipLayerId;  // Which layer the selected clip is in
    int selectedClipId;       // Which clip within that layer
    
    // Track timing to determine which effects bus to use
    enum class LastSelectionType {
        NONE,
        LAYER,
        CLIP
    };
    LastSelectionType lastSelectionType;
    
    int extractNumber(const std::string& str) {
        std::string numStr = "";
        for (char c : str) {
            if (c >= '0' && c <= '9') {
                numStr += c;
            }
        }
        return numStr.empty() ? 0 : std::stoi(numStr);
    }
    
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
    
public:
    ResolumeTracker() : selectedColumnId(0), connectedColumnId(0), 
                       currentDeckId(0), deckInitialized(false),
                       selectedLayerId(0), selectedClipLayerId(0), selectedClipId(0),
                       lastSelectionType(LastSelectionType::NONE) {
        // Initialize layers (typically 8-10 layers in Resolume)
        for (int i = 1; i <= 10; i++) {
            layers.push_back(std::make_shared<Layer>(i));
        }
    }
    
    void processOSCMessage(const std::string& address, const std::vector<float>& floats, 
                          const std::vector<int>& integers, const std::vector<std::string>& strings) {
        // All messages should start with "/composition"
        if (address.find("/composition") != 0) return;
        
        // Check for deck selection messages to detect deck changes
        if (address.find("/composition/decks/") == 0) {
            size_t deckStart = 18; // Length of "/composition/decks/"
            size_t deckEnd = address.find('/', deckStart);
            if (deckEnd != std::string::npos) {
                std::string deckPart = address.substr(deckStart, deckEnd - deckStart);
                int deckId = extractNumber(deckPart);
                std::string remainder = address.substr(deckEnd + 1);
                
                // Check if this deck is being selected
                if (remainder == "select" || remainder == "selected") {
                    if (!integers.empty() && integers[0] == 1) {
                        // A deck has been selected
                        if (deckInitialized && deckId != currentDeckId) {
                            // Deck has changed, clear all data
                            std::cout << "Deck changed from " << currentDeckId << " to " << deckId << " - clearing all data" << std::endl;
                            clearAll();
                        }
                        currentDeckId = deckId;
                        deckInitialized = true;
                    }
                }
            }
            // Don't return here - let deck messages be processed normally below
        }
        
        // Remove "/composition" from the front
        std::string remainder = address.substr(12); // Length of "/composition"
        
        if (remainder.empty()) {
            // This is a root composition property
            return;
        }
        
        // Parse the next part
        size_t firstSlash = remainder.find('/', 1);
        std::string firstPart;
        std::string nextRemainder;
        
        if (firstSlash == std::string::npos) {
            firstPart = remainder.substr(1); // Remove leading slash
            nextRemainder = "";
        } else {
            firstPart = remainder.substr(1, firstSlash - 1);
            nextRemainder = remainder.substr(firstSlash);
        }
        
        if (firstPart == "layers") {
            // Extract layer number and pass remainder to layer
            if (nextRemainder.empty()) return;
            
            size_t secondSlash = nextRemainder.find('/', 1);
            if (secondSlash == std::string::npos) {
                // Just /layers/X
                int layerId = extractNumber(nextRemainder);
                auto layer = getLayer(layerId);
                if (layer) {
                    layer->processOSCMessage("/", floats, integers, strings);
                }
            } else {
                // /layers/X/something
                std::string layerPart = nextRemainder.substr(1, secondSlash - 1);
                int layerId = extractNumber(layerPart);
                std::string layerRemainder = nextRemainder.substr(secondSlash);
                
                // Check if this is a layer selection
                if (layerRemainder == "/select" || layerRemainder == "/selected") {
                    if (!integers.empty() && integers[0] == 1) {
                        selectedLayerId = layerId;
                        lastSelectionType = LastSelectionType::LAYER;
                    }
                }
                
                // Check if this is a clip selection within a layer
                size_t clipsPos = layerRemainder.find("/clips/");
                if (clipsPos != std::string::npos) {
                    size_t clipIdStart = clipsPos + 7; // Length of "/clips/"
                    size_t clipIdEnd = layerRemainder.find('/', clipIdStart);
                    if (clipIdEnd != std::string::npos) {
                        std::string clipIdStr = layerRemainder.substr(clipIdStart, clipIdEnd - clipIdStart);
                        int clipId = extractNumber(clipIdStr);
                        std::string clipProperty = layerRemainder.substr(clipIdEnd + 1);
                        
                        if (clipProperty == "select" || clipProperty == "selected") {
                            if (!integers.empty() && integers[0] == 1) {
                                selectedClipLayerId = layerId;
                                selectedClipId = clipId;
                                lastSelectionType = LastSelectionType::CLIP;
                            }
                        }
                    }
                }
                
                auto layer = getLayer(layerId);
                if (layer) {
                    layer->processOSCMessage(layerRemainder, floats, integers, strings);
                }
            }
            return;
        }
        
        if (firstPart == "decks") {
            // Handle deck-specific messages (layers within a deck)
            if (nextRemainder.empty()) return;
            
            size_t secondSlash = nextRemainder.find('/', 1);
            if (secondSlash != std::string::npos) {
                std::string deckPart = nextRemainder.substr(1, secondSlash - 1);
                int deckId = extractNumber(deckPart);
                std::string deckRemainder = nextRemainder.substr(secondSlash);
                
                // Check if this is for the current deck
                if (deckInitialized && deckId == currentDeckId) {
                    // Process deck-specific messages the same way as composition messages
                    if (deckRemainder.find("/layers/") == 0) {
                        // Remove "/layers" and process as layer message
                        std::string layerRemainder = deckRemainder.substr(7); // Length of "/layers"
                        
                        if (!layerRemainder.empty()) {
                            size_t thirdSlash = layerRemainder.find('/', 1);
                            if (thirdSlash == std::string::npos) {
                                // Just /layers/X
                                int layerId = extractNumber(layerRemainder);
                                auto layer = getLayer(layerId);
                                if (layer) {
                                    layer->processOSCMessage("/", floats, integers, strings);
                                }
                            } else {
                                // /layers/X/something
                                std::string layerPart = layerRemainder.substr(1, thirdSlash - 1);
                                int layerId = extractNumber(layerPart);
                                std::string layerMessages = layerRemainder.substr(thirdSlash);
                                
                                // Check for selections
                                if (layerMessages == "/select" || layerMessages == "/selected") {
                                    if (!integers.empty() && integers[0] == 1) {
                                        selectedLayerId = layerId;
                                        lastSelectionType = LastSelectionType::LAYER;
                                    }
                                }
                                
                                // Handle clip selections
                                size_t clipsPos = layerMessages.find("/clips/");
                                if (clipsPos != std::string::npos) {
                                    size_t clipIdStart = clipsPos + 7;
                                    size_t clipIdEnd = layerMessages.find('/', clipIdStart);
                                    if (clipIdEnd != std::string::npos) {
                                        std::string clipIdStr = layerMessages.substr(clipIdStart, clipIdEnd - clipIdStart);
                                        int clipId = extractNumber(clipIdStr);
                                        std::string clipProperty = layerMessages.substr(clipIdEnd + 1);
                                        
                                        if (clipProperty == "select" || clipProperty == "selected") {
                                            if (!integers.empty() && integers[0] == 1) {
                                                selectedClipLayerId = layerId;
                                                selectedClipId = clipId;
                                                lastSelectionType = LastSelectionType::CLIP;
                                            }
                                        }
                                    }
                                }
                                
                                auto layer = getLayer(layerId);
                                if (layer) {
                                    layer->processOSCMessage(layerMessages, floats, integers, strings);
                                }
                            }
                        }
                    }
                } else if (!deckInitialized) {
                    // If no deck has been initialized yet, assume this is the current deck
                    currentDeckId = deckId;
                    deckInitialized = true;
                }
            }
            return;
        }
        
        if (firstPart == "columns") {
            // Handle column selection/connection
            if (nextRemainder.empty()) return;
            
            size_t secondSlash = nextRemainder.find('/', 1);
            if (secondSlash != std::string::npos) {
                std::string columnPart = nextRemainder.substr(1, secondSlash - 1);
                int columnId = extractNumber(columnPart);
                std::string property = nextRemainder.substr(secondSlash + 1);
                
                if (property == "select" && !integers.empty()) {
                    if (integers[0] == 1) {
                        selectedColumnId = columnId;
                    } else if (selectedColumnId == columnId) {
                        selectedColumnId = 0;
                    }
                } else if (property == "connect" && !integers.empty()) {
                    if (integers[0] == 1) {
                        connectedColumnId = columnId;
                    } else if (connectedColumnId == columnId) {
                        connectedColumnId = 0;
                    }
                }
            }
            return;
        }
        
        if (firstPart == "selectedlayer" || firstPart == "selectedclip" || firstPart == "selectedcolumn") {
            // These are just references, ignore them
            return;
        }
        
        // Everything else goes to deck properties
        std::string endpoint = remainder.substr(1); // Remove leading slash
        deckProperties.setFromOSCData(endpoint, floats, integers, strings);
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
    bool isTempoControllerPlaying() const { 
        return deckProperties.getInt("tempocontroller/play", 0) == 1;
    }
    
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
    
    PropertyDictionary& getDeckProperties() { return deckProperties; }
    const PropertyDictionary& getDeckProperties() const { return deckProperties; }
    
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
        deckProperties.clear();
        
        for (auto& layer : layers) {
            layer->clear();
        }
    }
    
    // Additional convenience methods for PushUI integration
    bool hasClip(int column, int layer) const {
        auto layerObj = getLayer(layer);
        if (!layerObj) return false;
        auto clipObj = layerObj->getClip(column);
        return clipObj && !clipObj->name.empty();
    }
    
    bool isColumnConnected(int column) const {
        return getConnectedColumnId() == column;
    }
    
    bool isClipPlaying(int column, int layer) const {
        auto layerObj = getLayer(layer);
        if (!layerObj) return false;
        auto clipObj = layerObj->getClip(column);
        return clipObj && clipObj->connected;
    }
    
    bool hasLayerContent(int layer) const {
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
    
    int getCurrentDeck() const {
        // Return current deck number (implement based on your tracker)
        return 1;  // Placeholder
    }
    
    bool hasDeckChanged() {
        // Return true if deck has changed since last check
        return false;  // Placeholder
    }
    
    // Print method for trickle-down printing
    void print(const std::string& indent = "") const {
        std::cout << indent << "ResolumeTracker:" << std::endl;
        std::cout << indent << "  Current Deck: " << currentDeckId << " (Initialized: " << (deckInitialized ? "Yes" : "No") << ")" << std::endl;
        std::cout << indent << "  Selected Column: " << selectedColumnId << ", Connected Column: " << connectedColumnId << std::endl;
        std::cout << indent << "  Selected Layer: " << selectedLayerId << ", Selected Clip: " << selectedClipId << " (Layer " << selectedClipLayerId << ")" << std::endl;
        
        if (!deckProperties.properties.empty()) {
            std::cout << indent << "  Deck Properties:" << std::endl;
            deckProperties.print(indent + "    ");
        }
        
        if (!layers.empty()) {
            std::cout << indent << "  Layers:" << std::endl;
            for (const auto& layer : layers) {
                layer->print(indent + "    ");
            }
        }
    }
};

