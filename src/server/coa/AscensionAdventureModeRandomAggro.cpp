/* Copyright (C) 2016+ AzerothCore, GNU AGPL v3. */

#include "ScriptMgr.h"
#include "SpellAuraDefines.h"
#include "SpellInfo.h"
#include "SpellScript.h"
#include "Unit.h"

#include <list>

namespace
{
bool IsUndetectedInvisibleUnit(Unit const* caster, WorldObject* target)
{
    Unit const* unit = target->ToUnit();
    return unit && unit->HasAuraType(SPELL_AURA_MOD_INVISIBILITY) && !caster->CanSeeOrDetect(target);
}

class spell_ascension_adventure_mode_random_aggro : public SpellScript
{
    PrepareSpellScript(spell_ascension_adventure_mode_random_aggro);

    SpellCastResult CheckCast()
    {
        return GetCaster()->HasSpellCooldown(GetSpellInfo()->Id) ? SPELL_FAILED_DONT_REPORT : SPELL_CAST_OK;
    }

    void KeepOnlyPerceivedTargets(std::list<WorldObject*>& targets)
    {
        Unit const* caster = GetCaster();
        if (!caster)
        {
            targets.clear();
            return;
        }
        targets.remove_if([caster](WorldObject* target)
        {
            return IsUndetectedInvisibleUnit(caster, target);
        });
    }

    void StartRecovery()
    {
        GetCaster()->AddSpellCooldown(GetSpellInfo()->Id, 0, GetSpellInfo()->RecoveryTime);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_ascension_adventure_mode_random_aggro::CheckCast);
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(
            spell_ascension_adventure_mode_random_aggro::KeepOnlyPerceivedTargets,
            EFFECT_0, TARGET_UNIT_SRC_AREA_ENEMY);
        AfterCast += SpellCastFn(spell_ascension_adventure_mode_random_aggro::StartRecovery);
    }
};
}

void AddSC_AscensionAdventureModeRandomAggro()
{
    RegisterSpellScript(spell_ascension_adventure_mode_random_aggro);
}
