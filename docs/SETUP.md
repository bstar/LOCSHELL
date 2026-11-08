# First-time setup: get LOC SHELL running

[Simple user guide](USER-GUIDE.md) · [Agent guide](AGENT-GUIDE.md) · [Build reference](../locshell/README.md)

**Do this once. If LOC SHELL already starts and says FTP Running, skip this
page and go to the user guide.**

This page is for the person preparing the PC. Starting with a bare DOS machine
requires its network card's correct DOS driver and working network settings.
LOC SHELL cannot receive its own initial setup before those are in place.

## 1. Get the right files

Get a built DOS package from the person supplying your installation, or build
and package the source as described below. The ZIP contains one `LOCSHELL` folder,
including network tools, helpers and the Island pack. Extract that folder to C:. **GitHub's Code → Download ZIP downloads source
code; it is not a ready-to-run DOS installer.** The repository has no automatic
first-time installer that detects every network card, game or boot arrangement.

Use a production build. A build with emulator test hooks enabled is for tests.
Games, the owner's game list and private credentials are not part of this
source distribution. The package command fetches/prepares the pinned Island pack
automatically; use `--without-island` to omit it.

## 2. Prepare the DOS PC and network

The setup person needs to establish:

1. A DOS installation and the machine's working boot/memory configuration.
2. A working network card, its DOS packet driver, and a connected local network.
3. `C:\LOCSHELL\NET\MTCP.CFG` with settings appropriate for that driver/network, including
   the FTP account-file setting `FTPSRV_PASSWORD_FILE`.
4. An FTP account with a private password and the intended root directory.
5. A way to put the initial files on the disk: local media/card transfer or an
   already working FTP connection.

Use the matching mTCP documentation and the actual card's instructions for
network setup. Preserve the card's known I/O address, interrupt and any ROM or
memory-manager exclusions. There is no universal driver command for all cards.

Ordinary uploading needs suitable write/directory permissions. Managed jobs and
verified replacement also need the appropriate rename/read permissions; job
handoff requires mTCP's `SITEQUIT` permission. These are powerful maintenance
credentials, not an anonymous public download account. Restrict the service to
your trusted local network; this FTP service does not encrypt traffic.

The guides assume the managed account maps FTP `/` to `C:\`. A different root
needs a matching connection card and deployment layout. Access to other DOS
drives is handled through local/managed DOS copying, not FTP path tricks.

## 3. Put the software in these directories

Create the directories if they do not exist. Existing installations must retain
their catalogue, observations, credentials and settings.

| DOS location | What belongs there |
| --- | --- |
| `C:\LOCSHELL` | `LOCSHELL.EXE`, `LOCHAND.EXE`, `MEMWATCH.COM`, `MEMLEARN.EXE`, `LOCSHELL.BAT`, `LCSMAIN.BAT`, and `LOCSHELL.TXT`. |
| `C:\LOCSHELL\NET\AGENT` | The matching `DOSAGENT.EXE` supervisor and `DOSINFO.EXE` diagnostic helper. |
| `C:\LOCSHELL\NET\JOBS` | Initially an empty directory for managed jobs. Do not clear it on an existing installation. |
| `C:\LOCSHELL\NET\BIN` | The standalone mTCP utilities built by `tools/build_suite.py`. |
| `C:\LOCSHELL\NET` | Your own `MTCP.CFG` and the account file it names. |
| `C:\LOCSHELL\ISLAND` | Optional compatible Island scene/music files. |

`LOCSOLO.EXE` and `CLASSIC.BAT` can also go in `C:\LOCSHELL` as a standalone
catalogue fallback. They are not the integrated FTP application. The NetDrive
driver may be supplied with the utilities; do not automatically add it to
`CONFIG.SYS` just to run LOC SHELL.

For a **new installation only**, create an empty game list if none exists.
At the DOS prompt:

```bat
IF NOT EXIST C:\LOCSHELL\LIBRARY.TXT TYPE NUL > C:\LOCSHELL\LIBRARY.TXT
```

An empty list is allowed. Add games through **Edit → Add…** afterwards. Missing
memory/learning records mean requirements are unknown; do not fill them with
invented figures. Preserve these files if already present:

`LIBRARY.TXT`, `MEMORY.TSV`, `LEARN.DAT`, `RECENT.TXT`, `LOCSHELL.CFG`.

Keep supplied DOS batch files as plain text with DOS CRLF line endings. Use
binary file transfer for the prepared package.

## 4. Start it locally before changing startup

1. Load the packet driver and establish the machine's mTCP network settings
   using its normal procedure. If it uses DHCP, obtain its address first.
2. At the DOS prompt, type `C:\LOCSHELL\LOCSHELL.BAT` and press **Enter**.
3. Press **F2**. Check that the menu works and FTP says **Running**.
4. Connect from the modern computer and transfer a disposable test file into a
   test folder. Download it again and compare its bytes.
5. Run a simple supervised diagnostic if using agents. Confirm the service
   returns, then retrieve its result. Do not use an interactive game installer
   for the first remote-job test.
6. Add and try one game locally, including returning from the game to the shell.

Only after that works should the machine's existing startup procedure call
`C:\LOCSHELL\LOCSHELL.BAT`, **after** network-driver/address setup. Preserve
other boot choices and a DOS recovery route. There is no universal AUTOEXEC or
CONFIG replacement; overwriting those files can break an otherwise working PC.

Fill out the [connection card](USER-GUIDE.md#send-something-from-your-modern-computer)
for the owner. Include the root mapping and where games should go. They should
not have to work those details out every time they add a game.

## Build on the documented Linux host

This part is for the builder or agent, not a requirement for daily use.
Install Git and Nix using the host's supported method, then:

```sh
git clone --recurse-submodules https://github.com/bstar/LOCSHELL.git
cd LOCSHELL
nix --extra-experimental-features 'nix-command flakes' develop path:./nix --command python tools/build.py --locshell
nix --extra-experimental-features 'nix-command flakes' develop path:./nix --command python tools/build_suite.py
nix --extra-experimental-features 'nix-command flakes' develop path:./nix --command python tools/test_locshell.py
python3 tools/test_saves.py
python3 tools/package.py --build build --output build/package
```

The pinned development shell targets x86-64 Linux. Successful builds produce
the executable/support files under `build/` and utilities under `build/suite/`.
Copy only the intended DOS payload, not object files or emulator test drives.
The build generates DOS-format `LOCSHELL.TXT` and batch wrappers.

For Island, choose a new output directory and run:

```sh
python3 third_party/dos-scenes/tools/pack_island.py --output /your/new/scene-pack-directory
```

Copy that pack to `C:\LOCSHELL\ISLAND`. Preserve
an existing `VOLUME.CFG`. The scene's separate artwork/music credits and license
remain in the dos-scenes repository.

`tools/stage.py` stages the older classic FTP supervisor. It is **not** a
complete LOC SHELL installer; use the layout and matching files above.

## Set up the agent's connection

Use the [agent guide](AGENT-GUIDE.md#for-the-agent-technical-working-rules) for
private credentials and controller commands. Keep connection details separate
from the public source. Test access from the actual agent host on the local
network; a successful connection from a different computer is not enough.

The [management reference](../management/README.md) documents jobs, result
recovery, file updates and deployment journals. Its older physical test notes
describe particular historical machines; they are not generic setup values.
