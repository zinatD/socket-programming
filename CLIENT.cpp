#include <iostream>
#include <string>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <crc32c/crc32c.h>
#include <bitset>

#define MAX 100
#define PARITY 100

bool perform2DParityCheck(const std::string& message);

void receiveChat(int clientSocket);

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

int main() {
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1) {
        std::cerr << "Error creating client socket" << std::endl;
        return -1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(12345);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        std::cerr << "Error connecting to server" << std::endl;
        return -1;
    }

    std::cout << "Connected to server. Enter your name: ";
    std::string clientName;
    std::getline(std::cin, clientName);

    send(clientSocket, clientName.c_str(), clientName.size(), 0);

    std::thread receiveThread(receiveChat, clientSocket);
    receiveThread.detach();

    while (true) {
        std::string message;
        std::getline(std::cin, message);

        if (message == "exit") {
            break;
        }

        uint32_t calculatedCRC = crc32c::Crc32c(message.c_str(), message.size());
        // Convert calculated CRC to binary and send to server
        std::bitset<32> calculatedCRCBinary(calculatedCRC);
        send(clientSocket, calculatedCRCBinary.to_string().c_str(), calculatedCRCBinary.size(), 0);

        // Perform 2D parity check before sending the message
        if (perform2DParityCheck(message)) {
            send(clientSocket, ("MESG|" + message + "\n").c_str(), 1024, 0);
        } else {
            std::cerr << "Error in communication" << std::endl;
        }
    }

    close(clientSocket);
    return 0;
}

void receiveChat(int clientSocket) {
    char buffer[1024];
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        
        // Receive CRC from the server
        int bytesReceivedCRC = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesReceivedCRC <= 0) {
            std::cerr << "Error receiving CRC from server" << std::endl;
            break;
        }

        std::string receivedCRC(buffer);

        // Receive message from the server
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesReceived <= 0) {
            std::cerr << "Error receiving message from server" << std::endl;
            break;
        }

        std::string receivedMessage(buffer);

        // Perform CRC error check
        if (performCRCErrorCheck(receivedMessage, receivedCRC)) {
            // Perform 2D parity check
            if (perform2DParityCheck(receivedMessage)) {
                std::cout << "Received: " << receivedMessage;
            } else {
                // Error detected in 2D parity, handle accordingly
                std::cerr << "Error in 2D parity check" << std::endl;
            }
        } else {
            // Error detected in CRC, handle accordingly
            std::cerr << "Error in CRC check" << std::endl;
        }
    }
}
