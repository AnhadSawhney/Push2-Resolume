#pragma once
#include <string>
#include <map>
#include <variant>
#include <iostream>
#include <sstream>
#include <vector>

#define PRINT_DETAILED_PROPERTIES

// Unified property value type
using PropertyValue = std::variant<float, int, std::string>;

class PropertyDictionary {
public:
    std::map<std::string, PropertyValue> properties;
    
    // Print method for trickle-down printing
    void print(const std::string& indent) const {        
        if (properties.empty()) return;
        
        #ifdef PRINT_DETAILED_PROPERTIES
        for (const auto& pair : properties) {
            std::cout << indent << pair.first << " = "
                      << getPropertyAsString(pair.first)
                      << " (" << getPropertyType(pair.first) << ")" << std::endl;
        }
        #endif
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