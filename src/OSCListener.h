#pragma once

#include "osc/OscOutboundPacketStream.h"
#include "osc/OscReceivedElements.h"
#include "osc/OscPacketListener.h"
#include "ip/UdpSocket.h"
#include "ip/IpEndpointName.h"
#include "ResolumeTrackerOSC.h"

using namespace osc;

class ResolumeOSCListener : public OscPacketListener {
private:
    ResolumeTracker& tracker;
    //PushUI* pushUI;  // Add reference to PushUI
    
public:
    ResolumeOSCListener(ResolumeTracker& resolumeTracker) : tracker(resolumeTracker) {} //, pushUI(nullptr)
    
    //void setPushUI(PushUI* ui) { pushUI = ui; }
    
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
            
            // Send to tracker
            tracker.processOSCMessage(address, floats, integers, strings);
            
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