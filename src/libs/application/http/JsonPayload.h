#ifndef DSSP_HTTP_JSONPAYLOAD_H
#define DSSP_HTTP_JSONPAYLOAD_H

#include <drogon/HttpRequest.h>

#include <dssphttp/HttpErrors.h>

namespace dssp::http {
    std::expected<Json::Value, PayloadErrors> readJsonPayload(const drogon::HttpRequest &request);
}

#endif // DSSP_HTTP_JSONPAYLOAD_H
