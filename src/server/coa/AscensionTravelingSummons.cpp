#include "Map.h"
#include "MotionMaster.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "TemporarySummon.h"
#include <cmath>
#include <vector>

namespace
{
constexpr char const* TRAVELING_SUMMONS = "coa_traveling_summons";
constexpr float FOLLOWER_SPACING = 1.5f;

struct TravelingSummon
{
    ObjectGuid guid;
    uint32 entry = 0;
    uint32 spell = 0;
    SummonPropertiesEntry const* properties = nullptr;
    uint32 remaining = 0;
    float health = 1.0f;
    float power = 1.0f;
    Powers powerType = POWER_MANA;
};

struct TravelingSummons : DataMap::Base
{
    uint32 mapId = 0;
    std::vector<TravelingSummon> summons;
};

bool FollowsOwner(TempSummon* summon)
{
    if (summon->GetMotionMaster()->GetCurrentMovementGeneratorType() == FOLLOW_MOTION_TYPE)
        return true;
    return summon->GetVictim() && summon->CanFreeMove();
}

std::vector<TempSummon*> Followers(Player* player)
{
    std::vector<TempSummon*> followers;
    for (Unit* unit : player->m_Controlled)
    {
        TempSummon* summon = unit ? unit->ToTempSummon() : nullptr;
        if (!summon || !summon->IsInWorld() || !summon->IsAlive() || summon->IsPet() || summon->IsTotem() ||
            summon->IsVehicle() || summon->GetOwnerGUID() != player->GetGUID())
            continue;
        if (FollowsOwner(summon))
            followers.push_back(summon);
    }
    return followers;
}

Position BesideOwner(float x, float y, float z, float orientation, std::size_t index)
{
    float const angle = orientation + float(M_PI) + (float(index) - 1.0f) * 0.6f;
    float const distance = FOLLOWER_SPACING + float(index / 4) * FOLLOWER_SPACING;
    return Position(x + distance * std::cos(angle), y + distance * std::sin(angle), z, orientation);
}

TravelingSummon Remember(TempSummon* summon)
{
    TravelingSummon kept;
    kept.guid = summon->GetGUID();
    kept.entry = summon->GetEntry();
    kept.spell = summon->GetUInt32Value(UNIT_CREATED_BY_SPELL);
    kept.properties = summon->m_Properties;
    if (summon->GetSummonType() == TEMPSUMMON_TIMED_DESPAWN ||
        summon->GetSummonType() == TEMPSUMMON_TIMED_OR_DEAD_DESPAWN ||
        summon->GetSummonType() == TEMPSUMMON_TIMED_OR_CORPSE_DESPAWN)
        kept.remaining = std::max<uint32>(1, summon->GetTimer());
    kept.health = summon->GetHealthPct() / 100.0f;
    kept.powerType = summon->getPowerType();
    if (uint32 const maximum = summon->GetMaxPower(kept.powerType))
        kept.power = float(summon->GetPower(kept.powerType)) / float(maximum);
    return kept;
}

void Restore(TempSummon* summon, TravelingSummon const& kept)
{
    summon->SetHealth(std::max(1u, uint32(std::round(float(summon->GetMaxHealth()) * kept.health))));
    if (uint32 const maximum = summon->GetMaxPower(kept.powerType))
        summon->SetPower(kept.powerType, int32(std::round(float(maximum) * kept.power)));
}
}

class coa_traveling_summons : public PlayerScript
{
public:
    coa_traveling_summons()
        : PlayerScript("coa_traveling_summons", {PLAYERHOOK_ON_BEFORE_TELEPORT, PLAYERHOOK_ON_MAP_CHANGED}) { }

    bool OnPlayerBeforeTeleport(Player* player, uint32 mapId, float x, float y, float z, float orientation,
        uint32, Unit*) override
    {
        if (!player->IsInWorld() || !player->IsAlive())
            return true;

        std::vector<TempSummon*> const followers = Followers(player);
        if (followers.empty())
            return true;

        if (mapId == player->GetMapId())
        {
            for (std::size_t index = 0; index < followers.size(); ++index)
            {
                Position const spot = BesideOwner(x, y, z, orientation, index);
                followers[index]->NearTeleportTo(spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(),
                    spot.GetOrientation());
            }
            return true;
        }

        TravelingSummons* travel = player->CustomData.GetDefault<TravelingSummons>(TRAVELING_SUMMONS);
        travel->mapId = mapId;
        travel->summons.clear();
        for (TempSummon* follower : followers)
            travel->summons.push_back(Remember(follower));
        return true;
    }

    void OnPlayerMapChanged(Player* player) override
    {
        TravelingSummons const* travel = player->CustomData.Get<TravelingSummons>(TRAVELING_SUMMONS);
        if (!travel)
            return;

        uint32 const mapId = travel->mapId;
        std::vector<TravelingSummon> const summons = travel->summons;
        player->CustomData.Erase(TRAVELING_SUMMONS);
        if (mapId != player->GetMapId() || !player->IsAlive())
            return;

        Map* map = player->GetMap();
        for (std::size_t index = 0; index < summons.size(); ++index)
        {
            TravelingSummon const& kept = summons[index];
            if (ObjectAccessor::GetCreature(*player, kept.guid))
                continue;

            Position const spot = BesideOwner(player->GetPositionX(), player->GetPositionY(),
                player->GetPositionZ(), player->GetOrientation(), index);
            if (TempSummon* summon = map->SummonCreature(kept.entry, spot, kept.properties, kept.remaining, player,
                    kept.spell))
                Restore(summon, kept);
        }
    }
};

void AddSC_AscensionTravelingSummons()
{
    new coa_traveling_summons();
}
