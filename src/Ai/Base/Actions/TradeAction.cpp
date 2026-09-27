/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "TradeAction.h"
#include "ChatHelper.h"
#include "Event.h"
#include "ItemCountValue.h"
#include "ItemVisitors.h"
#include "Bag.h"  // Local change
#include "CraftValue.h"  // Local change
#include "PlayerbotAI.h"
#include "Playerbots.h"  // Local change: AI_VALUE
#include "RandomPlayerbotMgr.h"  // Local change

bool TradeAction::Execute(Event event)
{
    std::string const text = event.getParam();

    // If text starts with any excluded prefix, don't process it further.
    for (auto const& prefix : sPlayerbotAIConfig.tradeActionExcludedPrefixes)
    {
        if (text.find(prefix) == 0)
            return false;
    }

    if (!bot->GetTrader())
    {
        GuidVector guids = chat->parseGameobjects(text);
        Player* player = nullptr;

        for (auto& guid : guids)
            if (guid.IsPlayer())
                player = ObjectAccessor::FindPlayer(guid);

        if (!player && botAI->GetMaster())
            player = botAI->GetMaster();

        if (!player)
            return false;

        if (!player->GetTrader())
        {
            WorldPacket packet(CMSG_INITIATE_TRADE);
            packet << player->GetGUID();
            bot->GetSession()->HandleInitiateTradeOpcode(packet);

            // Local change: the items named here go in once the window opens (TradeStatusAction)
            CraftData& craftData = AI_VALUE(CraftData&, "craft");
            craftData.pendingOrder = text;
            craftData.pendingOrderTime = time(nullptr);
            craftData.pendingOrderTrader = player->GetGUID();
            return true;
        }
        else if (player->GetTrader() != bot)
            return false;
    }

    uint32 copper = chat->parseMoney(text);
    if (copper > 0)
    {
        WorldPacket packet(CMSG_SET_TRADE_GOLD, 4);
        packet << copper;
        bot->GetSession()->HandleSetTradeGoldOpcode(packet);
    }

    size_t pos = text.rfind(" ");
    int count = pos != std::string::npos ? atoi(text.substr(pos + 1).c_str()) : 1;

    std::vector<Item*> found = parseItems(text);
    if (found.empty())
        return false;

    // Local change: a real player buying from a random bot names an amount of the item, not a number of stacks;
    // only an explicit trailing number counts (the default of 1 above also applies to names without a space)
    bool const hasAmount = pos != std::string::npos && pos + 1 < text.size() &&
                           text.find_first_not_of("0123456789", pos + 1) == std::string::npos;
    if (hasAmount && count > 0 && sPlayerbotAIConfig.randomBotCraftForPlayers && sRandomPlayerbotMgr.IsRandomBot(bot) &&
        IsRealPlayer(bot->GetTrader()))
        return TradeAmount(found, count);

    uint32 traded = 0;
    for (Item* item : found)
    {
        if (!bot->GetTrader() || item->IsInTrade())
            continue;

        int8 slot = item->CanBeTraded() ? -1 : TRADE_SLOT_NONTRADED;
        if (TradeItem(item, slot) && slot != TRADE_SLOT_NONTRADED && ++traded >= uint32(count))
            break;
    }

    return true;
}

// Local change
bool TradeAction::FindFreeSlot(Player* bot, uint32 itemId, uint32 count, uint8& bag, uint8& slot)
{
    ItemPosCountVec dest;
    auto const fits = [&](uint8 b, uint8 s)
    {
        dest.clear();
        if (bot->CanStoreNewItem(b, s, dest, itemId, count) != EQUIP_ERR_OK)
            return false;

        bag = b;
        slot = s;
        return true;
    };

    for (uint8 s = INVENTORY_SLOT_ITEM_START; s < INVENTORY_SLOT_ITEM_END; ++s)
        if (!bot->GetItemByPos(INVENTORY_SLOT_BAG_0, s) && fits(INVENTORY_SLOT_BAG_0, s))
            return true;

    for (uint8 b = INVENTORY_SLOT_BAG_START; b < INVENTORY_SLOT_BAG_END; ++b)
        if (Bag* pBag = bot->GetBagByPos(b))
            for (uint32 s = 0; s < pBag->GetBagSize(); ++s)
                if (!pBag->GetItemByPos(s) && fits(b, s))
                    return true;

    return false;
}

// Local change: whole stacks while they fit the amount, then the rest split off into a free slot
bool TradeAction::TradeAmount(std::vector<Item*> const& items, uint32 amount)
{
    bool traded = false;
    for (Item* item : items)
    {
        if (!amount || !bot->GetTrader())
            break;

        if (item->IsInTrade() || !item->CanBeTraded())
            continue;

        if (item->GetCount() <= amount)
        {
            if (TradeItem(item, -1))
            {
                amount -= item->GetCount();
                traded = true;
            }
            continue;
        }

        uint8 bag = 0;
        uint8 slot = 0;
        if (!FindFreeSlot(bot, item->GetEntry(), amount, bag, slot))
            break;

        bot->SplitItem(item->GetPos(), (uint16(bag) << 8) | slot, amount);
        if (Item* part = bot->GetItemByPos(bag, slot))
            if (TradeItem(part, -1))
            {
                amount = 0;
                traded = true;
            }
    }

    return traded;
}

bool TradeAction::TradeItem(Item const* item, int8 slot)
{
    int8 tradeSlot = -1;
    Item* itemPtr = const_cast<Item*>(item);

    TradeData* pTrade = bot->GetTradeData();
    if ((slot >= 0 && slot < TRADE_SLOT_COUNT) && pTrade->GetItem(TradeSlots(slot)) == nullptr)
        tradeSlot = slot;

    if (slot == TRADE_SLOT_NONTRADED)
        pTrade->SetItem(TRADE_SLOT_NONTRADED, itemPtr);
    else
    {
        for (uint8 i = 0; i < TRADE_SLOT_TRADED_COUNT && tradeSlot == -1; i++)
        {
            if (pTrade->GetItem(TradeSlots(i)) == itemPtr)
            {
                tradeSlot = i;

                WorldPacket packet(CMSG_CLEAR_TRADE_ITEM, 1);
                packet << (uint8)tradeSlot;
                bot->GetSession()->HandleClearTradeItemOpcode(packet);
                pTrade->SetItem(TradeSlots(i), nullptr);
                return true;
            }
        }

        for (uint8 i = 0; i < TRADE_SLOT_TRADED_COUNT && tradeSlot == -1; i++)
        {
            if (pTrade->GetItem(TradeSlots(i)) == nullptr)
                tradeSlot = i;
        }
    }

    if (tradeSlot == -1)
        return false;

    WorldPacket packet(CMSG_SET_TRADE_ITEM, 3);
    packet << (uint8)tradeSlot;
    packet << (uint8)item->GetBagSlot();
    packet << (uint8)item->GetSlot();
    bot->GetSession()->HandleSetTradeItemOpcode(packet);
    return true;
}
