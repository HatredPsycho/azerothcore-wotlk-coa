/* Copyright (C) 2016+ AzerothCore, GNU AGPL v3. */

#include "ScriptMgr.h"
#include "SpellAuraDefines.h"
#include "SpellScript.h"
#include "Unit.h"

#include <algorithm>
#include <list>

namespace
{
enum AdventureModeAggroSpells : uint32
{
    SPELL_ADVENTURE_MODE_RANDOM_AGGRO = 888071
};

bool IsUndetectedInvisibleUnit(Unit const* caster, WorldObject* target)
{
    Unit const* unit = target->ToUnit();
    return unit && unit->HasAuraType(SPELL_AURA_MOD_INVISIBILITY) && !caster->CanSeeOrDetect(target);
}

class spell_ascension_adventure_mode_random_aggro : public SpellScript
{
    PrepareSpellScript(spell_ascension_adventure_mode_random_aggro);

    bool Validate(SpellInfo const*) override
    {
        return ValidateSpellInfo({SPELL_ADVENTURE_MODE_RANDOM_AGGRO});
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

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(
            spell_ascension_adventure_mode_random_aggro::KeepOnlyPerceivedTargets,
            EFFECT_0, TARGET_UNIT_SRC_AREA_ENEMY);
    }
};
}

void AddSC_AscensionAdventureModeAggro()
{
    RegisterSpellScript(spell_ascension_adventure_mode_random_aggro);
}
