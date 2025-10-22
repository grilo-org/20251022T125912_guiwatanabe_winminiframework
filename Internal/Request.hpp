#pragma once

#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace HTTP {
	class Request {
	private:
	public:
		Request() : contentLength(0) {};

		std::string method;
		std::string path;

		std::unordered_map<std::string, std::string> headers;
		std::unordered_map<std::string, std::string> cookies;

		std::string protocol;
		std::string userAgent;
		std::string clientIp;

		std::unordered_map<std::string, std::string> query;
		std::unordered_map<std::string, std::string> form;
		std::unordered_map<std::string, std::string> params;

		std::string body;
		json json;

		std::string contentType;
		int contentLength;
	};
}