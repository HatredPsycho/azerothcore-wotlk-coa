/* Copyright (C) 2016+ AzerothCore, GNU AGPL v3. */
/*
 * mod-coa-companions - pushes the health of a player's summoned companions to their client.
 *
 * The 3.3.5a client can only read a unit it has a token for: pet, target, focus, mouseover,
 * targettarget and pettarget. A Necromancer's minions are guardians rather than the one pet, so
 * the client has no token for them and no way to draw a health bar. The CoACompanions addon
 * therefore only ever saw a companion while it happened to occupy one of those tokens - in
 * practice while it was the target's target during a fight, and never once the fight ended.
 *
 * This module supplies the missing half. Once per interval it walks the player's controlled
 * units and sends one addon message holding an entry, a current health and a maximum health per
 * companion. The addon draws its bars from that and falls back to the tokens when no message has
 * arrived, so it keeps working with the module disabled.
 *
 * The message is a whisper to the player themselves in LANG_ADDON, the shape the core's own
 * AddonChannelCommandHandler uses ("prefix\tbody"), which the client raises as CHAT_MSG_ADDON.
 * Nothing is registered client side in 3.3.5a: prefix registration only arrived in 4.x.
 *
 * Traffic is kept to what the display needs. A message goes out when the payload changed, or
 * every KeepAliveMs so the addon can tell a live reading from a stale one, and one final empty
 * message when the last companion is gone. A player with no companions costs one integer
 * comparison per update.
 */

#include "Chat.h"
#include "Config.h"
#include "Creature.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "Unit.h"
#include "WorldPacket.h"
#include "WorldSession.h"

#include <algorithm>
#include <string>
#include <unordered_map>

namespace
{
constexpr char const* AddonPrefix = "CoACompanions";
constexpr uint32 DefaultIntervalMs = 1000;
constexpr uint32 MinimumIntervalMs = 250;
constexpr uint32 KeepAliveMs = 3000;

// SMSG_MESSAGECHAT carries the body as a null terminated string; the client's own addon channel
// tolerates far more, but a compact budget keeps a full army inside a single message.
constexpr std::size_t MaximumBody = 200;

struct CompanionFeed
{
    uint32 sinceUpdate = 0;
    uint32 sinceSend = 0;
    std::string lastBody;
    bool primed = false;
};

std::unordered_map<ObjectGuid, CompanionFeed> feeds;

bool Enabled()
{
    return sConfigMgr->GetOption<bool>("CoACompanions.Enable", true);
}

uint32 Interval()
{
    return std::max(MinimumIntervalMs, sConfigMgr->GetOption<uint32>("CoACompanions.Interval", DefaultIntervalMs));
}

std::string BuildBody(Player* player)
{
    std::string body;

    for (Unit* controlled : player->m_Controlled)
    {
        Creature* companion = controlled ? controlled->ToCreature() : nullptr;
        if (!companion || !companion->IsAlive() || !companion->IsInWorld())
            continue;

        uint32 const maximum = companion->GetMaxHealth();
        if (!maximum)
            continue;

        std::string const row = std::to_string(companion->GetEntry()) + ':' +
            std::to_string(companion->GetHealth()) + ':' + std::to_string(maximum);

        if (body.size() + row.size() + 1 > MaximumBody)
            break;

        if (!body.empty())
            body += ';';
        body += row;
    }

    return body;
}

void Send(Player* player, std::string const& body)
{
    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player,
        std::string(AddonPrefix) + '\t' + body);
    player->GetSession()->SendPacket(&data);
}

class coa_companion_status : public PlayerScript
{
public:
    coa_companion_status() : PlayerScript("coa_companion_status",
        {PLAYERHOOK_ON_UPDATE, PLAYERHOOK_ON_LOGOUT}) { }

    void OnPlayerLogout(Player* player) override
    {
        feeds.erase(player->GetGUID());
    }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        if (!player || !player->GetSession() || !Enabled())
            return;

        CompanionFeed& feed = feeds[player->GetGUID()];
        feed.sinceUpdate += diff;
        feed.sinceSend += diff;
        if (feed.sinceUpdate < Interval())
            return;
        feed.sinceUpdate = 0;

        std::string const body = BuildBody(player);

        // Nothing to say, and nothing was said before: the common case for most players.
        if (body.empty() && !feed.primed)
            return;

        if (body == feed.lastBody && feed.sinceSend < KeepAliveMs)
            return;

        Send(player, body);
        feed.lastBody = body;
        feed.sinceSend = 0;
        feed.primed = !body.empty();
    }
};
}

void AddCoACompanionStatusScripts()
{
    new coa_companion_status();
}
