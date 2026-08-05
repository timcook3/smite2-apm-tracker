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
| `ServerDetector` | Finds the Smite 2 process and its established server connections |
| `PingMonitor` | Measures RTT to the detected server (TCP handshake probe, ICMP fallback), or to a configured fallback host |
| `OverlayWindow` | Layered (`UpdateLayeredWindow`), topmost, click-through GDI overlay |
| `main` | Wires the modules together and runs the message loop |

## How ping is measured

With `auto_detect_server = true` (default), the tracker finds the running
Smite 2 process (`game_process_name`), reads its established remote
connections from the Windows TCP table, and measures round-trip time to the
actual server endpoint — first with a TCP handshake probe against the
server's own port (needs no privileges and works even when ICMP is
blocked), falling back to an ICMP echo. If the endpoint disappears (match
ended, reconnect), it is re-detected automatically.

Limitation: Windows does not expose remote endpoints of UDP sockets, and
Smite 2's realtime gameplay traffic is UDP. Detection therefore uses the
game's TCP connections, which terminate in the same server infrastructure
and are representative of the route, but may differ slightly from the ping
the game itself reports.

When the game or its connection cannot be detected, the tracker pings the
configured `ping_host` instead and marks the value with a trailing `*` in
the overlay.

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
