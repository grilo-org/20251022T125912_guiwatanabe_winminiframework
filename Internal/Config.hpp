#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class Config {
private:
    Config() = default;

    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
public:
    std::string host = "127.0.0.1";
    int         port = 8080;

    static Config& getInstance() {
        static Config instance;
        return instance;
    }

    inline bool loadFile(const std::string& path) {
        std::ifstream in(path);

        if (!in.is_open()) {
            std::cerr << "[!] [Config] Unable to load config.json." << std::endl;
            return false;
        }

        json j;

        try {
            in >> j;
        } catch (const json::parse_error& e) {
            std::cerr << "[!] [Config] JSON parse error: " << e.what() << std::endl;
            return false;
        }

        host = j.value("host", "127.0.0.1");
        port = j.value("port", 8080);

        return true;
    }

    inline void outputConfig() {
        std::cout << "[*] [Config] Using hostname " << host << ":" << port << "." << std::endl;
    }
};