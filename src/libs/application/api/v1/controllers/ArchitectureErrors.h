#ifndef DSSP_APIV1_ARCHITECTUREERRORS_H
#define DSSP_APIV1_ARCHITECTUREERRORS_H

#include <drogon/HttpResponse.h>

#include <dssparch/Errors.h>

namespace dssp::api_v1 {
    drogon::HttpResponsePtr architectureProblemResponse(const arch::CatalogErrors &errors);
    drogon::HttpResponsePtr architectureProblemResponse(const arch::EnvironmentErrors &errors);
    drogon::HttpResponsePtr architectureProblemResponse(const arch::SynthesisErrors &errors);
}

#endif // DSSP_APIV1_ARCHITECTUREERRORS_H
