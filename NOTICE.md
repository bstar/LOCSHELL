# Attribution and provenance

**LOC SHELL** is an independently maintained project derived from **mTCP**, the DOS TCP/IP library
and applications by **Michael B. Brutman** and the contributors credited in
its source files.

- Official upstream: https://github.com/mbbrutman/mTCP
- Official mTCP home page: http://www.brutman.com/mTCP/mTCP.html
- Imported source identifies itself as the 2026-10-10 release.
- Upstream reference at the time this notice was written:
  `a6170bc7f4c951fcd625e97b5f5ef06f072bac47` (2026-10-15).
- LOC SHELL development and maintenance: **bstar / LoSs oF CoNtRoL (LoC)**.

Michael B. Brutman's and other upstream authors' copyright and license notices
remain in their files. No ownership of their contributions is claimed by LoC.
The repository retains the GNU General Public License version 3 in [LICENSE](LICENSE).
Individual files also contain their applicable license notices. LOC SHELL additions
marked GPL-3.0-or-later retain that designation; upstream notices are not relicensed.

## LOC SHELL changes

Development from the imported mTCP source began on 2026-11-01. Modified upstream files include
`src/APPS/FTPSRV/FTPSRV.CPP`, `FTPCL.CPP` and `FTPCL.H`: job handoff,
management capabilities, integrated LOC SHELL, transfer accounting, file-change
events and display behavior. The session timing and transfer observations were
added by LOC SHELL in November 2026. Original copyright and GPL notices remain
intact. The authored frontend, launch helpers and memory observer are under
`locshell/`; its private installation catalogue and observations are not shipped.

The `management/` directory adds the DOS foreground supervisor, identity and
inventory helper, host-side dosctl controller, scan flows, deployment journals,
operation history, dashboard and experimental serial observer. The `tools/`
and `nix/` directories add build, staging and verification tooling. Git history
records the individual changes and their authors.

LOC SHELL uses AI-assisted development. Upstream states that it does not use AI;
that statement describes upstream's own work, not these independently maintained
changes. LOC SHELL is not an official mTCP release, and upstream has not endorsed
or agreed to support it. Report LOC SHELL issues to its own maintainer.

The original upstream README is preserved at
[docs/upstream/README.md](docs/upstream/README.md) as historical documentation.
Its first-person statements and support instructions belong to upstream.

## Project identity

The primary repository is https://github.com/bstar/LOCSHELL, a standalone GitHub
repository. It retains the complete imported history and its mTCP provenance.
The earlier https://github.com/bstar/mTCP fork remains as a historical record;
new LOC SHELL development belongs in LOCSHELL. Changing the hosting relationship
does not change the authorship or license of inherited code.

## Distribution

Retain the license, attribution and applicable notices when redistributing this
work. Binary releases must meet the GPL's corresponding-source requirements;
include the source for the exact release, its changes, and the scripts required
to build it. Private deployment recipes, credentials, machine configuration and
inventories are not part of the public distribution.

The compact cloud/XMS music patch retains the MIT dos-scenes runtime notices.
The corrected Watcom heap-walk source and its Sybase Open Watcom Public License
are retained under `third_party/watcom-heap/`. See both dependencies for their
authors and terms.

LOC SHELL 2.3 (2026-11-08) consolidates owned DOS components and state under
C:\LOCSHELL, renames the authored frontend source/configuration, and adds pinned
Island installation, contained packages and checked legacy-directory imports.
The authenticated saver-selection extension is a LOC SHELL addition; it is not
an upstream mTCP feature. Original artwork/music files retain their credits.
