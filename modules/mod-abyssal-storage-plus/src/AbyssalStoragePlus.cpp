#include "AbyssalStoragePlus.h"
#include "AccountMgr.h"
#include "DatabaseEnv.h"
#include "ItemTemplate.h"
#include "Player.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "WorldSessionMgr.h"
#include "Chat.h"
#include "ObjectMgr.h"
#include "QuestDef.h"
#include "Log.h"
#include <cctype>

AbyssalPlayerData* GetAbyssalData(Player* player)
{
    if (!player)
        return nullptr;
    return player->CustomData.GetDefault<AbyssalPlayerData>(ABYSSAL_DATA_KEY);
}

AbyssalStoragePlusMgr* AbyssalStoragePlusMgr::instance()
{
    static AbyssalStoragePlusMgr instance;
    return &instance;
}

void AbyssalStoragePlusMgr::LoadAccountData(uint32 accountId)
{
    ++_accountRefs[accountId];

    if (_loadedAccounts.find(accountId) != _loadedAccounts.end())
        return; // already loaded, another character is online

    QueryResult result = CharacterDatabase.Query("SELECT item_entry, count FROM abyssal_storage WHERE account_id = {}", accountId);

    std::unordered_map<uint32, uint32> items;
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 itemEntry = fields[0].Get<uint32>();
            uint32 count = fields[1].Get<uint32>();
            items[itemEntry] = count;
        } while (result->NextRow());
    }

    // Do not publish data if the last character logged out during the query
    auto refIt = _accountRefs.find(accountId);
    if (refIt == _accountRefs.end() || refIt->second == 0)
        return;

    // Never overwrite a vault another load already populated
    if (_loadedAccounts.find(accountId) != _loadedAccounts.end())
        return;

    _storage[accountId] = std::move(items);
    _loadedAccounts.insert(accountId);
}

void AbyssalStoragePlusMgr::UnloadAccountData(uint32 accountId)
{
    auto refIt = _accountRefs.find(accountId);
    if (refIt != _accountRefs.end())
    {
        // Keep the cache alive while another character is still online
        if (refIt->second > 1)
        {
            --refIt->second;
            return;
        }
        _accountRefs.erase(refIt);
    }

    _storage.erase(accountId);
    _loadedAccounts.erase(accountId);
    _settings.erase(accountId);
}

// The vault row and the bag items change together: saving only the vault would let a crash before the next
// character save duplicate deposited materials or lose withdrawn ones.
static void CommitWithInventory(Player* player, CharacterDatabaseTransaction trans)
{
    if (player)
        player->SaveInventoryAndGoldToDB(trans);
    CharacterDatabase.CommitTransaction(trans);
}

void AbyssalStoragePlusMgr::DepositItem(Player* player, uint32 accountId, uint32 itemEntry, uint32 count)
{
    // Only touch the cache for fully loaded accounts
    if (_loadedAccounts.find(accountId) != _loadedAccounts.end())
        _storage[accountId][itemEntry] += count;

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    trans->Append("INSERT INTO abyssal_storage (account_id, item_entry, count) VALUES ({}, {}, {}) "
        "ON DUPLICATE KEY UPDATE count = count + {}", accountId, itemEntry, count, count);
    CommitWithInventory(player, trans);
}

bool AbyssalStoragePlusMgr::WithdrawItem(Player* player, uint32 accountId, uint32 itemEntry, uint32 count)
{
    auto accIt = _storage.find(accountId);
    if (accIt == _storage.end())
        return false;

    auto itemIt = accIt->second.find(itemEntry);
    if (itemIt == accIt->second.end() || itemIt->second < count)
        return false;

    itemIt->second -= count;

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    if (itemIt->second == 0)
    {
        accIt->second.erase(itemIt);
        trans->Append("DELETE FROM abyssal_storage WHERE account_id = {} AND item_entry = {}", accountId, itemEntry);
    }
    else
    {
        trans->Append("UPDATE abyssal_storage SET count = {} WHERE account_id = {} AND item_entry = {}",
            itemIt->second, accountId, itemEntry);
    }
    CommitWithInventory(player, trans);

    return true;
}

