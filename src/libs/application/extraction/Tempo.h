#ifndef DSSP_EXTRACTION_TEMPO_H
#define DSSP_EXTRACTION_TEMPO_H
#include <dsspextraction/ExtractorCatalog.h>
#include <dsspresource/TaskExecutor.h>
namespace dssp::extraction {
    void extractTempo(std::shared_ptr<const ExtractorCatalog>, const ExtractorRecord &, std::shared_ptr<resource::TaskExecutor>, Input, std::stop_token, Completion<TempoOutput>);
}
#endif
