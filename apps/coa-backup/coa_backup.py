import argparse
from dataclasses import dataclass
import datetime
import getpass
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import uuid


DESCRIPTION = "Back up and restore CoA accounts, characters, guilds and playerbot data as SQL."
GROUPS = ("auth", "characters", "playerbots")
FORMAT = 1
DEFAULT_OUTPUT = Path("coa-backups")
IDENTIFIER = re.compile(r"[A-Za-z_][A-Za-z_0-9]*\Z")
PROCESS_OPTIONS = {"creationflags": subprocess.CREATE_NO_WINDOW} if os.name == "nt" else {}
EMPTY_SET = "SELECT NULL FROM DUAL WHERE FALSE"
MAPS = ("account", "character", "guild", "pet", "mail", "equipset", "auction", "chest", "item")
UNSAFE_PATH = re.compile(r'[<>:"/\\|?*\x00-\x1f]')
MAX_NAMES_IN_DIRECTORY = 3
LOW_GUID = "0xFFFFFFFF"
HIGH_GUID = "0xFFFFFFFF00000000"


def identifier(value):
    if not IDENTIFIER.fullmatch(value):
        raise ValueError("Invalid database/table/column identifier: " + value)
    return "`" + value + "`"


def literal(value):
    escaped = str(value).replace("\\", "\\\\").replace("'", "\\'").replace("\0", "\\0")
    return "'" + escaped + "'"


def literals(values):
    return ",".join(literal(value) for value in values)


def text(column):
    return "CONVERT(" + column + " USING utf8mb4)"


def ref(kind, column):
    return "(SELECT n FROM {map_" + kind + "} WHERE o = " + column + ")"


def has(kind, column):
    return column + " IN (SELECT o FROM {map_" + kind + "})"


def ref_or_zero(kind, column):
    return "COALESCE(" + ref(kind, column) + ", 0)"


@dataclass(frozen=True)
class Table:
    name: str
    select: str
    keep: str
    values: tuple = ()
    mode: str = "insert"
    database: str = "characters"


def guid_table(name, column="guid", mode="insert"):
    return Table(name, column + " IN ({CHR})", has("character", "s." + column),
                 ((column, ref("character", "s." + column)),), mode)


def account_table(name, column, mode="ignore"):
    return Table(name, column + " IN ({ACC})", has("account", "s." + column),
                 ((column, ref("account", "s." + column)),), mode)


NEW_ACCOUNT = "s.{column} IN (SELECT o FROM {map_account} WHERE e = 0)"
BANK_OWNER_SELECT = "(owner_kind = 0 AND owner_id IN ({CHR})) OR (owner_kind = 1 AND owner_id IN ({ACC}))"
NEW_ACCOUNT_REF = "(SELECT n FROM {map_account} WHERE o = s.owner_id AND e = 0)"
BANK_OWNER = ("CASE s.owner_kind WHEN 0 THEN " + ref("character", "s.owner_id") + " WHEN 1 THEN "
              + NEW_ACCOUNT_REF + " END")
BANK_KEEP = BANK_OWNER + " IS NOT NULL"
TRIAL_KEEP = has("character", "s.guid") + " AND " + text("s.trialId") + " IN (SELECT o FROM {map_trial})"

AUTH_TABLES = (
    Table("account", "id IN ({ACC})", NEW_ACCOUNT.replace("{column}", "id"),
          (("id", ref("account", "s.id")), ("online", "0")), database="auth"),
    Table("account_access", "id IN ({ACC})", NEW_ACCOUNT.replace("{column}", "id"),
          (("id", ref("account", "s.id")),), database="auth"),
    Table("account_banned", "id IN ({ACC})", NEW_ACCOUNT.replace("{column}", "id"),
          (("id", ref("account", "s.id")),), database="auth"),
    Table("account_muted", "guid IN ({ACC})", NEW_ACCOUNT.replace("{column}", "guid"),
          (("guid", ref("account", "s.guid")),), database="auth"),
)

