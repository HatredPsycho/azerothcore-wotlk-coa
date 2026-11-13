#include "AbyssalStoragePlus.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "Item.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "QuestDef.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include <ctime>
#include <sstream>

// Build a clickable item link like "|cff1eff00|Hitem:2589:0:0:0:0:0:0:0:0:0|h[Linen Cloth]|h|r"
static std::string BuildItemLink(uint32 itemEntry)
{
    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemEntry);
    if (!proto)
        return "[Item #" + std::to_string(itemEntry) + "]";

    std::ostringstream ss;
    ss << "|c";
    ss << std::hex << ItemQualityColors[proto->Quality] << std::dec;
    ss << "|Hitem:" << itemEntry << ":0:0:0:0:0:0:0:0:0|h[";
    ss << proto->Name1 << "]|h|r";
    return ss.str();
}

// Build a clickable quest link like "|cffffff00|Hquest:33:2|h[Quest Name]|h|r"
static std::string BuildQuestLink(Quest const* quest)
{
    if (!quest)
        return "";

    std::ostringstream ss;
    ss << "|cffffff00|Hquest:" << quest->GetQuestId() << ":" << quest->GetQuestLevel();
    ss << "|h[" << quest->GetTitle() << "]|h|r";
    return ss.str();
}

// Record a vault pull plus the bag count before it
static void TrackMaterializedItem(AbyssalPlayerData* data, uint32 entry, uint32 baseline, uint32 amount)
{
    if (!data || amount == 0)
        return;

    auto it = data->materializedItems.find(entry);
    if (it == data->materializedItems.end())
        data->materializedItems.emplace(entry, MaterializedItem{ baseline, amount });
    else
        it->second.amount += amount;
}

// Remove only the items a StoreNewItem call actually added
static void UndoStoredItems(Player* player, uint32 itemEntry, uint32 bagBefore)
{
    if (!player)
        return;

    uint32 after = player->GetItemCount(itemEntry);
    uint32 stored = after > bagBefore ? after - bagBefore : 0;
    if (stored > 0)
        player->DestroyItemCount(itemEntry, stored, true);
}

// Return only the leftover vault-pulled items still in bags, not merged player stacks
static void RevaultMaterializedItems(Player* player, AbyssalPlayerData* data, uint32 accountId)
{
    if (!player || !data || data->materializedItems.empty())
        return;

    data->isMaterializing = true;
    for (auto const& [entry, mat] : data->materializedItems)
    {
        uint32 current = player->GetItemCount(entry);
        if (current <= mat.baseline)
            continue;

        uint32 leftover = std::min(current - mat.baseline, mat.amount);
        if (leftover == 0)
            continue;

        // Send invariant total before the bag packet to avoid a count flicker
        sAbyssalStoragePlusMgr->SendItemUpdate(player, entry,
            sAbyssalStoragePlusMgr->GetItemCount(accountId, entry));

        uint32 before = player->GetItemCount(entry);
        player->DestroyItemCount(entry, leftover, true);
        uint32 after = player->GetItemCount(entry);
        uint32 destroyed = before > after ? before - after : 0;
        if (destroyed == 0)
            continue;

        sAbyssalStoragePlusMgr->DepositItem(player, accountId, entry, destroyed);
        sAbyssalStoragePlusMgr->BroadcastItemUpdate(accountId, entry);
    }
    data->materializedItems.clear();
    data->isMaterializing = false;
}

// Push current bag+vault totals for each reagent of a spell
static void SendSpellReagentTotals(Player* player, SpellInfo const* spellInfo, uint32 accountId)
{
    if (!player || !spellInfo)
        return;

    for (uint8 i = 0; i < MAX_SPELL_REAGENTS; ++i)
    {
        int32 reagentEntry = spellInfo->Reagent[i];
        if (reagentEntry <= 0 || spellInfo->ReagentCount[i] == 0)
            continue;
        sAbyssalStoragePlusMgr->SendItemUpdate(player, uint32(reagentEntry),
            sAbyssalStoragePlusMgr->GetItemCount(accountId, uint32(reagentEntry)));
    }
}

// Pull up to amount of one item from the vault into bags, returning how many fit
static uint32 PullQuestMaterials(Player* player, uint32 accountId, uint32 itemEntry, uint32 amount)
{
    if (!player || amount == 0)
        return 0;

    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemEntry);
    if (!proto)
        return 0;

    uint32 maxStack = std::max<uint32>(proto->GetMaxStackSize(), 1);
    uint32 pulled = 0;

    while (amount > 0)
    {
        uint32 chunk = std::min(amount, maxStack);
        ItemPosCountVec dest;
        InventoryResult result = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemEntry, chunk);

        // Shrink the chunk until it fits (partial stacks, limited slots)
        while (result != EQUIP_ERR_OK && chunk > 1)
        {
            chunk /= 2;
            dest.clear();
            result = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemEntry, chunk);
        }

        if (result != EQUIP_ERR_OK)
            break;

        uint32 bagBefore = player->GetItemCount(itemEntry);
        Item* newItem = player->StoreNewItem(dest, itemEntry, true);
        if (!newItem)
            break;

        if (!sAbyssalStoragePlusMgr->WithdrawItem(player, accountId, itemEntry, chunk))
        {
            // Vault debit failed; undo exactly what was stored
            UndoStoredItems(player, itemEntry, bagBefore);
            break;
        }

        pulled += chunk;
        amount -= chunk;
    }

    return pulled;
}

// ============================================================================
// WorldScript — Config Loading
// ============================================================================

class AbyssalStoragePlusWorldScript : public WorldScript
{
public:
    AbyssalStoragePlusWorldScript() : WorldScript("AbyssalStoragePlusWorldScript") { }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        sAbyssalStoragePlusMgr->SetEnabled(sConfigMgr->GetOption<bool>("AbyssalStoragePlus.Enable", true));
    }
};

// ============================================================================
// PlayerScript — Login/Logout, Item Acquisition
// ============================================================================

