# Pending SQL updates

`Invoke-PendingSqlUpdates.ps1` applies the pending updates of this checkout to a running CoA
database and registers them, so a realm can pick up new migrations without a worldserver restart.

It compares `data/sql/updates/pending_db_auth`, `pending_db_characters` and `pending_db_world` —
and with `-IncludeModules` also every `modules/*/data/sql/db-auth`, `db-characters` and `db-world`,
subdirectories included — with the `updates` table of the matching database and behaves like the
server's own updater:

- the hash is the SHA1 of the file read in text mode (CRLF folded to LF), upper case,
- an applied file is registered with `REPLACE INTO updates (name, hash, state, speed)`, state
  `PENDING` and the measured run time,
- a file whose recorded hash differs is applied again (`-SkipChanged` leaves it alone),
- a file whose hash is recorded under a name that no longer exists is treated as a rename, and
  only the row is renamed,
- pending and module files of one database run in file name order, together, and are registered
  with their own state (`PENDING` or `MODULE`),
- the first failure stops the run without registering that file,
- a file is applied with `--default-character-set=utf8`, the setting the server's updater uses
  (`-CharacterSet` overrides it). Under `utf8mb4` a migration that compares a user variable with a
  `utf8mb4_unicode_ci` column fails with "Illegal mix of collations",
- a leading UTF-8 BOM is skipped, because MySQL reports it as a syntax error.

A SQL file is not run in a transaction. If one fails in the middle, the statements before the
failure are already applied while the file stays unregistered, so the next run repeats them.

Stop worldserver first, or let the server apply the updates itself. Running both at once can
apply the same file twice.

## The playerbots database

A module may keep its migrations in either of two layouts, and both are read:

- `data/sql/db-<group>`, the AzerothCore convention most modules follow,
- `data/sql/<group>/updates`, which the playerbots module uses, with a fourth database of its own.

`-Databases playerbots` (included by default) applies `data/sql/playerbots/updates` to
`acore_playerbots`, the module's own `updates` table. That table's enum knows only `RELEASED`,
`ARCHIVED` and `CUSTOM`, so a file there is registered as `RELEASED` — which is also what the
module's own `updates_include` row says the directory holds. A realm without that database is
skipped rather than failed.

## Driving it

`-Json` prints one line instead of the table: `{"dryRun":…,"summary":…,"files":[…]}`, with one entry
per file carrying `Database`, `Source`, `File`, `Action`, `Milliseconds` and `Note`. That is what the
repack launcher and the manager read; a person reads the table.

## Usage

```powershell
.\apps\coa-sql\Invoke-PendingSqlUpdates.ps1 -DefaultsFile admin-client.ini -IncludeModules -DryRun
.\apps\coa-sql\Invoke-PendingSqlUpdates.ps1 -DefaultsFile admin-client.ini -IncludeModules
.\apps\coa-sql\Invoke-PendingSqlUpdates.ps1 -DefaultsFile admin-client.ini -Databases world
```

Credentials come from a MySQL client option file (`-DefaultsFile`) or from `-MysqlHost`, `-Port`,
`-User` with `-PasswordPrompt`. `mysql.exe` is searched on `PATH` and in the MySQL and MariaDB
installation directories; `-Mysql` takes an explicit path. `-AuthDb`, `-CharactersDb` and
`-WorldDb` name the schemas, `-Root` points at another checkout.

The playerbots database (`modules/mod-playerbots/data/sql/playerbots/*`) is not covered; the server
applies it itself.
