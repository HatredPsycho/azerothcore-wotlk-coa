import argparse
from pathlib import Path
import unittest

import coa_backup


SETS = {key: "SELECT 1" for key in ("ACC", "CHR", "GLD", "MAIL", "PET", "CHEST", "ITEM")} | {"A": "`a`", "C": "`c`"}
NAMES = {name: "`" + name + "`" for name in ("target", "target_auth", "stage", "stage_auth")} | {
    "map_" + kind: "`m`.`" + kind + "`" for kind in coa_backup.MAPS + ("trial",)}


class Specification(unittest.TestCase):
    def test_literals_cannot_break_out_of_strings(self):
        self.assertEqual(coa_backup.literal("O'Brien\\"), "'O\\'Brien\\\\'")
        self.assertEqual(coa_backup.literals(["a", "b'"]), "'a','b\\''")

    def test_identifiers_reject_injection(self):
        self.assertEqual(coa_backup.identifier("acore_auth"), "`acore_auth`")
        for value in ("acore`; DROP", "a b", "1abc", ""):
            with self.assertRaises(ValueError):
                coa_backup.identifier(value)

    def test_every_table_is_unique_and_fully_formattable(self):
        names = [(table.auth, table.name) for table in coa_backup.TABLES]
        self.assertEqual(len(names), len(set(names)))
        for table in coa_backup.TABLES:
            self.assertIn(table.mode, ("insert", "ignore"))
            table.select.format(**SETS)
            table.keep.format(**NAMES)
            for column, expression in table.values:
                coa_backup.identifier(column)
                expression.format(**NAMES)

    def test_owned_rows_follow_their_owner(self):
        tables = {table.name: table for table in coa_backup.TABLES}
        self.assertIn("{ITEM}", tables["item_instance"].select)
        for name in ("characters", "character_inventory", "mail", "character_pet", "highrisk_chest",
                     "ascension_manastorm_cache", "character_appearance", "coa_character_challenge"):
            self.assertIn("{CHR}", tables[name].select, name)
        for name in ("account_appearance_collection", "account_vanity_collection", "account_ascension_settings"):
            self.assertEqual(tables[name].mode, "ignore", name)

    def test_realm_bank_is_restored_only_for_new_accounts(self):
        tables = {table.name: table for table in coa_backup.TABLES}
        for name in ("mod_ascension_bank_item", "mod_ascension_bank_money", "mod_ascension_bank_tab"):
            self.assertIn("AND e = 0", tables[name].keep, name)

    def test_selection_lists_are_split(self):
        args = coa_backup.parser().parse_args(["restore", "backup", "--characters", "Alice, Bob,,"])
        self.assertEqual(args.characters, ["Alice", "Bob"])
        self.assertEqual(args.backup, Path("backup"))

    def test_source_sets_skip_absent_optional_tables(self):
        args = argparse.Namespace(accounts=["ALICE"], account_regex=None, exclude_account_regex="^RNDBOT",
                                  characters=[])
        sets = coa_backup.source_sets("acore_auth", "acore_characters", {"characters", "character_inventory"}, args)
        self.assertEqual(sets["GLD"], coa_backup.EMPTY_SET)
        self.assertIn("character_inventory", sets["ITEM"])
        self.assertNotIn("guild_bank_item", sets["ITEM"])
        self.assertIn("NOT REGEXP '^RNDBOT'", sets["ACC"])


if __name__ == "__main__":
    unittest.main()
