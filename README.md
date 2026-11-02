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
