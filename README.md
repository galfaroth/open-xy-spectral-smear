# Spectral Smear by Veff

A stereo spectral send effect for Open-XY, ID `spectralsmear`. It analyzes the
input with a real 128-point FFT and reconstructs it with a 64-sample hop and
Hann overlap-add. Spectral memory follows bin phase between frames; neighboring
bins diffuse into each other. This is actual spectral analysis/resynthesis.

| Encoder | Control | Behavior |
| --- | --- | --- |
| E1 | Smear | Temporal spectral memory, from immediate response to a long decaying tail |
| E2 | Spread | Diffusion into neighboring frequency bins |
| E3 | Tone | Increasing high-frequency attenuation |
| E4 | Level | Wet output level |

Try Smear around 75% and Spread around 25% on a short chord or percussion send.
Turn Smear and Spread to zero for the neutral, delayed signal. The stock dry
path remains separate: the effect returns wet audio only. Smear is not a freeze;
its maximum retention is bounded below unity. Reset discards the spectral tail.

## Implementation and limits

At 44.1 kHz, bins are 344.53 Hz apart and latency is exactly 128 samples
(2.90 ms). This deliberately coarse spectrum gives a colored smear rather than
fine pitch separation. Stereo channels keep separate spectral histories while
sharing packed complex transforms. Fixed-point rounding creates very small
cross-channel residue; tests bound it below -96 dBFS. Scratch is 6,176 bytes per
send, initialized on first process after reset. Audio processing allocates nothing.

The original FFT and tables are included here. Regenerate constants with
`python tools/tables.py`. Internal headroom bounds the inverse transform;
restoration and overlap-add saturate at full scale. Extreme settings can clip.

**Experimental CPU load:** the official SDK host estimate is approximately
56,500 Blackfin instructions per block, above its 48,000 high-load threshold.
Several active tracks/notes may cause dropouts. Packing stereo transforms reduced
this from approximately 128,500 without reducing transform resolution. These
are host estimates, not device measurements. Hardware sound, stability and CPU
qualification remain pending; this revision is not catalog-qualified.

## Build and test

Use Open-XY SDK `7fd8fdf1757e11ab6fdca915b09f1773d7611a31` or a compatible revision.
Adjust the SDK path below to your checkout:

```sh
python ../open-xy/sdk.py test --machine .
```

Tests cover FFT bin identity, neutral broadband reconstruction, exact impulse
latency, stereo isolation, spectral decay/diffusion, poisoned scratch, reset,
full-scale controls and missing scratch. UBSan checks and an independent
30,000-block randomized stereo ASan/UBSan review also passed. Effects have no
browser preview. Native compilation needs a synth in the collection with this
SDK revision; include the core `fm2op` machine along with this effect.

Follow the SDK [build guide](https://github.com/etcetc000/open-xy/blob/staging/docs/build.md)
and [hardware profiling guide](https://github.com/etcetc000/open-xy/blob/staging/docs/profiling.md).
Keep stock firmware and all firmware outputs outside checkouts. Install only
when authorized. Test first at low volume, then measure worst-case CPU with the
intended track count before relying on this effect in a performance.

The generated CI template is retained, inactive, at `ci/machine.yml.example`;
move it to `.github/workflows/machine.yml` with workflow-authorized credentials
to enable it. The original design brief is in `docs/DESIGN-BRIEF.md`.
MIT license; see [LICENSE](LICENSE).
