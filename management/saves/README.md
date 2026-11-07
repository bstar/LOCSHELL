# Game-save sync — implementation status

This directory contains the Linux vault, HTTP API, host administration, and
filesystem reference client. **The native DOS sync client and automatic launch hooks
are not implemented yet. This is not an operational game-sync release.**
The host hub can be installed and populated with backup-only snapshots through
the existing DOS job/FTP mechanism. Native DOS sync remains unfinished. Do not
wire automatic game hooks to this reference implementation.

## Implemented

* Checksummed immutable objects; immutable saved-game snapshots with parents,
  source replica/machine identity, and hub collection time.
* SQLite durable transactions and compare-and-swap campaign publication.
  Lost acknowledgments can retry the same publication operation ID.
* Conservative three-way reconciliation: whole-world conflicts or independent
  slot merging. Missing local files are retained unless deletion is explicit.
* Preservation of every queued offline checkpoint before conflict detection.
* Explicit host conflict choices, historical restore, and campaign forks.
* Local snapshot and journal recovery reference implementation. A pending
  restore prevents another capture. Edition identity is checked before sync.
* Verified backup export/import into a new directory, and vault integrity checks.
* Authenticated HTTP/1.0 API with bounded request bodies and no request logging.

Use the existing CLI without contacting the DOS PC:

```
dosctl saves --help
dosctl saves --vault /absolute/private/vault status
dosctl saves --vault /absolute/private/vault verify
dosctl saves --vault /absolute/private/vault export /new/backup/directory
```

`create GAME COMPATIBILITY NAME` requires a SHA-256 edition fingerprint. Game
IDs: `u6`, `savage`, `martian`, `monkey-ega`, `monkey2`. Creating a campaign is
administrative metadata only; it does not verify a game profile.

`resolve` and `restore` take campaign and snapshot IDs plus `--expected-head`
and a unique `--operation` ID (16–80 ASCII letters/digits/hyphens). These publish
a preserved snapshot as a new revision; they never edit historical snapshots.
`fork CAMPAIGN SNAPSHOT NAME` creates a separate playthrough.

`serve --token-file FILE` reads a private mode-0600 file containing a random
ASCII token of at least 32 characters. It listens on loopback port8068 unless
`--bind` specifies an explicit LAN address. HTTP transport is intended for the
trusted local network, like existing mTCP FTP; it does not provide encryption.
The service template can be installed as a user service; installation is explicit.

## HTTP v1

All requests require `Authorization: Bearer TOKEN`. Bodies use Content-Length;
JSON is canonical ASCII on output. Chunked request bodies are rejected.

* GET/POST `/v1/campaigns`: list/create (`game`, `compatibility`, `name`).
* GET `/v1/campaigns/ID`: identity and current head.
* GET `/v1/campaigns/ID/history`: all snapshots, including unpublished branches.
* PUT/GET `/v1/blobs/SHA256`: original bytes, hash verified at both ends.
* POST `/v1/snapshots`: `campaign`, `files` (relative uppercase DOS8.3 paths to
  SHA256 hashes), `parents`, `replica`, `machine`. Returns immutable snapshot ID.
* GET `/v1/snapshots/ID`: immutable snapshot metadata.
* POST `/v1/campaigns/ID/head`: `snapshot`, `expected` head (null initially),
  `operation`. HTTP409 returns the actual head for reconciliation.

Uploading a snapshot does not make it active. Publication checks campaign,
parentage and all referenced content. Per-request connections own their SQLite
connection. Restore/resolve decisions remain host CLI operations in this stage.

## Remaining acceptance work

1. Implement foreground `SAVESYNC.EXE` with mTCP, DOS-native journal recovery,
   portable volume identities, enrollment/configuration, session locks, bounded
   network waits, and pending/conflict/recovery exit codes. Reference Replica
   assumes its caller holds a session lock; it is not a DOS implementation.
2. Verify game-owned files, slot independence, save-header checks, and edition
   fingerprints using disposable copies of all five original engines. Until
   then no real installation has a validated profile. Savage Empire root data
   and the shared Monkey Island directory need particular care.
3. Add DOS menu, campaign selection, and pre/post launch batches. Gate launch on
   recovery; preserve offline progress. Expose enrollment and remote sync via
   the existing serialized dosctl job mechanism.
4. Physical acceptance: backed-up current PC, second independent installation,
   and moving a card between PCs; verify unrelated assets byte for byte. Install
   the hub and enable per-install automatic hooks only after validation.

Run `python tools/test_saves.py` from the mTCP repository. These are host/reference
integration tests, not DOSBox or physical-machine acceptance.

## LOC SHELL backup card

`dosctl saves --vault /private/vault dashboard --output VAULT.STA` reads the
vault without modifying it, verifies published head metadata and emits a DOS
CRLF report. Use the ordinary verified `dosctl put` to publish it to
`/DOSSHELL/VAULT.STA`. No scheduled publisher or service is enabled by this
command. LOC SHELL displays the report timestamp, last backup and counts.
Automatic sync and local-change/conflict checks remain off.
