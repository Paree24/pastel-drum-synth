# Pastel Drum Synth — 12-Part Drum Synthesizer (VST3 + Standalone)

> **Disclaimer:** this project is vibe-coded for personal use. It is provided
> as-is, without warranty of any kind. The author is not responsible for
> anything — Use at your own risk.

Pastel Drum Synth is a 12-part analog-style drum synthesizer (kick, snare,
closed/open hats, ride, crash, 3 toms, membrane, metallic, SFX) with
per-part synth voices, per-part FX chains plus a global chain (EQ,
compressor, transient shaper, distortion, filter, delay, reverb), shared
send reverb/delay returns, part mixer (level, pan, mute/solo, choke groups),
part/kit randomization and identity-preserving mutation with locks and
undo, 80 factory kits in editable data files (never compiled in), user kit
save/load, and a full preset browser with search.

### Engine models, REVERSE and multi-out

Every part has a **MODEL** selector next to its name. **CLASSIC** is the
original voice, untouched (factory kits render bit-identically). The other
models are real-time synthesis engines, each with its own knobs on the SYNTH
tab:

| Part | Models |
|---|---|
| Kick | PUNCH, RUMBLE, BOOM |
| Snare | LAYERED, SWEPT, DRUM MACHINE, CLAP |
| Closed / open hat | FM METAL, SHAKER, TAMBOURINE |
| Ride | CLOUD, BELL |
| Crash | CLOUD, CLOUD DARK |
| Toms | MEMBRANE, CONGA |
| Membrane perc | CONGA, BONGO, RIM, WOOD, GLASS, SNAP, CLAP |
| Metallic perc | CLANG, COWBELL, AGOGO, TAMBOURINE, GLASS |
| SFX | RISER, IMPACT, LASER, SIREN, SUB DROP, VOX, ZAP, NOISE HIT, AIR HORN, BOOM |

Factory kits 65–80 are built on the models. Between them they use every
model, and some include REVERSE parts (Mirror Room, Ghost Swell). Choosing a
model loads its default settings (undoable). The defaults peak at
about the same level as the part's CLASSIC voice. With a model selected,
the AMP tab keeps only PAN (the model shapes its own envelope) and the
PITCH tab, WAVE and NOISE are hidden. Part TUNE, LEVEL, FX chains and sends
work with every model.

- **RANDOMIZE** (part or kit) may switch the model. **MUTATE** never does;
  it only nudges the current model's knobs. Neither one touches a model's
  envelope knobs (DECAY, LENGTH, TAIL, ATTACK, HOLD, GATE) or VEL>AMP.
  Right-click MODEL (or any model knob) to lock the model and its knobs.
- **REVERSE** plays the part backwards. It works with every model and with
  CLASSIC. The reversed hit is re-rendered in the background whenever the
  sound changes.
- **OUT** routes the part to MAIN or to one of 12 extra stereo outputs
  (OUT 2 to OUT 13). Enable the extra outputs in your host. A part routed to
  a disabled output plays on MAIN. An extra output carries the part after
  its own FX chain and the master level. It skips the global chain and the
  limiter. The part's reverb and delay sends still feed the shared returns,
  which play on MAIN.

## License

GPL version 3 — see [LICENSE](LICENSE). This also satisfies the JUCE
framework's licensing terms for this project.

## Building

Requirements: CMake 3.22+, a C++17 compiler, Ninja (or Make), Git (JUCE is
fetched automatically), and JUCE's Linux dependencies (ALSA, X11, freetype,
etc.) — see the JUCE docs for the full list.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Outputs:

- VST3: `build/ModernDrumSynth_artefacts/Release/VST3/Pastel Drum Synth.vst3`
  (+ factory kits under `Contents/Resources/Factory/`)
- Standalone: `build/ModernDrumSynth_artefacts/Release/Standalone/Pastel Drum Synth`
- Headless self-tests: `build/SmokeTest` (DSP regression suite) and
  `build/E2ETest` (real processor + APVTS/UI-path suite; exit 0 = all good)

Install the VST3 by copying `Pastel Drum Synth.vst3` to your plugin folder:

| OS      | VST3 folder                          | Notes                                        |
|---------|--------------------------------------|----------------------------------------------|
| Linux   | `~/.vst3/`                           | Rescan plugins in your DAW afterwards        |
| macOS   | `~/Library/Audio/Plug-Ins/VST3/`     | Unsigned build: right-click → Open once      |
| Windows | `C:\Program Files\Common Files\VST3\`| Rescan plugins in your DAW afterwards        |

User presets live in `Documents/Pastel Drum Synth Presets/` (created on
first save); factory overwrites shadow shipped files from
`Documents/Pastel Drum Synth Presets/Factory/` without ever touching the
install bundle.

### OS-specific notes

> Only the Linux build has actually been compiled and tested here. The macOS
> and Windows steps below follow the standard JUCE/CMake flow and *should*
> work, but if you hit an OS-specific snag, please file an issue with the
> failing command and its full output.

#### Linux (verified)

1. Install dependencies (Debian/Ubuntu shown; Fedora/Arch: equivalent
   `-devel` packages):
   ```sh
   sudo apt install build-essential cmake ninja-build git pkg-config \
     libasound2-dev libx11-dev libxcomposite-dev libxcursor-dev \
     libxinerama-dev libxrandr-dev libfreetype6-dev libfontconfig1-dev \
     libcurl4-openssl-dev libwebkit2gtk-4.1-dev
   ```
2. Configure + build (JUCE is fetched automatically — needs network access):
   ```sh
   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
   cmake --build build --config Release
   ```
3. Run the self-tests (must print `ALL SMOKE TESTS PASSED` /
   `ALL E2E TESTS PASSED`):
   ```sh
   ./build/SmokeTest
   xvfb-run -a ./build/E2ETest
   ```
4. Install: copy
   `build/ModernDrumSynth_artefacts/Release/VST3/Pastel Drum Synth.vst3` to
   `~/.vst3/` (factory kits travel inside the bundle), then rescan plugins
   in your DAW.

#### macOS (not yet built here)

1. Install Xcode command-line tools, CMake, Ninja and Git:
   ```sh
   xcode-select --install
   brew install cmake ninja git
   ```
2. Same configure + build as Linux. For a universal (Intel + Apple Silicon)
   binary, add `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"` to the configure step.
3. Run `./build/SmokeTest` and `./build/E2ETest` — expect `ALL ... PASSED`.
4. Install: copy `Pastel Drum Synth.vst3` to
   `~/Library/Audio/Plug-Ins/VST3/`. These are unsigned personal builds: if
   macOS refuses to load them, ad-hoc sign with
   `codesign --force --deep -s - <path>` and/or strip the quarantine flag
   with `xattr -dr com.apple.quarantine <path>`. Codesigning/notarisation for
   distribution is out of scope for this project.

#### Windows (not yet built here)

1. Install Visual Studio 2022 with the **Desktop development with C++**
   workload (MSVC v143 or newer), plus CMake, Ninja and Git
   (`winget install Kitware.CMake Ninja-build.Ninja Git.Git` works).
2. Open an **x64 Native Tools Command Prompt** (so MSVC is on `PATH`), then
   the same configure + build commands as Linux. Alternatively use the
   Visual Studio generator instead of Ninja:
   ```bat
   cmake -S . -B build -G "Visual Studio 17 2022" -A x64
   cmake --build build --config Release
   ```
3. Run `build\SmokeTest.exe` and `build\E2ETest.exe` — expect
   `ALL ... PASSED`.
4. Install: copy `Pastel Drum Synth.vst3` to
   `C:\Program Files\Common Files\VST3\`, then rescan plugins in your DAW.
   No ASIO SDK or extra setup required.

## Repository layout

- `Source/` — plugin DSP, UI, preset backend
  (`PluginProcessor.*`, `PluginEditor.*`, `PresetBrowser.*`, `Presets.*`,
  `PresetFiles.*`, `dsp/DrumEngine.*`, `dsp/Models.*`, `dsp/Effects.*`)
- `Factory/*.xml` — the 80 factory kits (data, editable by hand; 65–80 use the engine models)
- `TestSmoke.cpp` — DSP regression suite (voices, noise, FX, kit recipes)
- `TestE2E.cpp` — processor + APVTS/UI-path suite (plumbing, chains,
  chaos ops, sends, locks)
- `KitProbe.cpp` — renders factory recipes through the real processor,
  prints per-part metrics + writes audition WAVs (dev tool, never shipped)
- `PresetDump.cpp` — recipe → `Factory/*.xml` data-file generator
  (never shipped)
- `ModelProbe.cpp` — renders every engine model to WAV and prints peak /
  length / RMS (dev tool, never shipped)
- `RegressRender.cpp` — hashes the 64 classic kits × 12 parts; diff against a saved
  run to prove CLASSIC output is unchanged (dev tool, never shipped)
- `UiShot.cpp` — opens the real editor offscreen and writes PNG snapshots
  of every view/tab (dev tool, never shipped)