class AbyssalStoragePlusPlayerScript : public PlayerScript
{
public:
    AbyssalStoragePlusPlayerScript() : PlayerScript("AbyssalStoragePlusPlayerScript", {
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_LOGOUT,
        PLAYERHOOK_ON_UPDATE,
        PLAYERHOOK_ON_STORE_NEW_ITEM,
        PLAYERHOOK_ON_PLAYER_QUEST_ACCEPT,
        PLAYERHOOK_ON_SPELL_CAST,
        PLAYERHOOK_ON_QUEST_REWARD_ITEM,
        PLAYERHOOK_ON_TAKE_MAIL_ITEM,
        PLAYERHOOK_ON_BEFORE_STORE_OR_EQUIP_NEW_ITEM,
        PLAYERHOOK_ON_CREATE_ITEM
    }) { }

    void OnPlayerLogin(Player* player) override
    {
        if (!sAbyssalStoragePlusMgr->IsEnabled())
            return;

        if (sAbyssalStoragePlusMgr->IsBotAccount(player))
            return;

        uint32 accountId = player->GetSession()->GetAccountId();
        sAbyssalStoragePlusMgr->LoadAccountData(accountId);
        sAbyssalStoragePlusMgr->SendFullSync(player);
    }

    void OnPlayerLogout(Player* player) override
    {
        if (!player)
            return;

        uint32 accountId = player->GetSession()->GetAccountId();

        if (sAbyssalStoragePlusMgr->IsEnabled() && !sAbyssalStoragePlusMgr->IsBotAccount(player))
        {
            // Re-vault any materialized items still in inventory
            AbyssalPlayerData* data = GetAbyssalData(player);
            RevaultMaterializedItems(player, data, accountId);
        }

        // Release this character; the cache unloads when the last one leaves
        sAbyssalStoragePlusMgr->UnloadAccountData(accountId);
    }

    void OnPlayerStoreNewItem(Player* player, Item* item, uint32 count) override
    {
        if (!sAbyssalStoragePlusMgr->IsEnabled() || !player || !item)
            return;

        if (sAbyssalStoragePlusMgr->IsBotAccount(player))
            return;

        AbyssalPlayerData* data = GetAbyssalData(player);
        if (!data || !data->autoStoreEnabled)
            return;

        // Take the source tag before any early-out so it cannot leak
        AbyssalAcquireSource source = data->pendingSource;
        uint32 sourceTime = data->pendingSourceTime;
        data->pendingSource = AbyssalAcquireSource::Unknown;
        data->pendingSourceTime = 0;

        // Don't auto-store items being materialized from vault
        if (data->isMaterializing)
            return;

        // Don't auto-store materialized items
        if (data->materializedItems.count(item->GetEntry()))
            return;

        ItemTemplate const* itemTemplate = item->GetTemplate();
        if (!sAbyssalStoragePlusMgr->ShouldAutoStore(player, itemTemplate))
            return;

        uint32 accountId = player->GetSession()->GetAccountId();

        // An open loot window means this item was just looted
        if (!player->GetLootGUID().IsEmpty())
            source = AbyssalAcquireSource::Loot;
        else if (source != AbyssalAcquireSource::Unknown && time(nullptr) - sourceTime > 10)
            source = AbyssalAcquireSource::Unknown; // stale tag

        // Send the notice before the client prints its own chat line
        if (sAbyssalStoragePlusMgr->IsAutoVaultEnabled(accountId, source))
            sAbyssalStoragePlusMgr->SendVaultNotice(player, item->GetEntry(), count);

        // Defer the deposit; OnPlayerUpdate gates it by the resolved source
        data->pendingDeposits.push_back({ item->GetEntry(), count, source });
    }

    void OnPlayerSpellCast(Player* player, Spell* spell, bool /*skipCheck*/) override
    {
        if (!sAbyssalStoragePlusMgr->IsEnabled() || !player || !spell)
            return;

        SpellInfo const* spellInfo = spell->GetSpellInfo();
        AbyssalPlayerData* data = GetAbyssalData(player);
        if (!spellInfo || !data)
            return;

        // Tag the next store so crafted items use the craft setting
        if (spellInfo->HasAttribute(SPELL_ATTR0_IS_TRADESKILL))
        {
            data->pendingSource = AbyssalAcquireSource::Craft;
            data->pendingSourceTime = static_cast<uint32>(time(nullptr));
        }
    }

    void OnPlayerBeforeStoreOrEquipNewItem(Player* player, uint32 /*vendorslot*/, uint32& /*item*/,
        uint8 /*count*/, uint8 /*bag*/, uint8 /*slot*/, ItemTemplate const* /*pProto*/,
        Creature* /*pVendor*/, VendorItem const* /*crItem*/, bool bStore) override
    {
        if (!sAbyssalStoragePlusMgr->IsEnabled() || !player || !bStore)
            return;

        AbyssalPlayerData* data = GetAbyssalData(player);
        if (!data)
            return;

        // Tag the store that follows so vendor buys use the vendor setting
        data->pendingSource = AbyssalAcquireSource::Vendor;
        data->pendingSourceTime = static_cast<uint32>(time(nullptr));
    }

    void OnPlayerQuestRewardItem(Player* player, Item* item, uint32 count) override
    {
        if (!sAbyssalStoragePlusMgr->IsEnabled() || !player || !item)
            return;

        AbyssalPlayerData* data = GetAbyssalData(player);
        if (!data)
            return;

        // Tag the just-stored candidate so the quest setting applies
        bool tagged = false;
        for (auto it = data->pendingDeposits.rbegin(); it != data->pendingDeposits.rend(); ++it)
        {
            if (it->source == AbyssalAcquireSource::Unknown &&
                it->itemEntry == item->GetEntry() && it->count == count)
            {
                it->source = AbyssalAcquireSource::QuestReward;
                tagged = true;
                break;
            }
        }

        // Quest rewards print no loot line, so notify the client directly
        if (tagged && sAbyssalStoragePlusMgr->IsAutoVaultEnabled(
                player->GetSession()->GetAccountId(), AbyssalAcquireSource::QuestReward))
            sAbyssalStoragePlusMgr->SendVaultNotification(player, item->GetEntry(), count);
    }