CHARACTER_TABLES = (
    Table("characters", "guid IN ({CHR})", has("character", "s.guid"),
          (("guid", ref("character", "s.guid")), ("account", ref("account", "s.account")),
           ("online", "0"), ("instance_id", "0"))),
    *(guid_table(name) for name in (
        "character_account_data", "character_achievement", "character_achievement_offline_updates",
        "character_achievement_progress", "character_action", "character_arena_stats", "character_banned",
        "character_battleground_random", "character_brew_of_the_month", "character_declinedname",
        "character_entry_point", "character_glyphs", "character_homebind", "character_queststatus",
        "character_queststatus_daily", "character_queststatus_monthly", "character_queststatus_rewarded",
        "character_queststatus_seasonal", "character_queststatus_weekly", "character_reputation",
        "character_settings", "character_skills", "character_spell", "character_spell_cooldown",
        "character_stats", "character_talent", "battleground_deserters", "mail_server_character",
        "character_appearance", "character_appearance_settings", "character_ascension_state",
        "ascension_manastorm_bonus", "ascension_manastorm_xp", "ascension_manastorm_loadout",
        "coa_challenge_completion", "coa_challenge_failure", "coa_character_challenge",
        "coa_character_condition", "coa_character_fatigue", "coa_character_gamemode",
        "coa_character_gamemode_lives", "coa_character_objective", "coa_character_survival",
        "character_worldforged_loot", "mod_craftsmans_codex", "guild_member_withdraw")),
    *(guid_table(name, mode="ignore") for name in (
        "coa_custom_trial_active", "coa_custom_trial_completion", "coa_custom_trial_vote")),
    Table("coa_custom_trial", "guid IN ({CHR})", TRIAL_KEEP, (("guid", ref("character", "s.guid")),)),
    Table("coa_custom_trial_entry", "guid IN ({CHR})", TRIAL_KEEP, (("guid", ref("character", "s.guid")),)),
    account_table("account_data", "accountId"),
    account_table("account_tutorial", "accountId"),
    account_table("account_appearance_collection", "account_id"),
    account_table("account_vanity_collection", "account_id"),
    account_table("account_ascension_settings", "account_id"),
    Table("character_social", "guid IN ({CHR})",
          has("character", "s.guid") + " AND " + has("character", "s.friend"),
          (("guid", ref("character", "s.guid")), ("friend", ref("character", "s.friend")))),
    Table("character_pet", "owner IN ({CHR})", has("pet", "s.id"),
          (("id", ref("pet", "s.id")), ("owner", ref("character", "s.owner")))),
    Table("character_pet_declinedname", "owner IN ({CHR})", has("pet", "s.id"),
          (("id", ref("pet", "s.id")), ("owner", ref("character", "s.owner")))),
    *(Table(name, "guid IN ({PET})", has("pet", "s.guid"), (("guid", ref("pet", "s.guid")),))
      for name in ("pet_aura", "pet_spell", "pet_spell_cooldown")),
    Table("highrisk_chest", "owner IN ({CHR})", has("chest", "s.id"),
          (("id", ref("chest", "s.id")), ("owner", ref("character", "s.owner")),
           ("gold_claimant", ref_or_zero("character", "s.gold_claimant")))),
    Table("item_instance", "guid IN ({ITEM})", has("item", "s.guid"),
          (("guid", ref("item", "s.guid")), ("owner_guid", ref_or_zero("character", "s.owner_guid")),
           ("creatorGuid", ref_or_zero("character", "s.creatorGuid")),
           ("giftCreatorGuid", ref_or_zero("character", "s.giftCreatorGuid")))),
    Table("character_aura", "guid IN ({CHR})",
          has("character", "s.guid") + " AND (s.itemGuid = 0 OR "
          + has("item", "(s.itemGuid & " + LOW_GUID + ")") + ")",
          (("guid", ref("character", "s.guid")),
           ("casterGuid", "CASE WHEN s.casterGuid = s.guid THEN " + ref("character", "s.guid")
            + " ELSE s.casterGuid END"),
           ("itemGuid", "CASE WHEN s.itemGuid = 0 THEN 0 ELSE (s.itemGuid & " + HIGH_GUID + ") | "
            + ref("item", "(s.itemGuid & " + LOW_GUID + ")") + " END"))),
    Table("character_inventory", "guid IN ({CHR})",
          has("character", "s.guid") + " AND " + has("item", "s.item") + " AND (s.bag = 0 OR "
          + has("item", "s.bag") + ")",
          (("guid", ref("character", "s.guid")), ("item", ref("item", "s.item")),
           ("bag", "CASE WHEN s.bag = 0 THEN 0 ELSE " + ref("item", "s.bag") + " END"))),
    Table("character_gifts", "guid IN ({CHR})",
          has("character", "s.guid") + " AND " + has("item", "s.item_guid"),
          (("guid", ref("character", "s.guid")), ("item_guid", ref("item", "s.item_guid")))),
    Table("item_refund_instance", "item_guid IN ({ITEM})",
          has("item", "s.item_guid") + " AND " + has("character", "s.player_guid"),
          (("item_guid", ref("item", "s.item_guid")), ("player_guid", ref("character", "s.player_guid")))),
    Table("item_loot_storage", "containerGUID IN ({ITEM})", has("item", "s.containerGUID"),
          (("containerGUID", ref("item", "s.containerGUID")),)),
    Table("character_equipmentsets", "guid IN ({CHR})", has("equipset", "s.setguid"),
          (("guid", ref("character", "s.guid")), ("setguid", ref("equipset", "s.setguid")),
           *(("item" + str(slot), ref_or_zero("item", "s.item" + str(slot))) for slot in range(19)))),
    Table("mail", "receiver IN ({CHR})", has("mail", "s.id"),
          (("id", ref("mail", "s.id")), ("receiver", ref("character", "s.receiver")),
           ("sender", "CASE WHEN s.messageType = 0 THEN " + ref_or_zero("character", "s.sender")
            + " ELSE s.sender END"))),
    Table("mail_items", "mail_id IN ({MAIL})",
          has("mail", "s.mail_id") + " AND " + has("item", "s.item_guid"),
          (("mail_id", ref("mail", "s.mail_id")), ("item_guid", ref("item", "s.item_guid")),
           ("receiver", ref("character", "s.receiver")))),
    Table("auctionhouse", "itemowner IN ({CHR})",
          has("auction", "s.id") + " AND " + has("item", "s.itemguid"),
          (("id", ref("auction", "s.id")), ("itemguid", ref("item", "s.itemguid")),
           ("itemowner", ref("character", "s.itemowner")), ("buyguid", ref_or_zero("character", "s.buyguid")),
           ("lastbid", "CASE WHEN " + ref("character", "s.buyguid") + " IS NULL THEN 0 ELSE s.lastbid END"))),
    Table("mod_ascension_bank_tab", BANK_OWNER_SELECT, BANK_KEEP, (("owner_id", BANK_OWNER),), "ignore"),
    Table("mod_ascension_bank_money", BANK_OWNER_SELECT, BANK_KEEP, (("owner_id", BANK_OWNER),), "ignore"),
    Table("mod_ascension_bank_item", BANK_OWNER_SELECT, BANK_KEEP + " AND " + has("item", "s.item_guid"),
          (("owner_id", BANK_OWNER), ("item_guid", ref("item", "s.item_guid")))),
    Table("mod_ascension_bank_log", BANK_OWNER_SELECT, BANK_KEEP,
          (("owner_id", BANK_OWNER), ("player_guid", ref_or_zero("character", "s.player_guid")))),
    Table("highrisk_chest_item", "claimant = 0 AND chest_id IN ({CHEST})",
          has("chest", "s.chest_id") + " AND " + has("item", "s.item_guid"),
          (("chest_id", ref("chest", "s.chest_id")), ("item_guid", ref("item", "s.item_guid")))),
    Table("ascension_manastorm_clear", "guid IN ({CHR})", has("character", "s.guid"),
          (("guid", ref("character", "s.guid")), ("mail_id", ref_or_zero("mail", "s.mail_id")))),
    Table("ascension_manastorm_cache", "guid IN ({CHR})",
          has("character", "s.guid") + " AND " + has("item", "s.item"),
          (("guid", ref("character", "s.guid")), ("item", ref("item", "s.item")))),
    Table("coa_character_looted_item", "guid IN ({CHR})",
          has("character", "s.guid") + " AND " + has("item", "s.itemGuid"),
          (("guid", ref("character", "s.guid")), ("itemGuid", ref("item", "s.itemGuid")))),
    Table("guild", "guildid IN ({GLD})", has("guild", "s.guildid"),
          (("guildid", ref("guild", "s.guildid")), ("leaderguid", ref_or_zero("character", "s.leaderguid")))),
    *(Table(name, "guildid IN ({GLD})", has("guild", "s.guildid"), (("guildid", ref("guild", "s.guildid")),))
      for name in ("guild_rank", "guild_bank_tab", "guild_bank_right")),
    Table("guild_member", "guildid IN ({GLD}) AND guid IN ({CHR})",
          has("guild", "s.guildid") + " AND " + has("character", "s.guid"),
          (("guildid", ref("guild", "s.guildid")), ("guid", ref("character", "s.guid")))),
    Table("guild_bank_item", "guildid IN ({GLD})",
          has("guild", "s.guildid") + " AND " + has("item", "s.item_guid"),
          (("guildid", ref("guild", "s.guildid")), ("item_guid", ref("item", "s.item_guid")))),
)

