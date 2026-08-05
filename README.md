# Smite 2 APM Tracker

A lightweight Windows overlay for Smite 2 that shows your live **APM**
(actions per minute, from global keyboard/mouse hooks) and your current
**ping**, rendered as a transparent, click-through, always-on-top window.

Works with Smite 2 in **borderless windowed** mode (recommended). Like all
external overlays, it cannot draw over exclusive fullscreen.

## Modules

| Module | Responsibility |
| --- | --- |
| `Config` | Loads `config.ini` (ping host, intervals, overlay position/size) |
| `InputHook` | Global `WH_KEYBOARD_LL` / `WH_MOUSE_LL` hooks; counts key and mouse-button presses |
| `ApmCalculator` | Thread-safe sliding-window APM computation |
| `PingMonitor` | Background ICMP echo (`IcmpSendEcho`) probing with latest-result cache |
| `OverlayWindow` | Layered (`UpdateLayeredWindow`), topmost, click-through GDI overlay |
| `main` | Wires the modules together and runs the message loop |

## Ping host

Smite 2 does not expose its game-server address in a supported way, so the
tracker pings a configurable host (`ping_host` in `config.ini`). Smite 2
servers run on AWS; set the host to an endpoint in your play region (e.g.
`ec2.us-east-1.amazonaws.com` for NA-East) to get a number representative of
your in-game latency.

## Building

### On Windows (Visual Studio or MinGW)

```
cmake -B build
cmake --build build --config Release
```

### Cross-compiling from Linux (MinGW-w64)

```
sudo apt install g++-mingw-w64-x86-64 cmake
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-toolchain.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The result is a single static `smite2_apm_tracker.exe`. Put `config.ini`
next to it and run it before or during a game session. Close it from Task
Manager or by ending the process (it has no visible window chrome by design).

## Configuration

See the comments in [`config.ini`](config.ini). All keys are optional;
missing or malformed values fall back to safe defaults.

## Anti-cheat note

The tracker only *reads* input globally and never touches the game process,
injects code, or sends input — the same mechanism used by common APM tools.
Still, use third-party tools at your own discretion under Hi-Rez's terms of
service.