    void OnPlayerCreateItem(Player* player, Item* item, uint32 count) override
    {
        if (!sAbyssalStoragePlusMgr->IsEnabled() || !player || !item)
            return;

        AbyssalPlayerData* data = GetAbyssalData(player);
        if (!data)
            return;

        // Crafted items pass through here right after the store
        for (auto it = data->pendingDeposits.rbegin(); it != data->pendingDeposits.rend(); ++it)
        {
            if (it->source == AbyssalAcquireSource::Unknown &&
                it->itemEntry == item->GetEntry() && it->count == count)
            {
                it->source = AbyssalAcquireSource::Craft;
                break;
            }
        }
    }

    void OnPlayerTakeMailItem(Player* player, Mail const* /*mail*/, uint32 itemEntry) override
    {
        if (!sAbyssalStoragePlusMgr->IsEnabled() || !player)
            return;

        if (sAbyssalStoragePlusMgr->IsBotAccount(player))
            return;

        uint32 accountId = player->GetSession()->GetAccountId();
        if (!sAbyssalStoragePlusMgr->IsAutoVaultEnabled(accountId, AbyssalAcquireSource::Mail))
            return;

        AbyssalPlayerData* data = GetAbyssalData(player);
        if (!data || data->isMaterializing || data->materializedItems.count(itemEntry))
            return;

        if (!sAbyssalStoragePlusMgr->ShouldAutoStore(player, sObjectMgr->GetItemTemplate(itemEntry)))
            return;

        // The taken item may have merged into a bag stack, so the material as a whole moves to the vault
        uint32 const count = player->GetItemCount(itemEntry);
        if (!count)
            return;

        // Mail items never reach OnPlayerStoreNewItem, so queue them here
        data->pendingDeposits.push_back({ itemEntry, count, AbyssalAcquireSource::Mail });

        // Mail prints no loot line, so notify the client directly
        sAbyssalStoragePlusMgr->SendVaultNotification(player, itemEntry, count);
    }

    void OnPlayerUpdate(Player* player, uint32 /*p_time*/) override
    {
        if (!sAbyssalStoragePlusMgr->IsEnabled() || !player)
            return;

        // Cheap early-out: only players with deferred deposits need any work
        AbyssalPlayerData* data = player->CustomData.Get<AbyssalPlayerData>(ABYSSAL_DATA_KEY);
        if (!data || data->pendingDeposits.empty())
            return;

        if (sAbyssalStoragePlusMgr->IsBotAccount(player))
            return;

        // Move pending list out so we don't re-enter if DestroyItemCount triggers hooks
        std::vector<PendingDeposit> deposits = std::move(data->pendingDeposits);
        data->pendingDeposits.clear();

        uint32 accountId = player->GetSession()->GetAccountId();

        // One quest-log scan for the whole batch
        std::unordered_map<uint32, uint32> questReserved;
        sAbyssalStoragePlusMgr->GetQuestReservedCounts(player, questReserved);

        for (auto const& dep : deposits)
        {
            // Skip sources the player turned off in the settings
            if (!sAbyssalStoragePlusMgr->IsAutoVaultEnabled(accountId, dep.source))
                continue;

            // Verify the player still has the items (they may have been used/moved)
            uint32 playerHas = player->GetItemCount(dep.itemEntry);
            uint32 toDeposit = std::min(dep.count, playerHas);
            if (toDeposit == 0)
                continue;

            // Never deposit below the quest-required threshold
            auto reservedIt = questReserved.find(dep.itemEntry);
            uint32 questReservedCount = reservedIt != questReserved.end() ? reservedIt->second : 0;
            if (playerHas <= questReservedCount)
                continue;
            toDeposit = std::min(toDeposit, playerHas - questReservedCount);

            player->DestroyItemCount(dep.itemEntry, toDeposit, true);
            uint32 after = player->GetItemCount(dep.itemEntry);
            uint32 destroyed = playerHas > after ? playerHas - after : 0;
            if (destroyed == 0)
                continue;

            sAbyssalStoragePlusMgr->DepositItem(player, accountId, dep.itemEntry, destroyed);
            sAbyssalStoragePlusMgr->BroadcastItemUpdate(accountId, dep.itemEntry);
        }
    }

    void OnPlayerQuestAccept(Player* player, Quest const* quest) override
    {
        if (!sAbyssalStoragePlusMgr->IsEnabled() || !player || !quest)
            return;

        uint32 accountId = player->GetSession()->GetAccountId();

        if (!sAbyssalStoragePlusMgr->IsAutoWithdrawQuestAcceptEnabled(accountId))
            return;

        if (sAbyssalStoragePlusMgr->IsBotAccount(player))
            return;

        // Only quests that need items to be completed
        if (!quest->HasSpecialFlag(QUEST_SPECIAL_FLAGS_DELIVER))
            return;

        // Auto-complete quests do not require items in bags
        if (quest->IsAutoComplete() || !quest->GetQuestMethod())
            return;

        AbyssalPlayerData* data = GetAbyssalData(player);
        bool previousAutoStore = data ? data->autoStoreEnabled : true;
        if (data)
            data->autoStoreEnabled = false;

        for (uint8 i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; ++i)
        {
            uint32 itemEntry = quest->RequiredItemId[i];
            uint32 required = quest->RequiredItemCount[i];
            if (!itemEntry || !required)
                continue;

            uint32 inBags = player->GetItemCount(itemEntry);
            if (inBags >= required)
                continue;

            uint32 missing = required - inBags;
            uint32 vaultCount = sAbyssalStoragePlusMgr->GetItemCount(accountId, itemEntry);
            if (vaultCount == 0)
                continue;

            uint32 wanted = std::min(missing, vaultCount);
            uint32 pulled = PullQuestMaterials(player, accountId, itemEntry, wanted);

            if (pulled == 0)
            {
                ChatHandler(player->GetSession()).PSendSysMessage(
                    "|cFF00B0E8Abyssal Storage Plus: Not enough bag space to pull |r{}|cFF00B0E8 for quest - '{}|cFF00B0E8'.|r",
                    BuildItemLink(itemEntry), BuildQuestLink(quest));
                continue;
            }

            sAbyssalStoragePlusMgr->BroadcastItemUpdate(accountId, itemEntry);

            if (pulled < missing)
            {
                ChatHandler(player->GetSession()).PSendSysMessage(
                    "|cFF00B0E8Abyssal Storage Plus:|r |cff1cd3a2Pulled|r {} x{} |cff1cd3a2from storage for quest - |r{} {} still needed.",
                    BuildItemLink(itemEntry), pulled, BuildQuestLink(quest), missing - pulled);
            }
            else
            {
                ChatHandler(player->GetSession()).PSendSysMessage(
                    "|cFF00B0E8Abyssal Storage Plus:|r |cff1cd3a2Pulled|r {} x{} |cff1cd3a2from storage for quest - |r{}",
                    BuildItemLink(itemEntry), pulled, BuildQuestLink(quest));
            }
        }

        if (data)
            data->autoStoreEnabled = previousAutoStore;
    }
};