PLAYERBOT_TABLES = (
    Table("playerbots_account_type", "account_id IN ({ACC})", has("account", "s.account_id"),
          (("account_id", ref("account", "s.account_id")),), "ignore", "playerbots"),
    Table("playerbots_account_keys", "account_id IN ({ACC})", has("account", "s.account_id"),
          (("account_id", ref("account", "s.account_id")),), "ignore", "playerbots"),
    Table("playerbots_account_links", "account_id IN ({ACC}) AND linked_account_id IN ({ACC})",
          has("account", "s.account_id") + " AND " + has("account", "s.linked_account_id"),
          (("account_id", ref("account", "s.account_id")),
           ("linked_account_id", ref("account", "s.linked_account_id"))), "ignore", "playerbots"),
    Table("playerbots_random_bots", "bot IN ({CHR})", has("character", "s.bot"),
          (("bot", ref("character", "s.bot")),), database="playerbots"),
    Table("playerbots_db_store", "guid IN ({CHR})", has("character", "s.guid"),
          (("guid", ref("character", "s.guid")),), database="playerbots"),
    Table("playerbots_preferred_mounts", "guid IN ({CHR})", has("character", "s.guid"),
          (("guid", ref("character", "s.guid")),), database="playerbots"),
    Table("playerbots_custom_strategy", "owner IN ({CHR})", has("character", "s.owner"),
          (("owner", ref("character", "s.owner")),), database="playerbots"),
    Table("playerbots_guild_tasks", "owner IN ({CHR}) AND guildid IN ({GLD})",
          has("character", "s.owner") + " AND " + has("guild", "s.guildid"),
          (("owner", ref("character", "s.owner")), ("guildid", ref("guild", "s.guildid"))),
          database="playerbots"),
)

TABLES = AUTH_TABLES + CHARACTER_TABLES + PLAYERBOT_TABLES

ITEM_SOURCES = (
    ("character_inventory", "SELECT item FROM {C}.character_inventory WHERE guid IN ({CHR})"),
    ("mail_items", "SELECT item_guid FROM {C}.mail_items WHERE mail_id IN ({MAIL})"),
    ("auctionhouse", "SELECT itemguid FROM {C}.auctionhouse WHERE itemowner IN ({CHR})"),
    ("mod_ascension_bank_item", "SELECT item_guid FROM {C}.mod_ascension_bank_item WHERE " + BANK_OWNER_SELECT),
    ("highrisk_chest_item", "SELECT item_guid FROM {C}.highrisk_chest_item WHERE claimant = 0 "
                            "AND chest_id IN ({CHEST})"),
    ("ascension_manastorm_cache", "SELECT item FROM {C}.ascension_manastorm_cache WHERE guid IN ({CHR})"),
    ("guild_bank_item", "SELECT item_guid FROM {C}.guild_bank_item WHERE guildid IN ({GLD})"),
)


def install_directories():
    roots = [os.environ.get(name) for name in ("ProgramFiles", "ProgramW6432", "ProgramFiles(x86)")]
    found = []
    for root in dict.fromkeys(root for root in roots if root):
        for pattern in ("MySQL/MySQL Server */bin", "MariaDB */bin"):
            found += sorted(Path(root).glob(pattern), reverse=True)
    return found


def executable(path):
    if path.parent != Path(".") or path.is_file():
        if not path.is_file() and not shutil.which(str(path)):
            raise ValueError("--" + path.stem + " does not point to an existing file: " + str(path))
        return str(path)
    located = shutil.which(str(path))
    if located:
        return located
    suffix = ".exe" if os.name == "nt" else ""
    for directory in install_directories():
        candidate = directory / (path.name + suffix)
        if candidate.is_file():
            return str(candidate)
    raise ValueError(path.name + " was not found on PATH or in a MySQL/MariaDB installation; pass --"
                     + path.name + " with the full path to " + path.name + suffix)


