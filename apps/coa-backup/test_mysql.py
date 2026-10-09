import argparse
from contextlib import contextmanager
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time
import uuid

import coa_backup


ROOT = Path(__file__).resolve().parents[2]
PLAYERBOTS_SQL = ROOT / "modules/mod-playerbots/data/sql/playerbots"
DESCRIPTION = "Back up and restore CoA characters between two disposable MySQL schemas."
ITEM_HIGH = 0x4000000000000000

SOURCE = """
SET SESSION sql_mode = '';
INSERT INTO {auth}.account (id, username, salt, verifier, expansion) VALUES
 (1, 'ALICE', REPEAT('a', 32), REPEAT('b', 32), 2), (2, 'BOB', REPEAT('c', 32), REPEAT('d', 32), 2),
 (3, 'RNDBOT1', REPEAT('e', 32), REPEAT('f', 32), 2);
INSERT INTO {auth}.account_access (id, gmlevel, RealmID, comment) VALUES (1, 3, -1, 'gm');
INSERT INTO characters (guid, account, name, race, class, level, money, taximask, exploredZones, equipmentCache,
  knownTitles, online) VALUES
 (1, 1, 'Alice', 1, 1, 60, 12345, '', '', '', '', 0), (2, 1, 'Alicealt', 2, 3, 10, 0, '', '', '', '', 0),
 (3, 2, 'Bob', 3, 2, 70, 99, '', '', '', '', 0), (4, 3, 'Botty', 4, 4, 1, 0, '', '', '', '', 0),
 (5, 0, '', 1, 1, 5, 0, '', '', '', '', 0);
INSERT INTO item_instance (guid, itemEntry, owner_guid, creatorGuid, count, enchantments, text) VALUES
 (10, 4500, 1, 0, 1, '', ''), (11, 2589, 1, 3, 20, '', ''), (12, 19019, 1, 0, 1, '2564 0 0 ', ''),
 (13, 6948, 1, 0, 1, '', 'letter'), (14, 2770, 1, 0, 5, '', ''), (15, 2771, 0, 0, 3, '', ''),
 (16, 2772, 0, 0, 7, '', ''), (17, 2775, 0, 0, 1, '', ''), (18, 2776, 0, 0, 2, '', ''),
 (19, 2836, 0, 0, 4, '', ''), (20, 2838, 0, 0, 4, '', ''), (21, 375250, 1, 0, 1, '', ''),
 (22, 3575, 0, 0, 9, '', ''), (23, 4306, 3, 0, 11, '', ''), (24, 4338, 4, 0, 1, '', ''),
 (25, 2589, 2, 0, 1, '', '');
INSERT INTO character_inventory (guid, bag, slot, item) VALUES
 (1, 0, 19, 10), (1, 10, 0, 11), (1, 0, 15, 12), (3, 0, 23, 23), (4, 0, 23, 24), (2, 0, 23, 25);
INSERT INTO character_aura (guid, casterGuid, itemGuid, spell, effectMask, recalculateMask, stackCount, amount0,
  amount1, amount2, base_amount0, base_amount1, base_amount2, maxDuration, remainTime, remainCharges) VALUES
 (1, 1, {item_high} | 12, 100, 1, 1, 1, 0, 0, 0, 0, 0, 0, -1, -1, 0),
 (1, 0xF130000000000123, 0, 101, 1, 1, 1, 0, 0, 0, 0, 0, 0, -1, -1, 0);
INSERT INTO character_equipmentsets (guid, setguid, setindex, name, iconname, item15) VALUES
 (1, 1, 0, 'Tank', 'icon', 12);
INSERT INTO mail (id, messageType, stationery, mailTemplateId, sender, receiver, subject, body, has_items,
  expire_time, deliver_time, money, cod, checked) VALUES
 (100, 0, 41, 0, 3, 1, 'Hi', 'body, with (parens)', 1, 0, 0, 7, 0, 0),
 (101, 0, 41, 0, 2, 1, 'Reward', '', 0, 0, 0, 0, 0, 0),
 (102, 2, 41, 0, 1234, 3, 'From NPC', '', 0, 0, 0, 0, 0, 0);
INSERT INTO mail_items (mail_id, item_guid, receiver) VALUES (100, 13, 1);
INSERT INTO auctionhouse (id, houseid, itemguid, itemowner, buyoutprice, time, buyguid, lastbid, startbid, deposit)
 VALUES (50, 7, 14, 1, 1000, 0, 3, 500, 100, 5);
INSERT INTO mod_ascension_bank_item (owner_kind, owner_id, tab_index, slot, item_guid) VALUES
 (0, 1, 0, 0, 15), (1, 1, 0, 0, 16), (1, 2, 0, 0, 17), (1, 2, 0, 1, 18);
INSERT INTO mod_ascension_bank_money (owner_kind, owner_id, money) VALUES (0, 1, 20), (1, 2, 1000);
INSERT INTO mod_ascension_bank_tab (owner_kind, owner_id, tab_index, name) VALUES
 (0, 1, 0, 'Mine'), (1, 2, 0, 'Ours');
INSERT INTO mod_ascension_bank_log (owner_kind, owner_id, tab_index, event_type, player_guid) VALUES (0, 1, 0, 1, 1);
INSERT INTO highrisk_chest (id, owner, map, phase, x, y, z, o, gold, original_gold, gold_claimant) VALUES
 (7, 1, 0, 1, 1, 2, 3, 0, 50, 50, 0);
INSERT INTO highrisk_chest_item (chest_id, slot, item_guid, entry, count, claimant) VALUES
 (7, 0, 19, 2836, 4, 0), (7, 1, 20, 2838, 4, 3);
INSERT INTO ascension_manastorm_cache (item, guid) VALUES (21, 1);
INSERT INTO ascension_manastorm_clear (guid, mode, depth, scene, mail_id, completed_at) VALUES (1, 0, 3, 2, 101, 1);
INSERT INTO character_pet (id, entry, owner, modelid, CreatedBySpell, PetType, level, exp, Reactstate, name, renamed,
  slot, curhealth, curmana, curhappiness, savetime, abdata) VALUES
 (30, 299, 1, 903, 0, 1, 60, 0, 0, 'Wolfy', 0, 0, 100, 0, 0, 0, '');
INSERT INTO pet_spell (guid, spell, active) VALUES (30, 17253, 193);
INSERT INTO character_social (guid, friend, flags, note) VALUES (1, 3, 1, ''), (1, 4, 1, ''), (3, 1, 1, '');
INSERT INTO guild (guildid, name, leaderguid, info, motd, createdate, BankMoney) VALUES
 (5, 'Guild One', 3, '', '', 0, 77);
INSERT INTO guild_rank (guildid, rid, rname, rights, BankMoneyPerDay) VALUES
 (5, 0, 'GM', 1, 0), (5, 1, 'Officer', 1, 0), (5, 2, 'Veteran', 1, 0), (5, 3, 'Member', 1, 0),
 (5, 4, 'Initiate', 1, 0);
INSERT INTO guild_member (guildid, guid, `rank`, pnote, offnote) VALUES
 (5, 1, 1, '', ''), (5, 3, 0, '', ''), (5, 4, 2, '', '');
INSERT INTO guild_bank_tab (guildid, TabId, TabName, TabIcon, TabText) VALUES (5, 0, 'Tab', '', '');
INSERT INTO guild_bank_item (guildid, TabId, SlotId, item_guid) VALUES (5, 0, 0, 22);
INSERT INTO coa_character_looted_item (guid, itemGuid) VALUES (1, 12), (1, 999);
INSERT INTO coa_character_challenge (guid, challengeId, level) VALUES (1, 4, 1);
INSERT INTO character_appearance (guid, category_id, appearance_id) VALUES (1, 1, 555);
INSERT INTO account_appearance_collection (account_id, appearance_id, source_item) VALUES (1, 777, 0), (2, 778, 0);
INSERT INTO coa_custom_trial (guid, trialId, title, about, icon, author) VALUES (1, 'trial-a', 'A', 'x', 'i', 'Alice');
INSERT INTO coa_custom_trial_entry (guid, trialId, challengeId, level, description) VALUES (1, 'trial-a', 4, 1, 'd');
INSERT INTO coa_custom_trial_vote (guid, trialId, upvote, downvote) VALUES (3, 'trial-a', 1, 0);
INSERT INTO character_worldforged_loot (guid, spawn_id, entry) VALUES (1, 4242, 1);
INSERT INTO mod_craftsmans_codex (guid, slots) VALUES (1, 2);
"""