// ============================================================================
// AllSpellScript — Crafting Materialization
// ============================================================================

class AbyssalStoragePlusSpellScript : public AllSpellScript
{
public:
    AbyssalStoragePlusSpellScript() : AllSpellScript("AbyssalStoragePlusSpellScript", {
        ALLSPELLHOOK_ON_SPELL_CHECK_CAST,
        ALLSPELLHOOK_ON_CAST_CANCEL,
        ALLSPELLHOOK_ON_CAST
    }) { }

    void OnSpellCheckCast(Spell* spell, bool /*strict*/, SpellCastResult& res) override
    {
        if (!sAbyssalStoragePlusMgr->IsEnabled())
            return;

        if (res != SPELL_CAST_OK)
            return;

        Unit* caster = spell->GetCaster() ? spell->GetCaster()->ToUnit() : nullptr;
        if (!caster || !caster->IsPlayer())
            return;

        Player* player = caster->ToPlayer();

        if (sAbyssalStoragePlusMgr->IsBotAccount(player))
            return;

        SpellInfo const* spellInfo = spell->GetSpellInfo();

        // Check if this spell uses reagents
        bool hasReagents = false;
        for (uint8 i = 0; i < MAX_SPELL_REAGENTS; ++i)
        {
            if (spellInfo->Reagent[i] > 0 && spellInfo->ReagentCount[i] > 0)
            {
                hasReagents = true;
                break;
            }
        }

        if (!hasReagents)
            return;

        uint32 accountId = player->GetSession()->GetAccountId();
        AbyssalPlayerData* data = GetAbyssalData(player);
        if (!data)
            return;

        // First pass: verify vault can cover all deficits before materializing anything
        for (uint8 i = 0; i < MAX_SPELL_REAGENTS; ++i)
        {
            int32 reagentEntry = spellInfo->Reagent[i];
            uint32 reagentCount = spellInfo->ReagentCount[i];
            if (reagentEntry <= 0 || reagentCount == 0)
                continue;

            uint32 playerHas = player->GetItemCount(reagentEntry);
            if (playerHas >= reagentCount)
                continue;

            uint32 deficit = reagentCount - playerHas;
            uint32 vaultCount = sAbyssalStoragePlusMgr->GetItemCount(accountId, reagentEntry);

            if (vaultCount < deficit)
                return; // not enough even with vault
        }

        // Second pass: materialize deficits
        data->isMaterializing = true;
        for (uint8 i = 0; i < MAX_SPELL_REAGENTS; ++i)
        {
            int32 reagentEntry = spellInfo->Reagent[i];
            uint32 reagentCount = spellInfo->ReagentCount[i];
            if (reagentEntry <= 0 || reagentCount == 0)
                continue;

            uint32 playerHas = player->GetItemCount(reagentEntry);
            if (playerHas >= reagentCount)
                continue;

            uint32 deficit = reagentCount - playerHas;

            // Send invariant total before the bag packet to avoid a count flicker
            sAbyssalStoragePlusMgr->SendItemUpdate(player, uint32(reagentEntry),
                sAbyssalStoragePlusMgr->GetItemCount(accountId, uint32(reagentEntry)));

            ItemPosCountVec dest;
            InventoryResult invResult = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, reagentEntry, deficit);
            if (invResult != EQUIP_ERR_OK)
            {
                // Roll back reagents pulled for earlier slots
                RevaultMaterializedItems(player, data, accountId);
                data->isMaterializing = false;
                res = SPELL_FAILED_DONT_REPORT;
                ChatHandler(player->GetSession()).SendSysMessage("Abyssal Storage Plus: Not enough bag space to materialize crafting reagents.");
                return;
            }

            uint32 bagBefore = player->GetItemCount(reagentEntry);
            Item* newItem = player->StoreNewItem(dest, reagentEntry, true);
            if (!newItem)
            {
                // Roll back reagents pulled for earlier slots
                RevaultMaterializedItems(player, data, accountId);
                data->isMaterializing = false;
                res = SPELL_FAILED_DONT_REPORT;
                ChatHandler(player->GetSession()).SendSysMessage("Abyssal Storage Plus: Not enough bag space to materialize crafting reagents.");
                return;
            }

            if (!sAbyssalStoragePlusMgr->WithdrawItem(player, accountId, reagentEntry, deficit))
            {
                // Vault debit failed; undo exactly what was stored
                UndoStoredItems(player, uint32(reagentEntry), bagBefore);
                RevaultMaterializedItems(player, data, accountId);
                data->isMaterializing = false;
                res = SPELL_FAILED_DONT_REPORT;
                ChatHandler(player->GetSession()).SendSysMessage("Abyssal Storage Plus: Not enough stored reagents to cast.");
                return;
            }

            TrackMaterializedItem(data, uint32(reagentEntry), playerHas, deficit);
            sAbyssalStoragePlusMgr->BroadcastItemUpdate(accountId, uint32(reagentEntry));
        }
        data->isMaterializing = false;
    }

    void OnSpellCast(Spell* /*spell*/, Unit* caster, SpellInfo const* spellInfo, bool /*skipCheck*/) override
    {
        if (!sAbyssalStoragePlusMgr->IsEnabled())
            return;

        if (!caster || !caster->IsPlayer())
            return;

        Player* player = caster->ToPlayer();

        if (sAbyssalStoragePlusMgr->IsBotAccount(player))
            return;

        AbyssalPlayerData* data = GetAbyssalData(player);
        if (!data || data->materializedItems.empty())
            return;

        uint32 accountId = player->GetSession()->GetAccountId();

        // Refresh totals now that this craft consumed its reagents
        SendSpellReagentTotals(player, spellInfo, accountId);

        // Multi-craft: if more crafts remain, queue the next one instead of re-vaulting
        if (data->pendingCrafts > 0 && data->pendingSpellId == spellInfo->Id)
        {
            data->pendingCrafts--;
            if (data->pendingCrafts > 0)
            {
                player->CastSpell(player, data->pendingSpellId, false);
                return; // don't re-vault yet
            }
            // Last craft done — fall through to re-vault leftovers
        }

        // Re-vault any materialized items that are still in inventory (leftovers)
        RevaultMaterializedItems(player, data, accountId);
        data->pendingCrafts = 0;
        data->pendingSpellId = 0;
        data->autoStoreEnabled = true;
    }

    void OnSpellCastCancel(Spell* /*spell*/, Unit* caster, SpellInfo const* spellInfo, bool /*bySelf*/) override
    {
        if (!sAbyssalStoragePlusMgr->IsEnabled())
            return;

        if (!caster || !caster->IsPlayer())
            return;

        Player* player = caster->ToPlayer();

        if (sAbyssalStoragePlusMgr->IsBotAccount(player))
            return;

        AbyssalPlayerData* data = GetAbyssalData(player);
        if (!data || data->materializedItems.empty())
            return;

        uint32 accountId = player->GetSession()->GetAccountId();

        // Send the pre-revault total (invariant) before bags shrink
        SendSpellReagentTotals(player, spellInfo, accountId);

        // Interrupted craft: return pulled reagents to the vault
        RevaultMaterializedItems(player, data, accountId);
        data->pendingCrafts = 0;
        data->pendingSpellId = 0;
        data->autoStoreEnabled = true;
    }
};

