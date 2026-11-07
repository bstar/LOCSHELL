# DOS job management

`dosctl` runs trusted scripts/programs on the DOS PC through mTCP. No SSH gateway,
TSR networking or interactive remote console is installed. FTP disconnects during
execution and returns afterward. BIOS/direct-video output is not captured.

Build: `nix develop path:./nix --command python tools/build.py`
Test: `python tools/test_dos.py` (set DOSBOX to your DOSBox-X executable).
The test FTP server is a native DOS stub; these tests do not establish physical
packet-driver/network acceptance. Current compile target is 8086 real mode.

Stage: `python tools/stage.py` uploads only new files under C:\NET\AGENT and
creates C:\NET\JOBS. It refuses overwrites. Exit the current server locally and
run `C:\NET\AGENT\START.BAT` for initial acceptance. Existing boot entry points
remain unchanged until that acceptance succeeds. Revert by exiting the supervisor's
server with no pending job and running C:\NET\FTPSRV.EXE directly with MTCPCFG set to C:\NET\MTCP.CFG.
Once startup is enabled, restore its local .before backups to revert future boots.

Credentials: --credentials JSON with host, port, username, password. Default:
~/.config/dosctl/access.json; no alternate credential paths are embedded in the public tool. Credentials are never included in Git.

Examples:

```
dosctl run checks.bat --cwd 'D:\GAMES'
dosctl exec 'C:\DOS\MEM.EXE' /C
dosctl inventory
dosctl jobs
dosctl status
dosctl result J123ABCD
dosctl put local.ini /WINDOWS/EXAMPLE.INI
dosctl get /WINDOWS/EXAMPLE.INI downloaded.ini
```

Put verifies content and preserves an existing target under a unique 8.3 backup
before replacement. Get refuses to overwrite local files. FTP exposes only C:;
use supplied scripts with COPY to stage files from/to other drives. Batch scripts
use CP437 and CRLF. Native arguments use tab-separated request fields, max30;
batch arguments must be embedded in the supplied script. DOS path/tail limits
are validated before submission.

One outstanding job is allowed. Atomic MKD LOCK serializes submitters; the CLI
uploads and verifies payload/request, then atomically renames READY.NEW to READY.
Only afterward does it request SITE QUIT. The supervisor claims READY before
execution and never replays claimed work. Returning execution saves STATE.TXT,
RESULT.TXT and OUTPUT.TXT. On retrieval the CLI archives LOCK to its job ID,
releasing the submission slot. If a client dies, `dosctl result` retrieves the completed result and archives
LOCK to its ID, freeing the slot. Incomplete results retain LOCK for inspection. Never clear a queued or
running job merely to retry submission.

A CLI timeout does not cancel work. A hung program may require local recovery.
Partial submissions retain LOCK for explicit inspection. Job scripts have full
DOS privileges and may change persistent machine state. Batch results report
COMMAND.COM's status, not a guaranteed last-command errorlevel. Standard handles
1 and2 are captured together; stdin is NUL, but BIOS keyboard reads can still hang.

Keep Network management boot selected. NIC and modem share IRQ5: reboot before
modem use. Do not modify XTIDE/NIC settings or re-enable the After Dark TSR here.


## Physical acceptance (2026-11-01)

The actual DOS6.22 PC passed remote BAT execution, native MEM /C execution,
stdout/stderr/result retrieval, server reconnects, and replacement/read-back of
a disposable file with its previous version preserved. MEM reported596,784
conventional bytes free during native execution; supervisor occupies28,672 bytes.
All-drive inventory J50723F3 completed with shell_exit_code0 and30,156 lines of
retrieved result/output. Its initial180-second client wait expired; `dosctl result`
subsequently recovered the SAME job and archived it without re-execution.
Inventory now defaults to900 seconds, other jobs300; --timeout overrides both.
Startup entry points NETFTP.BAT and NETMGR.BAT were updated only after acceptance;
no reboot has been forced or claimed tested. Original FTPSRV remains in C:\NET.

## Machine operations display

FTPSRV now has a magenta/cyan LoC dashboard: link address, client/session counts,
job ID/state/command, and a bar for the largest allocatable DOS memory block.
This is not total free RAM or a guarantee of a child's memory availability.
The log stays below the dashboard. The supervisor displays RUNNING and the
launch command before handing DOS to a child; arbitrary programs can overwrite
that screen, and no live refresh during a child is claimed. Last result is
saved to C:\NET\JOBS\LAST.TXT and displayed after FTP returns.