TARGET = """
SET SESSION sql_mode = '';
INSERT INTO {auth}.account (id, username, salt, verifier, expansion) VALUES
 (1, 'CAROL', REPEAT('g', 32), REPEAT('h', 32), 2), (5, 'BOB', REPEAT('i', 32), REPEAT('j', 32), 2);
INSERT INTO characters (guid, account, name, race, class, level, taximask, exploredZones, equipmentCache,
  knownTitles) VALUES (1, 1, 'Carol', 1, 1, 1, '', '', '', ''), (7, 1, 'alicealt', 1, 1, 1, '', '', '', '');
INSERT INTO item_instance (guid, itemEntry, owner_guid, count, enchantments, text)
 WITH RECURSIVE n (i) AS (SELECT 1 UNION ALL SELECT i + 1 FROM n WHERE i < 40)
 SELECT i, 25, 1, 1, '', '' FROM n;
INSERT INTO mail (id, messageType, stationery, mailTemplateId, sender, receiver, subject, body, has_items,
  expire_time, deliver_time, money, cod, checked) VALUES (200, 0, 41, 0, 1, 1, '', '', 0, 0, 0, 0, 0, 0);
INSERT INTO character_pet (id, entry, owner, modelid, CreatedBySpell, PetType, level, exp, Reactstate, name, renamed,
  slot, curhealth, curmana, curhappiness, savetime, abdata) VALUES
 (50, 299, 1, 903, 0, 1, 1, 0, 0, 'Pet', 0, 0, 1, 0, 0, 0, '');
INSERT INTO auctionhouse (id, houseid, itemguid, itemowner, buyoutprice, time, buyguid, lastbid, startbid, deposit)
 VALUES (60, 7, 39, 1, 1, 0, 0, 0, 1, 1);
INSERT INTO highrisk_chest (id, owner, map, phase, x, y, z, o, gold, original_gold) VALUES
 (9, 1, 0, 1, 0, 0, 0, 0, 0, 0);
INSERT INTO guild (guildid, name, leaderguid, info, motd, createdate, BankMoney) VALUES (5, 'Other', 1, '', '', 0, 0);
INSERT INTO character_equipmentsets (guid, setguid, setindex, name, iconname) VALUES (1, 3, 0, 'x', 'y');
INSERT INTO mod_ascension_bank_item (owner_kind, owner_id, tab_index, slot, item_guid) VALUES (1, 5, 0, 0, 40);
INSERT INTO mod_ascension_bank_money (owner_kind, owner_id, money) VALUES (1, 5, 500);
"""

