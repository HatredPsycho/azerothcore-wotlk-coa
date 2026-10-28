-- Reverts the Seismic Smash rank 1 trainer row from rev_20261027_61: 300693 is the rank spell of
-- CharacterAdvancement entry 7498, a Mountainking (tab 91) talent that costs 1 Talent Essence and requires
-- 23 TE already invested in that tab. Selling or auto-granting it leaves a Wildwalker knowing a deep talent
-- entry with nothing invested, so every known-entries upload for the tab is refused with
-- CA_UPDATE_ENTRIES_NOT_TRAVERSIBLE / CA_LEARN_NOT_ENOUGH_INVESTED_TE and specialization 60 can no longer be
-- activated. Ranks 2-4 (301276-301279) keep their trainer rows and their ReqAbility1 gate on 300693, which
-- the talent grants. 300693 is the only ClassSpells spell that is also a TE-paid advancement entry; the
-- matching AscensionCompatData::ClassSpells row is removed in the same change.
DELETE FROM `trainer_spell` WHERE `TrainerId` = 900031 AND `SpellId` = 300693;
