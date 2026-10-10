#ifndef DSSP_EXTRACTION_SEPARATION_H
#define DSSP_EXTRACTION_SEPARATION_H
#include <dsspextraction/ExtractorCatalog.h>
#include <dsspresource/TaskExecutor.h>
namespace dssp::extraction {
    void extractSeparation(std::shared_ptr<const ExtractorCatalog>, const ExtractorRecord &, std::shared_ptr<resource::TaskExecutor>, SeparationInput, std::stop_token, Completion<SeparationOutput>);
}
#endif
