#ifndef SERVER_H
#define SERVER_H

#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <mutex>

class Server {
public:
    Server();
    ~Server();

    void setOnMessage(const std::function<void(const std::string&)>& onMessage);
    void runServer();
    void closeServer();

private:
    void initializeSocket();
    void bindSocket();
    void listenForClients();
    void handleClient(int clientSocket);
    void sendClientList(int clientSocket);
    void notifyNewClient(const std::string& clientName);
    void notifyClientDeparture(const std::string& clientName);
    void sendMessageClients(const std::string& message);
    void sendMessageAll(const std::string& message, const int& socket);
    bool performCRCErrorCheck(const std::string& message);

private:
    int serverSocket;
    std::vector<std::pair<int, std::string>> activeClients;
    std::mutex clientListMutex;
    std::function<void(const std::string&)> onMessageCallback;
    std::vector<std::thread> clientThreads;
};

#endif
