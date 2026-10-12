/* Copyright (C) 2016+ AzerothCore, GNU AGPL v3. */
#ifndef COA_CUSTOM_RACES_H
#define COA_CUSTOM_RACES_H

#include "Define.h"
#include <array>
#include <map>
#include <unordered_map>

class Player;

namespace CoaCustomRaces
{
constexpr uint8 MaxStageOneRace = 63;

struct Racial
{
    uint8 RaceId;
    std::array<uint32, 4> Spells;
};

inline constexpr std::array<Racial, 48> Racials =
{{
    {12, {14000121, 14000122, 14000123, 14000124}},
    {13, {14000131, 14000132, 14000133, 14000134}},
    {14, {14000141, 14000142, 14000143, 14000144}},
    {15, {14000151, 14000152, 14000153, 14000154}},
    {16, {14000161, 14000162, 14000163, 14000164}},
    {17, {14000171, 14000172, 14000173, 14000174}},
    {18, {14000181, 14000182, 14000183, 14000184}},
    {20, {14000201, 14000202, 14000203, 14000204}},
    {21, {14000211, 14000212, 14000213, 14000214}},
    {22, {14000221, 14000222, 14000223, 14000224}},
    {23, {14000231, 14000232, 14000233, 14000234}},
    {24, {14000241, 14000242, 14000243, 14000244}},
    {25, {14000251, 14000252, 14000253, 14000254}},
    {26, {14000261, 14000262, 14000263, 14000264}},
    {28, {14000281, 14000282, 14000283, 14000284}},
    {29, {14000291, 14000292, 14000293, 14000294}},
    {30, {14000301, 14000302, 14000303, 14000304}},
    {31, {14000311, 14000312, 14000313, 14000314}},
    {32, {14000321, 14000322, 14000323, 14000324}},
    {47, {14000471, 14000472, 14000473, 14000474}},
    {48, {14000481, 14000482, 14000483, 14000484}},
    {49, {14000491, 14000492, 14000493, 14000494}},
    {50, {14000501, 14000502, 14000503, 14000504}},
    {51, {14000511, 14000512, 14000513, 14000514}},
    {53, {14000531, 14000532, 14000533, 14000534}},
    {54, {14000541, 14000542, 14000543, 14000544}},
    {55, {14000551, 14000552, 14000553, 14000554}},
    {56, {14000561, 14000562, 14000563, 14000564}},
    {57, {14000571, 14000572, 14000573, 14000574}},
    {58, {14000581, 14000582, 14000583, 14000584}},
    {59, {14000591, 14000592, 14000593, 14000594}},
    {60, {14000601, 14000602, 14000603, 14000604}},
    {61, {14000611, 14000612, 14000613, 14000614}},
    {62, {14000621, 14000622, 14000623, 14000624}},
    {63, {14000631, 14000632, 14000633, 14000634}},
    {66, {14000661, 14000662, 14000663, 14000664}},
    {67, {14000671, 14000672, 14000673, 14000674}},
    {68, {14000681, 14000682, 14000683, 14000684}},
    {69, {14000691, 14000692, 14000693, 14000694}},
    {70, {14000701, 14000702, 14000703, 14000704}},
    {71, {14000711, 14000712, 14000713, 14000714}},
    {74, {14000741, 14000742, 14000743, 14000744}},
    {78, {14000781, 14000782, 14000783, 14000784}},
    {80, {14000801, 14000802, 14000803, 14000804}},
    {81, {14000811, 14000812, 14000813, 14000814}},
    {82, {14000821, 14000822, 14000823, 14000824}},
    {83, {14000831, 14000832, 14000833, 14000834}},
    {97, {14000971, 14000972, 14000973, 14000974}},
}};

inline Racial const* GetRacial(uint8 race)
{
    for (Racial const& racial : Racials)
        if (racial.RaceId == race)
            return &racial;
    return nullptr;
}

inline constexpr std::array<uint32, 12> VanillaRacialSkills =
{
    101, 124, 125, 126, 220, 733, 753, 754, 756, 760, 11125, 11760
};

inline constexpr bool IsVanillaRacialSkill(uint32 skill)
{
    for (uint32 vanilla : VanillaRacialSkills)
        if (vanilla == skill)
            return true;
    return false;
}

inline constexpr bool IsMaleOnly(uint8 race)
{
    switch (race)
    {
        case 15: case 17: case 18: case 22: case 23: case 24: case 25: case 26: case 32: case 36: case 83: case 97:
            return true;
        default:
            return false;
    }
}

class DisplayStore
{
public:
    static DisplayStore& Instance();

    void Clear() { _looks.clear(); }
    void Add(uint8 race, uint8 gender, uint32 index, uint32 displayId) { _looks[Key(race, gender)][index] = displayId; }
    bool Has(uint8 race, uint8 gender) const;
    uint32 Select(uint8 race, uint8 gender, uint8 skin, uint8 hairStyle) const;
    std::size_t Size() const;

private:
    static uint32 Key(uint8 race, uint8 gender) { return (uint32(race) << 8) | gender; }

    std::unordered_map<uint32, std::map<uint32, uint32>> _looks;
};

uint32 GetCustomDisplay(Player const* player);
}

#endif
