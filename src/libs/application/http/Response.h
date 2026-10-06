#ifndef DSSP_HTTP_RESPONSE_H
#define DSSP_HTTP_RESPONSE_H

#include <functional>
#include <string>
#include <string_view>

#include <drogon/HttpResponse.h>

#include <dssphttp/HttpErrors.h>

namespace dssp::http {
    using Callback = std::function<void(const drogon::HttpResponsePtr &)>;
    drogon::HttpResponsePtr jsonResponse(const Json::Value &body);
    drogon::HttpResponsePtr problemResponse(int status, std::string_view type, std::string_view title, std::string detail);
    drogon::HttpResponsePtr problemResponse(const RequestErrors &error);
    drogon::HttpResponsePtr problemResponse(const ValidationError &error);
}

#endif // DSSP_HTTP_RESPONSE_H
