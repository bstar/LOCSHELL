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
~/.config/dosctl/access.json; this workstation also supports the existing private
machine credential record. Credentials are never included in Git.

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
