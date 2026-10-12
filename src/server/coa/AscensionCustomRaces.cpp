/* Copyright (C) 2016+ AzerothCore, GNU AGPL v3. */
#include "AccountScript.h"
#include "Chat.h"
#include "CoaCustomRaces.h"
#include "CommandScript.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Player.h"
#include "PlayerScript.h"
#include "ScriptMgr.h"
#include "SpellMgr.h"
#include "WorldScript.h"
#include <algorithm>
#include <unordered_set>
#include <vector>

using namespace Acore::ChatCommands;

namespace
{
bool CustomRacesEnabled = true;
uint32 HighestCustomRace = CoaCustomRaces::MaxStageOneRace;
std::unordered_set<ObjectGuid::LowType> HiddenArmor;

constexpr uint8 HighestStockRace = 11;
constexpr uint8 GoblinRace = 9;

bool IsStockRace(uint8 race)
{
    return race >= 1 && race <= HighestStockRace && race != GoblinRace;
}

void LoadCustomRaceDisplays()
{
    CoaCustomRaces::DisplayStore& store = CoaCustomRaces::DisplayStore::Instance();
    store.Clear();
    if (QueryResult result = WorldDatabase.Query("SELECT race, gender, idx, displayId FROM custom_race_display"))
    {
        do
        {
            Field* fields = result->Fetch();
            store.Add(fields[0].Get<uint8>(), fields[1].Get<uint8>(), fields[2].Get<uint32>(), fields[3].Get<uint32>());
        } while (result->NextRow());
    }
    LOG_INFO("server.loading", ">> Loaded {} custom race looks", store.Size());
}

void RemoveForeignVanillaRacials(Player* player)
{
    CoaCustomRaces::Racial const* racial = CoaCustomRaces::GetRacial(player->getRace());
    if (!racial)
        return;

    std::vector<uint32> foreign;
    for (auto const& [spellId, spell] : player->GetSpellMap())
    {
        if (spell->State == PLAYERSPELL_REMOVED ||
            std::find(racial->Spells.begin(), racial->Spells.end(), spellId) != racial->Spells.end())
            continue;
        SkillLineAbilityMapBounds bounds = sSpellMgr->GetSkillLineAbilityMapBounds(spellId);
        for (auto itr = bounds.first; itr != bounds.second; ++itr)
        {
            if (CoaCustomRaces::IsVanillaRacialSkill(itr->second->SkillLine))
            {
                foreign.push_back(spellId);
                break;
            }
        }
    }
    for (uint32 spellId : foreign)
        player->removeSpell(spellId, SPEC_MASK_ALL, false);
    for (uint32 skill : CoaCustomRaces::VanillaRacialSkills)
        if (player->HasSkill(skill))
            player->SetSkill(skill, 0, 0, 0);
}

bool IsArmorSlot(uint8 slot)
{
    switch (slot)
    {
        case EQUIPMENT_SLOT_HEAD: case EQUIPMENT_SLOT_SHOULDERS: case EQUIPMENT_SLOT_BODY: case EQUIPMENT_SLOT_CHEST:
        case EQUIPMENT_SLOT_WAIST: case EQUIPMENT_SLOT_LEGS: case EQUIPMENT_SLOT_FEET: case EQUIPMENT_SLOT_WRISTS:
        case EQUIPMENT_SLOT_HANDS: case EQUIPMENT_SLOT_BACK: case EQUIPMENT_SLOT_TABARD:
            return true;
        default:
            return false;
    }
}

void RefreshArmor(Player* player)
{
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        if (IsArmorSlot(slot))
            player->SetVisibleItemSlot(slot, player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot));
}
}

class AscensionCustomRacesWorld : public WorldScript
{
public:
    AscensionCustomRacesWorld()
        : WorldScript("AscensionCustomRacesWorld", { WORLDHOOK_ON_BEFORE_CONFIG_LOAD, WORLDHOOK_ON_STARTUP }) { }

    void OnBeforeConfigLoad(bool) override
    {
        CustomRacesEnabled = sConfigMgr->GetOption<bool>("CoA.CustomRaces.Enable", true);
        HighestCustomRace = sConfigMgr->GetOption<uint32>("CoA.CustomRaces.HighestRace", CoaCustomRaces::MaxStageOneRace);
    }

    void OnStartup() override
    {
        LoadCustomRaceDisplays();
    }
};

class AscensionCustomRacesAccount : public AccountScript
{
public:
    AscensionCustomRacesAccount()
        : AccountScript("AscensionCustomRacesAccount", { ACCOUNTHOOK_CAN_ACCOUNT_CREATE_CHARACTER }) { }

    bool CanAccountCreateCharacter(uint32, uint8 race, uint8) override
    {
        if (IsStockRace(race))
            return true;
        return CustomRacesEnabled && race <= HighestCustomRace;
    }
};

class AscensionCustomRacesPlayer : public PlayerScript
{
public:
    AscensionCustomRacesPlayer() : PlayerScript("AscensionCustomRacesPlayer", {
        PLAYERHOOK_ON_LOGIN, PLAYERHOOK_ON_LOGOUT, PLAYERHOOK_ON_AFTER_SET_VISIBLE_ITEM_SLOT }) { }

    void OnPlayerLogin(Player* player) override
    {
        RemoveForeignVanillaRacials(player);
        if (CharacterDatabase.Query("SELECT 1 FROM coa_hide_armor WHERE guid = {}", player->GetGUID().GetCounter()))
        {
            HiddenArmor.insert(player->GetGUID().GetCounter());
            RefreshArmor(player);
        }
    }

    void OnPlayerLogout(Player* player) override
    {
        HiddenArmor.erase(player->GetGUID().GetCounter());
    }

    void OnPlayerAfterSetVisibleItemSlot(Player* player, uint8 slot, Item*) override
    {
        if (IsArmorSlot(slot) && HiddenArmor.count(player->GetGUID().GetCounter()))
        {
            player->SetUInt32Value(PLAYER_VISIBLE_ITEM_1_ENTRYID + (slot * 2), 0);
            player->SetUInt32Value(PLAYER_VISIBLE_ITEM_1_ENCHANTMENT + (slot * 2), 0);
        }
    }
};

class AscensionHideArmorCommand : public CommandScript
{
public:
    AscensionHideArmorCommand() : CommandScript("AscensionHideArmorCommand") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable commandTable =
        {
            { "hidearmor", HandleHideArmor, SEC_PLAYER, Console::No },
        };
        return commandTable;
    }

    static bool HandleHideArmor(ChatHandler* handler)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;
        ObjectGuid::LowType guid = player->GetGUID().GetCounter();
        bool const hide = !HiddenArmor.count(guid);
        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
        trans->Append("DELETE FROM coa_hide_armor WHERE guid = {}", guid);
        if (hide)
        {
            HiddenArmor.insert(guid);
            trans->Append("INSERT INTO coa_hide_armor (guid) VALUES ({})", guid);
        }
        else
            HiddenArmor.erase(guid);
        CharacterDatabase.CommitTransaction(trans);
        RefreshArmor(player);
        handler->SendSysMessage(hide ? "Armor hidden. Type .hidearmor again to show it." : "Armor shown again.");
        return true;
    }
};

void AddSC_AscensionCustomRaces()
{
    new AscensionCustomRacesWorld();
    new AscensionCustomRacesAccount();
    new AscensionCustomRacesPlayer();
    new AscensionHideArmorCommand();
}
