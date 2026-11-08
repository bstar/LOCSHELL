---
name: locshell-management
description: Install, update and manage LOC SHELL on a networked retro DOS PC, including fetching the pinned Island screensaver pack and verified game/configuration transfers. Use the LOC SHELL controller and preserve existing machine data.
---

# LOC SHELL management

LOC SHELL's DOS application tree is `C:\LOCSHELL`, including its network tools,
configuration, job queue, helpers, screensaver assets and user state. Host-side
credentials/state/vault default to `~/.local/share/locshell` (or `LOCSHELL_HOME`).
Read the machine-specific record before changing a physical PC. Do not assume
an old IP, drive mapping, boot mode or sound card still matches the machine.

Find the LOCSHELL repository, then use its `management/dosctl.py` controller
(`dosctl` if installed). The agent needs a Linux host with access to the PC's
LAN and the user's privately saved connection details. Do not publish those
credentials. A chat-only agent cannot perform the transfer.

## Island screensaver

For a request to install/pull Island:

1. Run `dosctl --json recipe doctor` and `dosctl --json status`. Resolve an
   outstanding job before submitting another operation. The server must advertise
   `layout=2`; use the migration guide for older installations instead of
   installing assets outside the application tree.
2. Run `dosctl --json scene install island --dry-run` for the intended target.
   The controller initializes the pinned dos-scenes submodule if needed, builds
   its complete pack, and validates it. It refuses to overwrite a different or
   modified pack-source checkout. No raw game-image editing is involved.
3. With installation covered by the user's request, run
   `dosctl --json scene install island`. Add `--select` when the user wants Island
   saved as the chosen saver. Add `--preview` when they want a preview after
   clients disconnect. A disabled idle timeout must be enabled in LOC SHELL
   before previewing; do not change unrelated preferences just to pass a test.
4. Report the release ID, pinned revision and verified destination. The controller
   preserves `VOLUME.CFG` and volume recovery files, keeps credits, reads back
   every installed asset and retains replacement backups. It does not reboot.
5. For a preview, disconnect clients, wait for a bounded preview interval, then
   reconnect and inspect `SITE SAVER`. Frames increasing with error/fault zero
   is native execution evidence; ask the user to judge the picture and music.
   Do not label file readback as physical visual acceptance.

On a transfer/release error, keep its release ID/journal and inspect recovery.
Do not rerun a mutation merely because the response was lost. Preserve the
user's saved volume and other scene choices. Missing assets are an installation
problem; don't silently edit original artwork or substitute another scene.

## Games and configuration

Read [the agent guide](../../docs/AGENT-GUIDE.md) for game installation, protected
catalogue files, configuration activation and job recovery. Use
[the game walkthrough](../../docs/INSTALL-GAMES.md) for steps the user performs
locally, and [the layout/migration guide](../../docs/MIGRATION.md) when moving an
older installation. These references are in the same repository as this skill;
follow the symlink to the repository if the skill was registered elsewhere.

Identify an exact game edition and its real save/companion files before an
update. Preserve game-code variants and label them. Uploading files does not
add a catalogue entry or prove gameplay/music works. Interactive DOS installers
need the local keyboard; unattended jobs have NUL input and can hang.

Use verified `put` or journaled `deploy`. The active shell protects its owned
catalogue/history/learning files from live FTP writes: stage a candidate and
use a checked native job while the shell is stopped. For job timeouts, retrieve
or resume the same ID. Never delete `LOCK`/`READY` or replay unknown work.
