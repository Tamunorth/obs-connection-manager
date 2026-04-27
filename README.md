# OBS Connection Manager

Adaptive bitrate manager for OBS Studio. Companion to [obs-squeezeback](https://github.com/Tamunorth/obs-squeezeback).

Watches your upload network during a stream and adjusts encoder bitrate to keep the stream alive when bandwidth degrades. Surfaces a clear alert when the stream is breaking.

A more configurable, more visible alternative to OBS's built-in "Dynamically change bitrate to manage congestion (Beta)" checkbox.

## Status

Early development. Worker A scaffolding only — plugin loads and logs `[connection-manager] loaded`. Adaptive logic, dock UI, settings, and notifications still pending.

See [ROADMAP.md](ROADMAP.md).

## Features (planned)

- Configurable upper / lower bitrate limits, step size, and recovery window
- Dock panel showing live bitrate, congestion, and stream state
- Tools menu dialog for advanced settings
- Red banner inside the dock + Windows desktop toast when the stream is breaking
- Auto-disables OBS's built-in DBR while active (restores on plugin disable)
- Per-stream session log of every adjustment

## Installation

Build the installer with `build_release.bat`, then run `dist\obs-connection-manager-0.1.0-setup.exe`. Restart OBS.

## Build

Requires:
- Visual Studio Build Tools 2022 (C++ workload)
- CMake 3.16+
- Qt6 6.7.0 (`C:\Qt\6.7.0\msvc2019_64`)
- Inno Setup 6 (for the installer step)
- OBS Studio installed (CMake will auto-detect)

```
build_release.bat
```

## License

GPL-2.0
