/* Copyright (C) 2016+ AzerothCore, GNU AGPL v3. */
#include "CoaCustomRaces.h"
#include "Player.h"
#include <iterator>

namespace CoaCustomRaces
{
DisplayStore& DisplayStore::Instance()
{
    static DisplayStore instance;
    return instance;
}

bool DisplayStore::Has(uint8 race, uint8 gender) const
{
    auto const itr = _looks.find(Key(race, gender));
    return itr != _looks.end() && !itr->second.empty();
}

uint32 DisplayStore::Select(uint8 race, uint8 gender, uint8 skin, uint8 hairStyle) const
{
    auto const itr = _looks.find(Key(race, gender));
    if (itr == _looks.end() || itr->second.empty())
        return 0;

    std::map<uint32, uint32> const& looks = itr->second;
    if (auto const look = looks.find(uint32(hairStyle) * 32 + skin); look != looks.end())
        return look->second;
    if (auto const bySkin = looks.find(skin); bySkin != looks.end())
        return bySkin->second;
    return std::next(looks.begin(), skin % looks.size())->second;
}

std::size_t DisplayStore::Size() const
{
    std::size_t size = 0;
    for (auto const& [key, looks] : _looks)
        size += looks.size();
    return size;
}

uint32 GetCustomDisplay(Player const* player)
{
    if (!player)
        return 0;
    return DisplayStore::Instance().Select(player->getRace(true), player->getGender(),
        player->GetByteValue(PLAYER_BYTES, 0), player->GetByteValue(PLAYER_BYTES, 2));
}
}
