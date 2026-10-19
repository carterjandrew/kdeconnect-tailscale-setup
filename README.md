# KDE Connect Tailscale Setup

> [!TIP]
> Are you tired of KDE connect being finicky over different networks? Then use this repo! Run it once and expirience superior consistency immediately

`kdeconnect-tailscale` is a small Linux command line interface helper that teaches KDE Connect about Android devices already present in the same Tailscale tailnet. This means it does not embed Tailscale, request an API token, or remain running in the background. It's a one time setup making KDE Connect robust across networks and space, as long as both devices are running Tailscale.

## Requirements

- Linux with a user session D-Bus
- KDE Connect desktop with its LAN backend (link provider) enabled
- Tailscale installed, connected, and available as `tailscale` on `PATH`
- A phone running both Tailscale and KDE Connect
- Qt 6.5 or newer, CMake 3.22 or newer, and a C++20 compiler to build
- Both devices running the kde connect app at the time of setup

The current KDE Connect Android source accepts Tailscale's `100.64.0.0/10` addresses. With an older Android release, also add the desktop's Tailscale IPv4 address using KDE Connect's existing **Add devices by IP** screen.

## Usage

### Building and running

With [Nix](https://nixos.org/download/) and flakes enabled, no clone or local build is required. Run the latest version directly from GitHub on NixOS or any other Linux distribution:

```sh
nix run github:carterjandrew/kdeconnect-tailscale-setup -- peers
```

To build the package locally with Nix:

```sh
git clone https://github.com/carterjandrew/kdeconnect-tailscale-setup.git
cd kdeconnect-tailscale-setup
nix build
./result/bin/kdeconnect-tailscale peers
```

Without Nix clone and build with CMake:

```sh
git clone https://github.com/carterjandrew/kdeconnect-tailscale-setup.git
cd kdeconnect-tailscale-setup
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

If you have compatability or missing dependency issues, check the `flake.nix` and `flake.lock` files to understand the exact requirements.

The resulting executable is `./build/kdeconnect-tailscale`. You can optionally install it with `cmake --install build`; depending on the install prefix.

### CLI usage

The examples below use `kdeconnect-tailscale`. Substitute `nix run github:carterjandrew/kdeconnect-tailscale-setup --` when running from GitHub, or `./build/kdeconnect-tailscale` when using an uninstalled CMake build.

List Android devices visible to the local Tailscale client:

```sh
kdeconnect-tailscale peers
```

Select a device interactively and trigger discovery:

```sh
kdeconnect-tailscale add
```

The selector can instead be an exact Tailscale hostname, DNS name, node key, or IPv4 address:

```sh
kdeconnect-tailscale add my-phone
```

KDE Connect performs its normal TLS pairing after discovery. Approve the pairing request on both devices. If Tailscale is restarted or a peer's address changes, rerun:

```sh
kdeconnect-tailscale repair
```

Other commands are `remove <peer>` and `doctor`. State is stored in `$XDG_CONFIG_HOME/kdeconnect-tailscale/state.json` (normally `~/.config/kdeconnect-tailscale/state.json`). Existing KDE Connect custom addresses not owned by this helper are preserved.

## Development and troubleshooting

Use the reproducible development shell when changing the project:

```sh
nix develop
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

`nix build` builds the complete package and runs its test suite. If a local checkout has untracked files that Nix needs but is nested inside another Git repository, use `nix build path:.` or `nix develop path:.` so Nix includes the working tree by path.

For CMake build failures, start with a fresh build directory, verify that CMake found Qt 6.5 or newer, and rebuild with verbose output:

```sh
cmake -E remove_directory build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --verbose
ctest --test-dir build --output-on-failure
```

For runtime problems, make sure `tailscale status` works for the current user and KDE Connect is running in the same graphical session, then run:

```sh
kdeconnect-tailscale doctor
```

The tool requires a user session D-Bus; running it through `sudo` can therefore make KDE Connect unavailable even when it is running normally.

## Network policy

The tailnet policy and local firewall must permit TCP and UDP ports `1714-1764` in both directions between the selected devices. Tailscale access rules are directional. This utility diagnoses the local integration but deliberately does not modify tailnet policy or firewall rules.

## Limitations

- This version supports Linux KDE and Tailscale IPv4 only.
- It relies on `tailscale status --json`, whose schema Tailscale documents as subject to change.
- It lists peers whose reported operating system is Android. Other KDE Connect targets can still be configured through KDE Connect's normal custom-device mechanism.
- It is a setup and repair utility, not an availability monitor.

## AI usage disclosure

AI-assisted tools have been used in the implementation and documentation of this project. AI-generated suggestions are treated like any other contribution: they should be reviewed, tested, and corrected before release.
