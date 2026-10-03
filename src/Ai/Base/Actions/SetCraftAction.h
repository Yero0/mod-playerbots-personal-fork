/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_SETCRAFTACTION_H
#define PLAYERBOTS_SETCRAFTACTION_H

#include "Action.h"
#include "CraftValue.h"

#include <vector>

class PlayerbotAI;
class Player;
class SpellInfo;

struct ItemTemplate;
struct SkillLineAbilityEntry;

// Local change: a tradeable item a random bot can craft on order
struct CraftableItem
{
    SpellInfo const* spell;
    ItemTemplate const* item;
    uint32 skill;
    uint32 count;  // items per craft
    uint32 price;  // per craft: craft fee plus the reagents the bot supplies
    bool scroll;   // an enchant made as its vellum scroll
};

class SetCraftAction : public Action
{
public:
    SetCraftAction(PlayerbotAI* botAI) : Action(botAI, "craft") {}

    bool Execute(Event event) override;

    static uint32 GetCraftFee(CraftData& craftData);
    static uint32 GetCraftFee(ItemTemplate const* proto);  // Local change
    // Local change: tradeable items from the bot's manufacturing profession recipes, best first per profession
    static std::vector<CraftableItem> GetCraftableItems(Player* bot);

private:
    void TellCraft();
    bool CraftForTrader(std::string const& link);  // Local change

    static SkillLineAbilityEntry const* GetSkillLine(uint32 spellId);  // Local change: replaces the racy map
};

#endif
