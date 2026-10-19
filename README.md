# KDE Connect Tailscale Setup

`kdeconnect-tailscale` is a small Linux helper that teaches KDE Connect about Android devices already present in the same Tailscale tailnet. It reuses KDE Connect's existing LAN transport, TLS pairing, and `customDevices` setting. It does not embed Tailscale, request an API token, or remain running in the background.

## Requirements

- Linux with a user session D-Bus
- KDE Connect desktop with its LAN link provider enabled
- Tailscale installed, connected, and available as `tailscale` on `PATH`
- An Android device running Tailscale and KDE Connect
- Qt 6.5 or newer, CMake 3.22 or newer, and a C++20 compiler to build

The current KDE Connect Android source accepts Tailscale's `100.64.0.0/10` addresses. With an older Android release, also add the desktop's Tailscale IPv4 address using KDE Connect's existing **Add devices by IP** screen.

## Build

On NixOS, enter the reproducible development shell and build:

```sh
nix develop path:.
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

The complete package and test suite can also be built with `nix build path:.`. The explicit `path:` form also works while this new directory is still untracked in its parent Git repository. After it is committed, ordinary `nix develop`, `nix build`, and `nix run` work as well.

On a system with Qt and CMake already installed:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

## Use

List Android devices visible to the local Tailscale client:

```sh
./build/kdeconnect-tailscale peers
```

Select a device interactively and trigger discovery:

```sh
./build/kdeconnect-tailscale add
```

The selector can instead be an exact Tailscale hostname, DNS name, node key, or IPv4 address:

```sh
./build/kdeconnect-tailscale add my-phone
```

KDE Connect performs its normal TLS pairing after discovery. Approve the pairing request on both devices. If Tailscale is restarted or a peer's address changes, rerun:

```sh
./build/kdeconnect-tailscale repair
```

Other commands are `remove <peer>` and `doctor`. State is stored in `$XDG_CONFIG_HOME/kdeconnect-tailscale/state.json` (normally `~/.config/kdeconnect-tailscale/state.json`). Existing KDE Connect custom addresses not owned by this helper are preserved.

## Network policy

The tailnet policy and local firewall must permit TCP and UDP ports `1714-1764` in both directions between the selected devices. Tailscale access rules are directional. This utility diagnoses the local integration but deliberately does not modify tailnet policy or firewall rules.

## Limitations

- The initial version supports Linux KDE and Tailscale IPv4 only.
- It relies on `tailscale status --json`, whose schema Tailscale documents as subject to change.
- It lists peers whose reported operating system is Android. Other KDE Connect targets can still be configured through KDE Connect's normal custom-device mechanism.
- It is a setup and repair utility, not an availability monitor.
