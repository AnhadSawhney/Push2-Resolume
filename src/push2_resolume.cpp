// main.cpp

// OSC pack (adjust include paths to your install)
#include "osc/OscOutboundPacketStream.h"
#include "osc/OscReceivedElements.h"
#include "osc/OscPacketListener.h"
#include "ip/UdpSocket.h"
#include "ip/IpEndpointName.h"
#include "ResolumeTracker.h"
#include <iostream>
#include <thread>
#include <atomic>

// Push 2 USB (adjust include paths to your install)
#include "PushUSB.h"

using namespace osc;

class ResolumeOSCListener : public OscPacketListener {
private:
    ResolumeTracker& tracker;
    
public:
    ResolumeOSCListener(ResolumeTracker& resolumeTracker) : tracker(resolumeTracker) {}
    
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
    
    try {
        // Create Resolume tracker
        ResolumeTracker resolumeTracker;

        // Initialize Push 2 connection state and device object
        bool pushConnected = false;
        // If you have a Push2 class/object, initialize it here
        // Example:
        PushUSB push;
        // pushConnected = push.isDeviceConnected();

        // Create OSC listener
        ResolumeOSCListener listener(resolumeTracker);
        
        // Create UDP socket for receiving OSC messages
        UdpListeningReceiveSocket socket(IpEndpointName(IpEndpointName::ANY_ADDRESS, OSC_PORT), &listener);
        
        std::cout << "Push2-Resolume Controller starting..." << std::endl;
        std::cout << "Listening for OSC messages on port " << OSC_PORT << std::endl;
        std::cout << "Press 'q' + Enter to quit" << std::endl;
        
        // Start listening in a separate thread
        std::atomic<bool> shouldStop(false);
        std::thread oscThread([&socket, &shouldStop]() {
            while (!shouldStop.load()) {
                socket.RunUntilSigInt();
            }
        });
        
        // Wait for user input to quit
        std::string input;
        while (std::getline(std::cin, input)) {
            if (input == "q" || input == "Q") {
                break;
            } else if (input == "clear") {
                resolumeTracker.clearAll();
                if (pushConnected) {
                    push.clearAllPads();
                    push.clearDisplay();
                }
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
            } else if (input == "test" && pushConnected) {
                std::cout << "Running Push 2 test..." << std::endl;
                push.fillDisplay(255, 0, 0); // Red screen
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                push.fillDisplay(0, 255, 0); // Green screen
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                push.fillDisplay(0, 0, 255); // Blue screen
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                push.clearDisplay();
                std::cout << "Test complete" << std::endl;
            } else if (input == "help") {
                std::cout << "\nAvailable commands:" << std::endl;
                std::cout << "  q/Q      - Quit the program" << std::endl;
                std::cout << "  clear    - Clear all tracked state" << std::endl;
                std::cout << "  status   - Show basic status information" << std::endl;
                std::cout << "  tree     - Print complete state tree" << std::endl;
                std::cout << "  print    - Same as tree" << std::endl;
                if (pushConnected) {
                    std::cout << "  test     - Run Push 2 display test" << std::endl;
                }
                std::cout << "  help     - Show this help message" << std::endl;
                std::cout << std::endl;
            }
        }
        
        shouldStop.store(true);
        if (oscThread.joinable()) {
            oscThread.join();
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