// ============================================================================
// CommandScript — Player Commands
// ============================================================================

using namespace Acore::ChatCommands;

class AbyssalStoragePlusCommandScript : public CommandScript
{
public:
    AbyssalStoragePlusCommandScript() : CommandScript("AbyssalStoragePlusCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable aspCommandTable =
        {
            { "withdraw", HandleWithdrawCommand,  SEC_PLAYER, Console::No },
            { "deposit",  HandleDepositCommand,   SEC_PLAYER, Console::No },
            { "sync",     HandleSyncCommand,      SEC_PLAYER, Console::No },
            { "settings", HandleSettingsCommand, SEC_PLAYER, Console::No },
            { "craft",    HandleCraftCommand,      SEC_PLAYER, Console::No },
            { "prepare",  HandlePrepareCommand,    SEC_PLAYER, Console::No },
            { "revault",  HandleRevaultCommand,    SEC_PLAYER, Console::No },
        };
        static ChatCommandTable commandTable =
        {
            { "asp",     aspCommandTable },
            { "abyssal", aspCommandTable },
        };
        return commandTable;
    }

    static bool HandleWithdrawCommand(ChatHandler* handler, uint32 itemEntry, Optional<uint32> optCount)
    {
        if (!sAbyssalStoragePlusMgr->IsEnabled())
            return false;

        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        uint32 accountId = player->GetSession()->GetAccountId();
        uint32 vaultCount = sAbyssalStoragePlusMgr->GetItemCount(accountId, itemEntry);

        if (vaultCount == 0)
        {
            handler->SendSysMessage("Abyssal Storage Plus: Item not found in vault.");
            return true;
        }

        uint32 count = optCount.value_or(vaultCount);
        if (count > vaultCount)
            count = vaultCount;

        ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemEntry);
        if (!itemTemplate)
        {
            handler->SendSysMessage("Abyssal Storage Plus: Invalid item.");
            return true;
        }

        // Disable auto-store BEFORE creating items, otherwise OnPlayerStoreNewItem
        // will immediately re-deposit them back into the vault
        AbyssalPlayerData* data = GetAbyssalData(player);
        if (data)
            data->autoStoreEnabled = false;

        // Add items to player in stacks respecting max stack size
        uint32 remaining = count;
        while (remaining > 0)
        {
            uint32 stackSize = std::min(remaining, itemTemplate->GetMaxStackSize());

            ItemPosCountVec dest;
            InventoryResult result = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemEntry, stackSize);
            if (result != EQUIP_ERR_OK)
            {
                handler->SendSysMessage("Abyssal Storage Plus: Not enough bag space.");
                break;
            }

            uint32 bagBefore = player->GetItemCount(itemEntry);
            Item* newItem = player->StoreNewItem(dest, itemEntry, true);
            if (!newItem)
            {
                handler->SendSysMessage("Abyssal Storage Plus: Not enough bag space.");
                break;
            }

            if (!sAbyssalStoragePlusMgr->WithdrawItem(player, accountId, itemEntry, stackSize))
            {
                // Vault debit failed; undo exactly what was stored
                UndoStoredItems(player, itemEntry, bagBefore);
                break;
            }

