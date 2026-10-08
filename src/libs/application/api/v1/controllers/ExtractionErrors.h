#ifndef DSSP_API_V1_CONTROLLERS_EXTRACTIONERRORS_H
#define DSSP_API_V1_CONTROLLERS_EXTRACTIONERRORS_H

#include <dsspextraction/ExtractionError.h>
#include <dssphttp/Response.h>

namespace dssp::api_v1 {
    drogon::HttpResponsePtr extractionProblemResponse(const extraction::CatalogErrors &, const http::RequestLog & = {});
    drogon::HttpResponsePtr extractionProblemResponse(const extraction::ExtractionErrors &, const http::RequestLog & = {});
}

#endif // DSSP_API_V1_CONTROLLERS_EXTRACTIONERRORS_H
