# LOC SHELL: use it with an agent

[Back to LOC SHELL](../README.md) · [Do it yourself](USER-GUIDE.md) · [First-time setup](SETUP.md)

**Tell the agent what you want. Leave the retro PC at LOC SHELL. Expect to
test the game yourself when it is ready.**

An agent is an assistant that can use tools on a modern computer. LOC SHELL
does not contain an AI. It provides a file service and a way for that assistant
to run DOS maintenance jobs.

## What do I need to provide?

1. A retro PC with LOC SHELL and FTP running. Press **F2** to see its address.
2. An agent with terminal/file access to a computer that can reach that address
   on your local network. A chat window with no tools cannot do the work.
3. Connection details stored privately on that computer. Do not post passwords
   in public issues or commit them to Git.
4. Your game files, if you are installing a game. Tell the agent where they are.

The documented automation and build environment uses a **Linux helper
computer**. A Mac or Windows computer can use ordinary FTP directly. For the
documented `dosctl` automation, give its agent access to a Linux helper or VM;
do not assume the controller works unchanged in native Windows Python.

If the PC has never been set up, give the agent the [setup guide](SETUP.md).
The agent needs to establish a working network and startup before remote
maintenance is possible.

## Install a game

Copy this message to your agent. Fill in the two bracketed parts:

> Install **[game name and edition]** from **[folder on my modern computer]**
> onto my retro PC through LOC SHELL. Use my saved connection details. Inspect
> the machine, available disk space and current game list first. Preserve my
> existing games, saves and settings. Use a new game folder, verify the copied
> files, and add a correctly labelled menu entry. Set up launch/setup commands
> for this edition and my sound hardware. Tell me if an installer needs my
> keyboard or a different boot choice. Give me a short playing test at the end.

You should get back:

- Where it was installed and the name to choose in Games.
- Whether to run the game's sound/setup program or change boot choices.
- What was checked by software, and what you still need to try yourself.
- Where the backups are, plus any job/release ID needed for recovery.

**You still do the playing test.** Start the game, check controls, picture and
music/sound, and quit back to LOC SHELL. If wanted, record one run with
**F7 → L**, then launch normally. An upload or an agent's successful command
does not prove that gameplay and music work.

## Change a setting

Use a specific request:

> Change **[game/program]** to use **[the setting I want]**. Read the existing
> configuration first and save a backup. Keep my other settings and saves.
> Explain whether the change applies on the next game launch, a LOC SHELL
> restart, or a reboot. Verify the saved file and give me a short check to try.

Good examples: select AdLib music in a game that supports it, change a launch
command, add a separate game-code variant with a clear name, or change a
screensaver. Choosing a sound card in software does not add that hardware.

For a system/boot change, say exactly what you want changed. The agent must
work from the current files and machine profile, preserve the other profiles
and explain any required local restart. Boot, network and driver changes are
not all activated merely by uploading a file.

## Update LOC SHELL

> Update LOC SHELL from **bstar/LOCSHELL**. Keep my catalogue, preferences,
> learned memory records, recent history, game files and saves. Use a production
> build, verified backups and readback. Install the complete matching support
> files, then request a safe reload. Check that FTP returns and the menu still
> contains my games. Tell me if anything needs a separate supervisor restart.

The application can restart itself after an update. Leave it at the menu,
finish/cancel any open edit and let the sender disconnect. It waits for active
FTP connections and unfinished editing rather than restarting through them.
Updates to an already loaded parent program or driver need that component to
be restarted separately.

## What should I see while the agent works?

| Stage | What the old PC may show |
| --- | --- |
| Inspecting or uploading | Home's connections, incoming files and file history. You can still browse. |
| Running a DOS maintenance job | A job screen and paused FTP. The job may draw its own screen. |
| Reloading LOC SHELL | A short disconnect, followed by the menu and FTP returning. |
| Running the game | The game uses the PC. FTP is unavailable until it returns. |

A remote job is allowed to run DOS commands, including commands that change
files and configuration. Give this access only to a helper you trust.

If the agent says it timed out, say:

> Inspect and recover the same job. Do not submit it again just because the
> waiting connection ended. Tell me whether it completed or needs me locally.

Do not reboot solely because the agent stopped waiting. A job can continue
after that timeout. A genuinely hung interactive program can need local
recovery; the agent must explain the evidence first.

## Backups and game saves

Ask explicitly before reinstalling or changing an existing game:

> Back up this installation's saves and settings first. Identify all the files
> that belong together; do not assume one file per save. Preserve game-code
> variants and label them. Do not call the backup card a successful sync.

The host save vault exists, but the automatic native DOS sync client and launch
hooks are unfinished. Home displays a dated backup report. It does not establish
that this PC matches another PC or that conflicts have been checked.

## For the agent: technical working rules

This section is for the assistant/operator. The user can stop reading here.

### Establish access and inspect before changing files

The source is [bstar/LOCSHELL](https://github.com/bstar/LOCSHELL). Read the
applicable workspace instructions and machine record. Use
[the management reference](../management/README.md) for the command protocol
and [the build/install reference](../locshell/README.md) for layout.

Examples below call the controller `dosctl`. If it is not installed on PATH,
run `python3 management/dosctl.py` from the repository in its place. The
controller uses Python's standard library on the documented Linux host.

Its default private credentials file is `~/.local/share/locshell/access.json`:

```json
{"host":"YOUR-RETRO-IP","port":21,"username":"YOUR-USER","password":"YOUR-PASSWORD"}
```

Replace those placeholders privately; they are not working credentials. Keep
the file readable only by its owner. An alternate file is selected with
`dosctl --credentials /private/access.json ...`. With JSON output,
`--json` must be the **first** option.

Start with these checks, not a recursive scan of every disk:

```sh
dosctl --json recipes
dosctl --json recipe doctor
dosctl --json status
dosctl --json history --limit 10
```

`recipes`, `history` and `reports` are local/discovery operations. `doctor`
uses FTP. Memory, identity, storage and native inventory checks can submit DOS
jobs and briefly stop FTP. Use scoped paths and bounded scans. Discover which
recipes are public versus installation-specific; the public repository does
not ship the owner's private game-deployment recipes.

### Installing a game: the full sequence

1. Identify the exact edition, source layout, launch file, installer needs,
   hardware requirements, sound configuration and target drive. Inspect existing
   variants before choosing a new directory. Ask for missing source files rather
   than substituting a different edition.
2. Get fresh disk-space and target-directory information. Keep room for staging
   and backups. Read back the current catalogue and configuration; keep a dated
   local copy. If updating a game, identify its actual saves/companion files first.
3. Prepare ordinary DOS files on the host. Preserve names and subdirectories;
   check 8.3 compatibility and case collisions. Use binary FTP, including files
   without extensions. Interactive installers belong at the local keyboard.
4. Create the destination parents explicitly. `put` and `deploy` do not create
   an entire directory tree. Use FTP directory creation within its permitted
   root, or a small checked native job. Do not overwrite an existing edition
   to resolve a path collision.
5. Install and verify all payload files. `dosctl put LOCAL /REMOTE` handles one
   file, reads it back and keeps a unique backup when replacing an existing
   target. For a set, prefer `deploy` with actual SHA-256 values and explicit
   preservation rules. A release accepts at most **256 manifest file entries**;
   larger games need planned batches and complete final verification.
6. Add/update the catalogue through the local editor or the managed-job method
   below. Include the correct drive and working directory. A `.BAT` start file
   needs `CALL`. Keep play and setup commands separate. Add `---BOOT---` metadata
   only for verified machine-specific mode keys.
7. Retrieve the job result, read back the final catalogue, and check the exact
   installed payload. Confirm LOC SHELL and FTP return. Record any release/job
   IDs, backup locations and remaining local tests.
8. Have the user play with the expected music/sound and quit back to LOC SHELL.
   Keep software verification separate from user-confirmed playing acceptance.

Example release structure, with intentionally nonworking placeholders:

```json
{
  "name": "My Game - exact edition",
  "preserve": ["/LOCSHELL/GAMES/MYGAME/SAVEGAME/*"],
  "files": [
    {
      "source": "/absolute/host/path/MYGAME/GAME.EXE",
      "destination": "/LOCSHELL/GAMES/MYGAME/GAME.EXE",
      "sha256": "REPLACE-WITH-THE-FILE-S-ACTUAL-64-HEX-DIGIT-SHA256"
    }
  ]
}
```

Expand `files` to the full payload, compute hashes, and replace the example
preserve pattern with the game's verified save/configuration paths. A preserve
pattern skips matching manifest targets; it is not a save-file discovery tool
or a full-drive backup. A multi-file release stages/verifies files first, but
its commit uses individual renames, not one atomic directory switch.

```sh
dosctl --json deploy /absolute/host/path/game-release.json --dry-run
dosctl --json deploy /absolute/host/path/game-release.json
```

The dry run still connects and reads targets; it does not install them. Keep the
returned release ID for `dosctl --json restore R123ABCD` with the **actual** ID.
Restore checks for subsequent independent changes and can refuse unsafe recovery.

### Other drives and protected catalogue files

In the standard managed account, FTP `/` maps to `C:\`. Confirm the mapping;
do not assume every mTCP account is identical. Drives D:/E:/F: require a native
job that copies from a staging directory on C:. Read back staged results through
C: as needed. Do not bypass the FTP sandbox with crafted paths.

The active server rejects live FTP writes to its owned catalogue/state files:
`LIBRARY.*`, `MEMORY.*`, `LOCSHELL.*`, `LEARN.*`, `NETSTATE.*` and `RECENT.*` in
`C:\LOCSHELL`. Sending `LIBRARY.TXT` directly and hoping for a refresh is not a
supported update method.

For a remote catalogue update: download the latest list, merge without losing
metadata/variants, validate the format, and upload the candidate under a new
name in a private staging directory outside `C:\LOCSHELL`. Submit a bounded
native job to recheck the expected old catalogue, retain a unique backup and
install the candidate while the shell is stopped. Check explicit copy/result
markers, then retrieve and compare the final file after the shell returns.
If the source changed since inspection, merge again instead of overwriting it.

```sh
dosctl --json run /absolute/host/path/checked-install.bat --cwd 'C:\' --label 'Install My Game menu entry'
```

This is a command to execute a script the agent must first author and review;
`checked-install.bat` is not a supplied universal installer. DOS 6 scripts use
CP437/plain ASCII and CRLF, and do not support `EXIT /B` or `%ERRORLEVEL%`.
Validate each operation explicitly. A batch's reported shell exit code alone
does not prove every copy succeeded. Do not run an interactive setup program
as an unattended job: standard input is NUL, and BIOS keyboard waits can hang.

### Incoming configuration and application updates

Read the live target first. Preserve unrelated settings, line endings and the
machine's boot/driver constraints. Send a specific configuration through
verified `put`/`deploy`, then activate it through the appropriate component.
An `.INI` arriving does not automatically launch its application; an uploaded
`CONFIG.SYS` does not reboot DOS. Relevant file changes make LOC SHELL reconsider
memory/setup context, but do not prove the configuration works.

For a shell release, finish matching helpers/docs before requesting reload.
Replacing `LOCSHELL.EXE` queues reload; it should be the final executable
replacement. After the complete release is installed, publishing
`C:\LOCSHELL\RELOAD.REQ` also requests reload, including when size/timestamps
are unchanged. Disconnect all FTP clients and let edits finish. A reload is not
a DOS reboot and cannot replace a resident driver or an already loaded parent.

Preserve `LIBRARY.TXT`, `LOCSHELL.CFG`, `MEMORY.TSV`, `LEARN.DAT`, `RECENT.TXT`,
network credentials/settings, game saves and private machine recipes. Do not
deploy emulator test-hook builds. Do not install experimental serial observers
or change NIC/IRQ/ROM settings as an incidental part of a game install.

### Timeouts: recover the same work

```sh
dosctl --json status
dosctl --json resume
dosctl --json result J123ABCD
```

Use the returned real job ID, not `J123ABCD`. A waiting/timeout result does not
cancel the submitted job. One outstanding job is serialized through `LOCK`.
Never delete `LOCK`/`READY` or replay a request just to get past a timeout.
Use result retrieval/recovery and inspect the journal if ownership is uncertain.
There is no general remote keyboard or guaranteed remote kill.

## Automatic Island installation and agent skill

Use the included [LOC SHELL management skill](../skills/locshell-management/SKILL.md)
for repeatable scene/game/configuration work. All DOS application components live
inside `C:\LOCSHELL`; private controller data lives inside
`~/.local/share/locshell` (or `LOCSHELL_HOME`).

Ask: “Install the Island screensaver, preserve my volume, select it, and preview
it when ready.” The controller commands are:

```sh
dosctl --json scene install island --dry-run
dosctl --json scene install island --select --preview
```

They fetch the pinned scene source if necessary, prepare the complete pack,
retain credits and verify every installed file. The existing volume/recovery
files are preserved. Read [migration and activation](MIGRATION.md) for old-layout
imports and the one-time local switchover. Do not deploy new-path binaries into
a previous layout and expect its older wrappers to find them.
