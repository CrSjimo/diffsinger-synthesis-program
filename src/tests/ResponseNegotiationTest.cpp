#include <gtest/gtest.h>

#include <dssphttp/ResponseNegotiation.h>

namespace dssp::tests {
    TEST(ResponseNegotiation, DefaultsAndWildcardsSelectJson) {
        for (const auto accept : {"", " \t", "*/*", "application/*", "application/json", "application/json, application/x-ndjson"}) {
            EXPECT_EQ(http::negotiateTaskResponse(accept), http::TaskRepresentation::Json) << accept;
        }
    }

    TEST(ResponseNegotiation, QualityAndSpecificitySelectTheRepresentation) {
        for (const auto accept : {"application/x-ndjson", "APPLICATION/X-NDJSON", "application/x-ndjson, */*", "application/json;q=0.2, application/x-ndjson;q=0.8", "application/json;q=0, */*;q=1", "application/x-ndjson; charset=\"UTF-8\";q=1"}) {
            EXPECT_EQ(http::negotiateTaskResponse(accept), http::TaskRepresentation::Ndjson) << accept;
        }
        for (const auto accept : {"application/x-ndjson;q=0, */*;q=1", "application/x-ndjson;q=0.3, application/json;q=0.9", "application/*;q=0.8, application/x-ndjson;q=0.1"}) {
            EXPECT_EQ(http::negotiateTaskResponse(accept), http::TaskRepresentation::Json) << accept;
        }
    }

    TEST(ResponseNegotiation, UnsupportedAndExcludedRepresentationsAreUnacceptable) {
        for (const auto accept : {"text/plain", "application/json;q=0, application/x-ndjson;q=0", "*/*;q=0", "application/x-ndjson;q=nan", "application/json;q=2", "application/x-ndjson;charset=latin1", "application/json;profile=\"a,b\""}) {
            EXPECT_EQ(http::negotiateTaskResponse(accept), http::TaskRepresentation::Unacceptable) << accept;
        }
    }
}
