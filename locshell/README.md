# LOC SHELL

**Looking for instructions rather than build details?** Read the
[simple user guide](../docs/USER-GUIDE.md),
[game installation walkthrough](../docs/INSTALL-GAMES.md), or
[agent guide](../docs/AGENT-GUIDE.md). Initial setup has its own
[step-by-step page](../docs/SETUP.md). This page is the technical build and
installation reference.

LOC SHELL combines a DOS game/application catalogue, the mTCP FTP server,
network controls, screensavers and standalone mTCP utility launchers in one
80×25 interface. FTP continues polling while the user browses and edits; it
pauses when a game, standalone tool or managed foreground job runs. The
supervisor and shell release their memory before a game starts.

## Build

```sh
git submodule update --init --recursive
nix develop path:./nix --command python tools/build.py --locshell
nix develop path:./nix --command python tools/build_suite.py
nix develop path:./nix --command python tools/test_locshell.py
python tools/test_saves.py
```

The pinned toolchain supplies Open Watcom, NASM, Python, patch and DOSBox-X.
The first command builds `build/LOCSHELL.EXE`, `DOSAGENT.EXE`, `LOCHAND.EXE`,
`MEMWATCH.COM`, `MEMLEARN.EXE`, the standalone `DSHELL.EXE` fallback and
`DOSINFO.EXE`, plus DOS-format help and batch wrappers. The second builds the
original mTCP utilities and NetDrive support under `build/suite/`.
Without `--locshell`, `tools/build.py` builds the classic `FTPSRV.EXE` interface.
`DOS_BUILD_DIR` selects another output directory. `DOS_SHELL_SOURCE` can override
the frontend source directory; `DOS_SHELL_TEST=1` enables emulator-only hooks.
Never install a test-hook build on a real machine.

The bundled frontend is in `src/dosshell/`. The network adapter lives in
`../src/APPS/FTPSRV/LOCFTP.H` and `../management/NETVIEWS.H`. The scene build
applies `../third_party/dos-scenes-compact.patch` to a private build copy of
the pinned MIT runtime. This preserves upstream while retaining the tested
single-orientation cloud buffer and XMS music window. Without XMS, music uses
conventional memory. The Watcom heap-walk correction includes its own source,
license and explanation under `../third_party/watcom-heap/`.

## Install layout

For an existing managed installation, install the shell and launch helpers in
`C:\DOSSHELL`, the supervisor in `C:\NET\AGENT`, and standalone network tools
in `C:\NET\BIN`. Keep the existing `C:\NET\MTCP.CFG`, credentials, profile,
packet driver and DOS boot configuration. `LOCSHELL.BAT` starts the combined
application through the supervisor. A compatible Island scene pack belongs in
`C:\ISLAND`; it is separate from the executable and the application falls back
when assets are unavailable. See the root README for scene credits and packing.

Machine catalogues, memory requirements and learned observations are private
installation data. This repository does not include games, saved games or a
user's catalogue. A catalogue entry has a title, optional `---GENRE---`,
`---SERIES---`, `---DEVELOPER---`, `---PUBLISHER---`, `---YEAR---`,
`---STATUS---` and `---FAVORITE---` lines, followed by DOS launch commands.
`---CONFIG---` introduces setup commands. A blank line ends an entry. Unknown
metadata is retained. `LIBRARY.TXT`, `MEMORY.TSV`, `LEARN.DAT`, `RECENT.TXT`
and `DOSSHELL.CFG` must be preserved during upgrades.

## Home dashboard

F2 opens the full-width Home page. It has six recent application/tool shortcuts,
FTP and backup cards, session counters, recent file changes and a wide current
transfer/job panel. Other views keep the blue sidebar. Connections and Active
job remain in the bottom two rows.

History records a validated dispatch when `LOCHAND /ENTER` runs. Rejected
launches, setup, file browsing and reloads do not count as plays. Network tools
have separate identities and reopen their argument dialog; arguments are never
stored in history. Until history fills, favorites with a positive current RAM
fit supply shortcuts. Normal boot and memory checks still gate game launches.
The persisted list holds eight entries; Home displays six.

The transfer panel uses observed byte counts and shows a percentage only when
the total is known. Completed and failed uploads/downloads, plus rename,
delete and directory events remain in session history. `SITE DASHBOARD` exposes
read-only counters. `SITE AGENT` advertises `dashboard=1 recent=1`.
An unchanged dashboard writes no VGA cells; bounded polling updates live data
without taking over a dialog or unfinished edit. Safe automatic reload and
managed job handoffs use the existing supervisor path.

## Backup report

```sh
dosctl saves --vault /path/to/private/vault dashboard --output /tmp/VAULT.STA
dosctl put /tmp/VAULT.STA /DOSSHELL/VAULT.STA
```

The first command opens the vault read-only and checks published snapshot
metadata. The second publishes the report using the normal verified FTP update.
The page rereads the report about every two seconds. It shows backup counts,
last backup time and report generation time. Missing/invalid reports show
unknown status. **Automatic DOS two-way sync remains unfinished; a backup
report does not mean local saves are synced or conflicts checked.**

Read [USERGUIDE.txt](USERGUIDE.txt) for controls, memory markers, recording,
launch recovery and update behavior.

## Validation

`tools/test_locshell.py` uses 48 synthetic entries and authored COM/EXE fixtures
in a 4MB emulated DOS machine. It checks full-width painting, unchanged-frame
writes, navigation/mouse, modal ownership, transfer progress including unknown
and large totals, report validation, boot/RAM guards, bounded recent history,
real helper dispatch/return and exclusion of non-play requests. It requires no
installed games, original game binaries, packet driver or private machine data.
DOSBox runs with dummy video/audio and close confirmation disabled.

Hardware acceptance for the dashboard included verified file readback,
native catalogue/memory checks and Island wake/restoration with FTP returning.
Additional private network tests exercised real emulated Ethernet FTP uploads,
downloads, aborts, file events, active navigation, supervisor jobs and reload
while an edit was open. Those installation-specific fixtures are not shipped.
These checks do not establish gameplay compatibility for every game or replace
physical keyboard/mouse acceptance by the user.