SOURCE_PLAYERBOTS = """
INSERT INTO playerbots_account_type (account_id, account_type) VALUES (1, 0), (2, 0), (3, 1);
INSERT INTO playerbots_account_keys (account_id, security_key) VALUES (1, 'secret'), (3, 'bot');
INSERT INTO playerbots_account_links (account_id, linked_account_id) VALUES (1, 2), (1, 3);
INSERT INTO playerbots_random_bots (owner, bot, time, event, value) VALUES (0, 1, 5, 'add', 1), (0, 4, 5, 'add', 1);
INSERT INTO playerbots_db_store (guid, `key`, value) VALUES
 (1, 'co', '+dps,+aoe'), (2, 'co', '+tank'), (4, 'nc', '+rpg');
INSERT INTO playerbots_preferred_mounts (guid, type, spellid) VALUES (1, 0, 458);
INSERT INTO playerbots_custom_strategy (name, idx, owner, action_line) VALUES ('mine', 1, 1, 'attack');
INSERT INTO playerbots_guild_tasks (owner, guildid, time, type, value) VALUES
 (1, 5, 5, 'kill', 3), (4, 5, 5, 'kill', 4);
"""

TARGET_PLAYERBOTS = """
INSERT INTO playerbots_account_type (account_id, account_type) VALUES (5, 2);
INSERT INTO playerbots_random_bots (owner, bot, time, event, value) VALUES (0, 1, 5, 'add', 1);
"""

