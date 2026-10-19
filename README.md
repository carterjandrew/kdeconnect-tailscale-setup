# KDE Connect Tailscale Setup

**Problem:** [KDE Connect](https://kdeconnect.kde.org/) is *great*, but very often devices cannot find each other on many types of internet (for example on a college campus, in a coffee shop, or 2am in the taco bell parking lot).

**Solution:** Use a one time script to set up KDE connect to use [Tailscale](https://tailscale.com/) so KDE connect works on practially any network, or even when both devices are not on the same network.

This small Bash utility adds a phone's Tailscale IPv4 address to KDE Connect's custom discovery addresses. KDE Connect still performs its normal encrypted pairing; the script only helps the devices discover each other.

## Requirements

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

Run the utility as your normal desktop user. Do not use `sudo`, because that generally cannot access your graphical session's D-Bus.

## Use

> [!WARNING]
> Don't be dumb, READ BASH SCRIPTS FROM THE INTERNET BEFORE YOU RUN THEM!!!
> Also, you will need to do some setup on your phone after running this command. See screenshots below.

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
