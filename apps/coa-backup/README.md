# CoA account and character backups

`coa_backup.py` writes SQL backups of CoA accounts, characters and guilds and restores them into the
same or another CoA server. It covers the core AzerothCore character data and the CoA tables:
Personal and Realm Bank, High-Risk chests, Manastorm progress and caches, appearance and vanity
collections, character-selection state, Challenges/Trials, Worldforged pickups and Craftsman's Codex.
When the `mod-playerbots` database exists, the per-account and per-character playerbot data is included.

It replaces the PowerShell scripts from
[WoW-export-import-scripts](https://github.com/AldebaraanMKII/WoW-export-import-scripts) for this fork.
FusionGEN and third-party transmog/bank modules are not part of CoA and are not handled.

## Requirements

- Python 3.11 or newer.
- MySQL 8 `mysql` and `mysqldump` clients (pass `--mysql`/`--mysqldump` if they are not on `PATH`).
- The auth and characters schemas on one MySQL server.
- A restore needs `CREATE`/`DROP` rights for two temporary `coa_restore_*` schemas.

Stop worldserver before a restore. A backup of a running realm contains the last saved state; the tool
warns about online characters.

## Connection

Use a MySQL client option file so passwords do not appear on the command line:

```ini
[client]
host=127.0.0.1
port=3306
user=acore
password=acore
```

Pass it with `--defaults-file`, or use `--host`, `--port`, `--user` with `--password-prompt`.
`--auth-db`, `--characters-db` and `--playerbots-db` default to `acore_auth`, `acore_characters` and
`acore_playerbots`. A missing playerbots database is skipped.

## Back up

```sh
python apps/coa-backup/coa_backup.py backup --defaults-file my.cnf
python apps/coa-backup/coa_backup.py backup --defaults-file my.cnf --accounts ALICE,BOB
python apps/coa-backup/coa_backup.py backup --defaults-file my.cnf --characters Alice --output D:/backups
python apps/coa-backup/coa_backup.py backup --defaults-file my.cnf --exclude-account-regex "^RNDBOT"
```

Each backup is a timestamped directory under `--output` (default `coa-backups/`) with one `mysqldump`
file per table in `auth/` and `characters/` and a `manifest.json` listing the accounts, characters,
guilds, row counts and file checksums. Deleted characters are not included. A guild is included when
one of the selected characters is a member; only the selected members are stored.

The files contain `CREATE TABLE` statements and filtered rows. Restore them with the `restore` command;
do not import them into a live database by hand.

`list` shows the contents of a backup:

```sh
python apps/coa-backup/coa_backup.py list coa-backups/20260922_113238
```

For recurring backups of selected characters, schedule the `backup` command, for example with the
Windows Task Scheduler or cron.

## Restore

```sh
python apps/coa-backup/coa_backup.py restore coa-backups/20260922_113238 --defaults-file my.cnf --dry-run
python apps/coa-backup/coa_backup.py restore coa-backups/20260922_113238 --defaults-file my.cnf
python apps/coa-backup/coa_backup.py restore coa-backups/20260922_113238 --defaults-file my.cnf \
    --characters Alice --target-account CAROL
```

A restore only adds data. It imports the backup into temporary schemas, assigns new IDs above the
target's current maxima and inserts everything in one transaction; any error rolls the whole restore
back. `--dry-run` performs the same work and rolls back at the end. `--report` writes the JSON result,
including the old and new account IDs and character GUIDs.

- An account whose name already exists in the target is reused; otherwise it is created with a new ID.
  `--target-account` restores the selected characters into one existing account instead.
- A character whose name already exists in the target (case-insensitive) is skipped and listed in
  `skippedCharacters`. Restoring the same backup twice therefore changes nothing.
- Character GUIDs, item GUIDs, pets, mails, equipment sets, auctions, High-Risk chests and guilds are
  renumbered, and every reference to them is rewritten, including items in bags, mail, the Personal and
  Realm Bank, guild bank, auctions, High-Risk chests and Manastorm caches.
- References to characters that are not part of the restore are cleared: friends, auction bids (with
  the bid), mail senders and item creators.
- Realm Bank contents are restored only for accounts the restore creates. For an existing account the
  target's Realm Bank is kept; the report lists the items and gold that were not restored.
- Account collections (appearances, vanity items) are merged into existing accounts.
- Guilds whose name already exists in the target are skipped. When the guild master is not restored,
  worldserver promotes another member at startup.
- Playerbots: account types, account keys and account links (both accounts restored) are kept from the
  target when the account already exists there. Random-bot state, stored bot strategies and values,
  preferred mounts, per-player custom strategies and guild tasks follow their character and guild.
  Shared playerbot data (texts, caches, global strategies, travel nodes, name pools) is not backed up.
  Random-bot accounts are ordinary accounts; exclude them with `--exclude-account-regex "^RNDBOT"`.
- Instance binds, corpses, groups, arena teams and log tables are not restored. `realmcharacters` is
  recalculated for the affected accounts (`--realm-id`, default 1).

## Checks

```sh
python apps/coa-backup/test_coa_backup.py
python apps/coa-backup/test_mysql.py --mysql-bin "C:/Program Files/MySQL/MySQL Server 8.0/bin"
```

The MySQL check starts a disposable server without TCP, builds both schemas from the repository SQL,
backs up a seeded source realm and restores it into a pre-populated target. It verifies the remapped
rows, a no-op dry run, an unchanged source, an idempotent second restore and a single-character
restore into another account.