PLAYERBOTS_EXPECTED = {
    "SELECT account_id, account_type FROM playerbots_account_type ORDER BY account_id": [["5", "2"], ["6", "0"]],
    "SELECT account_id, security_key FROM playerbots_account_keys": [["6", "secret"]],
    "SELECT account_id, linked_account_id FROM playerbots_account_links": [["6", "5"]],
    "SELECT owner, bot, event FROM playerbots_random_bots ORDER BY id": [["0", "1", "add"], ["0", "8", "add"]],
    "SELECT guid, `key`, value FROM playerbots_db_store": [["8", "co", "+dps,+aoe"]],
    "SELECT guid, type, spellid FROM playerbots_preferred_mounts": [["8", "0", "458"]],
    "SELECT owner, name, idx, action_line FROM playerbots_custom_strategy WHERE owner <> 0":
        [["8", "mine", "1", "attack"]],
    "SELECT owner, guildid, type, value FROM playerbots_guild_tasks": [["8", "6", "kill", "3"]],
}

EXPECTED = {
    "SELECT id, username FROM {auth}.account ORDER BY id":
        [["1", "CAROL"], ["5", "BOB"], ["6", "ALICE"]],
    "SELECT id, gmlevel FROM {auth}.account_access": [["6", "3"]],
    "SELECT guid, account, name, money, online FROM characters ORDER BY guid":
        [["1", "1", "Carol", "0", "0"], ["7", "1", "alicealt", "0", "0"], ["8", "6", "Alice", "12345", "0"],
         ["9", "5", "Bob", "99", "0"]],
    "SELECT guid, bag, slot, item FROM character_inventory ORDER BY item":
        [["8", "0", "19", "41"], ["8", "41", "0", "42"], ["8", "0", "15", "43"], ["9", "0", "23", "51"]],
    "SELECT guid, itemEntry, owner_guid, creatorGuid, count, enchantments FROM item_instance WHERE guid > 40 "
    "ORDER BY guid":
        [["41", "4500", "8", "0", "1", ""], ["42", "2589", "8", "9", "20", ""],
         ["43", "19019", "8", "0", "1", "2564 0 0 "], ["44", "6948", "8", "0", "1", ""],
         ["45", "2770", "8", "0", "5", ""], ["46", "2771", "0", "0", "3", ""], ["47", "2772", "0", "0", "7", ""],
         ["48", "2836", "0", "0", "4", ""], ["49", "375250", "8", "0", "1", ""], ["50", "3575", "0", "0", "9", ""],
         ["51", "4306", "9", "0", "11", ""]],
    "SELECT guid, casterGuid, itemGuid, spell FROM character_aura ORDER BY spell":
        [["8", "8", str(ITEM_HIGH | 43), "100"], ["8", str(0xF130000000000123), "0", "101"]],
    "SELECT guid, setguid, item15 FROM character_equipmentsets ORDER BY setguid":
        [["1", "3", "0"], ["8", "4", "43"]],
    "SELECT id, messageType, sender, receiver, body, has_items, money FROM mail ORDER BY id":
        [["200", "0", "1", "1", "", "0", "0"], ["201", "0", "9", "8", "body, with (parens)", "1", "7"],
         ["202", "0", "0", "8", "", "0", "0"], ["203", "2", "1234", "9", "", "0", "0"]],
    "SELECT mail_id, item_guid, receiver FROM mail_items": [["201", "44", "8"]],
    "SELECT id, itemguid, itemowner, buyguid, lastbid FROM auctionhouse ORDER BY id":
        [["60", "39", "1", "0", "0"], ["61", "45", "8", "9", "500"]],
    "SELECT owner_kind, owner_id, tab_index, slot, item_guid FROM mod_ascension_bank_item "
    "ORDER BY owner_kind, owner_id, slot":
        [["0", "8", "0", "0", "46"], ["1", "5", "0", "0", "40"], ["1", "6", "0", "0", "47"]],
    "SELECT owner_kind, owner_id, money FROM mod_ascension_bank_money ORDER BY owner_kind, owner_id":
        [["0", "8", "20"], ["1", "5", "500"]],
    "SELECT owner_kind, owner_id, name FROM mod_ascension_bank_tab ORDER BY owner_kind, owner_id":
        [["0", "8", "Mine"]],
    "SELECT owner_kind, owner_id, player_guid FROM mod_ascension_bank_log": [["0", "8", "8"]],
    "SELECT id, owner, gold FROM highrisk_chest ORDER BY id": [["9", "1", "0"], ["10", "8", "50"]],
    "SELECT chest_id, slot, item_guid, claimant FROM highrisk_chest_item": [["10", "0", "48", "0"]],
    "SELECT item, guid FROM ascension_manastorm_cache": [["49", "8"]],
    "SELECT guid, mail_id FROM ascension_manastorm_clear": [["8", "202"]],
    "SELECT id, owner, name FROM character_pet ORDER BY id": [["50", "1", "Pet"], ["51", "8", "Wolfy"]],
    "SELECT guid, spell FROM pet_spell": [["51", "17253"]],
    "SELECT guid, friend FROM character_social ORDER BY guid": [["8", "9"], ["9", "8"]],
    "SELECT guildid, name, leaderguid, BankMoney FROM guild ORDER BY guildid":
        [["5", "Other", "1", "0"], ["6", "Guild One", "9", "77"]],
    "SELECT guildid, guid, `rank` FROM guild_member ORDER BY guid": [["6", "8", "1"], ["6", "9", "0"]],
    "SELECT COUNT(*) FROM guild_rank WHERE guildid = 6": [["5"]],
    "SELECT guildid, item_guid FROM guild_bank_item": [["6", "50"]],
    "SELECT guid, itemGuid FROM coa_character_looted_item": [["8", "43"]],
    "SELECT guid, challengeId FROM coa_character_challenge": [["8", "4"]],
    "SELECT guid, appearance_id FROM character_appearance": [["8", "555"]],
    "SELECT account_id, appearance_id FROM account_appearance_collection ORDER BY account_id":
        [["5", "778"], ["6", "777"]],
    "SELECT guid, trialId FROM coa_custom_trial": [["8", "trial-a"]],
    "SELECT guid, trialId FROM coa_custom_trial_entry": [["8", "trial-a"]],
    "SELECT guid, trialId FROM coa_custom_trial_vote": [["9", "trial-a"]],
    "SELECT guid, spawn_id FROM character_worldforged_loot": [["8", "4242"]],
    "SELECT guid, slots FROM mod_craftsmans_codex": [["8", "2"]],
    "SELECT realmid, acctid, numchars FROM {auth}.realmcharacters ORDER BY acctid": [["1", "5", "1"], ["1", "6", "1"]],
}


