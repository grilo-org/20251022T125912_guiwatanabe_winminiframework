#pragma once

#include <WinSock2.h>
#include <sstream>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>

#include "Request.hpp"

using namespace HTTP;

struct ClientInfo {
    SOCKET socket;
    sockaddr_in addr;
};

template<typename T>
class SafeQueue {
private:
    std::queue<T> queue_;
    std::mutex mtx_;
    std::condition_variable cv_;
public:
    void push(T value);
    T pop();
};

class Server {
private:
    const int NUM_THREADS = 4;
    SOCKET serverSocket;
    SafeQueue<ClientInfo> clientQueue;
    std::vector<std::thread> workers;

    void handleClient(SOCKET clientSocket, const sockaddr_in& clientAddr);
    void workerThread();

    Request parseRequest(std::istringstream& stream);

public:
    Server();
    ~Server();
    void run(WSAEVENT& shutdownEvent);
};

template<typename T>
void SafeQueue<T>::push(T value) {
    std::lock_guard<std::mutex> lock(mtx_);
    queue_.push(std::move(value));
    cv_.notify_one();
}

template<typename T>
T SafeQueue<T>::pop() {
    std::unique_lock<std::mutex> lock(mtx_);
    cv_.wait(lock, [this]() { return !queue_.empty(); });
    T val = std::move(queue_.front());
    queue_.pop();
    return val;
}