uint32 AbyssalStoragePlusMgr::GetItemCount(uint32 accountId, uint32 itemEntry)
{
    auto accIt = _storage.find(accountId);
    if (accIt == _storage.end())
        return 0;

    auto itemIt = accIt->second.find(itemEntry);
    if (itemIt == accIt->second.end())
        return 0;

    return itemIt->second;
}

std::unordered_map<uint32, uint32> const* AbyssalStoragePlusMgr::GetAllItems(uint32 accountId)
{
    auto accIt = _storage.find(accountId);
    if (accIt == _storage.end())
        return nullptr;

    return &accIt->second;
}

uint32 AbyssalStoragePlusMgr::GetQuestReservedCount(Player* player, uint32 itemId)
{
    std::unordered_map<uint32, uint32> reserved;
    GetQuestReservedCounts(player, reserved);
    auto it = reserved.find(itemId);
    return it != reserved.end() ? it->second : 0;
}

void AbyssalStoragePlusMgr::GetQuestReservedCounts(Player* player, std::unordered_map<uint32, uint32>& out)
{
    out.clear();
    if (!player)
        return;

    for (uint8 i = 0; i < MAX_QUEST_LOG_SIZE; ++i)
    {
        uint32 questId = player->GetQuestSlotQuestId(i);
        if (!questId)
            continue;

        QuestStatus status = player->GetQuestStatus(questId);
        if (status != QUEST_STATUS_INCOMPLETE && status != QUEST_STATUS_COMPLETE)
            continue;

        Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
        if (!quest)
            continue;

        for (uint8 j = 0; j < QUEST_ITEM_OBJECTIVES_COUNT; ++j)
        {
            uint32 requiredItem = quest->RequiredItemId[j];
            if (requiredItem)
                out[requiredItem] += quest->RequiredItemCount[j];
        }
    }
}

bool AbyssalStoragePlusMgr::IsItemRequiredByActiveQuest(Player* player, uint32 itemId)
{
    uint32 reserved = GetQuestReservedCount(player, itemId);
    return reserved > 0 && player->GetItemCount(itemId) <= reserved;
}

bool AbyssalStoragePlusMgr::ShouldAutoStore(Player* player, ItemTemplate const* itemTemplate)
{
    return ShouldAutoStore(player, itemTemplate, nullptr);
}

bool AbyssalStoragePlusMgr::ShouldAutoStore(Player* player, ItemTemplate const* itemTemplate,
    std::unordered_map<uint32, uint32> const* reserved)
{
    if (!itemTemplate)
        return false;

    if (itemTemplate->Class != ITEM_CLASS_TRADE_GOODS && itemTemplate->Class != ITEM_CLASS_GEM)
        return false;

    // The vault is account-wide: materials that bind to the character stay in its bags
    bool const accountBound = itemTemplate->HasFlag(ITEM_FLAG_IS_BOUND_TO_ACCOUNT);
    if (!accountBound && (itemTemplate->Bonding == BIND_WHEN_PICKED_UP || itemTemplate->Bonding == BIND_QUEST_ITEM))
        return false;

    // Never auto-store profession tools (runed rods, kits, hammers)
    if (itemTemplate->TotemCategory != 0)
        return false;

    // Non-stackable devices are tools and gadgets, not materials
    if (itemTemplate->Class == ITEM_CLASS_TRADE_GOODS &&
        itemTemplate->SubClass == ITEM_SUBCLASS_DEVICES && itemTemplate->Stackable <= 1)
        return false;

    // Kits, grinders, and lockpicks lack TotemCategory data
    if (IsAbyssalToolItem(itemTemplate->ItemId))
        return false;

    uint32 reservedCount = 0;
    if (reserved)
    {
        auto it = reserved->find(itemTemplate->ItemId);
        if (it != reserved->end())
            reservedCount = it->second;
    }
    else
    {
        reservedCount = GetQuestReservedCount(player, itemTemplate->ItemId);
    }

    if (reservedCount > 0 && player->GetItemCount(itemTemplate->ItemId) <= reservedCount)
        return false;

    return true;
}

