#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <vector>
#include "Request.hpp"
#include "Response.hpp"
#include "Utils.hpp"

using namespace HTTP;
using namespace Utils;
using json = nlohmann::json;

class Router {
private:
    static constexpr size_t MAX_REQUEST_SIZE = 16 * 1024;
    inline static std::unordered_set<std::string> validMethods = {
        "GET", "POST", "PUT", "DELETE", "PATCH", "OPTIONS", "HEAD"
    };

    Router() = default;

    Router(const Router&) = delete;
    Router& operator=(const Router&) = delete;

    std::filesystem::path publicPath;
public:
    using Handler = std::function<Response(Request&)>;

    static Router& getInstance() {
        static Router instance;
        return instance;
    }

    size_t getMaxRequestSize() {
        return MAX_REQUEST_SIZE;
    }

    void setPublicPath(const std::filesystem::path& path) {
        publicPath = path;
    }

    void addRoute(const std::string& method, const std::string& path, Handler handler) {
        std::string normalizedMethod = normalizeMethod(method);
        if (normalizedMethod.empty()) {
            std::cout << "[!] [Router] Unsupported HTTP method '" << method << "' for route '" << path << "' - route definition not applied." << std::endl;
            return;
        }

        std::cout << "[+] [Router] Route created: " << normalizedMethod << " " << path << std::endl;
        routes.push_back({ normalizedMethod, splitPath(path), handler });
    }

    Response route(Request& req) {
        if (req.body.size() > MAX_REQUEST_SIZE) {
            return Response().setStatus(HttpStatus::PayloadTooLarge);
        }

        std::string normalizedPath = normalizePath(req.path);
        if (normalizedPath.empty()) {
            return Response().setStatus(HttpStatus::BadRequest);
        }

        for (auto& r : routes) {
            if (r.method != req.method) continue;

            std::unordered_map<std::string, std::string> params;
            if (matchRoute(r.parts, splitPath(normalizedPath), params)) {
                req.params = params;

                if (req.method == "POST" || req.method == "PUT") {
                    if (req.contentType.empty()) req.contentType = "application/json";

                    if (req.contentType == "application/x-www-form-urlencoded") {
                        req.form = parseParams(req.body);
                    } else if (req.contentType == "application/json") {
                        try {
                            req.json = json::parse(req.body);
                        } catch (...) {
                            return Response().setStatus(HttpStatus::UnprocessableEntity);
                        }
                    } else {
                        return Response().setStatus(HttpStatus::UnsupportedMediaType);
                    }
                }
                return r.handler(req);
            }
        }

        if (req.method == "GET" && !publicPath.empty()) {
            std::string publicFilePath = publicPath.string() + normalizedPath;

            if (publicFilePath.find("..") == std::string::npos && std::filesystem::exists(publicFilePath)) {
                std::ifstream f(publicFilePath, std::ios::binary);
                std::ostringstream ss;
                ss << f.rdbuf();

                Response res;
                res.setBody(ss.str());

                return res.setStatus(HttpStatus::OK);
            }
        }

        return Response().setStatus(HttpStatus::NotFound);
    }

private:
    struct Route {
        std::string method;
        std::vector<std::string> parts;
        Handler handler;
    };

    std::vector<Route> routes;

    static std::string normalizeMethod(const std::string& method) {
        std::string result = method;
        std::transform(result.begin(), result.end(), result.begin(), ::toupper);

        if (validMethods.find(result) == validMethods.end()) {
            return "";
        }

        return result;
    }
    
    static std::vector<std::string> splitPath(const std::string& path) {
        std::vector<std::string> parts;
        std::stringstream ss(path);

        std::string segment;
        while (std::getline(ss, segment, '/')) {
            if (!segment.empty()) parts.push_back(segment);
        }

        return parts;
    }

    static std::string normalizePath(const std::string& path) {
        std::vector<std::string> parts;
        std::stringstream ss(path);
        std::string segment;

        while (std::getline(ss, segment, '/')) {
            if (segment.empty() || segment == ".") {
                continue;
            }

            if (segment == "..") {
                if (parts.empty()) {
                    return "";
                }
                parts.pop_back();
            } else {
                parts.push_back(segment);
            }
        }

        std::string normalized;

        for (const auto& part : parts) {
            normalized += "/" + part;
        }

        return normalized.empty() ? "/" : normalized;
    }

    static bool matchRoute(const std::vector<std::string>& routeParts, const std::vector<std::string>& pathParts, std::unordered_map<std::string, std::string>& params) {
        size_t i = 0;
        for (; i < routeParts.size(); i++) {
            if (i >= pathParts.size()) return false;

            if (routeParts[i].size() > 1 && routeParts[i][0] == ':') {
                std::string key = routeParts[i].substr(1);
                params[key] = pathParts[i];
            } else if (routeParts[i].size() > 1 && routeParts[i][0] == '*') {
                std::string key = routeParts[i].substr(1);
                std::string rest;
                for (size_t j = i; j < pathParts.size(); j++) {
                    if (j > i) rest += "/";
                    rest += pathParts[j];
                }
                params[key] = rest;
                return true;
            } else if (routeParts[i] != pathParts[i]) {
                return false;
            }
        }

        return (i == pathParts.size());
    }

    static std::unordered_map<std::string, std::string> parseParams(const std::string& qs) {
        std::unordered_map<std::string, std::string> params;

        size_t start = 0;
        while (start < qs.size()) {
            size_t eq = qs.find('=', start);
            size_t amp = qs.find('&', start);
            if (eq == std::string::npos) break;
            std::string key = urlDecode(qs.substr(start, eq - start));
            std::string value = urlDecode(qs.substr(eq + 1, (amp == std::string::npos ? qs.size() : amp) - eq - 1));
            params[key] = value;
            if (amp == std::string::npos) break;
            start = amp + 1;
        }

        return params;
    }
};