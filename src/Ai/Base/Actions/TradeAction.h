/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_TRADEACTION_H
#define PLAYERBOTS_TRADEACTION_H

#include "InventoryAction.h"

class Item;
class PlayerbotAI;

class TradeAction : public InventoryAction
{
public:
    TradeAction(PlayerbotAI* botAI) : InventoryAction(botAI, "trade") {}

    bool Execute(Event event) override;

    bool TradeItem(Item const* item, int8 slot);  // Local change: public, used by SetCraftAction
    // Local change: an empty bag slot that can hold `count` of the item (no merging into other stacks)
    static bool FindFreeSlot(Player* bot, uint32 itemId, uint32 count, uint8& bag, uint8& slot);

private:
    bool TradeAmount(std::vector<Item*> const& items, uint32 amount);  // Local change

    static std::map<std::string, uint32> slots;
};

#endif
