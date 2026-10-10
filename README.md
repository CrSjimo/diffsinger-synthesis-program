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

See [docs/open-api](docs/open-api) for API specs.

## License

GPL-3.0-or-later

See [COPYING](COPYING).