def create_schema(connection, kind, name):
    connection.run("CREATE DATABASE " + name + " CHARACTER SET utf8mb4;")
    for path in sorted((ROOT / "data/sql/base" / ("db_" + kind)).glob("*.sql")):
        connection.load(path, name)
    applied = {row[0] for row in connection.run("SELECT name FROM updates;", name)}
    released = sorted((ROOT / "data/sql/updates" / ("db_" + kind)).glob("*.sql"))
    pending = list((ROOT / "data/sql/updates" / ("pending_db_" + kind)).glob("*.sql"))
    modules = list(ROOT.glob("modules/*/data/sql/db-" + kind + "/**/*.sql"))
    for path in released + sorted(pending + modules, key=lambda path: path.name):
        if path.name not in applied:
            connection.load(path, name)


def create_playerbots_schema(connection, name):
    connection.run("CREATE DATABASE " + name + " CHARACTER SET utf8mb4;")
    for path in sorted((PLAYERBOTS_SQL / "base").glob("*.sql")) + sorted((PLAYERBOTS_SQL / "updates").glob("*.sql")):
        connection.load(path, name)


def state(connection):
    return [checksums(connection, "dst_" + group) for group in ("auth", "characters", "playerbots")]


def checksums(connection, database):
    tables = sorted(coa_backup.existing_tables(connection, database))
    names = ",".join(coa_backup.identifier(database) + "." + coa_backup.identifier(t) for t in tables)
    return connection.run("CHECKSUM TABLE " + names + ";")


