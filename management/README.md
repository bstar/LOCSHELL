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
server with no pending job and running C:\NET\NETFTP.BAT.

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
