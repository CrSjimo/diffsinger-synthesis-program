# DSSP — DiffSinger Synthesis Program

DiffSinger Synthesis Program is a C++23 service that provides DiffSinger singing voice synthesis and audio analysis through the DiffScope Synthesis Platform API.

## Features

- DiffSinger singing voice synthesis, phoneme conversion, and duration prediction.
- Pitch and note extraction, audio slicing, and source separation.
- Inference device discovery, with CPU, CUDA, DirectML, or CoreML support depending on the runtime environment.

## Build

Requires a C++23 compiler and CMake 3.19 or later. Dependencies are managed with vcpkg. The manifest is located at `scripts/vcpkg-manifest/vcpkg.json`, with custom ports in `scripts/vcpkg-overrides` and `scripts/vcpkg/ports`.

After initializing submodules and configuring dependencies and the CMake toolchain, run:

```sh
cmake -S . -B build
cmake --build build --config Release
cmake --install build --config Release --prefix installed
```

## Usage

Prepare compatible model packages, then run the following commands from the installation's `bin` directory:

```sh
dssp dump-devices
dssp serve --package-dir /path/to/packages --execution-provider cpu
```

The server listens on `127.0.0.1:13711` by default. Use `--host` and `--port` to change the listening address, or run `dssp serve --help` for more options. Model packages must be obtained separately.

Logs are written to stderr. `--log-mode normal` (the default) uses the console format with colors where supported. Use `--log-mode structured` for one compact JSON object per line without colors:

```sh
dssp serve --log-mode structured --package-dir /path/to/packages --execution-provider cpu
```

Each JSON record contains `timestamp` (integer milliseconds since the Unix epoch), `timezone_offset` (integer seconds of local time minus UTC, including daylight saving time; UTC+8 is `28800`, UTC−5 is `-18000`), `module` (log category), `level` (uppercase severity), and `message`. Newlines and control characters in messages are JSON-escaped. Both modes support `--log-level`, `--verbose`, and `--quiet`.

See [docs/open-api](docs/open-api) for API specs.

## License

GPL-3.0-or-later

See [COPYING](COPYING).
