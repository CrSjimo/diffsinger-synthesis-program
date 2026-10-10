# Demucs source layout and export mapping

The ONNX neural core produces two tensors with the same source axis:
`spectrum [1, S, 4, 2048, 336]` and `waveform_sources [1, S, 2, 343980]`.
`configuration.modelTracks` names that axis in order. Supported layouts are
`["drums", "bass", "other", "vocals"]` and
`["drums", "bass", "other", "vocals", "guitar", "piano"]`.

`exports.tracks` describes only the public results, in result order. Each
`configuration.trackMapping` key is an exported track ID; its string value
selects a source from `modelTracks`. Mapping keys must exactly match the exported
IDs, and every value must exist in the declared model layout. Object key order
does not affect result order. A mapping can select a subset, change order, or
give a source a different public ID.

For example, the FT vocals model still computes four neural sources but exports
only its specialized vocals track:

```json
{
  "exports": {
    "sampleRate": 44100,
    "channelCount": 2,
    "maxSegmentDuration": 0,
    "tracks": [{"id": "vocals", "description": "Singing voices"}]
  },
  "configuration": {
    "model": "../models/htdemucs_ft_vocals.onnx",
    "segmentSamples": 343980,
    "fftSize": 4096,
    "hopLength": 1024,
    "overlap": 0.25,
    "modelTracks": ["drums", "bass", "other", "vocals"],
    "trackMapping": {"vocals": "vocals"}
  }
}
```

Both neural branches use the mapping. Only selected frequency sources undergo
inverse STFT, and only exported tracks are accumulated, denormalized, and
returned. The original ONNX source count is still checked against `modelTracks`.
Selecting tracks does not reduce the neural core's own work or change its weights.

Declare `modelTracks` and `trackMapping` together. For compatibility, both may be
omitted only when `exports.tracks` contains the complete official ordered four or
six sources; the interpreter then uses identity mapping. A subset or reordered
export requires an explicit mapping. Invalid declarations fail during package
loading, before a model session is opened.

The machine-readable configuration contract is
[`schemas/demucs-configuration.schema.json`](schemas/demucs-configuration.schema.json).
Cross-field constraints described above are validated by the interpreter.