class Connection:
    def __init__(self, args):
        self.mysql = executable(args.mysql)
        self.mysqldump = executable(args.mysqldump)
        self.temporary = None
        if args.password_prompt and args.defaults_file:
            raise ValueError("Use either --defaults-file or --password-prompt")
        if args.defaults_file:
            options = ["--defaults-extra-file=" + str(args.defaults_file.resolve())]
        elif args.password_prompt:
            password = getpass.getpass("MySQL password: ")
            handle, name = tempfile.mkstemp(prefix="coa-backup-", suffix=".cnf")
            with os.fdopen(handle, "w", encoding="utf-8", newline="\n") as stream:
                stream.write("[client]\npassword=\"" + password.replace("\\", "\\\\").replace("\"", "\\\"")
                             + "\"\n")
            self.temporary = Path(name)
            options = ["--defaults-extra-file=" + name]
        else:
            options = ["--no-defaults"]
        for flag in ("host", "port", "user", "socket", "protocol"):
            value = getattr(args, flag)
            if value is not None:
                options.append("--" + flag + "=" + str(value))
        self.options = options

    def close(self):
        if self.temporary is not None:
            self.temporary.unlink(missing_ok=True)
            self.temporary = None

    def client(self, database=None):
        command = [self.mysql, *self.options, "--batch", "--raw", "--skip-column-names",
                   "--default-character-set=utf8mb4", "--max-allowed-packet=1GB", "--connect-timeout=10"]
        if database:
            command.append("--database=" + database)
        return command

    def run(self, sql, database=None):
        result = subprocess.run(self.client(database), input=sql.encode("utf-8"), capture_output=True,
                                **PROCESS_OPTIONS)
        if result.returncode:
            raise RuntimeError(result.stderr.decode("utf-8", errors="replace").strip() or "mysql failed")
        return [line.split("\t") for line in result.stdout.decode("utf-8").splitlines()]

    def load(self, path, database):
        with tempfile.TemporaryFile() as errors, path.open("rb") as stream:
            process = subprocess.Popen(self.client(database), stdin=subprocess.PIPE, stdout=subprocess.DEVNULL,
                                       stderr=errors, **PROCESS_OPTIONS)
            try:
                process.stdin.write(b"SET NAMES utf8mb4; SET FOREIGN_KEY_CHECKS=0;\n")
                while block := stream.read(1024 * 1024):
                    process.stdin.write(block)
                process.stdin.close()
                status = process.wait()
            except BrokenPipeError:
                status = process.wait()
            if status:
                errors.seek(0)
                raise RuntimeError(path.name + ": " + errors.read().decode("utf-8", errors="replace").strip())

    def dump_options(self):
        result = subprocess.run([self.mysqldump, "--help"], capture_output=True, **PROCESS_OPTIONS)
        help_text = result.stdout.decode("utf-8", errors="replace")
        options = ["--single-transaction", "--skip-lock-tables", "--skip-add-drop-table", "--no-tablespaces",
                   "--skip-triggers", "--skip-comments", "--skip-dump-date", "--hex-blob", "--complete-insert",
                   "--default-character-set=utf8mb4", "--max-allowed-packet=1GB"]
        if "--set-gtid-purged" in help_text:
            options.append("--set-gtid-purged=OFF")
        if "--column-statistics" in help_text:
            options.append("--column-statistics=0")
        return options

    def dump(self, database, table, where, path, options):
        command = [self.mysqldump, *self.options, *options, "--result-file=" + str(path), "--where=" + where,
                   database, table]
        result = subprocess.run(command, capture_output=True, **PROCESS_OPTIONS)
        if result.returncode:
            raise RuntimeError(table + ": " + result.stderr.decode("utf-8", errors="replace").strip())


def existing_tables(connection, database):
    rows = connection.run("SELECT TABLE_NAME FROM information_schema.TABLES WHERE TABLE_SCHEMA = "
                          + literal(database) + " AND TABLE_TYPE = 'BASE TABLE';")
    return {row[0] for row in rows}


def columns(connection, database):
    rows = connection.run("SELECT TABLE_NAME, COLUMN_NAME, EXTRA FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = "
                          + literal(database) + " ORDER BY TABLE_NAME, ORDINAL_POSITION;")
    found = {}
    for table, column, extra in rows:
        found.setdefault(table, []).append((column, extra.lower()))
    return found


def source_sets(auth, characters, present, args):
    a, c = identifier(auth), identifier(characters)
    account_filters = []
    if args.accounts:
        account_filters.append(text("username") + " IN (" + literals(args.accounts) + ")")
    if args.account_regex:
        account_filters.append("username REGEXP " + literal(args.account_regex))
    if args.exclude_account_regex:
        account_filters.append("username NOT REGEXP " + literal(args.exclude_account_regex))
    name_filter = ""
    if args.characters:
        names = text("name") + " IN (" + literals(args.characters) + ")"
        account_filters.append("id IN (SELECT account FROM " + c + ".characters WHERE " + names + ")")
        name_filter = " AND " + names
    sets = {"A": a, "C": c}
    sets["ACC"] = "SELECT id FROM " + a + ".account" + (" WHERE " + " AND ".join(account_filters)
                                                         if account_filters else "")
    sets["CHR"] = "SELECT guid FROM " + c + ".characters WHERE account IN (" + sets["ACC"] + ")" + name_filter
    optional = {"GLD": ("guild_member", "SELECT guildid FROM {C}.guild_member WHERE guid IN ({CHR})"),
                "MAIL": ("mail", "SELECT id FROM {C}.mail WHERE receiver IN ({CHR})"),
                "PET": ("character_pet", "SELECT id FROM {C}.character_pet WHERE owner IN ({CHR})"),
                "CHEST": ("highrisk_chest", "SELECT id FROM {C}.highrisk_chest WHERE owner IN ({CHR})")}
    for key, (table, sql) in optional.items():
        sets[key] = sql.format(**sets) if table in present else EMPTY_SET
    parts = [sql.format(**sets) for table, sql in ITEM_SOURCES if table in present]
    sets["ITEM"] = " UNION ".join(parts) if parts else EMPTY_SET
    return sets


