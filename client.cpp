#include <iostream>
#include <string>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>

#define MAX 100
#define PARITY 100

bool perform2DParityCheck(const std::string& message);

void receiveChat(int clientSocket);

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

bool perform2DParityCheck(const std::string& message) {
    int i, j, seg, flag = 0;
    char arr[MAX], vrc[PARITY], lrc[PARITY];

    // Copy message to a char array
    strncpy(arr, message.c_str(), sizeof(arr));
    arr[sizeof(arr) - 1] = '\0'; // Ensure null-terminated string

    for (i = 2; i < strlen(arr); i++) {
        if (strlen(arr) % i == 0) {
            flag = 1;
            break;
        } else {
            flag = 0;
        }
    }

    if (flag == 1) {
        seg = strlen(arr) / i;
    } else {
        seg = strlen(arr);
    }

    for (i = 0; i < strlen(arr); i++) {
        if (i % (strlen(arr) / seg) == 0) {
            vrc[i / (strlen(arr) / seg)] = arr[i];
            continue;
        }
        if (vrc[i / (strlen(arr) / seg)] == arr[i]) {
            vrc[i / (strlen(arr) / seg)] = '0';
        } else {
            vrc[i / (strlen(arr) / seg)] = '1';
        }
    }

    for (i = 0; i < strlen(arr); i++) {
        if (i < (strlen(arr) / seg)) {
            lrc[i] = arr[i];
            continue;
        }
        if (lrc[i % (strlen(arr) / seg)] == arr[i]) {
            lrc[i % (strlen(arr) / seg)] = '0';
        } else {
            lrc[i % (strlen(arr) / seg)] = '1';
        }
    }

    std::cout << "\n\n2D parity is :\n\n";
    for (i = 0; i < strlen(arr); i++) {
        std::cout << "  " << arr[i] << "  ";

        if (((i) % (strlen(arr) / seg)) == (strlen(arr) / seg) - 1) {
            std::cout << "|  " << vrc[i / (strlen(arr) / seg)] << "  " << std::endl;
        }
    }

    for (i = 0; i < (strlen(arr) / seg); i++) {
        std::cout << "-----";
    }
    std::cout << std::endl;

    for (i = 0; i < (strlen(arr) / seg); i++) {
        std::cout << "  " << lrc[i] << "  ";
    }
    std::cout << std::endl;

    return 0; 
}

void receiveChat(int clientSocket) {
    char buffer[1024];
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);

        if (bytesReceived <= 0) {
            std::cerr << "Error receiving message from server" << std::endl;
            break;
        }

        std::string receivedMessage(buffer);
        // Perform 2D parity check before processing the received message
        if (perform2DParityCheck(receivedMessage)) {
            std::cout << "Received: " << receivedMessage;
        } else {
            std::cerr << "Error in communication" << std::endl;
        }
    }
}
