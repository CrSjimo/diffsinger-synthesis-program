#ifndef DSSP_HTTP_RESPONSENEGOTIATION_H
#define DSSP_HTTP_RESPONSENEGOTIATION_H

#include <string_view>

namespace dssp::http {
    enum class TaskRepresentation {
        Json,
        Ndjson,
        Unacceptable
    };

    // Returns the acceptable task representation with the highest preference.
    TaskRepresentation negotiateTaskResponse(std::string_view accept);
}

#endif // DSSP_HTTP_RESPONSENEGOTIATION_H
