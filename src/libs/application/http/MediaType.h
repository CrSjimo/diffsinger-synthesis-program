#ifndef DSSP_HTTP_MEDIATYPE_H
#define DSSP_HTTP_MEDIATYPE_H

#include <expected>
#include <string_view>

#include <dssphttp/HttpErrors.h>

namespace dssp::http {
    // Negotiation fields contain only type/subtype, without parameters or wildcards.
    std::expected<bool, HttpBoundaryError> matchesMediaTypeName(std::string_view text);
    // HTTP Content-Type permits parameters and optional whitespace.
    std::expected<bool, HttpBoundaryError> matchesJsonContentType(std::string_view text);
}

#endif // DSSP_HTTP_MEDIATYPE_H
