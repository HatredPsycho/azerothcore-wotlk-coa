/* Copyright (C) 2016+ AzerothCore, GNU AGPL v3. */

#include "AscensionNecromancer.h"
#include "Creature.h"
#include "DBCStores.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "TemporarySummon.h"

#include <algorithm>
#include <unordered_map>
#include <vector>

namespace
{
struct StoredCompanion
{
    uint32 Entry;
    uint32 SummonSpell;
    uint32 Properties;
    uint32 Health;
};

struct FlightState
{
    bool aboard = false;
    std::vector<StoredCompanion> stored;
};

std::unordered_map<ObjectGuid, FlightState> flights;

bool LastsUntilDismissed(TempSummon* summon)
{
    switch (summon->GetSummonType())
    {
        case TEMPSUMMON_MANUAL_DESPAWN:
        case TEMPSUMMON_DEAD_DESPAWN:
        case TEMPSUMMON_CORPSE_DESPAWN:
            return true;
        default:
            return false;
    }
}

void ReconcileLifeForce(Player* player)
{
    if (player->getClass() == CLASS_NECROMANCER)
        AscensionNecromancer::Sync(player);
}

void BoardWithoutCompanions(Player* player, FlightState& flight)
{
    flight.stored.clear();

    std::vector<Unit*> controlled(player->m_Controlled.begin(), player->m_Controlled.end());
    for (Unit* unit : controlled)
    {
        Creature* companion = unit ? unit->ToCreature() : nullptr;
        if (!companion || companion->IsPet())
            continue;

        TempSummon* summon = companion->ToTempSummon();
        if (!summon)
            continue;

        if (LastsUntilDismissed(summon))
            flight.stored.push_back({companion->GetEntry(),
                companion->GetUInt32Value(UNIT_CREATED_BY_SPELL),
                summon->m_Properties ? summon->m_Properties->Id : 0u,
                uint32(companion->GetHealth())});

        summon->DespawnOrUnsummon();
    }

    ReconcileLifeForce(player);
}

void LandWithCompanions(Player* player, FlightState& flight)
{
    std::vector<StoredCompanion> const stored = std::move(flight.stored);
    flight.stored.clear();

    if (!player->IsInWorld() || !player->IsAlive())
        return;

    float index = 0.0f;
    for (StoredCompanion const& entry : stored)
    {
        SummonPropertiesEntry const* properties =
            entry.Properties ? sSummonPropertiesStore.LookupEntry(entry.Properties) : nullptr;

        Position const point = player->GetNearPosition(2.0f + index * 0.5f, index * 2.4f);
        index += 1.0f;

        TempSummon* summon = player->GetMap()->SummonCreature(entry.Entry, point, properties, 0,
            player, entry.SummonSpell);
        if (!summon)
            continue;

        summon->SetTempSummonType(TEMPSUMMON_DEAD_DESPAWN);
        if (entry.Health && summon->IsAlive())
            summon->SetHealth(std::min<uint32>(entry.Health, uint32(summon->GetMaxHealth())));
    }

    ReconcileLifeForce(player);
}

class ascension_companion_flight : public PlayerScript
{
public:
    ascension_companion_flight() : PlayerScript("ascension_companion_flight",
        {PLAYERHOOK_ON_UPDATE, PLAYERHOOK_ON_LOGOUT}) { }

    void OnPlayerLogout(Player* player) override
    {
        flights.erase(player->GetGUID());
    }

    void OnPlayerUpdate(Player* player, uint32) override
    {
        if (!player)
            return;

        bool const aboard = player->IsInFlight();
        auto itr = flights.find(player->GetGUID());

        if (itr == flights.end())
        {
            if (!aboard)
                return;
            itr = flights.emplace(player->GetGUID(), FlightState()).first;
        }

        FlightState& flight = itr->second;
        if (flight.aboard == aboard)
            return;

        flight.aboard = aboard;
        if (aboard)
            BoardWithoutCompanions(player, flight);
        else
            LandWithCompanions(player, flight);
    }
};
}

void AddSC_AscensionCompanionFlight()
{
    new ascension_companion_flight();
}
