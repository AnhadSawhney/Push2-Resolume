// main.cpp

// OSC pack (adjust include paths to your install)
#include "osc/OscOutboundPacketStream.h"
#include "osc/OscReceivedElements.h"
#include "osc/OscPacketListener.h"
#include "ip/UdpSocket.h"
#include "ip/IpEndpointName.h"
#include "ResolumeTracker.h"
#include "PushUI.h"
#include <iostream>
#include <thread>
#include <atomic>
#include <memory>

// Push 2 USB (adjust include paths to your install)
#include "PushUSB.h"

using namespace osc;

// OSC Sender implementation
class OSCSenderImpl : public OSCSender {
private:
    UdpTransmitSocket socket;
    IpEndpointName remoteEndpoint;
    
public:
    OSCSenderImpl(const std::string& address, int port) 
        : socket(IpEndpointName(address.c_str(), port)), 
          remoteEndpoint(address.c_str(), port) {}
    
    void sendMessage(const std::string& address, float value) override {
        char buffer[1024];
        osc::OutboundPacketStream p(buffer, 1024);
        p << osc::BeginMessage(address.c_str()) << value << osc::EndMessage;
        socket.Send(p.Data(), p.Size());
        
        std::cout << "OSC: " << address << " " << value << std::endl;
    }
    
    void sendMessage(const std::string& address, int value) override {
        char buffer[1024];
        osc::OutboundPacketStream p(buffer, 1024);
        p << osc::BeginMessage(address.c_str()) << value << osc::EndMessage;
        socket.Send(p.Data(), p.Size());
        
        std::cout << "OSC: " << address << " " << value << std::endl;
    }
    
    void sendMessage(const std::string& address, const std::string& value) override {
        char buffer[1024];
        osc::OutboundPacketStream p(buffer, 1024);
        p << osc::BeginMessage(address.c_str()) << value.c_str() << osc::EndMessage;
        socket.Send(p.Data(), p.Size());
        
        std::cout << "OSC: " << address << " " << value << std::endl;
    }
};

class ResolumeOSCListener : public OscPacketListener {
private:
    ResolumeTracker& tracker;
    PushUI* pushUI;  // Add reference to PushUI
    
public:
    ResolumeOSCListener(ResolumeTracker& resolumeTracker) : tracker(resolumeTracker), pushUI(nullptr) {}
    
    void setPushUI(PushUI* ui) { pushUI = ui; }
    
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

// ------------------------
// main()
// ------------------------
int main(int argc, char* argv[]) {
    const int OSC_PORT = 7000;  // Default Resolume OSC port
    const std::string RESOLUME_IP = "127.0.0.1";  // Localhost
    const int RESOLUME_PORT = 7000;  // Resolume OSC input port
    
    try {
        // Create Resolume tracker
        ResolumeTracker resolumeTracker;

        // Initialize Push 2 connection
        PushUSB push;
        if (!push.initialize()) {
            std::cerr << "Failed to initialize Push 2 MIDI" << std::endl;
            return 1;
        }
        
        bool pushConnected = push.connect();
        if (pushConnected) {
            std::cout << "Push 2 connected successfully!" << std::endl;
        } else {
            std::cout << "Push 2 not connected - continuing without Push 2" << std::endl;
        }

        // Create OSC sender for sending commands to Resolume
        auto oscSender = std::make_unique<OSCSenderImpl>(RESOLUME_IP, RESOLUME_PORT);

        // Create PushUI (only if Push is connected)
        std::unique_ptr<PushUI> pushUI;
        if (pushConnected) {
            pushUI = std::make_unique<PushUI>(push, resolumeTracker, std::move(oscSender));
            
            // Set up MIDI callback to handle Push 2 input
            push.setMidiCallback([&pushUI](const PushMidiMessage& msg) {
                if (pushUI) {
                    pushUI->onMidiMessage(msg);
                }
            });
            
            if (!pushUI->initialize()) {
                std::cerr << "Failed to initialize Push UI" << std::endl;
                pushUI.reset();
                pushConnected = false;
            }
        }

        // Create OSC listener
        ResolumeOSCListener listener(resolumeTracker);
        if (pushUI) {
            listener.setPushUI(pushUI.get());
        }
        
        // Create UDP socket for receiving OSC messages
        UdpListeningReceiveSocket socket(IpEndpointName(IpEndpointName::ANY_ADDRESS, OSC_PORT), &listener);
        
        std::cout << "Push2-Resolume Controller starting..." << std::endl;
        std::cout << "Listening for OSC messages on port " << OSC_PORT << std::endl;
        std::cout << "Sending OSC messages to " << RESOLUME_IP << ":" << RESOLUME_PORT << std::endl;
        std::cout << "Press 'q' + Enter to quit, 'help' for commands" << std::endl;
        
        // Start listening in a separate thread
        std::atomic<bool> shouldStop(false);
        std::thread oscThread([&socket, &shouldStop]() {
            try {
                socket.RunUntilSigInt();
            } catch (...) {
                shouldStop.store(true);
            }
        });
        
        // Main update loop
        std::thread updateThread([&pushUI, &shouldStop]() {
            while (!shouldStop.load()) {
                if (pushUI) {
                    pushUI->update();
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 20 FPS
            }
        });
        
        // Wait for user input to quit
        std::string input;
        while (std::getline(std::cin, input)) {
            if (input == "q" || input == "Q") {
                break;
            } else if (input == "clear") {
                resolumeTracker.clearAll();
                std::cout << "Cleared all state" << std::endl;
            } else if (input == "status") {
                std::cout << "Tempo controller playing: " << (resolumeTracker.isTempoControllerPlaying() ? "Yes" : "No") << std::endl;
                std::cout << "Selected layer: " << resolumeTracker.getSelectedLayerId() << std::endl;
                auto selectedClip = resolumeTracker.getSelectedClip();
                std::cout << "Selected clip: layer " << selectedClip.first << ", clip " << selectedClip.second << std::endl;
                std::cout << "Selected column: " << resolumeTracker.getSelectedColumnId() << std::endl;
                std::cout << "Push 2 connected: " << (pushConnected && push.isDeviceConnected() ? "Yes" : "No") << std::endl;
            } else if (input == "tree" || input == "print") {
                resolumeTracker.printStateTree();
            } else if (input == "help") {
                std::cout << "\nAvailable commands:" << std::endl;
                std::cout << "  q/Q      - Quit the program" << std::endl;
                std::cout << "  clear    - Clear all tracked state" << std::endl;
                std::cout << "  status   - Show basic status information" << std::endl;
                std::cout << "  tree     - Print complete state tree" << std::endl;
                std::cout << "  print    - Same as tree" << std::endl;
                if (pushConnected && pushUI) {
                    std::cout << "  test     - Run Push 2 lighting test" << std::endl;
                }
                std::cout << "  help     - Show this help message" << std::endl;
                std::cout << std::endl;
            }
        }
        
        shouldStop.store(true);
        if (oscThread.joinable()) {
            oscThread.join();
        }
        if (updateThread.joinable()) {
            updateThread.join();
        }
        
        std::cout << "Push2-Resolume Controller stopped." << std::endl;
        
    } catch (Exception& e) {
        std::cerr << "OSC Error: " << e.what() << std::endl;
        return 1;
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}