#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <string>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <algorithm>
#include <crc32c/crc32c.h>
#include <bitset>

std::mutex clientListMutex;
std::vector<std::pair<int, std::string>> activeClients;

std::string xorfun(const std::string& encoded, const std::string& crc) {
    int crclen = crc.length();

    std::string modifiedEncoded = encoded;
    for (int i = 0; i <= (modifiedEncoded.length() - crclen);) {
        for (int j = 0; j < crclen; j++) {
            modifiedEncoded[i + j] = modifiedEncoded[i + j] == crc[j] ? '0' : '1';
        }
        for (; i < modifiedEncoded.length() && modifiedEncoded[i] != '1'; i++);
    }

    return modifiedEncoded;
}

bool performCRCErrorCheck(const std::string& message, const std::string& crc) {
    uint32_t calculatedCRC = crc32c::Crc32c(message.c_str(), message.size());

    // Convert calculated CRC to binary
    std::bitset<32> calculatedCRCBinary(calculatedCRC);

    // Convert provided CRC to binary
    std::bitset<32> expectedCRCBinary(std::stoi(crc));

    // XOR operation with the provided CRC
    std::bitset<32> encodedMessageBinary = calculatedCRCBinary ^ expectedCRCBinary;

    // Check if all bits are zero (indicating no error)
    return encodedMessageBinary.none();
}

bool perform2DParityCheck(const std::string& message) {
    // Your 2D parity check implementation here
    // ...

    // Placeholder for example (you may need to customize this)
    return true;
}

void handleClient(int clientSocket) {
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));

    // Receive client name during connection
    if (recv(clientSocket, buffer, sizeof(buffer), 0) == -1) {
        std::cerr << "Error receiving client name" << std::endl;
        close(clientSocket);
        return;
    }

    std::string clientName(buffer);
    clientName.pop_back(); // Remove newline character

    {
        std::lock_guard<std::mutex> lock(clientListMutex);
        activeClients.emplace_back(clientSocket, clientName);
    }

    // Send list of current clients to the new client
    {
        std::lock_guard<std::mutex> lock(clientListMutex);
        for (const auto& client : activeClients) {
            send(clientSocket, client.second.c_str(), client.second.size(), 0);
            send(clientSocket, "\n", 1, 0);
        }
    }

    // Notify other clients about the new client
    {
        std::lock_guard<std::mutex> lock(clientListMutex);
        for (const auto& client : activeClients) {
            if (client.first != clientSocket) {
                send(client.first, ("CONN|" + clientName + "\n").c_str(), 1024, 0);
            }
        }
    }

    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);

        if (bytesReceived <= 0) {
            // Client disconnected
            {
                std::lock_guard<std::mutex> lock(clientListMutex);
                auto it = std::remove_if(activeClients.begin(), activeClients.end(),
                                         [clientSocket](const auto& client) {
                                             return client.first == clientSocket;
                                         });
                activeClients.erase(it, activeClients.end());
            }

            // Notify other clients about the departure
            {
                std::lock_guard<std::mutex> lock(clientListMutex);
                for (const auto& client : activeClients) {
                    send(client.first, ("GONE|" + clientName + "\n").c_str(), 1024, 0);
                }
            }

            close(clientSocket);
            return;
        }

        // Simulate error detection (e.g., CRC)
        std::string receivedMessage(buffer);
        uint32_t calculatedCRC = crc32c::Crc32c(receivedMessage.c_str(), receivedMessage.size());

        if (performCRCErrorCheck(receivedMessage, std::to_string(calculatedCRC))) {
            // Perform 2D parity check
            if (perform2DParityCheck(receivedMessage)) {
                // No error detected, process the message
                {
                    std::lock_guard<std::mutex> lock(clientListMutex);
                    for (const auto& client : activeClients) {
                        if (client.first != clientSocket) {
                            send(client.first, receivedMessage.c_str(), receivedMessage.size(), 0);
                        }
                    }
                }
            } else {
                // Error detected in 2D parity, handle accordingly
                std::cerr << "Error in 2D parity check" << std::endl;
            }
        } else {
            // Error detected in CRC, send error message back to the client
            send(clientSocket, "MERR\n", 1024, 0);
        }
    }
}

int main() {
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        std::cerr << "Error creating server socket" << std::endl;
        return -1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(12345);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        std::cerr << "Error binding socket" << std::endl;
        return -1;
    }

    if (listen(serverSocket, 5) == -1) {
        std::cerr << "Error listening on socket" << std::endl;
        return -1;
    }

    std::cout << "Server listening on port 12345..." << std::endl;

    while (true) {
        int clientSocket = accept(serverSocket, nullptr, nullptr);
        if (clientSocket == -1) {
            std::cerr << "Error accepting client connection" << std::endl;
            continue;
        }

        std::thread clientThread(handleClient, clientSocket);
        clientThread.detach();
    }

    close(serverSocket);
    return 0;
}
