#pragma once

#define TRACKING_WITH_REST
#ifndef TRACKING_WITH_OSC

#include <string>
#include <vector>
#include "httplib.h"           // https://github.com/yhirose/cpp-httplib
#include "json.hpp"  // https://github.com/nlohmann/json

// Represents a single clip in Resolume
struct Clip {
    int64_t id;
    std::string name;
    bool connected;
    bool selected;

    // Print with indent
    void print(int indent = 0) const {
        std::string pad(indent, ' ');
        std::cout << pad << "Clip[ID=" << id
                  << ", name='" << name << "'"
                  << ", connected=" << std::boolalpha << connected
                  << ", selected=" << selected
                  << "]\n";
    }
};

// Represents a single layer, containing multiple clips
struct Layer {
    int64_t id;
    int index;        // 1-based position in the deck
    bool selected;
    std::vector<Clip> clips;

    // Print with indent and propagate to clips
    void print(int indent = 0) const {
        std::string pad(indent, ' ');
        std::cout << pad << "Layer[ID=" << id
                  << ", index=" << index
                  << ", selected=" << std::boolalpha << selected
                  << ", clips=" << clips.size() << "]\n";
        for (const auto &c : clips) c.print(indent + 2);
    }
};

// Represents a single effect in Resolume
struct Effect {
    int64_t id;
    std::string name;
    nlohmann::json parameters; // raw JSON of effect parameters

    void print(int indent = 0) const {
        std::string pad(indent, ' ');
        std::cout << pad << "Effect[ID=" << id
                  << ", name='" << name << "']\n";
        if (!parameters.is_null()) {
            std::cout << pad << "  Params: " << parameters.dump() << "\n";
        }
    }
};

// Tracks the state of the currently connected Resolume deck
class ResolumeTracker {
public:
    // Connects to Resolume REST API at given host and port
    ResolumeTracker(const std::string& host = "localhost", int port = 8080)
        : cli_(host.c_str(), port), deckIndex_(-1) {}

    void clear() {
        layers_.clear();
        deckIndex_ = -1;
    }
    
