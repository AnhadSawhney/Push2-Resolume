#ifndef RESOLUME_TRACKER_H
#define RESOLUME_TRACKER_H

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
    
    // Helper function to print properties nicely
    void printProperties(const PropertyDictionary& props, const std::string& indent) const {
        if (props.empty()) return;
        
        for (const auto& pair : props) {
            std::cout << indent << "├── " << pair.first << " = " 
                     << props.getPropertyAsString(pair.first) 
                     << " (" << props.getPropertyType(pair.first) << ")" << std::endl;
        }
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
        deckProperties.setFromOSCData(endpoint, floats, integers, strings);
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
    
    void printStateTree() const {
        std::cout << "\n========== RESOLUME STATE TREE ==========\n" << std::endl;
        
        // Print composition/deck properties
        std::cout << "COMPOSITION/DECK PROPERTIES:" << std::endl;
        if (deckProperties.empty()) {
            std::cout << "  └── (no properties)" << std::endl;
        } else {
            printProperties(deckProperties, "  ");
        }
        
        // Print selection state
        std::cout << "\nSELECTION STATE:" << std::endl;
        std::cout << "  ├── Selected Column: " << (selectedColumnId ? std::to_string(selectedColumnId) : "none") << std::endl;
        std::cout << "  ├── Connected Column: " << (connectedColumnId ? std::to_string(connectedColumnId) : "none") << std::endl;
        std::cout << "  ├── Selected Layer: " << (selectedLayerId ? std::to_string(selectedLayerId) : "none") << std::endl;
        std::cout << "  ├── Selected Clip: " << (selectedClipLayerId && selectedClipId ? 
                     "Layer " + std::to_string(selectedClipLayerId) + ", Clip " + std::to_string(selectedClipId) : "none") << std::endl;
        std::cout << "  └── Last Selection Type: ";
        switch (lastSelectionType) {
            case LastSelectionType::NONE: std::cout << "none"; break;
            case LastSelectionType::LAYER: std::cout << "layer"; break;
            case LastSelectionType::CLIP: std::cout << "clip"; break;
        }
        std::cout << std::endl;
        
        // Print layers
        std::cout << "\n🎬 LAYERS:" << std::endl;
        bool hasAnyLayerData = false;
        
        for (const auto& layer : layers) {
            bool hasLayerData = !layer->properties.empty() || !layer->effects.empty();
            
            // Check if any clips have data
            bool hasClipData = false;
            for (const auto& clip : layer->clips) {
                if (!clip->name.empty() || !clip->properties.empty() || !clip->effects.empty()) {
                    hasClipData = true;
                    break;
                }
            }
            
            if (hasLayerData || hasClipData) {
                hasAnyLayerData = true;
                std::string layerPrefix = (layer->id == selectedLayerId) ? "🟢 " : "   ";
                std::cout << layerPrefix << "Layer " << layer->id << ":" << std::endl;
                
                // Print layer properties
                if (!layer->properties.empty()) {
                    std::cout << "     ├── Properties:" << std::endl;
                    printProperties(layer->properties, "     │   ");
                }
                
                // Print layer effects
                if (!layer->effects.empty()) {
                    bool isLast = !hasClipData;
                    std::cout << "     " << (isLast ? "└── " : "├── ") << "Video Effects:" << std::endl;
                    for (size_t i = 0; i < layer->effects.size(); i++) {
                        const auto& effect = layer->effects[i];
                        bool isLastEffect = (i == layer->effects.size() - 1);
                        std::string effectPrefix = isLastEffect ? "└── " : "├── ";
                        std::cout << "     " << (isLast ? "    " : "│   ") << effectPrefix << effect->name << ":" << std::endl;
                        
                        // Print effect properties
                        printProperties(effect->properties, "     " + (isLast ? "    " : "│   ") + (isLastEffect ? "    " : "│   "));
                    }
                }
                
                // Print clips
                if (hasClipData) {
                    std::cout << "     └── Clips:" << std::endl;
                    for (const auto& clip : layer->clips) {
                        bool hasThisClipData = !clip->name.empty() || !clip->properties.empty() || !clip->effects.empty();
                        
                        if (hasThisClipData) {
                            bool isSelected = (layer->id == selectedClipLayerId && clip->id == selectedClipId);
                            std::string clipPrefix = isSelected ? "🔵 " : "   ";
                            std::cout << "         " << clipPrefix << "Clip " << clip->id;
                            if (!clip->name.empty()) {
                                std::cout << " (\"" << clip->name << "\")";
                            }
                            std::cout << ":" << std::endl;
                            
                            // Print clip properties
                            if (!clip->properties.empty()) {
                                std::cout << "             ├── Properties:" << std::endl;
                                printProperties(clip->properties, "             │   ");
                            }
                            
                            // Print clip effects
                            if (!clip->effects.empty()) {
                                bool hasProps = !clip->properties.empty();
                                std::cout << "             " << (hasProps ? "└── " : "└── ") << "Video Effects:" << std::endl;
                                for (size_t i = 0; i < clip->effects.size(); i++) {
                                    const auto& effect = clip->effects[i];
                                    bool isLastEffect = (i == clip->effects.size() - 1);
                                    std::string effectPrefix = isLastEffect ? "└── " : "├── ";
                                    std::cout << "                 " << effectPrefix << effect->name << ":" << std::endl;
                                    
                                    // Print effect properties
                                    printProperties(effect->properties, "                 " + (isLastEffect ? "    " : "│   "));
                                }
                            }
                        }
                    }
                }
                std::cout << std::endl;
            }
        }
        
        if (!hasAnyLayerData) {
            std::cout << "  └── (no layer data)" << std::endl;
        }
        
        // Print summary statistics
        int totalLayers = 0, totalClips = 0, totalEffects = 0, totalProperties = 0;
        
        for (const auto& layer : layers) {
            bool hasData = !layer->properties.empty() || !layer->effects.empty();
            
            for (const auto& clip : layer->clips) {
                if (!clip->name.empty() || !clip->properties.empty() || !clip->effects.empty()) {
                    hasData = true;
                    totalClips++;
                    totalEffects += clip->effects.size();
                    totalProperties += clip->properties.size();
                }
            }
            
            if (hasData) {
                totalLayers++;
                totalEffects += layer->effects.size();
                totalProperties += layer->properties.size();
            }
        }
        
        totalProperties += deckProperties.size();
        
        std::cout << "\n📊 SUMMARY STATISTICS:" << std::endl;
        std::cout << "  ├── Active Layers: " << totalLayers << std::endl;
        std::cout << "  ├── Active Clips: " << totalClips << std::endl;
        std::cout << "  ├── Total Effects: " << totalEffects << std::endl;
        std::cout << "  └── Total Properties: " << totalProperties << std::endl;
        
        std::cout << "\n==========================================\n" << std::endl;
    }
};

#endif // RESOLUME_TRACKER_H