def databases(args):
    return {"auth": args.auth_db, "characters": args.characters_db, "playerbots": args.playerbots_db}


def backup_name(args, accounts, characters, stamp):
    if args.characters:
        kind, names = "character", characters
    elif args.accounts:
        kind, names = "account", accounts
    else:
        return stamp
    shown = names[:MAX_NAMES_IN_DIRECTORY]
    label = "+".join(shown)
    if len(names) > len(shown):
        label += "+" + str(len(names) - len(shown)) + "more"
    label = UNSAFE_PATH.sub("_", label).rstrip(". ")
    return kind + "_" + label + "_" + stamp


def backup(connection, args):
    names = databases(args)
    auth, characters = names["auth"], names["characters"]
    present = {group: existing_tables(connection, name) for group, name in names.items()}
    for group, required in (("auth", "account"), ("characters", "characters")):
        if required not in present[group]:
            raise ValueError("Database " + names[group] + " has no table " + required)
    sets = source_sets(auth, characters, present["characters"], args)
    a, c = sets["A"], sets["C"]
    accounts = connection.run("SELECT id, username FROM " + a + ".account WHERE id IN (" + sets["ACC"]
                              + ") ORDER BY id;")
    if not accounts:
        raise ValueError("No account matches the selection")
    character_rows = connection.run(
        "SELECT guid, account, name, race, class, gender, level, money, online FROM " + c
        + ".characters WHERE guid IN (" + sets["CHR"] + ") ORDER BY account, guid;")
    guilds = []
    if "guild" in present["characters"] and "guild_member" in present["characters"]:
        guilds = connection.run("SELECT guildid, name FROM " + c + ".guild WHERE guildid IN (" + sets["GLD"]
                                + ") ORDER BY guildid;")
    online = [row[2] for row in character_rows if row[8] != "0"]
    if online:
        print("Warning: characters are online, stop worldserver for a consistent backup: " + ", ".join(online),
              file=sys.stderr)
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%d_%H%M%S")
    name = backup_name(args, [row[1] for row in accounts], [row[2] for row in character_rows], stamp)
    target = args.output / name
    if target.exists():
        raise ValueError("Backup directory already exists: " + str(target))
    options = connection.dump_options()
    files, missing = [], []
    for table in TABLES:
        group = table.database
        database = names[group]
        if table.name not in present[group]:
            missing.append(group + "." + table.name)
            continue
        where = table.select.format(**sets)
        path = target / group / (table.name + ".sql")
        path.parent.mkdir(parents=True, exist_ok=True)
        connection.dump(database, table.name, where, path, options)
        rows = int(connection.run("SELECT COUNT(*) FROM " + identifier(database) + "." + identifier(table.name)
                                  + " WHERE " + where + ";")[0][0])
        files.append({"database": group, "table": table.name, "file": group + "/" + table.name + ".sql",
                      "rows": rows, "sha256": hashlib.sha256(path.read_bytes()).hexdigest()})
    manifest = {
        "format": FORMAT,
        "created": stamp,
        "source": names,
        "accounts": [{"id": int(row[0]), "username": row[1]} for row in accounts],
        "characters": [{"guid": int(row[0]), "account": int(row[1]), "name": row[2], "race": int(row[3]),
                        "class": int(row[4]), "gender": int(row[5]), "level": int(row[6]), "money": int(row[7])}
                       for row in character_rows],
        "guilds": [{"guildid": int(row[0]), "name": row[1]} for row in guilds],
        "files": files,
        "missingTables": missing,
    }
    (target / "manifest.json").write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n",
                                          encoding="utf-8", newline="\n")
    return target, manifest


def resolve_backup(path):
    candidates = [path]
    if not path.is_absolute():
        candidates.append(DEFAULT_OUTPUT / path)
    for candidate in candidates:
        if (candidate / "manifest.json").is_file():
            return candidate
    tried = ", ".join(str(candidate.resolve()) for candidate in candidates)
    raise ValueError("No backup (manifest.json) found at " + tried
                     + "; run 'list' to see the backups in " + str(DEFAULT_OUTPUT))


def available_backups(root=None):
    root = root or DEFAULT_OUTPUT
    if not root.is_dir():
        return "No backups in " + str(root.resolve())
    names = sorted(entry.name for entry in root.iterdir() if (entry / "manifest.json").is_file())
    return "\n".join(names) if names else "No backups in " + str(root.resolve())


def load_manifest(directory):
    path = directory / "manifest.json"
    if not path.is_file():
        raise ValueError("No manifest.json in " + str(directory))
    manifest = json.loads(path.read_text(encoding="utf-8"))
    if manifest.get("format") != FORMAT:
        raise ValueError("Unsupported backup format: " + str(manifest.get("format")))
    for entry in manifest["files"]:
        file = directory / entry["file"]
        if not file.is_file() or hashlib.sha256(file.read_bytes()).hexdigest() != entry["sha256"]:
            raise ValueError("Backup file is missing or changed: " + entry["file"])
    return manifest