            remaining -= stackSize;
        }

        // Re-enable auto-store even if the withdrawal stopped early
        if (data)
            data->autoStoreEnabled = true;

        uint32 withdrawn = count - remaining;
        if (withdrawn > 0)
        {
            sAbyssalStoragePlusMgr->BroadcastItemUpdate(accountId, itemEntry);

            handler->PSendSysMessage("|cFF00B0E8Abyssal Storage Plus:|r Withdrew {} x{}.", BuildItemLink(itemEntry), withdrawn);
        }

        return true;
    }

    // Ensure the player has at least `need` of an item in bags, pulling the deficit
    static bool HandlePrepareCommand(ChatHandler* handler, uint32 itemEntry, uint32 need)
    {
        if (!sAbyssalStoragePlusMgr->IsEnabled())
            return false;

        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        if (need == 0)
            return true;

        uint32 playerHas = player->GetItemCount(itemEntry);
        if (playerHas >= need)
            return true;

        uint32 accountId = player->GetSession()->GetAccountId();
        uint32 vaultCount = sAbyssalStoragePlusMgr->GetItemCount(accountId, itemEntry);
        if (vaultCount == 0)
            return true;

        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemEntry);
        if (!proto)
            return true;

        uint32 toWithdraw = std::min(need - playerHas, vaultCount);

        AbyssalPlayerData* data = GetAbyssalData(player);
        bool wasAutoStore = data ? data->autoStoreEnabled : true;
        if (data)
            data->autoStoreEnabled = false;

        uint32 maxStack = std::max<uint32>(proto->GetMaxStackSize(), 1);
        uint32 remaining = toWithdraw;
        uint32 stored = 0;
        while (remaining > 0)
        {
            uint32 chunk = std::min(remaining, maxStack);
            ItemPosCountVec dest;
            InventoryResult result = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemEntry, chunk);
            if (result != EQUIP_ERR_OK)
                break;

            uint32 bagBefore = player->GetItemCount(itemEntry);
            Item* newItem = player->StoreNewItem(dest, itemEntry, true);
            if (!newItem)
                break;

            if (!sAbyssalStoragePlusMgr->WithdrawItem(player, accountId, itemEntry, chunk))
            {
                // Vault debit failed; undo exactly what was stored
                UndoStoredItems(player, itemEntry, bagBefore);
                break;
            }

            stored += chunk;
            remaining -= chunk;
        }

        if (data)
        {
            // Track so leftovers return to the vault on cancel/deselect
            if (stored > 0)
                TrackMaterializedItem(data, itemEntry, playerHas, stored);
            data->autoStoreEnabled = wasAutoStore;
        }

        if (stored > 0)
        {
            sAbyssalStoragePlusMgr->BroadcastItemUpdate(accountId, itemEntry);
        }

        // Bag space ran out before the full amount could be pulled
        if (stored < toWithdraw)
        {
            handler->PSendSysMessage("|cFF00B0E8Abyssal Storage Plus: |cff1cd3a2Not enough bag space to pull |r{}.|r",
                BuildItemLink(itemEntry));
        }

        return true;
    }

    // Return any crafting/quest reagents that were pulled but not consumed
    static bool HandleRevaultCommand(ChatHandler* handler)
    {
        if (!sAbyssalStoragePlusMgr->IsEnabled())
            return false;

        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        AbyssalPlayerData* data = GetAbyssalData(player);
        if (!data || data->materializedItems.empty())
            return true;

        uint32 accountId = player->GetSession()->GetAccountId();
        RevaultMaterializedItems(player, data, accountId);
        return true;
    }

    static bool HandleDepositCommand(ChatHandler* handler, Optional<uint32> optItemEntry, Optional<uint32> optCount,
                                     Optional<uint32> optBag, Optional<uint32> optSlot)
    {
        if (!sAbyssalStoragePlusMgr->IsEnabled())
            return false;

        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        uint32 accountId = player->GetSession()->GetAccountId();

        if (optItemEntry)
        {
            uint32 itemEntry = *optItemEntry;
            ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemEntry);
            if (!itemTemplate)
            {
                handler->SendSysMessage("Abyssal Storage Plus: Invalid item.");
                return true;
            }

            if (!sAbyssalStoragePlusMgr->ShouldAutoStore(player, itemTemplate))
            {
                if (sAbyssalStoragePlusMgr->IsItemRequiredByActiveQuest(player, itemEntry))
                    handler->SendSysMessage("Abyssal Storage Plus: That item is needed for a quest.");
                else
                    handler->SendSysMessage("Abyssal Storage Plus: Only materials can be deposited.");
                return true;
            }

            uint32 playerHas = player->GetItemCount(itemEntry);
            if (playerHas == 0)
            {
                handler->SendSysMessage("Abyssal Storage Plus: You do not have that item.");
                return true;
            }

            uint32 questReserved = sAbyssalStoragePlusMgr->GetQuestReservedCount(player, itemEntry);
            if (playerHas <= questReserved)
            {
                handler->SendSysMessage("Abyssal Storage Plus: That item is needed for a quest.");
                return true;
            }

            uint32 count = optCount.value_or(0);
            if (count == 0 || count > playerHas)
                count = playerHas;
            count = std::min(count, playerHas - questReserved);

            if (optBag.has_value() != optSlot.has_value())
            {
                handler->SendSysMessage("Abyssal Storage Plus: Invalid bag slot.");
                return true;
            }

            Item* exactItem = nullptr;
            if (optBag.has_value() && optSlot.has_value())
            {
                uint32 bagId = *optBag;
                uint32 slotId = *optSlot;
                uint32 const maxBag = INVENTORY_SLOT_BAG_END - INVENTORY_SLOT_BAG_START;
                uint32 const backpackSlots = INVENTORY_SLOT_ITEM_END - INVENTORY_SLOT_ITEM_START;

                if (bagId > maxBag || slotId < 1 || slotId > 255 || (bagId == 0 && slotId > backpackSlots))
                {
                    handler->SendSysMessage("Abyssal Storage Plus: Invalid bag slot.");
                    return true;
                }

                // Client slots are 1-based; server bag slots are 0-based
                uint8 serverBag = INVENTORY_SLOT_BAG_0;
                uint8 serverSlot = static_cast<uint8>(slotId - 1);
                if (bagId == 0)
                    serverSlot = static_cast<uint8>(INVENTORY_SLOT_ITEM_START + slotId - 1);
                else
                    serverBag = static_cast<uint8>(INVENTORY_SLOT_BAG_START + bagId - 1);

                exactItem = player->GetItemByPos(serverBag, serverSlot);
                if (!exactItem || exactItem->GetEntry() != itemEntry || exactItem->IsInTrade())
                {
                    handler->SendSysMessage("Abyssal Storage Plus: That item moved. Try again.");
                    return true;
                }

                count = std::min(count, exactItem->GetCount());
                if (count == 0)
                    return true;
            }

            uint32 destroyed = 0;
            if (exactItem)
            {
                // This overload leaves the undestroyed remainder in destroyCount
                uint32 destroyCount = count;
                player->DestroyItemCount(exactItem, destroyCount, true);
                destroyed = count - destroyCount;
            }
            else
            {
                uint32 before = player->GetItemCount(itemEntry);
                player->DestroyItemCount(itemEntry, count, true);
                uint32 after = player->GetItemCount(itemEntry);
                destroyed = before > after ? before - after : 0;
            }

            if (destroyed == 0)
            {
                handler->SendSysMessage("Abyssal Storage Plus: That item moved. Try again.");
                return true;
            }

            sAbyssalStoragePlusMgr->DepositItem(player, accountId, itemEntry, destroyed);

            AbyssalPlayerData* data = GetAbyssalData(player);
            if (data)
                data->autoStoreEnabled = true;

            sAbyssalStoragePlusMgr->BroadcastItemUpdate(accountId, itemEntry);
            handler->PSendSysMessage("|cFF00B0E8Abyssal Storage Plus:|r Deposited {} x{}.", BuildItemLink(itemEntry), destroyed);
            return true;
        }

        // Collect totals first — DestroyItemCount searches the whole inventory,
        // so destroying during iteration can skip stacks of the same item
        std::unordered_map<uint32, uint32> toDeposit; // itemEntry -> totalCount

        // One quest-log scan for the whole deposit
        std::unordered_map<uint32, uint32> questReserved;
        sAbyssalStoragePlusMgr->GetQuestReservedCounts(player, questReserved);

        for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
        {
            if (Bag* pBag = player->GetBagByPos(bag))
            {
                for (uint8 slot = 0; slot < pBag->GetBagSize(); ++slot)
                {
                    Item* item = pBag->GetItemByPos(slot);
                    if (!item)
                        continue;

                    if (sAbyssalStoragePlusMgr->ShouldAutoStore(player, item->GetTemplate(), &questReserved))
                        toDeposit[item->GetEntry()] += item->GetCount();
                }
            }
        }

        // Also scan the default backpack (slots 23-38)
        for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (!item)
                continue;

            if (sAbyssalStoragePlusMgr->ShouldAutoStore(player, item->GetTemplate(), &questReserved))
                toDeposit[item->GetEntry()] += item->GetCount();
        }

        // Now destroy and deposit in one pass per item entry
        uint32 depositedCount = 0;
        for (auto const& [entry, count] : toDeposit)
        {
            // Never move items an active quest still needs
            uint32 playerHas = player->GetItemCount(entry);
            auto reservedIt = questReserved.find(entry);
            uint32 reserved = reservedIt != questReserved.end() ? reservedIt->second : 0;
            uint32 surplus = playerHas > reserved ? playerHas - reserved : 0;
            uint32 toMove = std::min(count, surplus);
            if (toMove == 0)
                continue;

            player->DestroyItemCount(entry, toMove, true);
            uint32 after = player->GetItemCount(entry);
            uint32 destroyed = playerHas > after ? playerHas - after : 0;
            if (destroyed == 0)
                continue;

            sAbyssalStoragePlusMgr->DepositItem(player, accountId, entry, destroyed);
            ++depositedCount;
        }

        AbyssalPlayerData* data = GetAbyssalData(player);
        if (data)
            data->autoStoreEnabled = true;

        sAbyssalStoragePlusMgr->BroadcastFullSync(accountId);
        handler->PSendSysMessage("|cFF00B0E8Abyssal Storage Plus:|r Deposited {} item stacks.", depositedCount);

        return true;
    }

    static bool HandleSyncCommand(ChatHandler* handler)
    {
        if (!sAbyssalStoragePlusMgr->IsEnabled())
            return false;

        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        sAbyssalStoragePlusMgr->SendFullSync(player);
        handler->PSendSysMessage("|cFF00B0E8Abyssal Storage Plus:|r Sync complete.");
        return true;
    }

    // .asp settings <loot> <craft> <mail> <quest> <vendor> <accept>
    // Each value is 0 or 1; sent by the addon options panel
    static bool HandleSettingsCommand(ChatHandler* handler, uint32 loot, uint32 craft, uint32 mail,
                                      uint32 quest, uint32 vendor, uint32 accept)
    {
        if (!sAbyssalStoragePlusMgr->IsEnabled())
            return false;

        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        uint32 accountId = player->GetSession()->GetAccountId();

        AbyssalSettings settings;
        settings.autoVaultLoot = loot != 0;
        settings.autoVaultCraft = craft != 0;
        settings.autoVaultMail = mail != 0;
        settings.autoVaultQuestReward = quest != 0;
        settings.autoVaultVendor = vendor != 0;
        settings.autoWithdrawQuestAccept = accept != 0;

        sAbyssalStoragePlusMgr->SetSettings(accountId, settings);
        handler->PSendSysMessage("|cFF00B0E8Abyssal Storage Plus:|r Settings saved.");
        return true;
    }

    // .asp craft <spellId> [count]
    // Materializes reagents from vault and casts the crafting spell
    static bool HandleCraftCommand(ChatHandler* handler, uint32 spellId, Optional<uint32> optCount)
    {
        if (!sAbyssalStoragePlusMgr->IsEnabled())
            return false;

        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!spellInfo)
        {
            handler->SendSysMessage("|cFF00B0E8Abyssal Storage Plus:|r Invalid spell.");
            return true;
        }

        if (!player->HasSpell(spellId) || !spellInfo->HasAttribute(SPELL_ATTR0_IS_TRADESKILL))
        {
            handler->SendSysMessage("|cFF00B0E8Abyssal Storage Plus:|r You do not know that recipe.");
            return true;
        }

        // Verify spell has reagents
        bool hasReagents = false;
        for (uint8 i = 0; i < MAX_SPELL_REAGENTS; ++i)
        {
            if (spellInfo->Reagent[i] > 0 && spellInfo->ReagentCount[i] > 0)
            {
                hasReagents = true;
                break;
            }
        }

        if (!hasReagents)
        {
            handler->SendSysMessage("|cFF00B0E8Abyssal Storage Plus:|r Spell has no reagents.");
            return true;
        }

        uint32 accountId = player->GetSession()->GetAccountId();
        uint32 craftCount = optCount.value_or(1);
        if (craftCount == 0)
            craftCount = 1;

        // Gather reagent info: what the player has, what the vault has, per-craft need
        struct ReagentInfo
        {
            int32  entry;
            uint32 perCraft;
            uint32 playerHas;
            uint32 vaultHas;
            uint32 maxStack;
        };
        std::vector<ReagentInfo> reagents;

        for (uint8 i = 0; i < MAX_SPELL_REAGENTS; ++i)
        {
            int32 reagentEntry = spellInfo->Reagent[i];
            uint32 reagentCount = spellInfo->ReagentCount[i];
            if (reagentEntry <= 0 || reagentCount == 0)
                continue;

            uint32 playerHas = player->GetItemCount(reagentEntry);
            uint32 vaultHas = sAbyssalStoragePlusMgr->GetItemCount(accountId, reagentEntry);

            if (playerHas + vaultHas < reagentCount)
            {
                handler->SendSysMessage("|cFF00B0E8Abyssal Storage Plus:|r Not enough reagents.");
                return true;
            }

            ItemTemplate const* tmpl = sObjectMgr->GetItemTemplate(reagentEntry);
            uint32 maxStack = tmpl ? tmpl->GetMaxStackSize() : 1;

            reagents.push_back({ reagentEntry, reagentCount, playerHas, vaultHas, maxStack });
        }

        // Count free bag slots
        uint32 freeSlots = 0;
        for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        {
            if (!player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
                freeSlots++;
        }
        for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
        {
            if (Bag* pBag = player->GetBagByPos(bag))
            {
                for (uint8 slot = 0; slot < pBag->GetBagSize(); ++slot)
                {
                    if (!pBag->GetItemByPos(slot))
                        freeSlots++;
                }
            }
        }

        // Count how many distinct reagents need vault withdrawal
        uint32 vaultReagentSlots = 0;
        for (auto const& r : reagents)
        {
            if (r.playerHas < r.perCraft)
                vaultReagentSlots++;
        }

        // Need: 1 slot per vault reagent type + 1 for the crafted product
        if (freeSlots < vaultReagentSlots + 1)
        {
            handler->SendSysMessage("|cFF00B0E8Abyssal Storage Plus:|r Not enough bag space (need room for reagents + product).");
            return true;
        }

        // Cap craft count by total available reagents (inventory + vault)
        uint32 maxCrafts = craftCount;
        for (auto const& r : reagents)
        {
            uint32 possible = (r.playerHas + r.vaultHas) / r.perCraft;
            maxCrafts = std::min(maxCrafts, possible);
        }

        // Cap further: each vault reagent gets at most 1 max-stack of bag space,
        // so the amount we can withdraw is limited
        for (auto const& r : reagents)
        {
            if (r.playerHas >= r.perCraft * maxCrafts)
                continue; // inventory alone covers this reagent for maxCrafts

            // Available in bags = what player already has + up to 1 max stack from vault
            uint32 availableInBags = r.playerHas + std::min(r.vaultHas, r.maxStack);
            uint32 possible = availableInBags / r.perCraft;
            maxCrafts = std::min(maxCrafts, possible);
        }

        if (maxCrafts == 0)
        {
            handler->SendSysMessage("|cFF00B0E8Abyssal Storage Plus:|r Not enough reagents.");
            return true;
        }

        craftCount = std::min(craftCount, maxCrafts);

        AbyssalPlayerData* data = GetAbyssalData(player);
        if (data)
            data->isMaterializing = true;

        // Materialize reagents needed for craftCount crafts (at most 1 stack per type)
        for (auto const& r : reagents)
        {
            uint32 totalNeeded = r.perCraft * craftCount;
            if (r.playerHas >= totalNeeded)
                continue;

            uint32 deficit = totalNeeded - r.playerHas;
            uint32 toWithdraw = std::min(deficit, r.vaultHas);
            if (toWithdraw == 0)
                continue;

            // Send invariant total before the bag packet to avoid a count flicker
            sAbyssalStoragePlusMgr->SendItemUpdate(player, r.entry,
                sAbyssalStoragePlusMgr->GetItemCount(accountId, r.entry));

            ItemPosCountVec dest;
            InventoryResult invResult = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, r.entry, toWithdraw);
            if (invResult != EQUIP_ERR_OK)
            {
                // Roll back reagents pulled for earlier types
                if (data)
                {
                    RevaultMaterializedItems(player, data, accountId);
                    data->isMaterializing = false;
                }
                handler->SendSysMessage("|cFF00B0E8Abyssal Storage Plus:|r Not enough bag space for reagents.");
                return true;
            }

            uint32 bagBefore = player->GetItemCount(r.entry);
            Item* newItem = player->StoreNewItem(dest, r.entry, true);
            if (!newItem)
            {
                // Roll back reagents pulled for earlier types
                if (data)
                {
                    RevaultMaterializedItems(player, data, accountId);
                    data->isMaterializing = false;
                }
                handler->SendSysMessage("|cFF00B0E8Abyssal Storage Plus:|r Not enough bag space for reagents.");
                return true;
            }

            if (!sAbyssalStoragePlusMgr->WithdrawItem(player, accountId, uint32(r.entry), toWithdraw))
            {
                // Vault debit failed; undo exactly what was stored
                UndoStoredItems(player, uint32(r.entry), bagBefore);
                if (data)
                {
                    RevaultMaterializedItems(player, data, accountId);
                    data->isMaterializing = false;
                }
                handler->SendSysMessage("|cFF00B0E8Abyssal Storage Plus:|r Not enough stored reagents to craft.");
                return true;
            }

            if (data)
                TrackMaterializedItem(data, r.entry, r.playerHas, toWithdraw);

            sAbyssalStoragePlusMgr->BroadcastItemUpdate(accountId, uint32(r.entry));
        }

        if (data)
        {
            data->isMaterializing = false;
            data->pendingCrafts = craftCount;  // OnSpellCast will decrement and re-cast
            data->pendingSpellId = spellId;
        }

        // Cast once — OnSpellCast will chain the remaining crafts
        SpellCastResult castResult = player->CastSpell(player, spellId, false);
        if (castResult != SPELL_CAST_OK)
        {
            // Return withdrawn reagents if the cast is rejected
            if (data)
            {
                RevaultMaterializedItems(player, data, accountId);
                data->pendingCrafts = 0;
                data->pendingSpellId = 0;
                data->autoStoreEnabled = true;
            }
            handler->SendSysMessage("|cFF00B0E8Abyssal Storage Plus:|r Recipe could not be cast; materials returned.");
        }

        return true;
    }
};

// ============================================================================
// Registration
// ============================================================================

void AddSC_abyssal_storage_plus_scripts()
{
    new AbyssalStoragePlusWorldScript();
    new AbyssalStoragePlusPlayerScript();
    new AbyssalStoragePlusSpellScript();
    new AbyssalStoragePlusCommandScript();
}