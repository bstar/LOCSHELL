# Dashboard transfer recording

`dashboard-transfers.gif` shows the actual LOC SHELL Home dashboard receiving
sample game data and a sound settings file, then sending the data back over FTP.
It is recorded in a 4MB DOSBox-X machine with an emulated Ethernet adapter.
Transfers are deliberately paced so the changing status is readable. This is
a feature demonstration, not a physical-machine speed benchmark.

The frames come from the running application's VGA text memory and the
emulator's BIOS font. The recorder converts those captured cells to images and
scales them with nearest-neighbor pixels. It does not invent progress, draw a
replacement interface or overlay status text. The upload/download bytes are
compared, and the native server's file counters must agree before export.

The catalogue, files, account and machine name are disposable examples. The
backup card reports an actual empty temporary vault. No private connection
file, real game, saved game or physical PC is used. The recording is silent.

## Record it again

Use the pinned Nix environment, a separately supplied NE2000 DOS packet driver
(`NE2000.COM`), and `ffmpeg` available on PATH. The driver is not redistributed
here. DOSBox-X must have its slirp network backend enabled; the Nix build used
by this project provides it.

From the repository root:

```sh
DOS_BUILD_DIR="$PWD/build-doc-demo" DOS_SHELL_TEST=1 nix --extra-experimental-features 'nix-command flakes' develop path:./nix --command python tools/build.py --locshell
nix --extra-experimental-features 'nix-command flakes' develop path:./nix --command python tools/record_dashboard.py --build build-doc-demo --packet-driver /path/to/NE2000.COM
```

This creates a temporary emulated drive under the build directory and writes
the GIF here. It uses host control port 15321 and passive ports 15400–15655.
The control port can be changed with `--port`. The emulated server exists only
for the recording and is shut down afterwards. DOSBox runs with dummy video
and audio, no desktop window and no close-confirmation prompt.

The working directory retains the captured frames, emulation log and
`recording.json` with the transfer digest, native counters and recording details.
Test-hook builds belong only in emulators; never deploy them to a real PC.