AbyssalSettings const& AbyssalStoragePlusMgr::GetSettings(uint32 accountId) const
{
    static AbyssalSettings const defaults;
    auto it = _settings.find(accountId);
    return it != _settings.end() ? it->second : defaults;
}

void AbyssalStoragePlusMgr::SetSettings(uint32 accountId, AbyssalSettings const& settings)
{
    _settings[accountId] = settings;
}

bool AbyssalStoragePlusMgr::IsAutoVaultEnabled(uint32 accountId, AbyssalAcquireSource source) const
{
    AbyssalSettings const& settings = GetSettings(accountId);

    switch (source)
    {
        case AbyssalAcquireSource::Loot:
            return settings.autoVaultLoot;
        case AbyssalAcquireSource::Craft:
            return settings.autoVaultCraft;
        case AbyssalAcquireSource::Mail:
            return settings.autoVaultMail;
        case AbyssalAcquireSource::QuestReward:
            return settings.autoVaultQuestReward;
        case AbyssalAcquireSource::Vendor:
            return settings.autoVaultVendor;
        default:
            return false;
    }
}

bool AbyssalStoragePlusMgr::IsAutoWithdrawQuestAcceptEnabled(uint32 accountId) const
{
    return GetSettings(accountId).autoWithdrawQuestAccept;
}

bool AbyssalStoragePlusMgr::IsBotAccount(Player* player)
{
    if (!player || !player->GetSession())
        return false;

    uint32 accountId = player->GetSession()->GetAccountId();

    auto cached = _botAccountCache.find(accountId);
    if (cached != _botAccountCache.end())
        return cached->second;

    std::string username;
    if (!sAccountMgr->GetName(accountId, username))
        return false; // don't cache failures so the lookup is retried later

    static char const botPrefix[] = "rndbot";
    bool isBot = username.size() >= 6;
    for (size_t i = 0; isBot && i < 6; ++i)
        isBot = std::tolower(static_cast<unsigned char>(username[i])) == botPrefix[i];

    _botAccountCache[accountId] = isBot;
    return isBot;
}

// Send a raw LANG_ADDON packet. The WoW 3.3.5 client splits on '\t':
//   arg1 (prefix) = everything before first \t
//   arg2 (body)   = everything after first \t
// Client fires CHAT_MSG_ADDON event.
static void SendOnePacket(Player* player, std::string const& msg)
{
    WorldPacket data;
    std::size_t len = msg.length();
    data.Initialize(SMSG_MESSAGECHAT, 1 + 4 + 8 + 4 + 8 + 4 + 1 + len + 1);
    data << uint8(CHAT_MSG_WHISPER);
    data << uint32(LANG_ADDON);
    data << uint64(0);
    data << uint32(0);
    data << uint64(0);
    data << uint32(len + 1);
    data << msg;
    data << uint8(0);
    player->GetSession()->SendPacket(&data);
}

