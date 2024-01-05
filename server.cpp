#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <string>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>

std::mutex clientListMutex;
std::vector<std::pair<int, std::string>> activeClients;

std::string xorfun(std::string encoded, std::string crc) {
    int crclen = crc.length();

    for (int i = 0; i <= (encoded.length() - crclen);) {
        for (int j = 0; j < crclen; j++) {
            encoded[i + j] = encoded[i + j] == crc[j] ? '0' : '1';
        }
        for (; i < encoded.length() && encoded[i] != '1'; i++)
            ;
    }

    return encoded;
}

bool performCRCErrorCheck(const std::string& message, const std::string& crc) {
    std::string encodedMessage = message + std::string(crc.length() - 1, '0');
    encodedMessage = xorfun(encodedMessage, crc);
    return encodedMessage.substr(encodedMessage.length() - crc.length() + 1) == std::string(crc.length() - 1, '0');
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
        if (performCRCErrorCheck(receivedMessage, "YourActualCRC")) {
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
            // Error detected, send error message back to the client
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