class Restore:
    def __init__(self, connection, directory, args):
        self.connection = connection
        self.directory = directory
        self.args = args
        self.manifest = load_manifest(directory)
        token = uuid.uuid4().hex[:12]
        self.targets = databases(args)
        self.stages = {group: "coa_restore_" + token + "_" + group for group in GROUPS}
        self.stage = self.stages["characters"]
        self.names = {}
        for group in GROUPS:
            suffix = "" if group == "characters" else "_" + group
            self.names["target" + suffix] = identifier(self.targets[group])
            self.names["stage" + suffix] = identifier(self.stages[group])
        for kind in MAPS + ("trial",):
            self.names["map_" + kind] = self.names["stage"] + "." + identifier("map_" + kind)

    def name_filter(self):
        if not self.args.characters:
            return ""
        return " AND " + text("s.name") + " IN (" + literals(self.args.characters) + ")"

    def sql(self, template):
        return template.format(**self.names)

    def stage_backup(self):
        self.connection.run(" ".join("CREATE DATABASE " + identifier(name) + " CHARACTER SET utf8mb4;"
                                     for name in self.stages.values()))
        for entry in self.manifest["files"]:
            database = self.stages[entry["database"]]
            self.connection.load(self.directory / entry["file"], database)
            rows = int(self.connection.run("SELECT COUNT(*) FROM " + identifier(database) + "."
                                           + identifier(entry["table"]) + ";")[0][0])
            if rows != entry["rows"]:
                raise ValueError("Staged row count differs for " + entry["table"])
        maps = []
        for kind in MAPS:
            extra = ", e TINYINT UNSIGNED NOT NULL DEFAULT 0" if kind == "account" else ""
            maps.append("CREATE TABLE {map_" + kind + "} (o BIGINT UNSIGNED NOT NULL PRIMARY KEY, "
                        "n BIGINT UNSIGNED NOT NULL" + extra + ") ENGINE=InnoDB;")
        maps.append("CREATE TABLE {map_trial} (o VARCHAR(64) CHARACTER SET utf8mb4 NOT NULL PRIMARY KEY) "
                    "ENGINE=InnoDB;")
        self.connection.run(self.sql(" ".join(maps)))

    def drop_stage(self):
        self.connection.run(" ".join("DROP DATABASE IF EXISTS " + identifier(name) + ";"
                                     for name in self.stages.values()))

    def fresh(self, kind, table, key, where):
        base = "(SELECT COALESCE(MAX(" + key + "), 0) FROM {target}." + table + ")"
        return ("INSERT INTO {map_" + kind + "} (o, n) SELECT r.o, " + base
                + " + ROW_NUMBER() OVER (ORDER BY r.o) FROM (SELECT DISTINCT s." + key + " AS o FROM {stage}."
                + table + " s WHERE " + where + ") r;")

    def mapping_sql(self, staged, target):
        args = self.args
        statements = []
        selected = []
        if args.characters:
            selected.append("s.id IN (SELECT c.account FROM {stage}.characters c WHERE " + text("c.name") + " IN ("
                            + literals(args.characters) + "))")
        if args.accounts:
            selected.append(text("s.username") + " IN (" + literals(args.accounts) + ")")
        account_where = " AND ".join(selected) if selected else "TRUE"
        if args.target_account:
            statements.append("INSERT INTO {map_account} (o, n, e) SELECT s.id, t.id, 1 FROM {stage_auth}.account s "
                              "JOIN {target_auth}.account t ON " + text("t.username") + " = "
                              + literal(args.target_account) + " WHERE " + account_where + ";")
        else:
            statements.append("INSERT INTO {map_account} (o, n, e) SELECT s.id, t.id, 1 FROM {stage_auth}.account s "
                              "JOIN {target_auth}.account t ON " + text("t.username") + " = " + text("s.username")
                              + " WHERE " + account_where + ";")
            statements.append("INSERT INTO {map_account} (o, n, e) SELECT r.id, (SELECT COALESCE(MAX(id), 0) FROM "
                              "{target_auth}.account) + ROW_NUMBER() OVER (ORDER BY r.id), 0 FROM (SELECT s.id FROM "
                              "{stage_auth}.account s WHERE " + account_where
                              + " AND s.id NOT IN (SELECT o FROM {map_account})) r;")
        name_where = self.name_filter()
        statements.append(self.fresh(
            "character", "characters", "guid", has("account", "s.account") + name_where + " AND NOT EXISTS (SELECT 1 "
            "FROM {target}.characters t WHERE " + text("t.name") + " = " + text("s.name") + ")"))
        if "guild" in staged and "guild_member" in staged:
            statements.append(self.fresh(
                "guild", "guild", "guildid", "s.guildid IN (SELECT m.guildid FROM {stage}.guild_member m WHERE "
                + has("character", "m.guid") + ") AND NOT EXISTS (SELECT 1 FROM {target}.guild t WHERE "
                + text("t.name") + " = " + text("s.name") + ")"))
        for kind, table, key, where in (
                ("pet", "character_pet", "id", has("character", "s.owner")),
                ("mail", "mail", "id", has("character", "s.receiver")),
                ("equipset", "character_equipmentsets", "setguid", has("character", "s.guid")),
                ("auction", "auctionhouse", "id", has("character", "s.itemowner")),
                ("chest", "highrisk_chest", "id", has("character", "s.owner"))):
            if table in staged and table in target:
                statements.append(self.fresh(kind, table, key, where))
        sources = (
            ("character_inventory", "SELECT s.item AS o FROM {stage}.character_inventory s WHERE "
             + has("character", "s.guid")),
            ("mail_items", "SELECT s.item_guid AS o FROM {stage}.mail_items s WHERE " + has("mail", "s.mail_id")),
            ("auctionhouse", "SELECT s.itemguid AS o FROM {stage}.auctionhouse s WHERE " + has("auction", "s.id")),
            ("mod_ascension_bank_item", "SELECT s.item_guid AS o FROM {stage}.mod_ascension_bank_item s WHERE "
             "(s.owner_kind = 0 AND " + has("character", "s.owner_id") + ") OR (s.owner_kind = 1 AND "
             + NEW_ACCOUNT_REF + " IS NOT NULL)"),
            ("highrisk_chest_item", "SELECT s.item_guid AS o FROM {stage}.highrisk_chest_item s "
             "WHERE s.claimant = 0 AND " + has("chest", "s.chest_id")),
            ("ascension_manastorm_cache", "SELECT s.item AS o FROM {stage}.ascension_manastorm_cache s WHERE "
             + has("character", "s.guid")),
            ("guild_bank_item", "SELECT s.item_guid AS o FROM {stage}.guild_bank_item s WHERE "
             + has("guild", "s.guildid")),
        )
        parts = [sql for table, sql in sources if table in staged and table in target]
        if parts and "item_instance" in staged:
            statements.append(
                "INSERT INTO {map_item} (o, n) SELECT r.o, (SELECT COALESCE(MAX(guid), 0) FROM "
                "{target}.item_instance) + ROW_NUMBER() OVER (ORDER BY r.o) FROM (SELECT DISTINCT u.o FROM ("
                + " UNION ALL ".join(parts) + ") u WHERE u.o IN (SELECT guid FROM {stage}.item_instance)) r;")
        if "coa_custom_trial" in staged and "coa_custom_trial" in target:
            statements.append("INSERT INTO {map_trial} (o) SELECT DISTINCT " + text("s.trialId")
                              + " FROM {stage}.coa_custom_trial s WHERE " + has("character", "s.guid")
                              + " AND NOT EXISTS (SELECT 1 FROM {target}.coa_custom_trial t WHERE "
                              + text("t.trialId") + " = " + text("s.trialId") + ");")
        return statements

    def insert_sql(self, table, stage_columns, target_columns):
        target_names = {name: extra for name, extra in target_columns}
        stage_names = {name for name, _ in stage_columns}
        overrides = dict(table.values)
        missing = [column for column in overrides if column not in target_names
                   or (column not in stage_names and not overrides[column].isdigit())]
        if missing:
            raise ValueError(table.name + " lacks mapped columns: " + ", ".join(missing))
        selected = [name for name, extra in target_columns
                    if "generated" not in extra and (name in overrides or (name in stage_names
                                                                            and "auto_increment" not in extra))]
        suffix = "" if table.database == "characters" else "_" + table.database
        stage_db = "{stage" + suffix + "}"
        target_db = "{target" + suffix + "}"
        verb = "INSERT IGNORE INTO " if table.mode == "ignore" else "INSERT INTO "
        sql = (verb + target_db + "." + identifier(table.name) + " (" + ",".join(identifier(c) for c in selected)
               + ") SELECT " + ",".join(overrides.get(c, "s." + identifier(c)) for c in selected) + " FROM "
               + stage_db + "." + identifier(table.name) + " s WHERE " + table.keep)
        return sql + ";"

    def kept_sql(self, staged):
        statements = []
        merged = "s.owner_kind = 1 AND s.owner_id IN (SELECT o FROM {map_account} WHERE e = 1)"
        if "mod_ascension_bank_item" in staged:
            statements.append("SELECT 'kept', 'realmBankItemsNotRestored', COUNT(*), '' FROM "
                              "{stage}.mod_ascension_bank_item s WHERE " + merged + ";")
        if "mod_ascension_bank_money" in staged:
            statements.append("SELECT 'kept', 'realmBankMoneyNotRestored', COALESCE(SUM(s.money), 0), '' FROM "
                              "{stage}.mod_ascension_bank_money s WHERE " + merged + ";")
        if "guild" in staged:
            statements.append("SELECT 'kept', 'guildsNotRestored', COUNT(*), '' FROM {stage}.guild s WHERE "
                              "s.guildid NOT IN (SELECT o FROM {map_guild});")
        return statements

    def report_sql(self):
        return ("SELECT 'account', o, n, e FROM {map_account}; "
                "SELECT 'character', o, n, s.name FROM {map_character} m JOIN {stage}.characters s ON s.guid = m.o; "
                "SELECT 'skipped', s.guid, s.name, '' FROM {stage}.characters s "
                "WHERE s.account IN (SELECT o FROM {map_account}) AND s.guid NOT IN (SELECT o FROM {map_character})"
                + self.name_filter() + ";")

    def run(self):
        target_tables = existing_tables(self.connection, self.args.characters_db)
        auth_tables = existing_tables(self.connection, self.args.auth_db)
        if "characters" not in target_tables or "account" not in auth_tables:
            raise ValueError("Target databases do not contain the account/characters tables")
        online = int(self.connection.run("SELECT COUNT(*) FROM " + self.names["target"]
                                         + ".characters WHERE online <> 0;")[0][0])
        if online and not self.args.allow_online:
            raise ValueError("Target has online characters; stop worldserver or pass --allow-online")
        if self.args.target_account:
            found = self.connection.run("SELECT id FROM " + self.names["target_auth"] + ".account WHERE "
                                        + text("username") + " = " + literal(self.args.target_account) + ";")
            if not found:
                raise ValueError("Target account does not exist: " + self.args.target_account)
        try:
            self.stage_backup()
            staged = existing_tables(self.connection, self.stage)
            stage_columns = {group: columns(self.connection, name) for group, name in self.stages.items()}
            target_columns = {group: columns(self.connection, name) for group, name in self.targets.items()}
            statements = ["SET NAMES utf8mb4;", "START TRANSACTION;"]
            statements += [self.sql(statement) for statement in self.mapping_sql(staged, target_tables)]
            statements += [self.sql(statement) for statement in self.kept_sql(staged)]
            skipped_tables = []
            for table in TABLES:
                available = target_columns[table.database]
                staged_columns = stage_columns[table.database]
                if table.name not in available or table.name not in staged_columns:
                    if any(entry["table"] == table.name for entry in self.manifest["files"]):
                        skipped_tables.append(table.database + "." + table.name)
                    continue
                statements.append(self.sql(self.insert_sql(table, staged_columns[table.name],
                                                           available[table.name])))
                statements.append("SELECT 'rows', " + literal(table.name) + ", ROW_COUNT(), '';")
            if "realmcharacters" in auth_tables:
                statements.append(self.sql(
                    "REPLACE INTO {target_auth}.realmcharacters (realmid, acctid, numchars) SELECT "
                    + str(int(self.args.realm_id)) + ", a.n, (SELECT COUNT(*) FROM {target}.characters c WHERE "
                    "c.account = a.n) FROM (SELECT DISTINCT n FROM {map_account}) a;"))
            statements.append(self.sql(self.report_sql()))
            statements.append("ROLLBACK;" if self.args.dry_run else "COMMIT;")
            rows = self.connection.run("\n".join(statements))
        finally:
            if not self.args.keep_staging:
                self.drop_stage()
        report = {"backup": str(self.directory), "dryRun": bool(self.args.dry_run), "accounts": [], "characters": [],
                  "skippedCharacters": [], "rows": {}, "notRestored": {}, "tablesMissingInTarget": skipped_tables}
        for kind, first, second, third in rows:
            if kind == "account":
                report["accounts"].append({"old": int(first), "new": int(second), "existing": third == "1"})
            elif kind == "character":
                report["characters"].append({"name": third, "old": int(first), "new": int(second)})
            elif kind == "skipped":
                report["skippedCharacters"].append(second)
            elif kind == "rows":
                report["rows"][first] = int(second)
            elif kind == "kept":
                report["notRestored"][first] = int(second)
        return report


