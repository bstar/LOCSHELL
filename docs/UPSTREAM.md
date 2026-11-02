# Upstream relationship

Keep this repository in GitHub's mTCP fork network, retain its Git history,
and keep `upstream` pointing to https://github.com/mbbrutman/mTCP.git.
A distinct product name describes our DOS management environment; mTCP remains
the credited networking foundation. Branding does not require detaching the fork
or starting a new history.

## Reviewing upstream changes

```sh
git fetch upstream
git log --oneline HEAD..upstream/main
git diff HEAD...upstream/main -- src/
```

Review updates on a branch, merge deliberately, resolve the FTP server integration,
and run the pinned Nix build and relevant tests before deployment. Do not blindly
reset fork files to upstream. Preserve all upstream license and copyright notices,
and keep NOTICE.md accurate as the modified upstream file set changes.

We currently modify the FTP server itself, so retaining a fork is useful. A future
split into a separate management repository and an mTCP dependency would make
sense only if the integration becomes a small, maintainable upstream patch or
supported extension. No source split is needed for the present rebrand.

## Support and contributions

Use this fork's issue tracker for its dashboard, jobs, recipes and deployment
behavior. Reproduce suspected networking defects on unmodified upstream before
attributing them to mTCP. Upstream controls its own contribution policy; it
currently limits pull requests and states that its development does not use AI.
Do not imply that upstream accepts or endorses this fork's AI-assisted changes.

GitHub CLI may select the upstream repository by default when operating inside
a fork. Use explicit `--repo bstar/mTCP` for fork-specific GitHub commands.
Update that repository identifier if the fork is renamed.
