# One LOCSHELL folder

[Simple guide](USER-GUIDE.md) · [Agent guide](AGENT-GUIDE.md) · [Setup](SETUP.md)

LOC SHELL 2.3 keeps all its DOS application files and data under `C:\LOCSHELL`:

```text
C:\LOCSHELL
  LOCSHELL.EXE       menu, dashboard and integrated FTP server
  LOCSHELL.BAT       start here
  LCSMAIN.BAT        launch-and-return wrapper
  LOCHAND.EXE        validated launch helper
  MEMWATCH.COM       optional memory observer
  MEMLEARN.EXE       memory recording collector
  LOCSHELL.CFG       preferences
  LIBRARY.TXT        your game/application menu
  MEMORY.TSV        documented memory needs
  LEARN.DAT         observed playing sessions
  RECENT.TXT        recent launches
  VAULT.STA         dated backup report
  NET\              private mTCP configuration and network support
    AGENT\          supervisor and diagnostic helper
    BIN\            standalone network tools
    DRIVERS\        supplied hardware packet driver files
    JOBS\           managed job queue and results
    TASKS\          bounded scan state
  ISLAND\           scene, music, credits and saved volume
  BACKUP\           retained installation/recovery files
  GAMES\            default folder for new game installations
```

Existing menu entries can still point to games on other drives. Importing the
application does not relocate or reinstall those games or touch their saves.
DOS's own startup files stay in the locations required by DOS and point to the
application directory. Shared DOS/OS components are machine prerequisites.

On the Linux helper, private connection details, state and vault data share
`~/.local/share/locshell` (`access.json`, `state/`, `vault/`). `LOCSHELL_HOME`
selects a different private container. Keep it outside the public Git checkout.

## New installation

Build `tools/build.py --locshell`, then `tools/build_suite.py`, then:

```sh
python3 tools/package.py --build build --output build/package
```

The output contains `LOCSHELL.ZIP`, a `LOCSHELL/` tree and a hash manifest.
Island is prepared from the repository-pinned source automatically. The package
includes the utilities, matching supervisor/helpers, scene credits and licenses.
It does not contain private network credentials or somebody else's catalogue.
Use [first-time setup](SETUP.md) to configure the PC's actual packet driver and
network account. Do not replace an existing catalogue with the empty template.

## Bring over an existing installation

An agent/operator should inspect the machine and identify its current shell,
network and scene directories. The import command takes those paths explicitly;
it does not guess names or scan unrelated drives.

The names below are placeholders for the existing directories and preferences:

```sh
dosctl --json import-installation --shell /OLDAPP --network /OLDNET --scene /OLDSAVER --preferences OLD.CFG --package /host/path/package --dry-run
```

The dry run reads active source files and reports the proposed destination,
file count, byte count and explicitly named bootstrap changes. It writes no
remote files. Old backups/logs and finished job archives stay in the old trees
for intact archival after switchover; they are not activated as new queued work.

Resolve a pending job, local launch or learning request before import. If the
new application directory already exists, inspect it instead of importing over
an uncertain partial installation. If a catalogue launches programs by an old
application path, review that entry rather than blindly changing its identity.

When the reviewed plan matches the request, use the same command without
`--dry-run`. `--startup` can update explicitly reviewed bootstrap files after
staging, but only use it when those batch files are not executing. A batch
paused at `CALL` while its child runs is still executing. For a live shell,
stage without `--startup`; after the old session has returned to DOS and the
new shell is verified, deploy reviewed startup copies with backups inside
`C:\LOCSHELL\BACKUP`. The importer:

1. Checks the package hashes and retains host backups of imported active files.
2. Stages a complete production payload beside the running installation.
3. Brings over the catalogue, preferences, memory records and recent history.
4. Copies the current Island assets/volume unchanged and relocates explicit
   network configuration/startup paths into the new tree.
5. Uses checked release batches and reads back every final file.
6. Updates only the explicitly listed DOS startup files after the payload is ready.
   Their transaction backups are retained inside `LOCSHELL\BACKUP`.

It leaves the original application trees present. A stopped client or partial
failure does not mean that rerunning the import is safe: retain the import ID
and release journals and inspect recovery first.

## Switch over once, then archive

The importer returns `ready-for-local-start`, not a claim that the new interface
has been physically tested. At the retro keyboard:

1. Disconnect the modern computer's FTP client.
2. Exit the previous shell to DOS.
3. Type `C:\LOCSHELL\LOCSHELL.BAT` and press Enter.
4. Check Home, mouse/keyboard, the game list and FTP. Try the Island preview.

The agent can then check the new server's `SITE AGENT` response for `layout=2`,
perform native diagnostics and record the user's actual playing/input results.
Before archiving, verify that every actual boot/profile entry point now calls
the new tree. Otherwise a later reboot could still request a previous folder.
For an inspected import, archive the old trees with its actual ID:

```sh
dosctl --json archive-import I123ABCD
```

This refuses to run against the old server or after source user state changed
without reconciliation. It renames the previous directories into
`C:\LOCSHELL\BACKUP\<import ID>`, preserving their contents. It does not delete
them. An unsupported/failed rename is reported for recovery; it does not fall
back to deleting source files. Keep the journal if the connection is lost.

Past release/history records may contain the paths that were true at the time.
They remain historical evidence, not active configuration.

## Pull or update Island later

```sh
dosctl --json scene install island --dry-run
dosctl --json scene install island --select --preview
```

The controller initializes the pinned dos-scenes source if needed, prepares all
17 pack files, installs through a journaled release, verifies the bytes and keeps
`VOLUME.CFG` and its recovery files. Credits travel with the pack. `--select`
saves Island as the chosen saver; `--preview` queues a preview after clients
disconnect. The idle timeout must be nonzero for a preview. Leave those flags
off to install the assets without changing the selected saver.

The helper refuses a server with the older layout, a pending managed job, or a
modified/mismatched pack-source checkout. Original images are copied, not edited.
A file-level install is not visual/music acceptance: inspect native saver
counters and have the user check the screen and sound.

## Reusable agent skill

The complete skill lives in
[`skills/locshell-management/SKILL.md`](../skills/locshell-management/SKILL.md).
Register that directory with the agent's skill system. For Codex, a symlink from
its skills directory keeps the actual skill files in the LOCSHELL repository:

```sh
ln -s /absolute/path/LOCSHELL/skills/locshell-management ~/.codex/skills/locshell-management
```

Use a new directory name if that registration already exists; do not overwrite
an unrelated skill. Then ask: “Use $locshell-management to install Island.”
The skill follows the same verified process and preserves machine data.