    // Sends requests to update internal state from Resolume
    void update() {
        std::cout << "[DEBUG] Starting optimized update" << std::endl;
        layers_.clear();

        // 1. Get entire composition in one request - this gives us everything we need
        std::cout << "[DEBUG] Requesting full composition" << std::endl;
        auto res = cli_.Get("/api/v1/composition");
        if (!res) {
            std::cout << "[DEBUG] No response for composition" << std::endl;
            return;
        }
        std::cout << "[DEBUG] Composition HTTP status: " << res->status << std::endl;
        if (res->status != 200) return;

        std::cout << "[DEBUG] Parsing composition JSON (first 500 chars): " << res->body.substr(0, 500) << "..." << std::endl;
        auto compositionJson = nlohmann::json::parse(res->body, nullptr, false);
        if (compositionJson.is_discarded()) {
            std::cout << "[DEBUG] Failed to parse composition JSON" << std::endl;
            return;
        }

        // 2. Find selected deck from decks array
        deckIndex_ = -1;
        auto decks = compositionJson.value("decks", nlohmann::json::array());
        std::cout << "[DEBUG] Found " << decks.size() << " decks" << std::endl;
        
        for (size_t i = 0; i < decks.size(); ++i) {
            auto& deck = decks[i];
            if (deck.value("selected", nlohmann::json()).value("value", false)) {
                deckIndex_ = static_cast<int>(i + 1); // 1-based indexing
                std::cout << "[DEBUG] Selected deck found at index: " << deckIndex_ << std::endl;
                break;
            }
        }
        
        if (deckIndex_ < 0) {
            std::cout << "[DEBUG] No selected deck found" << std::endl;
            return;
        }

        // 3. Process layers from composition - they're already in the response
        auto layersArray = compositionJson.value("layers", nlohmann::json::array());
        std::cout << "[DEBUG] Found " << layersArray.size() << " layers in composition" << std::endl;
        
        for (size_t layerIdx = 0; layerIdx < layersArray.size(); ++layerIdx) {
            auto& layerJson = layersArray[layerIdx];
            std::cout << "[DEBUG] Processing layer " << (layerIdx + 1) << std::endl;

            Layer layer;
            layer.id = layerJson.value("id", -1LL);
            layer.index = static_cast<int>(layerIdx + 1); // 1-based
            
            // Check if layer is selected
            auto selectedParam = layerJson.value("selected", nlohmann::json());
            layer.selected = selectedParam.value("value", false);

            // Process clips in this layer
            auto clipsArray = layerJson.value("clips", nlohmann::json::array());
            std::cout << "[DEBUG] Layer " << layer.index << " has " << clipsArray.size() << " clips" << std::endl;
            
            for (auto& clipJson : clipsArray) {
                std::cout << "[DEBUG] Processing clip: " << clipJson.dump().substr(0, 200) << "..." << std::endl;
                
                Clip clip;
                clip.id = clipJson.value("id", -1LL);
                
                // Get clip name
                auto nameParam = clipJson.value("name", nlohmann::json());
                clip.name = nameParam.value("value", "");
                
                // Check if clip is connected
                auto connectedParam = clipJson.value("connected", nlohmann::json());
                if (connectedParam.is_object()) {
                    // It's a choice parameter - check if value indicates connected
                    std::string connValue = connectedParam.value("value", "");
                    clip.connected = (connValue == "Connected" || connValue == "1" || !connValue.empty());
                } else {
                    clip.connected = connectedParam.value("value", false);
                }
                
                // Check if clip is selected
                auto clipSelectedParam = clipJson.value("selected", nlohmann::json());
                clip.selected = clipSelectedParam.value("value", false);
                
                std::cout << "[DEBUG] Clip processed: id=" << clip.id << ", name='" << clip.name 
                         << "', connected=" << clip.connected << ", selected=" << clip.selected << std::endl;
                
                layer.clips.push_back(clip);
            }
            
            layers_.push_back(std::move(layer));
        }
        
        std::cout << "[DEBUG] Update completed. Processed " << layers_.size() << " layers" << std::endl;
    }

    // Print entire structure
    void print(int indent = 0) const {
        std::string pad(indent, ' ');
        std::cout << pad << "ResolumeTracker[deckIndex=" << deckIndex_
                  << ", layers=" << layers_.size() << "]\n";
        for (const auto &l : layers_) l.print(indent + 2);
        // print selected/connected columns, etc.
        int selLayer = getSelectedLayer();
        int selCol = getSelectedColumn();
        int connCol = getConnectedColumn();
        std::cout << pad << "  SelectedLayer=" << selLayer
                  << ", SelectedColumn=" << selCol
                  << ", ConnectedColumn=" << connCol << "\n";
    }

    // Number of layers in the current deck
    int getLayerCount() const { return static_cast<int>(layers_.size()); }

    bool doesLayerExist(int layerIdx) const {
        return layerIdx >= 1 && layerIdx <= getLayerCount();
    }

    // Number of columns = max clips in any layer
    int getColumnCount() const {
        int maxCols = 0;
        for (auto& layer : layers_) {
            maxCols = std::max<int>(maxCols, static_cast<int>(layer.clips.size()));
        }
        return maxCols;
    }

    // Clip-level queries
    bool doesClipExist(int layerIdx, int clipIdx) const {
        if (layerIdx < 1 || layerIdx > getLayerCount()) return false;
        return clipIdx >= 1 && clipIdx <= static_cast<int>(layers_[layerIdx-1].clips.size());
    }

    bool isClipConnected(int layerIdx, int clipIdx) const {
        if (!doesClipExist(layerIdx, clipIdx)) return false;
        return layers_[layerIdx-1].clips[clipIdx-1].connected;
    }

    std::vector<Clip> getConnectedClips() const {
        std::vector<Clip> out;
        for (auto& layer : layers_) {
            for (auto& clip : layer.clips) {
                if (clip.connected) { out.push_back(clip); break; }
            }
        }
        return out;
    }

