# FLAtlas V2

FLAtlas V2 is a C++ rework of the original FLAtlas Python editor for **Freelancer** modding.
The goal is a faster and more native Windows-focused editor for systems, universe data, and related modding workflows.

## Status

This repository is in active development. Expect missing features, rough edges, and occasional breaking changes while the V2 codebase evolves.
The classic Python FLAtlas project remains available separately.

## Features in Scope

- visual system and universe editing
- resource-backed Freelancer modding workflows
- native desktop UI and faster editor interactions
- testable C++ core code

## Project Structure

- `src`: application and core source code
- `tests`: test targets
- `resources`: bundled editor resources
- `guides`: project notes and implementation guidance
- `tools`: helper scripts
- `third_party`: vendored or external dependencies

## Build

Install a C++ toolchain with CMake support, then build with the included script:

```powershell
.\build.cmd
```

You can also configure with CMake directly:

```powershell
cmake -S . -B build
cmake --build build
```

## Run

```powershell
.\launch.cmd
```

## Contributing

Bug reports and focused issue reports are welcome. Please include the affected Freelancer install or mod, the action you tried, and any relevant logs or screenshots.

## License

MIT License. See [LICENSE](LICENSE).
