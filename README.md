# AYTHER Tessera

An open-source multi-system emulation core designed for deterministic execution, structured observability, and runtime graphics and audio replacement.

**Status:** project bootstrap. This starter contains the project specification, contribution rules, and implementation roadmap. It does not contain an emulator implementation or a working build yet.

## Purpose

Tessera aims to give applications a stable way to run an emulated system, inspect coherent frame data, and change its presentation without changing the emulated machine's behavior.

The architecture is extensible across console families. Initial implementation focuses on Sega Genesis / Mega Drive, followed by Master System. Other systems and advanced audio replacement are later milestones, subject to independent implementation and validation. See the [system strategy](docs/SYSTEMS.md).

## Planned capabilities

- Deterministic input replay and save-state continuation.
- Versioned capability discovery and explicit buffer ownership.
- Coherent snapshots of supported video, memory, and audio state.
- Frame-boundary transactions for presentation controls.
- Graphics replacement with explicit fallback to original video.
- Optional audio events and output controls.
- Headless tools and a public test corpus using original or authorized content.

These are design goals, not currently implemented features.

## Architecture

The core separates emulation/session management, per-system backend adapters, observation, presentation controls, and persistence. A synthetic backend will establish the contract before integrating a commercially compatible backend or implementing additional emulation components.

The AYTHER authoring application and its UI are consumers of this core. Building an editor is outside the first Tessera core milestone.

See [Architecture](docs/ARCHITECTURE.md), [Roadmap](docs/ROADMAP.md), and [Decisions](docs/DECISIONS.md).

## Getting started

There are no build commands yet because the implementation language, build system, and real backend have not been selected. The first milestone is to record those decisions, create a minimal executable, and publish reproducible build instructions with its tests.

Start with the tasks in [Roadmap](docs/ROADMAP.md). Development changes must include the relevant validation rather than placeholder success commands.

## Licensing and commercial use

Original Tessera code and documentation in this starter are offered under the [MIT License](LICENSE). MIT permits commercial use and redistribution under its terms.

Third-party components retain their own licenses. Backend selection and integration must allow commercial use and satisfy all distribution obligations. See [Licensing policy](legal/LICENSING.md) and the [dependency register](legal/DEPENDENCIES.csv).

The new implementation must not incorporate restricted Genesis Plus GX code without sufficient additional permission. Published source availability does not by itself grant commercial rights.

Games, BIOS files, audio, and graphics have separate rights. Public demos and fixtures must be original or explicitly authorized.

## Contributing

Read [CONTRIBUTING.md](CONTRIBUTING.md). Report reproducible defects and proposals through the issue templates, and include tests and provenance for contributed material.
