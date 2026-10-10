#ifndef DSSP_API_V1_CONTROLLERS_EXTRACTIONMAPPING_H
#define DSSP_API_V1_CONTROLLERS_EXTRACTIONMAPPING_H

#include <dsspapiv1/Requests.h>
#include <dsspapiv1/Responses.h>
#include <dsspextraction/Types.h>

namespace dssp::api_v1 {
    extraction::Input toExtraction(dto::ExtractionRequest);
    extraction::SeparationInput toExtraction(dto::SeparationExtractionRequest);
    dto::ExtractorInfo toDto(extraction::ExtractorInfo);
    dto::SeparationExtractorInfo toSeparationDto(extraction::ExtractorInfo);
    dto::ExtractorList toDto(extraction::ExtractorList);
    dto::NotesOutput<dto::ExtractedNote> toDto(extraction::NoteOutput);
    dto::PitchExtractionOutput toDto(extraction::PitchOutput);
    dto::TempoExtractionOutput toDto(extraction::TempoOutput);
    dto::SeparationExtractionOutput toDto(extraction::SeparationOutput);
}

#endif // DSSP_API_V1_CONTROLLERS_EXTRACTIONMAPPING_H