@contextmanager
def isolated_server(mysql_bin):
    suffix = ".exe" if os.name == "nt" else ""
    with tempfile.TemporaryDirectory(prefix="coa-backup-mysql-", ignore_cleanup_errors=True) as scratch:
        directory = Path(scratch)
        server = [str(mysql_bin / ("mysqld" + suffix)), "--no-defaults", "--basedir=" + str(mysql_bin.parent),
                  "--datadir=" + str(directory / "data"), "--log-error=" + str(directory / "mysql.log")]
        subprocess.run(server + ["--initialize-insecure"], check=True, capture_output=True, timeout=120,
                       **coa_backup.PROCESS_OPTIONS)
        endpoint = "coa_backup_" + uuid.uuid4().hex if os.name == "nt" else str(directory / "mysql.sock")
        server += ["--skip-networking", "--mysqlx=OFF", "--max-allowed-packet=1GB", "--socket=" + endpoint]
        arguments = ["--user", "root", "--socket", endpoint,
                     "--mysql", str(mysql_bin / ("mysql" + suffix)),
                     "--mysqldump", str(mysql_bin / ("mysqldump" + suffix))]
        if os.name == "nt":
            server.append("--enable-named-pipe")
            arguments += ["--protocol", "PIPE", "--host", "."]
        else:
            arguments += ["--protocol", "SOCKET"]
        process = subprocess.Popen(server, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                   **coa_backup.PROCESS_OPTIONS)
        admin = coa_backup.Connection(coa_backup.parser().parse_args(["backup", *arguments]))
        try:
            deadline = time.monotonic() + 60
            while True:
                if process.poll() is not None:
                    raise RuntimeError((directory / "mysql.log").read_text(encoding="utf-8"))
                try:
                    if admin.run("SELECT 1;") == [["1"]]:
                        break
                except RuntimeError:
                    if time.monotonic() > deadline:
                        raise
                time.sleep(0.5)
            yield admin, arguments
        finally:
            try:
                admin.run("SHUTDOWN;")
                process.wait(timeout=30)
            except (RuntimeError, subprocess.TimeoutExpired):
                process.kill()
                process.wait(timeout=10)


