# LOC SHELL

**LOC SHELL** is a DOS game and application launcher from **LoSs oF CoNtRoL
(LoC)**, maintained by **bstar**. It combines a memory-aware catalogue, a
full-width Home dashboard, FTP management, network tools and screensavers.

The standalone project is [bstar/LOCSHELL](https://github.com/bstar/LOCSHELL).
Its networking foundation is **Michael B. Brutman's mTCP**. Original mTCP
source, copyright notices, licenses and Git history are retained. See
[NOTICE.md](NOTICE.md) for provenance and the
[official mTCP project](https://github.com/mbbrutman/mTCP) for upstream.

## Features

- A game/application catalogue with boot/RAM checks, learned memory profiles
  and recent launch shortcuts.
- A full-width Home dashboard with live FTP transfers, file changes, active
  jobs and backup-vault reports.
- Integrated FTP controls and standalone mTCP tools in the LOC SHELL interface.
- Starfield, 387 Sculpture, progressive Mandelbrot and Island AdLib screensavers.
- A foreground job supervisor and host-side `dosctl` CLI for verified FTP
  updates, script execution, result retrieval and timeout recovery.
- Resumable inventories, machine identity, deployment/restore journals,
  operation history and discoverable diagnostic recipes.
- An experimental serial heartbeat and text-screen observer, tested in DOSBox
  and not installed automatically or claimed hardware-validated.

The production interface uses FTP and foreground DOS jobs. FTP pauses during a
job and returns when it exits. This is not an interactive SSH service; arbitrary
DOS programs can still require local recovery if they hang.

## Build and use

The pinned Nix environment provides Open Watcom, NASM, Python and DOSBox-X:

```sh
git submodule update --init --recursive
nix develop path:./nix --command python tools/build.py --locshell
nix develop path:./nix --command python tools/build_suite.py
nix develop path:./nix --command python tools/test_locshell.py
python management/dosctl.py --help
```

See the [management guide](management/README.md) for installation, credentials,
commands, testing, limitations and physical acceptance notes. The
[LOC SHELL guide](locshell/README.md) describes the full install layout and
headless acceptance tests. Builds produce `LOCSHELL.EXE`, its supervisor,
launch/memory helpers and batch wrappers under `build/`; standalone mTCP
utilities are in `build/suite/`. The classic `FTPSRV.EXE` build remains available
by running `tools/build.py` without `--locshell`.

Home shows backup metadata from the save vault. Automatic DOS two-way save sync
remains unfinished; backup availability does not mean local saves are synced.

Credentials and host-side operational state belong outside Git. Machine-specific
recipes belong in gitignored `custom_recipes/`; public examples must not contain
machine passwords, account data, private inventories or deployment policies.

## Upstream, support and license

LOC SHELL is hosted as a standalone repository. The `upstream` Git remote still
points to mTCP so networking fixes remain reviewable. Read the
[upstream maintenance policy](docs/UPSTREAM.md).
The original upstream README is preserved [here](docs/upstream/README.md).

Report LOC SHELL issues to
[this repository](https://github.com/bstar/LOCSHELL/issues). Michael Brutman is not
responsible for supporting these changes. LOC SHELL development is AI-assisted;
upstream's development statements describe its own project.

mTCP's original attribution and copyright notices remain in place. The GNU GPL
version 3 is included in [LICENSE](LICENSE); consult individual source notices
as well. LOC SHELL does not claim to have created mTCP or to be its official
release. See [NOTICE.md](NOTICE.md) for distribution and source requirements.

## Scene screensaver

NETFTP embeds the MIT-licensed [dos-scenes](https://github.com/bstar/dos-scenes)
runtime through a pinned Git submodule under `third_party/dos-scenes`. Scene
rendering, AdLib timing and keyboard volume controls live in that repository;
LOC SHELL owns the FTP lifecycle and display restoration. Run
`git submodule update --init --recursive` before building. To test another
checkout, set `DOS_SCENES_SOURCE` to its `src` directory.

The classic server also supports 387 Sculpture and progressive Mandelbrot savers when a 386/387 is available.
Alt-V cycles and saves Starfield / 387 Sculpture / 387 Mandelbrot / Island / Off in `C:\NET\AGENT\SAVER.CFG`.
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
runtime counters, errors, sound-card detection, volume and the loaded twinkle
record count.

Build and exercise the real DOS integration with the bundled scene assets:

```sh
nix develop path:./nix --command python tools/build.py
nix develop path:./nix --command python tools/test_island.py
```

The test uses the bundled island artwork, music and twinkles. It covers repeated activation, keyboard and FTP wake, text/font/palette/
cursor/IRQ restoration, saved selection, missing assets and every rendered
pixel. It runs as a 386 without an FPU, with 160KB of memory reserved. NETFTP
remains a foreground application; the independent player and optional resident
TSR are provided separately by dos-scenes. No DOS installer is added.

The host remains GPL-3.0-or-later. The runtime's MIT license is retained in its
submodule. The bundled island pack retains its original [artwork, concept and music
credits](https://github.com/bstar/dos-scenes/blob/main/scenes/island/CREDITS.md).
Build it with `python third_party/dos-scenes/tools/pack_island.py` and copy the
resulting scene files into `C:\ISLAND`, preserving existing volume settings.
