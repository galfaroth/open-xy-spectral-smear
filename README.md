# Spectral Smear for Open-XY

**Status: planning only.** This repository currently contains a design brief, not an implemented or installable effect. No DSP, firmware, presets or measured hardware results are included.

## Goal

Diffuse sound over time and frequency to turn sharp events into sustained, blurred textures. Intended for pads, transitions and atmospheric processing.

## Proposed MVP signal flow

Stereo input → windowed spectral analysis → temporal magnitude smoothing / bounded bin spreading → phase-aware resynthesis → overlap-add wet return.

The first implementation must choose and document a fixed transform size, hop size, latency and buffer layout. Prototype the spectral path and measure its cost before committing to an FFT implementation on Blackfin. Do not substitute a conventional reverb and label it spectral processing. Freeze and more elaborate phase modes are follow-up work.

## Proposed controls

| Primary | Purpose |
|---|---|
| Smear | Persistence across analysis frames |
| Spread | Frequency-bin spreading |
| Tone | Spectral emphasis / damping |
| Level | Wet output level |

Keep the initial control set small. A later freeze mode should have a defined entry/exit crossfade.

Control assignments are a starting design, not a published compatibility contract. Provide at least a neutral/default sound and a few useful settings when implementation begins.

## Routing and resources

A wet send is the initial routing. Use bounded SDK scratch memory, with a complete budget for input history, windows, transform workspace, spectral state and overlap-add output. Respect the SDK fixed-point constraints. Avoid concentrating an entire expensive transform into one audio block unless measured timing proves it safe.

Target a standalone Open-XY `effect` machine. The SDK supplies the adapters; this repository owns the DSP, controls, artwork and sound tests. SDK changes, if needed, belong in separate upstream PRs.

The current SDK processes stereo Q31 blocks of 64 samples at 44.1 kHz. Unbuffered state is at most 476 bytes; buffered state at most 472 bytes, with an additional reduction when using a shift row. Audio callbacks cannot allocate, block, perform I/O, log or draw. Do not assume a host estimate proves device timing.

## Implementation handoff

1. Start from a current SDK checkout beside this repository and read its authoring/UX instructions.
2. Generate the official effect starter into a fresh sibling directory, then bring its scaffold into this repository while preserving this brief:
   ```sh
   python3 ../open-xy/sdk.py new --repository --kind effect --id spectralsmear --name "Spectral Smear" --out ../open-xy-spectral-smear-starter
   ```
3. Implement the smallest signal path, controls and default settings; keep firmware addresses and platform code out of the effect.
4. Run the machine's tests with `python3 ../open-xy/sdk.py test --machine .` once the scaffold is present. Follow the generated README if its layout changes.
5. Measure memory and worst-case device CPU, then test an explicitly authorized development build before catalog submission.

No catalog entry or upstream PR is part of this planning repository. No firmware or stock content may be committed here.

## Validation plan

Neutral analysis/resynthesis error and latency; impulse response; overlap-add gain; sine-bin leakage; silence and DC; smoothing decay; extreme spectral spreads; reset/uninitialized-memory behavior; measured worst-case block time.

Host numerical tests, native build verification, listening tests and revision-bound hardware CPU measurements are separate requirements. No browser audition support is assumed for effects.

## SDK reference

Planning checked against Open-XY staging revision [`64bd090`](https://github.com/etcetc000/open-xy/tree/64bd0900a08a134e274a8b8decbc8b734ca534dd) on 2026-11-01:

- [Effect API and scratch buffers](https://github.com/etcetc000/open-xy/blob/64bd0900a08a134e274a8b8decbc8b734ca534dd/docs/api.md#effects)
- [Authoring journey](https://github.com/etcetc000/open-xy/blob/64bd0900a08a134e274a8b8decbc8b734ca534dd/docs/first-machine.md)
- [UX conventions](https://github.com/etcetc000/open-xy/blob/64bd0900a08a134e274a8b8decbc8b734ca534dd/docs/ux.md)
- [Hardware profiling](https://github.com/etcetc000/open-xy/blob/64bd0900a08a134e274a8b8decbc8b734ca534dd/docs/profiling.md)

## Next step

Assign a dedicated implementation agent after the brief is reviewed. Do not start other effects or modify the SDK as part of this repository's initial implementation.