def run(mysql_bin):
    with isolated_server(mysql_bin) as (admin, connection_args):
        admin.run("SET GLOBAL innodb_flush_log_at_trx_commit=2; SET GLOBAL sync_binlog=0;")
        for side in ("src", "dst"):
            for kind in ("auth", "characters"):
                create_schema(admin, kind, side + "_" + kind)
            create_playerbots_schema(admin, side + "_playerbots")
            print("Schema ready:", side, flush=True)
        admin.run(SOURCE.format(auth="src_auth", item_high=ITEM_HIGH), "src_characters")
        admin.run(TARGET.format(auth="dst_auth"), "dst_characters")
        admin.run(SOURCE_PLAYERBOTS, "src_playerbots")
        admin.run(TARGET_PLAYERBOTS, "dst_playerbots")
        source_before = checksums(admin, "src_characters")
        with tempfile.TemporaryDirectory(prefix="coa-backup-") as scratch:
            common = connection_args + ["--auth-db", "src_auth", "--characters-db", "src_characters",
                                        "--playerbots-db", "src_playerbots"]
            directory = coa_backup.main(["backup", *common, "--output", scratch,
                                         "--exclude-account-regex", "^RNDBOT"])
            manifest = coa_backup.load_manifest(directory)
            names = [c["name"] for c in manifest["characters"]]
            if names != ["Alice", "Alicealt", "Bob"] or [g["name"] for g in manifest["guilds"]] != ["Guild One"]:
                raise AssertionError("Unexpected backup selection: " + json.dumps(manifest))
            if checksums(admin, "src_characters") != source_before:
                raise AssertionError("Backup changed the source database")
            if directory.name != manifest["created"]:
                raise AssertionError("A full backup is named by its timestamp: " + directory.name)
            for selection, prefix, expected in (("--accounts", "account_ALICE_", ["Alice", "Alicealt"]),
                                                ("--characters", "character_Bob_", ["Bob"])):
                value = "alice" if selection == "--accounts" else "bob"
                named = coa_backup.main(["backup", *common, "--output", scratch, selection, value])
                selected = [c["name"] for c in coa_backup.load_manifest(named)["characters"]]
                if not named.name.startswith(prefix) or selected != expected:
                    raise AssertionError("Selected backup differs: " + named.name + " " + json.dumps(selected))
            print(coa_backup.list_backup(directory), flush=True)
            restore = ["restore", str(directory), *connection_args, "--auth-db", "dst_auth",
                       "--characters-db", "dst_characters", "--playerbots-db", "dst_playerbots"]
            target_before = state(admin)
            dry = coa_backup.main(restore + ["--dry-run"])
            if state(admin) != target_before:
                raise AssertionError("Dry run changed the target")
            if dry["skippedCharacters"] != ["Alicealt"]:
                raise AssertionError("Dry run selection differs: " + json.dumps(dry))
            report = coa_backup.main(restore)
            failures = {}
            for sql, expected in EXPECTED.items():
                actual = admin.run(sql.format(auth="dst_auth") + ";", "dst_characters")
                if actual != expected:
                    failures[sql] = {"expected": expected, "actual": actual}
            for sql, expected in PLAYERBOTS_EXPECTED.items():
                actual = admin.run(sql + ";", "dst_playerbots")
                if actual != expected:
                    failures[sql] = {"expected": expected, "actual": actual}
            orphans = admin.run(
                "SELECT guid FROM item_instance WHERE guid > 40 AND guid NOT IN (SELECT item FROM character_inventory "
                "UNION ALL SELECT item_guid FROM mail_items UNION ALL SELECT itemguid FROM auctionhouse UNION ALL "
                "SELECT item_guid FROM mod_ascension_bank_item UNION ALL SELECT item_guid FROM highrisk_chest_item "
                "UNION ALL SELECT item FROM ascension_manastorm_cache "
                "UNION ALL SELECT item_guid FROM guild_bank_item);",
                "dst_characters")
            if orphans:
                failures["orphanItems"] = orphans
            if report["notRestored"] != {"realmBankItemsNotRestored": 2, "realmBankMoneyNotRestored": 1000,
                                         "guildsNotRestored": 0}:
                failures["notRestored"] = report["notRestored"]
            if failures:
                raise AssertionError(json.dumps(failures, indent=2))
            print("Restore matches all expectations.", flush=True)
            restored_state = state(admin)
            again = coa_backup.main(restore)
            if sorted(again["skippedCharacters"]) != ["Alice", "Alicealt", "Bob"] or again["characters"]:
                raise AssertionError("Repeated restore restored characters again: " + json.dumps(again))
            if state(admin) != restored_state:
                raise AssertionError("Repeated restore changed the target")
            single = coa_backup.main(restore + ["--characters", "Alicealt", "--target-account", "CAROL"])
            if single["characters"] != [] or single["skippedCharacters"] != ["Alicealt"]:
                raise AssertionError("Conflicting single character was restored: " + json.dumps(single))
            admin.run("UPDATE characters SET name = 'Carolalt' WHERE guid = 7;", "dst_characters")
            single = coa_backup.main(restore + ["--characters", "Alicealt", "--target-account", "CAROL"])
            moved = admin.run("SELECT c.guid, c.account, i.item FROM characters c JOIN character_inventory i "
                              "ON i.guid = c.guid WHERE c.name = 'Alicealt';", "dst_characters")
            if moved != [["10", "1", "52"]]:
                raise AssertionError("Single character restore differs: " + json.dumps(moved))
            leftovers = admin.run("SHOW DATABASES LIKE 'coa_restore_%';")
            if leftovers:
                raise AssertionError("Staging databases were left behind: " + json.dumps(leftovers))
        return {"backupCharacters": names, "restoredTables": len(report["rows"]),
                "checks": len(EXPECTED) + len(PLAYERBOTS_EXPECTED),
                "repeatRestoreUnchanged": True, "singleCharacterRestore": True}


def main():
    parser = argparse.ArgumentParser(description=DESCRIPTION)
    parser.add_argument("--mysql-bin", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(run(args.mysql_bin), indent=2))


if __name__ == "__main__":
    main()
