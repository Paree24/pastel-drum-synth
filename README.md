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
undo, 64 factory kits in editable data files (never compiled in), user kit
save/load, and a full preset browser with search.

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
  `PresetFiles.*`, `dsp/DrumEngine.*`, `dsp/Effects.*`)
- `Factory/*.xml` — the 64 factory kits (data, editable by hand)
- `TestSmoke.cpp` — DSP regression suite (voices, noise, FX, kit recipes)
- `TestE2E.cpp` — processor + APVTS/UI-path suite (plumbing, chains,
  chaos ops, sends, locks)
- `KitProbe.cpp` — renders factory recipes through the real processor,
  prints per-part metrics + writes audition WAVs (dev tool, never shipped)
- `PresetDump.cpp` — recipe → `Factory/*.xml` data-file generator
  (never shipped)
- `UiShot.cpp` — opens the real editor offscreen and writes PNG snapshots
  of every view/tab (dev tool, never shipped)