void AbyssalStoragePlusMgr::SendAddonMessage(Player* player, std::string const& message)
{
    // Prefix with "ABYS\t" so client receives arg1="ABYS", arg2=message
    size_t const MAX_MSG_LEN = ABYSSAL_MAX_ADDON_MESSAGE_LEN;
    std::string fullMsg = "ABYS\t" + message;

    if (fullMsg.length() <= MAX_MSG_LEN)
    {
        SendOnePacket(player, fullMsg);
        return;
    }

    // For SYNC messages, split by semicolons to create valid chunks
    size_t prefixEnd = message.find(':');
    if (prefixEnd == std::string::npos)
    {
        SendOnePacket(player, ("ABYS\t" + message.substr(0, MAX_MSG_LEN - ABYSSAL_ADDON_PREFIX_LEN)));
        return;
    }
    prefixEnd++;

    std::string prefix = message.substr(0, prefixEnd);
    std::string payload = message.substr(prefixEnd);

    std::string chunk = prefix;
    size_t pos = 0;
    while (pos < payload.length())
    {
        size_t nextSep = payload.find(';', pos);
        std::string entry;
        if (nextSep == std::string::npos)
        {
            entry = payload.substr(pos);
            pos = payload.length();
        }
        else
        {
            entry = payload.substr(pos, nextSep - pos);
            pos = nextSep + 1;
        }

        if (chunk.length() > prefix.length() && chunk.length() + 1 + entry.length() + ABYSSAL_ADDON_PREFIX_LEN > MAX_MSG_LEN)
        {
            SendOnePacket(player, "ABYS\t" + chunk);
            chunk = prefix;
        }

        if (chunk.length() > prefix.length())
            chunk += ";";
        chunk += entry;
    }

    if (chunk.length() > prefix.length())
        SendOnePacket(player, "ABYS\t" + chunk);
}

void AbyssalStoragePlusMgr::SendFullSync(Player* player)
{
    uint32 accountId = player->GetSession()->GetAccountId();
    std::unordered_map<uint32, uint32> const* items = GetAllItems(accountId);

    // Explicit begin/end markers let the client replace, not merge, stale data
    SendAddonMessage(player, "SYNCB");

    if (items && !items->empty())
    {
        std::string msg = "SYNC:";
        msg.reserve(items->size() * 16);
        bool first = true;
        for (auto const& pair : *items)
        {
            if (!first)
                msg += ';';
            msg += std::to_string(pair.first);
            msg += ',';
            msg += std::to_string(pair.second);
            first = false;
        }

        SendAddonMessage(player, msg);
    }

    SendAddonMessage(player, "SYNCE");
}

void AbyssalStoragePlusMgr::SendItemUpdate(Player* player, uint32 itemEntry, uint32 count)
{
    // Include bag+vault total so the client can render without a mid-move race
    uint32 bagCount = player ? player->GetItemCount(itemEntry) : 0;
    std::string msg = "UPD:" + std::to_string(itemEntry) + "," + std::to_string(count)
        + "," + std::to_string(bagCount + count);
    SendAddonMessage(player, msg);
}

void AbyssalStoragePlusMgr::SendItemDelete(Player* player, uint32 itemEntry)
{
    SendAddonMessage(player, "DEL:" + std::to_string(itemEntry));
}

void AbyssalStoragePlusMgr::SendVaultNotice(Player* player, uint32 itemEntry, uint32 count)
{
    SendAddonMessage(player, "VAULT:" + std::to_string(itemEntry) + "," + std::to_string(count));
}

void AbyssalStoragePlusMgr::SendVaultNotification(Player* player, uint32 itemEntry, uint32 count)
{
    SendAddonMessage(player, "VAULTN:" + std::to_string(itemEntry) + "," + std::to_string(count));
}

// Collect every online character on the account
static std::vector<Player*> GetAccountPlayers(uint32 accountId)
{
    std::vector<Player*> players;
    sWorldSessionMgr->DoForAllOnlinePlayers([accountId, &players](Player* player)
    {
        if (player->GetSession() && player->GetSession()->GetAccountId() == accountId)
            players.push_back(player);
    });
    return players;
}

void AbyssalStoragePlusMgr::BroadcastItemUpdate(uint32 accountId, uint32 itemEntry)
{
    uint32 count = GetItemCount(accountId, itemEntry);

    for (Player* player : GetAccountPlayers(accountId))
    {
        if (count > 0)
            SendItemUpdate(player, itemEntry, count);
        else
            SendItemDelete(player, itemEntry);
    }
}

void AbyssalStoragePlusMgr::BroadcastFullSync(uint32 accountId)
{
    for (Player* player : GetAccountPlayers(accountId))
        SendFullSync(player);
}
