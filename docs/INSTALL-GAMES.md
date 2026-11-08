# Put a game on your retro PC

[Simple user guide](USER-GUIDE.md) · [Have an agent do it](AGENT-GUIDE.md#install-a-game)

**There are three jobs: send the files, add a menu entry, and try the game.**

This walkthrough uses a prepared DOS game folder called `MYGAME`, with a start
file called `GAME.EXE`. These are example names. Use your game's actual start
filename. **Do not rename a game's EXE to match this guide.**

## Before you start

You need:

- LOC SHELL already running on the old PC, with FTP **Running**.
- A modern computer on the same local network.
- Your connection card from the [user guide](USER-GUIDE.md#send-something-from-your-modern-computer).
- Your DOS game files, unpacked into a normal folder.

Open the game folder on the modern computer. It should contain the DOS game's
program and data files, not just a ZIP or a modern Windows installer. Extract
archives on the modern computer first. Keep all subfolders and game filenames.

If you have floppy/CD images or an installer that asks questions, it may need
the game's original installation procedure. LOC SHELL can transfer those files,
but does not automatically mount images, unpack archives or click through an
installer. Run an interactive DOS installer at the retro keyboard.

Check the game's CPU, memory, graphics and sound requirements against your PC.
LOC SHELL does not convert a newer game into a 386-compatible game.

## 1. Connect the two computers

The example app is **FileZilla Client**, available for modern computers from
the [official FileZilla download page](https://filezilla-project.org/download.php?type=client).
Another FTP client works too. Choose Client, not Server.

1. On the **old PC**, press **F2**. Write down the FTP address and port shown.
2. On the **modern computer**, open FileZilla.
3. Choose **File → Site Manager → New site**. Name it `My retro PC`.
4. Set **Protocol** to **FTP - File Transfer Protocol**.
5. Set **Host** to the address you just wrote down. Use the port on your card,
   normally `21`.
6. Set **Encryption** to **Only use plain FTP (insecure)**, or the equivalent
   plain-FTP choice. This DOS service does not provide TLS or SSH.
7. Set **Logon Type** to **Normal**. Fill in your FTP **User** and **Password**.
8. Click **Connect**.

These settings are in FileZilla's [Site Manager](https://wiki.filezilla-project.org/Site_Manager).
**You should now see two file lists. Left is your modern computer. Right is
the old PC.** If login fails, check the address and credentials before proceeding.
The [official FileZilla tutorial](https://wiki.filezilla-project.org/FileZilla_Client_Tutorial_%28en%29)
shows the two-pane layout and transfer queue.

Choose **Transfer → Transfer type → Binary** before copying games. This keeps
file bytes unchanged, including data files without extensions. See the
[FileZilla transfer-type guide](https://filezillapro.com/docs/v3/advanced/file-type-classifications-for-ftp-and-ftps/).
If connections are unreliable on an old PC, limit this saved site's simultaneous
connections to **1** under **Transfer Settings**. Keep the normal passive mode
unless the person who configured your network supplied other settings.

## 2. Copy the complete game folder

This example installs to `C:\LOCSHELL\GAMES\MYGAME`. Use a **new folder**, not an existing
game installation containing saves.

The following assumes your FTP account's `/` is `C:\`, as in the standard
managed setup. If your connection card gives a different mapping, use that
mapping. `/LOCSHELL/GAMES` would then refer to somewhere else.

1. In FileZilla's **right-hand** list, open `/LOCSHELL`.
2. Find `GAMES`. If it does not exist, right-click and create a directory named
   `GAMES`. Open it.
3. In the **left-hand** list, open the folder containing your `MYGAME` folder.
4. Drag the whole `MYGAME` folder from **left to right**.
5. Wait until the transfer queue is empty. Check **Failed transfers** for errors.
6. In the right-hand list, open `MYGAME`. Check that the game's start file and
   data files are there, with their subfolders.

Use DOS-friendly names for new folders: at most eight characters, no spaces.
DOS filenames usually have at most eight characters before the dot and three
after it. Preserve the game's supplied names; do not casually rename its data
files. If your source depends on long filenames, get a DOS-compatible package.

**On the old PC:** Home should show incoming files and file-change history.
A moving progress symbol is useful; an unknown percentage is not an error.

Copying a file does not run it or automatically add a game to the menu. If the
queue contains failures, resolve them before trying to play. A partial folder
is not a finished installation. A transfer app may ask before overwriting;
stop if you did not intend to replace an existing file.

## 3. Add it to the menu

Do this on the **old PC**. You can leave LOC SHELL running while files copy,
but wait for copying to finish before running the game.

1. Press **F2**, then **G** to open Games.
2. Open **Edit → Add…**. The shortcut is **Ctrl+A**.
3. If Add is greyed out, choose **File → Lock…**, confirm unlocking, and try again.
4. Enter a friendly **Title**, such as `My Game`.
5. Beside **Run App Commands**, choose **Browse…**.
6. In **File Name**, enter the full DOS path, for example
   `C:\LOCSHELL\GAMES\MYGAME\GAME.EXE`, and choose **OK**. You can also browse folders.
7. LOC SHELL fills in the drive, folder and launch command for you.
8. If the game has a separate DOS setup program, use the second **Browse…**
   beside **Configure Commands** to select that program. Otherwise leave it empty.
9. Choose the editor's **OK** to save the entry.

You should now see the game in the list. An **Unknown** memory status is normal
for a game that has no requirements or playing session recorded yet.

For reference, the example's generated Run App Commands look like this:

```bat
C:
CD \LOCSHELL\GAMES\MYGAME
GAME.EXE
```

If the game's start file is a batch file, Browse adds `CALL` for you. Use the
game's supplied start batch when its instructions require it. Do not choose
`INSTALL.EXE` as the ordinary play command.

**Details…** lets you add categories and mark a Favorite. You can do that later.

## 4. Set up sound, play, and check it

1. On the modern computer, choose **Server → Disconnect** in FileZilla.
   An idle FTP connection can hold up a game launch.
2. On the old PC, select the game.
3. If you added setup commands, choose **File → Configure Selected…**
   (**Ctrl+C**). Follow the game's own setup screens.
4. Choose sound hardware your PC actually has. For example, an AdLib card does
   not provide every Sound Blaster digital-effects feature.
5. Back in LOC SHELL, press **F7**, then **L** to record one learning run if wanted.
6. Press **Enter** to play. Check the picture, controls, music and sound effects
   the game supports on your hardware. Try loading/saving if applicable.
7. Quit the game normally. LOC SHELL returns. Answer the learning question if
   you armed a recording. Choose **Not sure** if you did not check it.

**Finished means:** all files arrived, the menu points to the right start file,
and you successfully played and returned. A successful upload alone cannot
establish that the game works.

## My games belong on D: instead

Your retro PC can have several drives even though this FTP account shows only
one. Do not try `/D:` or escape the FTP directory to reach another drive.

The easy options are to ask an [agent/helper](AGENT-GUIDE.md#install-a-game),
or stage the files on C: and copy them locally:

1. Create `UPLOAD` under `/LOCSHELL` if needed. Use the same upload steps,
   but put the folder in `/LOCSHELL/UPLOAD/MYGAME`.
2. Disconnect FileZilla.
3. On the old PC, choose **File → Exit** to reach DOS. Keep enough free space
   for both the staged and final copies.
4. For this **new-folder example**, type each line below, pressing Enter after
   each. If a directory already exists, DOS may say so; do not overwrite an
   existing game's files to resolve that message.

   ```bat
   MD D:\GAMES
   MD D:\GAMES\MYGAME
   XCOPY C:\LOCSHELL\UPLOAD\MYGAME\*.* D:\GAMES\MYGAME\ /S /E /V
   DIR D:\GAMES\MYGAME
   C:\LOCSHELL\LOCSHELL.BAT
   ```

   If `XCOPY` reports an error, stop before the remaining lines and resolve it.

5. Add the menu entry using `D:\GAMES\MYGAME\GAME.EXE` in Browse.
6. Keep the staged copy until the game has been checked. Remove it later only
   when you are certain which folder it is.

This copy example assumes DOS `XCOPY` is available and the prepared game has
ordinary files. Install media, hidden/system files or unusual installers need
the game's own procedure. If the copy reports an error, stop and resolve it.

## Updating a game you already play

Download a backup of its **whole folder** before changing it. Saves can be
several files and may live beside game data. Some games share installation
folders. Replacing every file with a fresh download can erase progress or sound
settings. Use the edition's documented update procedure; keep the original
until the updated game and saves have been checked.

If that folder is on D: or another drive outside your FTP account, make the
backup locally or ask the agent to stage it for download through C:.

For larger changes, ask an agent to use a verified deployment with explicit
preservation rules. It must identify the real save files first; there is no
universal `SAVE.*` rule that covers every game.

## If it does not work

| Problem | First thing to check |
| --- | --- |
| Connected, but no files appear | FTP permissions, account mapping and passive connection settings. |
| `550` or permission denied | The destination may be protected or outside your account. Ask the setup helper. |
| Files copied, no game in Games | Add the menu entry; there is no automatic folder-to-menu scan. |
| Missing file / missing directory | Press **F9**. Compare the actual file/folder against the entry's commands. |
| No music | Run the game's setup and choose the installed sound hardware. |
| Boot-mode message | Use the indicated boot choice for this PC. |
| Disk full or interrupted upload | Keep the source, fix space/connection, and complete the missing transfers before playing. |

For everyday controls, return to the [simple user guide](USER-GUIDE.md).
