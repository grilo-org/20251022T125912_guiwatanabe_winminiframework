#pragma once

#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include "HttpStatus.hpp"

using json = nlohmann::json;

namespace HTTP {
	class Response {
	public:
		int statusCode;
		std::string reason;

		std::unordered_map<std::string, std::string> headers;
		std::unordered_map<std::string, std::string> cookies;

		std::string body;

		std::string contentType;
		int contentLength;

		Response() : statusCode(static_cast<int>(HttpStatus::OK)),
			reason(reasonPhrase(HttpStatus::OK)),
			contentType("text/html"),
			contentLength(0) {
		};

		Response& setStatusCode(int code = 200) {
			if (!httpStatusValid(code)) {
				code = static_cast<int>(HttpStatus::InternalServerError);
			}
			statusCode = code;
			reason = reasonPhrase(httpStatusFromInt(statusCode));
			return *this;
		}

		Response& setStatus(HttpStatus status)
		{
			statusCode = static_cast<int>(status);
			reason = reasonPhrase(status);
			return *this;
		}

		Response& setHeader(const std::string& header, const std::string& value)
		{
			headers[header] = value;
			return *this;
		}

		Response& setCookie(const std::string& cookie, const std::string& value)
		{
			cookies[cookie] = value;
			return *this;
		}

		Response& setContentType(const std::string& type = "text/tml")
		{
			contentType = type;
			return setHeader("Content-Type", contentType);
		}

		Response& setBody(const std::string& data)
		{
			body = data;
			contentLength = static_cast<int>(body.size());
			return setHeader("Content-Length", std::to_string(contentLength));
		}

		Response& setJSON(const json& data)
		{
			setContentType("application/json");
			return setBody(data.dump());
		}
	};
}