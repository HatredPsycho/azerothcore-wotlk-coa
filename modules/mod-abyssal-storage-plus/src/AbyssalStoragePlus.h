#ifndef ABYSSAL_STORAGE_PLUS_H
#define ABYSSAL_STORAGE_PLUS_H

#include "DataMap.h"
#include "Define.h"
#include <cstddef>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>

class Player;
struct ItemTemplate;

constexpr char const* ABYSSAL_DATA_KEY = "AbyssalData";

// Addon message budget in bytes, prefix included
constexpr size_t ABYSSAL_MAX_ADDON_MESSAGE_LEN = 240;
constexpr size_t ABYSSAL_ADDON_PREFIX_LEN = 5; // "ABYS\t"

// Trade goods kits that lack TotemCategory data
constexpr uint32 ABYSSAL_ITEM_RUSTY_THIEVES_TOOLS = 7872;
constexpr uint32 ABYSSAL_ITEM_JEWELERS_KIT = 20815;
constexpr uint32 ABYSSAL_ITEM_SIMPLE_GRINDER = 20824;

inline bool IsAbyssalToolItem(uint32 itemId)
{
    return itemId == ABYSSAL_ITEM_RUSTY_THIEVES_TOOLS
        || itemId == ABYSSAL_ITEM_JEWELERS_KIT
        || itemId == ABYSSAL_ITEM_SIMPLE_GRINDER;
}

// Where a newly stored item came from
enum class AbyssalAcquireSource : uint8
{
    Unknown = 0,
    Loot = 1,
    Craft = 2,
    Mail = 3,
    QuestReward = 4,
    Vendor = 5,
};

// Player-facing vault behaviour, synced from the addon
struct AbyssalSettings
{
    bool autoVaultLoot = true;
    bool autoVaultCraft = false;
    bool autoVaultMail = false;
    bool autoVaultQuestReward = false;
    bool autoVaultVendor = false;
    bool autoWithdrawQuestAccept = true;
};

struct PendingDeposit
{
    uint32 itemEntry;
    uint32 count;
    AbyssalAcquireSource source = AbyssalAcquireSource::Unknown;
};

// Tracks one vault-pulled stack; baseline is bag count before the pull
struct MaterializedItem
{
    uint32 baseline = 0;
    uint32 amount = 0;
};

// Per-player transient state stored via DataMap
struct AbyssalPlayerData : public DataMap::Base
{
    bool autoStoreEnabled = true;
    bool isMaterializing = false; // true while materializing items (suppress auto-deposit)
    std::unordered_map<uint32, MaterializedItem> materializedItems; // entry -> tracking
    std::vector<PendingDeposit> pendingDeposits; // deferred auto-deposits
    uint32 pendingCrafts = 0;    // remaining crafts in a multi-craft batch
    uint32 pendingSpellId = 0;   // spell ID for multi-craft batch
    AbyssalAcquireSource pendingSource = AbyssalAcquireSource::Unknown; // next store's source
    uint32 pendingSourceTime = 0; // unix time the source tag was set
};

AbyssalPlayerData* GetAbyssalData(Player* player);

class AbyssalStoragePlusMgr
{
public:
    static AbyssalStoragePlusMgr* instance();

    void LoadAccountData(uint32 accountId);
    void UnloadAccountData(uint32 accountId);

    void DepositItem(Player* player, uint32 accountId, uint32 itemEntry, uint32 count);
    bool WithdrawItem(Player* player, uint32 accountId, uint32 itemEntry, uint32 count);
    uint32 GetItemCount(uint32 accountId, uint32 itemEntry);
    std::unordered_map<uint32, uint32> const* GetAllItems(uint32 accountId);

    bool ShouldAutoStore(Player* player, ItemTemplate const* itemTemplate);
    bool ShouldAutoStore(Player* player, ItemTemplate const* itemTemplate,
                         std::unordered_map<uint32, uint32> const* reserved);
    bool IsItemRequiredByActiveQuest(Player* player, uint32 itemId);
    uint32 GetQuestReservedCount(Player* player, uint32 itemId);
    void GetQuestReservedCounts(Player* player, std::unordered_map<uint32, uint32>& out);

    bool IsBotAccount(Player* player);

    // Per-account settings, pushed by the addon
    AbyssalSettings const& GetSettings(uint32 accountId) const;
    void SetSettings(uint32 accountId, AbyssalSettings const& settings);
    bool IsAutoVaultEnabled(uint32 accountId, AbyssalAcquireSource source) const;
    bool IsAutoWithdrawQuestAcceptEnabled(uint32 accountId) const;

    // Messaging helpers
    void SendAddonMessage(Player* player, std::string const& message);
    void SendFullSync(Player* player);
    void SendItemUpdate(Player* player, uint32 itemEntry, uint32 count);
    void SendItemDelete(Player* player, uint32 itemEntry);
    void SendVaultNotice(Player* player, uint32 itemEntry, uint32 count);
    void SendVaultNotification(Player* player, uint32 itemEntry, uint32 count);
    void BroadcastItemUpdate(uint32 accountId, uint32 itemEntry);
    void BroadcastFullSync(uint32 accountId);

    bool IsEnabled() const { return _enabled; }
    void SetEnabled(bool enabled) { _enabled = enabled; }

private:
    AbyssalStoragePlusMgr() = default;

    // accountId -> (itemEntry -> count)
    std::unordered_map<uint32, std::unordered_map<uint32, uint32>> _storage;
    std::unordered_set<uint32> _loadedAccounts; // accounts fully loaded from DB
    std::unordered_map<uint32, uint32> _accountRefs; // accountId -> online characters

    // accountId -> is random bot account (cached, avoids per-call DB lookups)
    std::unordered_map<uint32, bool> _botAccountCache;

    bool _enabled = true;
    std::unordered_map<uint32, AbyssalSettings> _settings; // accountId -> settings
};

#define sAbyssalStoragePlusMgr AbyssalStoragePlusMgr::instance()

#endif // ABYSSAL_STORAGE_PLUS_H