An integrated text-mode starfield starts after120 idle seconds with no active
FTP clients. Keyboard input wakes it (the wake key is consumed); connecting
clients or log activity also wake it. FTP processing continues during animation.
The screen is restored exactly. Set DOSAGENT_IDLE in the DOS environment before
starting the supervisor to choose0(disabled) through3600 seconds. No After Dark
TSR, interrupt hooks, or resident animation are installed. Saver does not run
inside arbitrary foreground jobs. `python tools/test_ui.py` tests the actual
shared display code under DOSBox-X8086, including wake/restore/client inhibition.
After replacing DOSAGENT.EXE, exit the server with no pending job and start
C:\NET\NETFTP.BAT to load the new supervisor; uploading cannot change a resident
parent executable. Original binaries are preserved under unique .BAK names.

The starfield now uses first-person x/z,y/z perspective, with a precomputed
256-entry Q8 depth table and48 fixed stars. It uses integer arithmetic only,
no per-frame division/allocation, and at most96 text-cell writes per frame
(erase old positions, then draw new positions). Only entry clears the screen.
Frame pacing targets30fps using read-only8254 channel0 status/count latching,
with mode2/mode3 handling; BIOS interrupt frequency is unchanged. Unsupported
timer modes fall back to BIOS-tick pacing. No waits block network processing. There is no overlay text; the hardware cursor is hidden
and restored on wake. Near stars brighten, accelerate outward and recycle.
The projection follows the standard geometry demonstrated by the
[raylib starfield example](https://github.com/raysan5/raylib/blob/master/examples/shapes/shapes_starfield_effect.c);
the DOS integer/text implementation is original and does not depend on raylib.
Native tests exercise1000 frames, projected depth motion, bounds, text-free
output, active-client wake, and exact dashboard restoration. These tests do
not constitute a measured CPU-load benchmark on the physical PC.


## Physical performance and flicker follow-up

Header refresh now composes640 cells off-screen and writes only differences.
Native tests verify an unchanged redraw makes zero screen writes, a session
counter increment changes one cell, and shorter fields clear stale characters.
Session counts are part of that composition, not a separate redraw/clear pass.

On GREENTREE (16MHz386SX with coprocessor), projection benchmarks over1000
48-star batches measured19 BIOS ticks for integer16 versus86 ticks for inline
387 floating point. Integer16 remains the production path. Products fit signed
16bits; the depth table replaces per-star division. FPU support is an explicit
benchmark option DA_FPU_PROJECTION, not a requirement for the shipped server.
A complete1000-frame sparse-VRAM render benchmark measured29 BIOS ticks
(~1.6seconds). A timed run produced148 frames over91 BIOS ticks (~5seconds;
the coarse start/end window limits rate precision). This supports the30fps
target, not a guarantee during arbitrary hardware stalls. Timer read-back
support was verified on this physical PC. Dashboard metadata/disk polling is
skipped entirely while the saver sleeps. No per-frame text overlays or cursor.
Benchmark sources: tools/star_bench.c (integer or -fpi87 -fp3
-dDA_FPU_PROJECTION; latter requires a387) and tools/star_render_bench.c.
The new FTPSRV was activated automatically via job J8C46033 after upload;
no manual restart is needed for the starfield/header updates.

## GD5422 pixel starfield and fallback

The FTP server now defaults to a320x200,256-color VGA(mode13h) saver with96
stars and32 grayscale palette entries. It writes directly to the GD5422's VGA
aperture: at most192 byte pixel writes per frame, no bank switching, full-frame
RAM buffer, hardware blitter assumption, or card clock/ISA timing changes.
It retains the integer30fps pacing and network polling. Graphics-only font/state
buffers are compiled into FTPSRV, not the resident supervisor.

Entry checks VGA BIOS capability,80x25 mode3, standard font-bank selection,
and BIOS video-state save support before switching; it verifies the resulting
mode. Unsupported setups automatically use the compatible text starfield.
Set DOSAGENT_VIDEO=text before starting NETFTP to force text; unset it or use
DOSAGENT_VIDEO=auto for automatic selection. A failed switch restores the saved
text setup before fallback. Wake restores text cells, the8KB font plane, VGA
hardware/BIOS/DAC state and cursor. Windows settings/drivers are not changed.
Tests: `python tools/test_vga.py` exercises1000 frames, bounds and exact text,
custom-font and palette restoration, forced text fallback and automatic EGA
fallback. `python tools/test_ui.py` retains text/header/saver regression checks.
Physical GD5422 benchmark:1000 complete96-star pixel frames took59 BIOS ticks
(~3.2ms/frame); timed animation149frames/91 BIOS ticks(~5seconds), VGA13=1.
The production server was uploaded/readback verified and activated by job
J7482E95. No manual restart is needed for the pixel saver.

Visible events (connections, log messages, console notices and keyboard input)
reset the full idle countdown even when the dashboard is already awake. If a
log/console event arrives during the saver, restore text mode and refresh the
header before writing the message. Periodic status polling does not count as
activity. Native text/VGA tests cover event wake/restoration and idle reset.

## Agent recipes and structured results

```
dosctl --json recipes
dosctl --json recipe doctor
dosctl --json recipe memory
dosctl --json recipe storage-check
dosctl --json recipe storage-check --deep --path 'D:\DATA'
dosctl --json inventory --path 'C:\NET\AGENT'
dosctl --json recipe logs
dosctl --json recipe deployment-check --manifest release.json
dosctl --json status
dosctl --json resume
dosctl --json reports --limit 10
```

The public library contains doctor, memory, inventory, storage-check, logs and
deployment-check. FTP-only checks do not stop the server. Scans/capacity/memory
use a read-only DOSINFO.EXE helper or MEM job. Deploy DOSINFO.EXE to
C:\NET\AGENT after building it with tools/build.py. No repair/delete command
is part of these recipes. Inventory defaults to C:; --all-drives is explicit.
Storage is capacity-only by default; --deep adds scoped file-size analysis.
Optional --hash reads file contents to calculate CRC32 duplicate candidates,
not collision-free identity proof. Active LOCK files are excluded from scans.
A deployment manifest is {"files":{"/REMOTE/FILE":"SHA256 hex"}}.

Recipe JSON includes collection time, state, host, findings, job IDs and local
artifact directory. A needs-attention diagnostic can finish successfully;
waiting means a submitted job continues after the client stops waiting.
status returns idle when there is no outstanding remote job. resume consults
persistent local job records and retrieves the same ID without resubmission.
Archived IDs remain available with result JOBID. Reports and job records are
private under ~/.local/state/dosctl (DOSCTL_STATE_DIR overrides it); reports
lists cached collection times without connecting. Memory metrics describe the
job context with FTP absent, not memory while the FTP child is resident.
Use --label on run/exec to supply a readable task name; recipes do so automatically.

Credentials are read from ~/.config/dosctl/access.json or --credentials. Keep
that file private. No machine/account credentials are included in this project.

### Private extensions

Put machine-specific recipes in custom_recipes/*.py, which is gitignored, or
select a separate directory with DOSCTL_CUSTOM_RECIPES. The public library works
with that directory absent. Each plugin exports RECIPES (name mapped to a
transport/description pair) and execute(args, api), returning a JSON-compatible
report. Names must be unique. Files beginning with an underscore are helpers
and are not loaded. These are trusted local Python extensions.

Example plugin interface, without any machine-specific configuration:

```python
RECIPES = {"local-check": ("FTP", "Read server status")}
def execute(args, api):
    with api.connect(args) as ftp:
        return {"recipe": args.name, "state": "completed",
                "status": ftp.sendcmd("NOOP")}
```

Keep private recipe implementations, inventories, application configurations
and custom test fixtures out of public source and commits. Discovery identifies
extensions as source=custom. The CLI does not make custom recipes public.

## Resumable scan flows

```
dosctl --json flow start --path 'D:\DATA' --batch 512
dosctl --json flow list
dosctl --json flow step T123ABCD
dosctl --json flow status T123ABCD
dosctl --json flow run T123ABCD
dosctl --json flow cancel T123ABCD
dosctl --json flow result T123ABCD
```

Start creates a durable scan; step performs one bounded batch and returns FTP;
run advances batches until completion/cancellation or a wait timeout. Status
reads the local checkpoint without contacting DOS. Result exports merged records
and flags partial scans. Cancellation stops before the next batch, never claims
to kill a running DOS program. A batch limit bounds directory entries processed,
not time spent on a failing disk. --hash still uses the explicit whole-file CRC
path; it is not presented as interruptible within a file.

DOSINFO persists DOS find/DTA state and a directory queue under its own task
folder. Cursor replacement is journaled; output is retained per step. The host
records submission ownership before waiting, recovers the same job ID and checks
the returned generation before advancing. Ambiguous/interrupted work is held,
not replayed. Runner locks prevent concurrent advancement and cancellation is
sticky across checkpoints. Transient421 connection-slot failures and421/425/426
responses while polling are retried only for connection/read-only result
collection. A mutation is never blindly repeated. Credential-profile binding
prevents advancing a task against another configured target.

Non-hash inventory/deep storage recipes use these flows. Custom audits can reuse
flows.scan_records and retain partial reports. Flow history exposes each step's
file count; local state is written atomically with fsync. Active tasks and reports
remain private under DOSCTL_STATE_DIR.

## Live identity and operational history

`dosctl --json machine` executes the native identity helper and collects actual
MACHINE/CONFIG/WINMACH environment values, DOS version, job-context memory,
current video mode, helper version, active FTP protocol and installed file hashes.
An absent resident-parent version remains unknown until that parent is restarted;
installed hashes are never treated as proof of what is already resident. A
private extension can corroborate machine-specific Windows selection records.

`dosctl --json history --limit 30` lists durable UTC events for jobs, file updates,
flow checkpoints, releases and verification notes. Releases distinguish verified
bytes from untested runtime behavior. Use `dosctl annotate SUBJECT 'observed
behavior' --verification software|user-confirmed|pending-physical` to attach
actual evidence. Do not promote software checks to physical acceptance.

## Transactional application releases

```
dosctl --json deploy release.json --dry-run
dosctl --json deploy release.json
dosctl --json restore R123ABCD
```

A release manifest contains name, files (source, destination, sha256) and preserve
patterns. Destinations are absolute FTP paths with DOS8.3 components. Example:

```json
{"name":"application","preserve":["/APP/*.CFG"],"files":[
 {"source":"/local/payload/PROGRAM.EXE","destination":"/APP/PROGRAM.EXE",
  "sha256":"replace-with-actual-64-digit-sha256"}
]}
```

All changed files are staged and read-back verified before commit. Original
bytes are retained locally and under unique remote backup names. A durable
release journal records each rename. Failure rolls back owned changes when the
connection remains available; otherwise the result provides release ID/journal
for explicit recovery. Restore preflights the entire set and refuses independent
changes or damaged backups. Replaced/new bytes are moved aside, not discarded.
A successful byte installation records runtime_verified=false until supported
by actual acceptance evidence. Named application policies live in private custom
recipes; the public project contains no machine-specific release definitions.

## Experimental resident observer

The separate RXMON prototype offers only heartbeat and80x25 text snapshots over
an explicitly selected spare serial port. It never executes commands, calls DOS
from its ISR, hooks the NIC, or registers an Ethernet protocol. It is not added
to production startup and is not automatically uploaded/installed on real PCs.

Build with `nix develop path:./nix --command python tools/build_resident.py`.
The experiment explicitly installs on exclusive COM2(2F8h)/IRQ3 at9600baud;
it refuses a port missing from the BIOS table. Removal currently requires a
reboot. Verify hardware/IRQ ownership and a suitable cable before physical use.

```
dosctl --json resident heartbeat --serial /dev/ttyUSB0
dosctl --json resident screen --serial /dev/ttyUSB0
```

--tcp localhost:PORT supports a raw DOSBox/serial bridge for testing; it is not
an SSH gateway. The observer reports unsupported graphics modes rather than
misinterpreting VGA memory. tools/test_resident.py uses a DOSBox-X build with
nullmodem support and proves heartbeat/snapshot delivery while an unrelated EXE
runs without servicing networking. Measured prototype residence18,064 bytes;
one emulator baseline comparison showed0.42% fewer busy-loop iterations during
heartbeat/snapshot requests. It generates no DOS Ethernet traffic. These are
emulator measurements, not physical UART/network acceptance. Existing DOS
management FTPSRV/job execution remains the production interface.

## LOC SHELL Home and backup reports

The integrated launcher, native FTP views and full-width Home dashboard are
now built from this repository. See [LOC SHELL](../locshell/README.md) for build,
installation layout and acceptance commands. F2 opens Home. Recent plays are
recorded at validated dispatch; live FTP progress, file changes, queued jobs and
reload status share the same interface. Existing catalogue, memory observations,
boot/driver configuration and saved preferences must survive an upgrade.

`dosctl saves dashboard --output VAULT.STA` exports a read-only report of actual
published backups. Publish it as `C:\DOSSHELL\VAULT.STA`. The report includes
backup/report times; it does not claim local saves are synced. Native automatic
two-way sync remains unfinished. See [vault status](saves/README.md).
