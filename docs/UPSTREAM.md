# Upstream relationship

[LOC SHELL](https://github.com/bstar/LOCSHELL) is a standalone project derived
from Michael B. Brutman's mTCP. It retains the complete Git history, mTCP's
source notices and the `upstream` remote at
https://github.com/mbbrutman/mTCP.git. Its own repository is the destination for
LOC SHELL development, issues and pull requests.

The earlier [bstar/mTCP fork](https://github.com/bstar/mTCP) remains available
with its historical pull requests. New work is published to LOCSHELL; the old
fork is not the project's primary repository. LOC SHELL is outside GitHub's
mTCP fork network, while its networking changes can still be reviewed against
upstream through ordinary Git.

## Reviewing upstream changes

```sh
git fetch upstream
git log --oneline HEAD..upstream/main
git diff HEAD...upstream/main -- src/
```

Review updates on a branch, merge deliberately, resolve the FTP server integration,
and run the pinned Nix build and relevant tests before deployment. Preserve all
upstream license and copyright notices, and keep NOTICE.md accurate as the
modified upstream file set changes.

## Support and contributions

Use [LOC SHELL's issue tracker](https://github.com/bstar/LOCSHELL/issues) for its
launcher, dashboard, jobs, recipes and deployment behavior. Reproduce suspected
networking defects on unmodified upstream before attributing them to mTCP.
Upstream controls its own contribution policy; its development statements
apply to its own project. LOC SHELL uses AI-assisted development and does not
imply that upstream accepts, endorses or supports those changes.

Use explicit `--repo bstar/LOCSHELL` for project-specific GitHub commands.
`origin` should point to `https://github.com/bstar/LOCSHELL.git`; keep `upstream`
pointing to Michael Brutman's repository for comparison and deliberate updates.
