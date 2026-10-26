# Dialogue Boost

VST3 plugin for DaVinci Resolve (works in any VST3 host). Louder, clearer dialogue without clipping.

**Chain:** 80 Hz low-cut → dialogue EQ (mud cut, presence and air lift) → gentle 2:1 compressor → Boost → look-ahead limiter.

| Control | Range | Default |
|---|---|---|
| Boost | 0 to +12 dB | +6 dB |
| Clarity (EQ amount) | 0–200 % | 100 % |
| Ceiling (max peak) | −6 to 0 dB | −1.5 dB |

Keep the **Limiting** meter under ~6 dB; lower Boost if it turns red.

## Install

Download from [Releases](../../releases).

- **Windows:** run `DialogueBoost-1.0.0-Windows-Setup.exe`. Installs to `C:\Program Files\Common Files\VST3`.
- **Mac:** right-click `DialogueBoost-1.0.0-Mac-Installer.pkg` → **Open** (the installer is not notarized). Installs to `/Library/Audio/Plug-Ins/VST3`. Apple Silicon and Intel.

Restart Resolve, then: **Fairlight → Mixer → Effects + → Dynamics → VST → Dialogue Boost**.

## Build

- Windows (cross-compiled on Linux): `./build.sh`, then `makensis installer/win/DialogueBoost.nsi`
- Mac: `./build_mac.sh`

Both need the [VST3 SDK](https://github.com/steinbergmedia/vst3sdk) (MIT) cloned into `vst3sdk/`. CI builds both on every push to `main`.
