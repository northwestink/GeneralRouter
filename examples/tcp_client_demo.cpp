#include "../src/tcpclient.h"
#include <iostream>
#include <chrono>
#include <thread>

constexpr std::string_view LOGON = "8=FIX.4.29=7035=A34=149=CLIENT152=20250314-15:24:42.19156=EXECUTOR98=0108=3010=088\n";

int main() {
    // Create TCP client instance
    TCPClient client;

    // Set up message handler
    client.setMessageHandler([](const std::string& msg) {
        std::cout << "Received: " << msg << std::endl;
    });

    // Connect to server
    client.connect("127.0.0.1", 8080, [](bool success) {
        if (success) {
            std::cout << "Connected to server!" << std::endl;
        } else {
            std::cout << "Connection failed!" << std::endl;
        }
    });

    // Main loop
    int count = 0;
    while (count < 5) {
        // Process events
        client.poll();

        // Send a message if connected
        if (client.isConnected()) {
            std::string message = LOGON.data();
            client.asyncSend(message);
            std::cout << "Sent: " << message << std::endl;
        }

        // Wait a bit before next iteration
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // Cleanup
    client.disconnect();
    return 0;
}
