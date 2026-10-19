# KDE Connect Tailscale Setup

**Problem:** [KDE Connect](https://kdeconnect.kde.org/) is *great*, but very often devices cannot find each other on many types of internet (for example on a college campus, in a coffee shop, or like all of my places of employment and my house sometimes).

**Solution:** Use a one time script to set up KDE Connect to use [Tailscale](https://tailscale.com/) so it works on practially any network, or even when both devices are not on the same network.

This small Bash utility adds a phone's Tailscale IPv4 address to KDE Connect's custom discovery addresses. KDE Connect continues to work completely like normal.

## Requirements

This is a bash script, so we rely on some other tools:

- Linux with KDE Connect and a user session D-Bus
- Tailscale running on the computer and phone
- KDE Connect's LAN backend enabled on both devices
- [`jq`](https://jqlang.org/)
- `busctl`, provided by systemd

Check whether the required commands are available:

```bash
for command in bash tailscale jq busctl; do
    command -v "$command" >/dev/null || printf 'Missing: %s\n' "$command"
done
```

Do not use `sudo` to run this because that generally cannot access your graphical session's D-Bus.

## Use

> [!WARNING]
> Don't be dumb, READ BASH SCRIPTS FROM THE INTERNET BEFORE YOU RUN THEM!!!
> 
> **Also, you will need to do some setup on your phone after running this command. See screenshots below.**

The *safest* approach is to download and inspect the script before running it:

```bash
curl -fsSLO https://raw.githubusercontent.com/carterjandrew/kdeconnect-tailscale-setup/main/kdeconnect-tailscale
less kdeconnect-tailscale
```

And then run:

```bash
bash kdeconnect-tailscale
```

Or, if you really want to, you can run it all in one command:

```bash
curl -fsSL https://raw.githubusercontent.com/carterjandrew/kdeconnect-tailscale-setup/main/kdeconnect-tailscale | bash
```

## Commands

> [!NOTE]
> To pass a command through the one-line download form, put arguments after `bash -s --`:
> 
> ```bash
> curl -fsSL https://raw.githubusercontent.com/carterjandrew/kdeconnect-tailscale-setup/main/kdeconnect-tailscale \
>     | bash -s -- add my-phone
> ```


Running without a command is the same as `add`: it lists Android peers in your tailnet and asks which one to configure.

```bash
bash kdeconnect-tailscale
```

List peers:

```bash
bash kdeconnect-tailscale peers
```

Add a peer interactively:

```bash
bash kdeconnect-tailscale add
bash kdeconnect-tailscale add my-phone
```

Repair an obsolete custom address by selecting the existing address and its current Tailscale peer:

```bash
bash kdeconnect-tailscale repair
```

The non-interactive form takes the exact old address followed by the Tailscale peer selector:

```bash
bash kdeconnect-tailscale repair 100.70.1.1 my-phone
```

After running `add` you will need to manually set the IP address on your phone as well:

## Phone Setup

### 1. Get your computers Tailnet IP Address

You can do this using
```sh
tailscale ip -4
```

<img width="397" height="68" alt="Get Tailscale IP" src="https://github.com/user-attachments/assets/db02d03b-6b83-4766-8023-013573e4534c" />

### 2. On your phone, open kde connect and click "Pair new device" from the side panel

<img style="height: 70vh;" alt="Open Pair Devices" src="https://github.com/user-attachments/assets/fcf943c0-7556-4e22-bd4f-a2fa244ac59e" />

### 3. Use the hamburger icon in the top left to select "Add devices by IP"

<img style="height: 70vh;" alt="Open IP List" src="https://github.com/user-attachments/assets/3b4a6e58-d3dd-4055-95a3-f918d81c322b" />

### 4. Type in that Tailnet IP address

<img style="height: 70vh;" alt="Enter Tailscale IP Address" src="https://github.com/user-attachments/assets/2b597d27-1996-4ad7-90f9-f0d9d32da675" />

### 5. Now pair the device from your computer

This does not seem to get the request through correctly from phones for some reason

<img style="height: 70vh;" alt="Accept connection" src="https://github.com/user-attachments/assets/919d14ef-b68b-4d3a-b776-1a85b3522e7b" />

Now both devices know to connect to each other over Tailscales Tailnet instead of LAN internet discovery.

## Network policy

The tailnet policy and local firewall must permit TCP and UDP ports `1714-1764` in both directions between the devices. Tailscale access rules are directional. This utility checks its immediate dependencies and reports command failures, but it does not modify tailnet policy or firewall rules.

## Limitations

- Linux systems with systemd's `busctl` are supported.
- Only Android Tailscale peers are listed.
- Only Tailscale IPv4 addresses in `100.64.0.0/10` are used.
- The script relies on `tailscale status --json`, whose schema may change.
- This is an initial setup and repair utility, not an availability monitor.

## AI usage disclosure

AI-assisted tools have been used in the implementation and documentation of this project. AI-generated suggestions are treated like any other contribution: they should be reviewed, tested, and corrected before release.
