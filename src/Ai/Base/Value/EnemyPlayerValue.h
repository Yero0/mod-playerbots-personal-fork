/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ENEMYPLAYERVALUE_H
#define PLAYERBOTS_ENEMYPLAYERVALUE_H

#include "PlayerbotAIConfig.h"
#include "PossibleTargetsValue.h"
#include "TargetValue.h"

class PlayerbotAI;
class Unit;

class NearestEnemyPlayersValue : public PossibleTargetsValue
{
public:
    NearestEnemyPlayersValue(PlayerbotAI* botAI, float range = sPlayerbotAIConfig.grindDistance)
        : PossibleTargetsValue(botAI, "nearest enemy players", range)
    {
    }

public:
    bool AcceptUnit(Unit* unit) override;
};

class EnemyPlayerValue : public UnitCalculatedValue
{
public:
    EnemyPlayerValue(PlayerbotAI* botAI, std::string const name = "enemy player")
        : UnitCalculatedValue(botAI, name, 1 * IN_MILLISECONDS)
    {
    }

    Unit* Calculate() override;

private:
    float GetMaxAttackDistance();
};

// Local change: battleground (not arena) bot facing at least BG_OUTNUMBERED_MARGIN more enemy players
// than allies within TargetValue::BG_ROLE_RANGE (AiPlayerbot.BattlegroundCombatTactics). Once outnumbered,
// enemies count up to BG_OUTNUMBERED_EXIT_RANGE, so chasers falling a few yards behind don't end it.
class BgOutnumberedValue : public BoolCalculatedValue
{
public:
    BgOutnumberedValue(PlayerbotAI* botAI) : BoolCalculatedValue(botAI, "bg outnumbered", 2) {}

    bool Calculate() override;

    static constexpr uint32 BG_OUTNUMBERED_MARGIN = 2;
    static constexpr float BG_OUTNUMBERED_EXIT_RANGE = 45.0f;

private:
    bool _outnumbered = false;
};

#endif
