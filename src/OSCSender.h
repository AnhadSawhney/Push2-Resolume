#pragma once

#include "osc/OscOutboundPacketStream.h"
#include "osc/OscReceivedElements.h"
#include "osc/OscPacketListener.h"
#include "ip/UdpSocket.h"
#include "ip/IpEndpointName.h"

using namespace osc;

//#define DEBUG_OSC 1

// OSC Sender implementation
class OSCSender {
private:
    UdpTransmitSocket socket;
    IpEndpointName remoteEndpoint;
    
public:
    OSCSender(const std::string& address, int port) 
        : socket(IpEndpointName(address.c_str(), port)), 
          remoteEndpoint(address.c_str(), port) {}
    
    void sendMessage(const std::string& address, float value) {
        char buffer[1024];
        osc::OutboundPacketStream p(buffer, 1024);
        p << osc::BeginMessage(address.c_str()) << value << osc::EndMessage;
        socket.Send(p.Data(), p.Size());
        #ifdef DEBUG_OSC
        std::cout << "OSC: " << address << " " << value << std::endl;
        #endif
    }
    
    void sendMessage(const std::string& address, int value) {
        char buffer[1024];
        osc::OutboundPacketStream p(buffer, 1024);
        p << osc::BeginMessage(address.c_str()) << value << osc::EndMessage;
        socket.Send(p.Data(), p.Size());
        #ifdef DEBUG_OSC
        std::cout << "OSC: " << address << " " << value << std::endl;
        #endif
    }
    
    void sendMessage(const std::string& address, const std::string& value) {
        char buffer[1024];
        osc::OutboundPacketStream p(buffer, 1024);
        p << osc::BeginMessage(address.c_str()) << value.c_str() << osc::EndMessage;
        socket.Send(p.Data(), p.Size());
        #ifdef DEBUG_OSC
        std::cout << "OSC: " << address << " " << value << std::endl;
        #endif
    }
};