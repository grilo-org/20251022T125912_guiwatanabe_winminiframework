#pragma once

#include "../Internal/Request.hpp"
#include "../Internal/Response.hpp"

using namespace HTTP;

class TestController {
public:
	Response statusPage(Request request) {
		static std::string htmlContent = "<html><body><h1>Status: Online</h1></body></html>";
		return Response().setBody(htmlContent);
	}

	Response testJson(Request request) {
		json jsonPayload = request.json;
		return Response().setStatus(HttpStatus::OK).setJSON(jsonPayload);
	}

	Response hello(Request request) {
		std::string hello = "Hello ";
		std::string name = request.form.count("name") ? request.form.at("name") : "World";
		hello.append(name);
		
		return Response().setStatus(HttpStatus::OK).setBody(hello);
	}
};