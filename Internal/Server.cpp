#include <WS2tcpip.h>

#include "Server.hpp"
#include "Router.hpp"
#include "Config.hpp"

#pragma comment(lib, "ws2_32.lib")

Server::Server() {

    Config& config = Config::getInstance();

    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        throw std::runtime_error("Socket creation failed");
    }

    BOOL opt = TRUE;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(config.port);
    inet_pton(AF_INET, config.host.c_str(), &serverAddr.sin_addr);

    if (bind(serverSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) != 0) {
        closesocket(serverSocket);
        throw std::runtime_error("Bind failed");
    }

    if (listen(serverSocket, 20) != 0) {
        closesocket(serverSocket);
        throw std::runtime_error("Listen failed");
    }

    std::cout << "[*] [Server] Server listening on " << config.host.c_str() << ":" << config.port << "\n";

    for (int i = 0; i < NUM_THREADS; ++i) {
        workers.emplace_back(&Server::workerThread, this);
    }
}

Server::~Server() {
    for (int i = 0; i < NUM_THREADS; ++i) {
        clientQueue.push({ INVALID_SOCKET, {} });
    }
    for (auto& t : workers) {
        if (t.joinable()) {
            t.join();
        }
    }
    closesocket(serverSocket);
}

void Server::run(WSAEVENT& shutdownEvent) {
    WSAEVENT serverEvent = WSACreateEvent();
    WSAEventSelect(serverSocket, serverEvent, FD_ACCEPT);

    WSAEVENT events[2] = { serverEvent, shutdownEvent };

    while (true) {
        DWORD wait = WSAWaitForMultipleEvents(2, events, FALSE, WSA_INFINITE, FALSE);
        if (wait == WSA_WAIT_FAILED) break;
        if (wait == WSA_WAIT_EVENT_0 + 1) break;

        if (wait == WSA_WAIT_EVENT_0) {
            WSANETWORKEVENTS ne;
            if (WSAEnumNetworkEvents(serverSocket, serverEvent, &ne) == SOCKET_ERROR) {
                std::cout << "[!] [Server] WSAEnumNetworkEvents failed: " << WSAGetLastError() << std::endl;
                continue;
            }

            if (ne.lNetworkEvents & FD_ACCEPT) {
                SOCKET clientSocket;
                while (true) {
                    sockaddr_in clientAddr;
                    int clientLen = sizeof(clientAddr);
                    memset(&clientAddr, 0, sizeof(clientAddr));

                    clientSocket = accept(serverSocket, (SOCKADDR*)&clientAddr, &clientLen);

                    if (clientSocket == INVALID_SOCKET) {
                        if (WSAGetLastError() != WSAEWOULDBLOCK) {
                            std::cout << "[!] [Server] accept() failed: " << WSAGetLastError() << std::endl;
                        }
                        break;
                    }
                    clientQueue.push({ clientSocket, clientAddr });
                }
            }
        }
    }
    WSACloseEvent(serverEvent);
}
void Server::handleClient(SOCKET clientSocket, const sockaddr_in& clientAddr) {
    char clientIp[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clientAddr.sin_addr, clientIp, INET_ADDRSTRLEN);

    DWORD timeout = 5000;
    setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    setsockopt(clientSocket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));

    Router& router = Router::getInstance();

    bool keepAlive = true;
    try {
        while (keepAlive) {
            fd_set read_fds{};
            FD_ZERO(&read_fds);
            FD_SET(clientSocket, &read_fds);

            timeval timeout_val;
            timeout_val.tv_sec = 5;
            timeout_val.tv_usec = 0;

            int select_result = select(0, &read_fds, NULL, NULL, &timeout_val);
            if (select_result <= 0) break;

            std::string requestString;
            std::vector<char> buffer(1024);
            size_t totalBytesRead = 0;
            const size_t maxRequestSize = router.getMaxRequestSize();

            while (totalBytesRead < maxRequestSize) {
                int bytesRead = recv(clientSocket, buffer.data(), static_cast<int>(buffer.size()), 0);
                if (bytesRead <= 0) break;
                requestString.append(buffer.data(), bytesRead);
                totalBytesRead += bytesRead;
                if (requestString.find("\r\n\r\n") != std::string::npos) break;
            }

            if (requestString.empty() || totalBytesRead >= maxRequestSize) break;

            if (requestString.find("Connection: close") != std::string::npos) keepAlive = false;

            std::istringstream requestStream(requestString);

            Request request = parseRequest(requestStream);
            Response response = router.route(request);

            std::ostringstream responseStream;
            responseStream << "HTTP/1.1 " << response.statusCode << "\r\n";
            responseStream << "Content-Type: " << response.contentType << "\r\n";
            responseStream << "Content-Length: " << response.contentLength << "\r\n";
            responseStream << (keepAlive ? "Connection: keep-alive\r\n" : "Connection: close\r\n");
            responseStream << "\r\n";
            responseStream << response.body;

            std::string responseStr = responseStream.str();
            int totalSent = 0;
            while (totalSent < (int)responseStr.size()) {
                int sent = send(clientSocket, responseStr.c_str() + totalSent, (int)responseStr.size() - totalSent, 0);
                if (sent <= 0) break;
                totalSent += sent;
            }

        }
    }
    catch (...) {
        std::cout << "[!] [Server] Unexpected exception." << std::endl;
    }

    closesocket(clientSocket);
}

