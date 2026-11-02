# Attribution and provenance

This repository is a modified derivative of **mTCP**, the DOS TCP/IP library
and applications by **Michael B. Brutman** and the contributors credited in
its source files.

- Official upstream: https://github.com/mbbrutman/mTCP
- Official mTCP home page: http://www.brutman.com/mTCP/mTCP.html
- Imported source identifies itself as the 2026-10-10 release.
- Upstream reference at the time this notice was written:
  `a6170bc7f4c951fcd625e97b5f5ef06f072bac47` (2026-10-15).
- Fork development and maintenance: **bstar / LoSs oF CoNtRoL (LoC)**.

Michael B. Brutman's and other upstream authors' copyright and license notices
remain in their files. No ownership of their contributions is claimed by LoC.
The repository retains the GNU General Public License version 3 in [LICENSE](LICENSE).
Individual files also contain their applicable license notices. Fork additions
marked GPL-3.0-or-later retain that designation; upstream notices are not relicensed.

## Changes made in this fork

Fork work began on 2026-11-01. The modified upstream file is
`src/APPS/FTPSRV/FTPSRV.CPP`: job handoff, management capability reporting,
LoC dashboard integration, event wake and idle display behavior. Its original
copyright and GPL notices remain intact.

The `management/` directory adds the DOS foreground supervisor, identity and
inventory helper, host-side dosctl controller, scan flows, deployment journals,
operation history, dashboard and experimental serial observer. The `tools/`
and `nix/` directories add build, staging and verification tooling. Git history
records the individual changes and their authors.

This fork uses AI-assisted development. Upstream states that it does not use AI;
that statement describes upstream's own work, not these independently maintained
changes. The fork is not an official mTCP release, and upstream has not endorsed
or agreed to support it. Report fork-specific issues to the fork maintainer.

The original upstream README is preserved at
[docs/upstream/README.md](docs/upstream/README.md) as historical documentation.
Its first-person statements and support instructions belong to upstream.

## Distribution

Retain the license, attribution and applicable notices when redistributing this
work. Binary releases must meet the GPL's corresponding-source requirements;
include the source for the exact release, its changes, and the scripts required
to build it. Private deployment recipes, credentials, machine configuration and
inventories are not part of the public distribution.
