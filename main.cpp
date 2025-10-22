#include <iostream>
#include <filesystem>

#include "Internal/Config.hpp"
#include "Internal/Router.hpp"
#include "Internal/Server.hpp"

#include "Controllers/TestController.hpp"

WSAEVENT shutdownEvent = nullptr;

BOOL WINAPI ConsoleHandler(DWORD ctrlType) {
    if (ctrlType == CTRL_C_EVENT) {
        std::cout << "[*] Shutting down..." << std::endl;
        if (shutdownEvent) {
            WSASetEvent(shutdownEvent);
        }
        return TRUE;
    }
    return FALSE;
}

int main() {
	std::cout << "[*] Starting..." << std::endl;

    SetConsoleCtrlHandler(ConsoleHandler, TRUE);

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "[!] Winsock WSAStartup failed, exiting." << std::endl;
        return -1;
    }

    shutdownEvent = WSACreateEvent();
    if (!shutdownEvent) {
        std::cerr << "[!] Failed to create shutdown event." << std::endl;
        WSACleanup();
        return -1;
    }

    Config& config = Config::getInstance();
    std::filesystem::path cwdPath = std::filesystem::current_path();
    std::filesystem::path configPath = cwdPath / "config.json";
    if (!config.loadFile(configPath.string())) {
        WSACloseEvent(shutdownEvent);
        WSACleanup();
        return -1;
    }

    config.outputConfig();

    Router& router = Router::getInstance();
    std::filesystem::path publicPath = cwdPath / "public";
    router.setPublicPath(publicPath);

    TestController testController;
    router.addRoute("GET", "/status", [&testController](Request& request) {
        return testController.statusPage(request);
    });
    router.addRoute("POST", "/json", [&testController](Request& request) {
        return testController.testJson(request);
    });
    router.addRoute("POST", "/hello", [&testController](Request& request) {
        return testController.hello(request);
    });

    try {
        Server server;
        server.run(shutdownEvent);
    }
    catch (const std::runtime_error& e) {
        std::cerr << "[!] Server failed to start: " << e.what() << std::endl;

        WSACloseEvent(shutdownEvent);
        WSACleanup();
        return -1;
    }

    WSACloseEvent(shutdownEvent);
    WSACleanup();

	return 0;
}