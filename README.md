# LoC DOS management

A DOS machine management environment from **LoSs oF CoNtRoL (LoC)**, maintained
by **bstar**, built on **Michael B. Brutman's mTCP**.

This is an independently maintained fork. The official mTCP project is
[mbbrutman/mTCP](https://github.com/mbbrutman/mTCP); its
[home page](http://www.brutman.com/mTCP/mTCP.html) documents the original TCP/IP
library and applications. See [NOTICE.md](NOTICE.md) for credits, licensing and
which parts this fork changes.

## What this fork adds

- A DOS operations dashboard with job status, a memory indicator and an idle
  VGA starfield with a text-mode fallback.
- A foreground job supervisor and host-side `dosctl` CLI for verified FTP
  updates, script execution, result retrieval and recovery after client timeouts.
- Resumable inventory scans with bounded steps, partial results and cancellation
  between steps.
- Live machine identity, deployment and restore journals, and operation history.
- Discoverable diagnostic recipes with gitignored private extensions.
- An experimental serial heartbeat and text-screen observer. It has been tested
  in DOSBox and is not installed automatically or claimed hardware-validated.

The production interface uses FTP and foreground DOS jobs. FTP pauses during a
job and returns when it exits. This is not an interactive SSH service; arbitrary
DOS programs can still require local recovery if they hang.

## Build and use

The pinned Nix environment provides Open Watcom, Python and DOSBox-X:

```sh
git submodule update --init --recursive
nix develop path:./nix --command python tools/build.py
python management/dosctl.py --help
```

See the [management guide](management/README.md) for installation, credentials,
commands, testing, limitations and physical acceptance notes. Builds produce
`FTPSRV.EXE`, `DOSAGENT.EXE` and `DOSINFO.EXE` under `build/`. Existing executable
names, DOS paths and the `dosctl` command remain stable.

Credentials and host-side operational state belong outside Git. Machine-specific
recipes belong in gitignored `custom_recipes/`; public examples must not contain
machine passwords, account data, private inventories or deployment policies.

## Upstream, support and license

We retain mTCP's fork relationship and the `upstream` Git remote so that its
networking fixes remain reviewable. Read the [upstream maintenance policy](docs/UPSTREAM.md).
The original upstream README is preserved [here](docs/upstream/README.md).

Report problems with this fork's additions to
[this repository](https://github.com/bstar/mTCP/issues). Michael Brutman is not
responsible for supporting these changes. Fork development is AI-assisted;
upstream's development statements describe its own project.

mTCP's original attribution and copyright notices remain in place. The GNU GPL
version 3 is included in [LICENSE](LICENSE); consult individual source notices
as well. This fork does not claim to have created mTCP or to be its official
release. See [NOTICE.md](NOTICE.md) for distribution and source requirements.

## Scene screensaver

NETFTP embeds the MIT-licensed [dos-scenes](https://github.com/bstar/dos-scenes)
runtime through a pinned Git submodule under `third_party/dos-scenes`. Scene
rendering, AdLib timing and keyboard volume controls live in that repository;
this fork owns the FTP lifecycle and display restoration. Run
`git submodule update --init --recursive` before building. To test another
checkout, set `DOS_SCENES_SOURCE` to its `src` directory.

Alt-V cycles and saves Starfield / Island / Off in `C:\NET\AGENT\SAVER.CFG`.
Alt-P previews the selection. `DOSAGENT_SAVER=stars|island|off` overrides the
saved selection; `DOSAGENT_IDLE=0` disables activation. Place a scene pack in
`C:\ISLAND` (or override `DA_ISLAND_DIRECTORY` at compile time). The Island slot
accepts any compatible pack, including the original Night Coast demo shipped
with dos-scenes. Missing assets fall back to the existing starfield.

During scene playback, `+` / `-` adjust and remember the AdLib level in the pack's
`VOLUME.CFG`; `M` toggles mute and `N` advances the playlist. Other keys wake the
dashboard. A connected FTP client wakes it immediately. Authenticated clients
with SITE QUIT permission may queue `SITE SAVER PREVIEW island`; it starts after
all clients disconnect and preserves the saved selection. `SITE SAVER` reports
runtime counters, errors, sound-card detection and volume.

Build and exercise the real DOS integration with only redistributable assets:

```sh
nix develop path:./nix --command python tools/build.py
nix develop path:./nix --command python tools/test_island.py
```

The test covers repeated activation, keyboard and FTP wake, text/font/palette/
cursor/IRQ restoration, saved selection, missing assets and every rendered
pixel. It runs as a 386 without an FPU, with 160KB of memory reserved. NETFTP
remains a foreground application; the independent player and optional resident
TSR are provided separately by dos-scenes. No DOS installer is added.

The host remains GPL-3.0-or-later. The runtime's MIT license is retained in its
submodule. Third-party island art and music are not included: see the upstream
[island credits](https://github.com/bstar/dos-scenes/blob/feat/scene-runtime-and-tsr/scenes/island/CREDITS.md)
and extraction instructions before preparing a local pack.
