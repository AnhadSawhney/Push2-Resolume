#ifndef RESOLUME_TRACKER_H
#define RESOLUME_TRACKER_H

#include <vector>
#include <string>
#include <map>
#include <memory>

class PropertyDictionary {
public:
    std::map<std::string, float> floats;
    std::map<std::string, int> integers;
    std::map<std::string, std::string> strings;
    
    void setFloat(const std::string& key, float value) {
        floats[key] = value;
    }
    
    void setInt(const std::string& key, int value) {
        integers[key] = value;
    }
    
    void setString(const std::string& key, const std::string& value) {
        strings[key] = value;
    }
    
    float getFloat(const std::string& key, float defaultValue = 0.0f) const {
        auto it = floats.find(key);
        return (it != floats.end()) ? it->second : defaultValue;
    }
    
    int getInt(const std::string& key, int defaultValue = 0) const {
        auto it = integers.find(key);
        return (it != integers.end()) ? it->second : defaultValue;
    }
    
    std::string getString(const std::string& key, const std::string& defaultValue = "") const {
        auto it = strings.find(key);
        return (it != strings.end()) ? it->second : defaultValue;
    }
    
    void clear() {
        floats.clear();
        integers.clear();
        strings.clear();
    }
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
        // At this level, we just store the property directly
        std::string endpoint = address.substr(1); // Remove leading slash
        
        if (!floats.empty()) {
            properties.setFloat(endpoint, floats[0]);
        } else if (!integers.empty()) {
            properties.setInt(endpoint, integers[0]);
        } else if (!strings.empty()) {
            properties.setString(endpoint, strings[0]);
        }
    }
    
    void clear() {
        properties.clear();
    }
};

class Clip {
public:
    int id;
    std::string name; // Hardcoded as critically important
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
            if (!floats.empty()) {
                properties.setFloat(endpoint, floats[0]);
            } else if (!integers.empty()) {
                properties.setInt(endpoint, integers[0]);
            } else if (!strings.empty()) {
                properties.setString(endpoint, strings[0]);
            }
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
                    effect->processOSCMessage("/", floats, integers, strings); // Just a placeholder
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
        if (!floats.empty()) {
            properties.setFloat(endpoint, floats[0]);
        } else if (!integers.empty()) {
            properties.setInt(endpoint, integers[0]);
        } else if (!strings.empty()) {
            properties.setString(endpoint, strings[0]);
        }
    }
    
    void clear() {
        name = "";
        properties.clear();
        effects.clear();
    }
};

class Layer {
public:
    int id;
    PropertyDictionary properties;
    std::vector<std::shared_ptr<Effect>> effects;
    std::vector<std::shared_ptr<Clip>> clips;
    
    Layer(int layerId) : id(layerId) {
        // Initialize clips (Resolume typically has 5 clips per layer)
        for (int i = 1; i <= 5; i++) {
            clips.push_back(std::make_shared<Clip>(i));
        }
    }
    
    std::shared_ptr<Clip> getClip(int clipId) {
        if (clipId >= 1 && clipId <= clips.size()) {
            return clips[clipId - 1];
        }
        return nullptr;
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
            if (!floats.empty()) {
                properties.setFloat(endpoint, floats[0]);
            } else if (!integers.empty()) {
                properties.setInt(endpoint, integers[0]);
            } else if (!strings.empty()) {
                properties.setString(endpoint, strings[0]);
            }
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
        if (!floats.empty()) {
            properties.setFloat(endpoint, floats[0]);
        } else if (!integers.empty()) {
            properties.setInt(endpoint, integers[0]);
        } else if (!strings.empty()) {
            properties.setString(endpoint, strings[0]);
        }
    }
    
    void clear() {
        properties.clear();
        effects.clear();
        for (auto& clip : clips) {
            clip->clear();
        }
    }
};

class ResolumeTracker {
private:
    std::vector<std::shared_ptr<Layer>> layers;
    PropertyDictionary deckProperties;
    int selectedColumnId;
    int connectedColumnId;
    
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
    
public:
    ResolumeTracker() : selectedColumnId(0), connectedColumnId(0), 
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
        
        if (firstPart == "columns") {
            // Handle column selection/connection
            if (nextRemainder.empty()) return;
            
            size_t secondSlash = nextRemainder.find('/', 1);
            if (secondSlash != std::string::npos) {
                std::string columnPart = nextRemainder.substr(1, secondSlash - 1);
                int columnId = extractNumber(columnPart);
                std::string property = nextRemainder.substr(secondSlash + 1);
                
                if (property == "selected" && !integers.empty()) {
                    if (integers[0] == 1) {
                        selectedColumnId = columnId;
                    } else if (selectedColumnId == columnId) {
                        selectedColumnId = 0;
                    }
                } else if (property == "connected" && !integers.empty()) {
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
        if (!floats.empty()) {
            deckProperties.setFloat(endpoint, floats[0]);
        } else if (!integers.empty()) {
            deckProperties.setInt(endpoint, integers[0]);
        } else if (!strings.empty()) {
            deckProperties.setString(endpoint, strings[0]);
        }
    }
    
    std::shared_ptr<Layer> getLayer(int layerId) {
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
};

#endif // RESOLUME_TRACKER_H