    Clip getSelectedClip() const {
        for (auto& layer : layers_) {
            for (auto& clip : layer.clips) {
                if (clip.selected) return clip;
            }
        }
        return Clip{};
    }

    // Layer-level queries
    // Retrieves selected layer via API, then matches to loaded layers to get its index
    int getSelectedLayer() const {
        auto res = cli_.Get("/composition/layers/selected");
        if (res && res->status == 200) {
            auto layerJson = nlohmann::json::parse(res->body);
            int64_t selId = layerJson.value("id", -1LL);
            for (auto& layer : layers_) {
                if (layer.id == selId) return layer.index;
            }
        }
        return -1;
    }

    // Column-level queries (global)
    // Find which column is selected in Resolume (via API)
    int getSelectedColumn() const {
        int maxCols = getColumnCount();
        for (int idx = 1; idx <= maxCols; ++idx) {
            auto res = cli_.Get(("/composition/columns/" + std::to_string(idx)).c_str());
            if (!res || res->status != 200) continue;
            auto colJson = nlohmann::json::parse(res->body);
            if (colJson.value("selected", false)) return idx;
        }
        return -1;
    }

    // Find which column is connected in Resolume (via API)
    int getConnectedColumn() const {
        int maxCols = getColumnCount();
        for (int idx = 1; idx <= maxCols; ++idx) {
            auto res = cli_.Get(("/composition/columns/" + std::to_string(idx)).c_str());
            if (!res || res->status != 200) continue;
            auto colJson = nlohmann::json::parse(res->body);
            if (colJson.value("connected", false)) return idx;
        }
        return -1;
    }

    // Effects retrieval for selected contexts
    std::vector<Effect> getEffectsForSelectedLayer() const {
        int selLayer = getSelectedLayer();
        return getEffectsForLayer(selLayer);
    }

    std::vector<Effect> getEffectsForSelectedClip() const {
        int selLayer = getSelectedLayer();
        int selCol   = getSelectedColumn();
        return getEffectsForLayerClip(selLayer, selCol);
    }

private:
    mutable httplib::Client cli_;
    int deckIndex_;
    std::vector<Layer> layers_;

    std::vector<Effect> getEffectsForLayer(int layerIdx) const {
        std::vector<Effect> out;
        if (layerIdx < 1 || layerIdx > getLayerCount()) return out;
        std::string path = "/composition/decks/" + std::to_string(deckIndex_) +
                           "/layers/" + std::to_string(layerIdx) + "/effects";
        auto res = cli_.Get(path.c_str());
        if (res && res->status == 200) {
            auto arr = nlohmann::json::parse(res->body);
            for (auto& e : arr) {
                Effect fx;
                fx.id         = e.value("id", -1LL);
                fx.name       = e.value("name", "");
                fx.parameters = e.value("parameters", nlohmann::json::object());
                out.push_back(fx);
            }
        }
        return out;
    }

    std::vector<Effect> getEffectsForLayerClip(int layerIdx, int clipIdx) const {
        std::vector<Effect> out;
        if (layerIdx < 1 || layerIdx > getLayerCount()) return out;
        if (!doesClipExist(layerIdx, clipIdx)) return out;
        std::string clipId = std::to_string(layers_[layerIdx-1].clips[clipIdx-1].id);
        std::string path   = "/composition/decks/" + std::to_string(deckIndex_) +
                             "/layers/" + std::to_string(layerIdx) +
                             "/clips/"  + clipId + "/effects";
        auto res = cli_.Get(path.c_str());
        if (res && res->status == 200) {
            auto arr = nlohmann::json::parse(res->body);
            for (auto& e : arr) {
                Effect fx;
                fx.id         = e.value("id", -1LL);
                fx.name       = e.value("name", "");
                fx.parameters = e.value("parameters", nlohmann::json::object());
                out.push_back(fx);
            }
        }
        return out;
    }
};

#endif // TRACKING_WITH_OSC