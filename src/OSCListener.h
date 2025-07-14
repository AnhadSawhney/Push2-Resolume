#pragma once

#include "osc/OscOutboundPacketStream.h"
#include "osc/OscReceivedElements.h"
#include "osc/OscPacketListener.h"
#include "ip/UdpSocket.h"
#include "ip/IpEndpointName.h"
#include <mutex>
#include <condition_variable>
#include <map>
#include <functional>

// Forward declaration
class OSCSender;

using namespace osc;

struct QueryResult {
    bool hasValue = false;
    std::vector<float> floats;
    std::vector<int> integers;
    std::vector<std::string> strings;
};

class ResolumeOSCListener : public OscPacketListener {
private:
    std::function<void(const std::string&, const std::vector<float>&, const std::vector<int>&, const std::vector<std::string>&)> messageCallback;
    OSCSender* oscSender;
    
    // Query mechanism
    std::mutex queryMutex;
    std::condition_variable queryCondition;
    std::map<std::string, QueryResult> pendingQueries;
    
public:
    ResolumeOSCListener(OSCSender* sender = nullptr) 
        : oscSender(sender) {}
    
    void setOSCSender(OSCSender* sender) { oscSender = sender; }
    
    void setMessageCallback(std::function<void(const std::string&, const std::vector<float>&, const std::vector<int>&, const std::vector<std::string>&)> callback) {
        messageCallback = callback;
    }
    
    // Blocking query function
    QueryResult query(const std::string& address, int timeoutMs = 5000) {
        if (!oscSender) {
            throw std::runtime_error("OSCSender not set");
        }
        
        std::unique_lock<std::mutex> lock(queryMutex);
        
        // Clear any existing result for this address
        pendingQueries[address] = QueryResult{};
        
        // Send query
        oscSender->sendMessage(address, std::string("?"));
        
        // Wait for response
        bool received = queryCondition.wait_for(lock, std::chrono::milliseconds(timeoutMs), 
            [this, &address]() { 
                return pendingQueries[address].hasValue; 
            });
        
        if (!received) {
            pendingQueries.erase(address);
            throw std::runtime_error("Query timeout for address: " + address);
        }
        
        QueryResult result = pendingQueries[address];
        pendingQueries.erase(address);
        return result;
    }
    
    // Convenience wrappers for specific types
    int QueryInt(const std::string& address, int timeoutMs = 5000) {
        QueryResult result = query(address, timeoutMs);
        if (result.integers.empty()) {
            throw std::runtime_error("No integer value received for address: " + address);
        }
        return result.integers[0];
    }
    
    float QueryFloat(const std::string& address, int timeoutMs = 5000) {
        QueryResult result = query(address, timeoutMs);
        if (result.floats.empty()) {
            throw std::runtime_error("No float value received for address: " + address);
        }
        return result.floats[0];
    }
    
    std::string QueryString(const std::string& address, int timeoutMs = 5000) {
        QueryResult result = query(address, timeoutMs);
        if (result.strings.empty()) {
            throw std::runtime_error("No string value received for address: " + address);
        }
        return result.strings[0];
    }
    
protected:
    virtual void ProcessMessage(const ReceivedMessage& m, const IpEndpointName& remoteEndpoint) override {
        try {
            std::string address(m.AddressPattern());
            std::vector<float> floats;
            std::vector<int> integers;
            std::vector<std::string> strings;
            
            // Parse arguments
            ReceivedMessage::const_iterator arg = m.ArgumentsBegin();
            while (arg != m.ArgumentsEnd()) {
                if (arg->IsFloat()) {
                    floats.push_back(arg->AsFloat());
                } else if (arg->IsInt32()) {
                    integers.push_back(arg->AsInt32());
                } else if (arg->IsString()) {
                    strings.push_back(std::string(arg->AsString()));
                }
                ++arg;
            }
            
            // Check if this is a response to a pending query
            {
                std::lock_guard<std::mutex> lock(queryMutex);
                auto it = pendingQueries.find(address);
                if (it != pendingQueries.end() && !it->second.hasValue) {
                    it->second.hasValue = true;
                    it->second.floats = floats;
                    it->second.integers = integers;
                    it->second.strings = strings;
                    queryCondition.notify_all();
                }
            }
            
            // Send to callback if set
            if (messageCallback) {
                messageCallback(address, floats, integers, strings);
            }
            
            // Debug output
            #ifdef DEBUG_OSC
                std::cout << "Received: " << address;
                if (!floats.empty()) {
                    std::cout << " floats=[";
                    for (size_t i = 0; i < floats.size(); ++i) {
                        if (i > 0) std::cout << ", ";
                        std::cout << floats[i];
                    }
                    std::cout << "]";
                }
                if (!integers.empty()) {
                    std::cout << " integers=[";
                    for (size_t i = 0; i < integers.size(); ++i) {
                        if (i > 0) std::cout << ", ";
                        std::cout << integers[i];
                    }
                    std::cout << "]";
                }
                if (!strings.empty()) {
                    std::cout << " strings=[";
                    for (size_t i = 0; i < strings.size(); ++i) {
                        if (i > 0) std::cout << ", ";
                        std::cout << "\"" << strings[i] << "\"";
                    }
                    std::cout << "]";
                }
                std::cout << std::endl;
            #endif
            
        } catch (Exception& e) {
            std::cerr << "Error parsing OSC message: " << e.what() << std::endl;
        }
    }
    
    virtual void ProcessBundle(const ReceivedBundle& b, const IpEndpointName& remoteEndpoint) override {
        // Process each element in the bundle
        ReceivedBundle::const_iterator iter = b.ElementsBegin();
        while (iter != b.ElementsEnd()) {
            if (iter->IsMessage()) {
                ProcessMessage(ReceivedMessage(*iter), remoteEndpoint);
            } else if (iter->IsBundle()) {
                ProcessBundle(ReceivedBundle(*iter), remoteEndpoint);
            }
            ++iter;
        }
    }
};