void Server::workerThread() {
    while (true) {
        ClientInfo client = clientQueue.pop();
        if (client.socket == INVALID_SOCKET) break;

        try {
            handleClient(client.socket, client.addr);
        }
        catch (const std::exception& e) {
            std::cerr << "[!] [Server] Worker thread caught exception: " << e.what() << std::endl;
            closesocket(client.socket);
        }
        catch (...) {
            std::cerr << "[!] [Server] Worker thread caught unknown exception." << std::endl;
            closesocket(client.socket);
        }
    }
}

Request Server::parseRequest(std::istringstream& stream)
{
    Request req;

    std::string requestLine;
    std::getline(stream, requestLine);
    std::istringstream rl(requestLine);
    rl >> req.method;
    rl >> req.path;
    rl >> req.protocol;

    std::string headerLine;
    while (std::getline(stream, headerLine))
    {
        if (headerLine == "\r" || headerLine.empty())
            break;

        size_t colonPos = headerLine.find(':');
        if (colonPos == std::string::npos)
            continue;

        std::string key = headerLine.substr(0, colonPos);
        std::string value = headerLine.substr(colonPos + 1);

        // trim key
        key.erase(0, key.find_first_not_of(" \r\t"));
        key.erase(key.find_last_not_of(" \r\t") + 1);

        // trim value
        value.erase(0, value.find_first_not_of(" \r\t"));
        value.erase(value.find_last_not_of(" \r\t") + 1);

        req.headers[key] = value;

        if (key == "Content-Type")
            req.contentType = value;

        if (key == "Content-Length")
            req.contentLength = std::stoi(value);

        if (key == "User-Agent")
            req.userAgent = value;

        if (key == "Cookie") {
            std::stringstream cs(value);
            std::string kv;
            while (std::getline(cs, kv, ';')) {
                size_t eq = kv.find('=');
                if (eq == std::string::npos) continue;

                std::string ckey = kv.substr(0, eq);
                std::string cval = kv.substr(eq + 1);

                ckey.erase(0, ckey.find_first_not_of(" \r\t"));
                ckey.erase(ckey.find_last_not_of(" \r\t") + 1);
                cval.erase(0, cval.find_first_not_of(" \r\t"));
                cval.erase(cval.find_last_not_of(" \r\t") + 1);

                req.cookies[urlDecode(ckey)] = urlDecode(cval);
            }
        }
    }

    if (req.contentLength > 0) {
        std::string body(req.contentLength, '\0');
        stream.read(&body[0], req.contentLength);
        req.body = body;
    }

    auto pos = req.path.find('?');
    if (pos != std::string::npos) {
        std::string qs = req.path.substr(pos + 1);
        req.path = req.path.substr(0, pos);

        std::stringstream ss(qs);
        std::string kv;
        while (std::getline(ss, kv, '&')) {
            size_t eq = kv.find('=');
            if (eq == std::string::npos) continue;

            auto key = urlDecode(kv.substr(0, eq));
            auto value = urlDecode(kv.substr(eq + 1));
            req.query[key] = value;
        }
    }

    req.path = urlDecode(req.path);

    return req;
}