def list_backup(directory):
    manifest = load_manifest(directory)
    accounts = {entry["id"]: entry["username"] for entry in manifest["accounts"]}
    lines = ["Backup " + manifest["created"] + " from " + manifest["source"]["characters"]]
    for entry in manifest["accounts"]:
        lines.append("  " + entry["username"] + " (account " + str(entry["id"]) + ")")
        for character in manifest["characters"]:
            if character["account"] == entry["id"]:
                lines.append("    " + character["name"] + " level " + str(character["level"]) + " race "
                             + str(character["race"]) + " class " + str(character["class"]) + " guid "
                             + str(character["guid"]))
    for guild in manifest["guilds"]:
        lines.append("  guild " + guild["name"] + " (" + str(guild["guildid"]) + ")")
    orphaned = [c["name"] for c in manifest["characters"] if c["account"] not in accounts]
    if orphaned:
        lines.append("  characters without account: " + ", ".join(orphaned))
    return "\n".join(lines)


def split(value):
    return [item.strip() for item in value.split(",") if item.strip()] if value else []


def parser():
    root = argparse.ArgumentParser(description=DESCRIPTION)
    commands = root.add_subparsers(dest="command", required=True)
    connection = argparse.ArgumentParser(add_help=False)
    connection.add_argument("--mysql", type=Path, default=Path("mysql"))
    connection.add_argument("--mysqldump", type=Path, default=Path("mysqldump"))
    connection.add_argument("--defaults-file", type=Path, help="MySQL client option file with the credentials")
    connection.add_argument("--password-prompt", action="store_true")
    connection.add_argument("--host")
    connection.add_argument("--port", type=int)
    connection.add_argument("--user")
    connection.add_argument("--socket")
    connection.add_argument("--protocol")
    connection.add_argument("--auth-db", default="acore_auth")
    connection.add_argument("--characters-db", default="acore_characters")
    connection.add_argument("--playerbots-db", default="acore_playerbots",
                            help="mod-playerbots database; skipped when it does not exist")
    selection = argparse.ArgumentParser(add_help=False)
    selection.add_argument("--accounts", type=split, default=[], help="Comma-separated account names")
    selection.add_argument("--characters", type=split, default=[], help="Comma-separated character names")
    make = commands.add_parser("backup", parents=[connection, selection], help="Write a SQL backup")
    make.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    make.add_argument("--account-regex")
    make.add_argument("--exclude-account-regex")
    restore = commands.add_parser("restore", parents=[connection, selection], help="Restore a backup")
    restore.add_argument("backup", type=Path, help="Backup directory, also looked up in " + str(DEFAULT_OUTPUT))
    restore.add_argument("--target-account", help="Restore the selected characters into this existing account")
    restore.add_argument("--realm-id", type=int, default=1)
    restore.add_argument("--dry-run", action="store_true")
    restore.add_argument("--allow-online", action="store_true")
    restore.add_argument("--keep-staging", action="store_true")
    restore.add_argument("--report", type=Path)
    show = commands.add_parser("list", help="List the backups, or show the contents of one backup")
    show.add_argument("backup", type=Path, nargs="?")
    return root


def main(argv=None):
    args = parser().parse_args(argv)
    if args.command == "list":
        print(list_backup(resolve_backup(args.backup)) if args.backup else available_backups())
        return None
    for name in databases(args).values():
        identifier(name)
    connection = Connection(args)
    try:
        if args.command == "backup":
            directory, manifest = backup(connection, args)
            print(json.dumps({"backup": str(directory), "accounts": len(manifest["accounts"]),
                              "characters": len(manifest["characters"]), "guilds": len(manifest["guilds"])}))
            return directory
        report = Restore(connection, resolve_backup(args.backup), args).run()
    finally:
        connection.close()
    if args.report:
        args.report.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8",
                               newline="\n")
    print(json.dumps(report, indent=2, ensure_ascii=False))
    return report


if __name__ == "__main__":
    try:
        main()
    except (ValueError, RuntimeError, OSError, subprocess.SubprocessError) as error:
        print(str(error), file=sys.stderr)
        raise SystemExit(1)
