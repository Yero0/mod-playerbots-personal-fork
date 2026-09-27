/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "TradeStatusAction.h"
#include "CraftValue.h"
#include "Event.h"
#include "GuildTaskMgr.h"
#include "ItemUsageValue.h"
#include "ItemVisitors.h"
#include "PlayerbotMgr.h"
#include "PlayerbotSecurity.h"
#include "PlayerbotTextMgr.h"
#include "Playerbots.h"
#include "RandomPlayerbotMgr.h"
#include "SetCraftAction.h"
#include "TradeAction.h"  // Local change

#include <algorithm>  // Local change

bool TradeStatusAction::Execute(Event event)
{
    if (IsSelfBot(bot))
        return false;

    if (!bot->GetSession())
        return false;

    Player* trader = bot->GetTrader();
    if (!trader)
        return false;

    bool const traderIsGameClientPlayer = IsRealPlayer(trader) || IsSelfBot(trader);
    Player* master = GetMaster();
    // Local change: with RandomBotCraftForPlayers a random bot trades with any real player, never other bots
    bool const craftCustomer =
        sPlayerbotAIConfig.randomBotCraftForPlayers && sRandomPlayerbotMgr.IsRandomBot(bot) && IsRealPlayer(trader);

    // Bots refuse to trade with a person (whether active or selfbotting) who is neither their
    // master nor a group member. Bot traders (other than selfbots) are handled further down.
    if (trader != master && traderIsGameClientPlayer && !craftCustomer &&  // Local change: craftCustomer
        (!bot->GetGroup() || !bot->GetGroup()->IsMember(trader->GetGUID())))
    {
        bot->Whisper(PlayerbotTextMgr::instance().GetBotTextOrDefault(
                         "trade_busy_now", "I'm kind of busy now", {}),
                     LANG_UNIVERSAL, trader);
        CancelTrade();
        return false;
    }

    if (sPlayerbotAIConfig.enableRandomBotTrading == 0 &&
        (sRandomPlayerbotMgr.IsRandomBot(bot)|| sRandomPlayerbotMgr.IsAddclassBot(bot)))
    {
        bot->Whisper(PlayerbotTextMgr::instance().GetBotTextOrDefault(
                         "trade_disabled", "Trading is disabled", {}),
                     LANG_UNIVERSAL, trader);
        CancelTrade();
        return false;
    }

    // Bots also refuse their own master when ungrouped and security withholds full access.
    if ((!bot->GetGroup() || !bot->GetGroup()->IsMember(trader->GetGUID())) &&
        (trader != master || !botAI->GetSecurity()->CheckLevelFor(PLAYERBOT_SECURITY_ALLOW_ALL, true, master)) &&
        traderIsGameClientPlayer && !craftCustomer)  // Local change: craftCustomer
    {
        CancelTrade();
        return false;
    }

    WorldPacket p(event.getPacket());
    p.rpos(0);
    uint32 status;
    p >> status;

    if (status == TRADE_STATUS_TRADE_ACCEPT ||
        (status == TRADE_STATUS_BACK_TO_TRADE &&
         trader->GetTradeData() && trader->GetTradeData()->IsAccepted()))
    {
        WorldPacket p;
        uint32 status = 0;
        p << status;

        uint32 discount = sRandomPlayerbotMgr.GetTradeDiscount(bot, trader);
        if (CheckTrade())
        {
            std::map<uint32, uint32> givenItemIds, takenItemIds;
            for (uint32 slot = 0; slot < TRADE_SLOT_TRADED_COUNT; ++slot)
            {
                Item* item = trader->GetTradeData()->GetItem((TradeSlots)slot);
                if (item)
                    givenItemIds[item->GetTemplate()->ItemId] += item->GetCount();

                item = bot->GetTradeData()->GetItem((TradeSlots)slot);
                if (item)
                    takenItemIds[item->GetTemplate()->ItemId] += item->GetCount();
            }

            bot->GetSession()->HandleAcceptTradeOpcode(p);
            if (bot->GetTradeData())
            {
                sRandomPlayerbotMgr.SetTradeDiscount(bot, trader, discount);
                return false;
            }

            for (std::map<uint32, uint32>::iterator i = givenItemIds.begin(); i != givenItemIds.end(); ++i)
            {
                uint32 itemId = i->first;
                uint32 count = i->second;

                CraftData& craftData = AI_VALUE(CraftData&, "craft");
                if (!craftData.IsEmpty() && craftData.IsRequired(itemId))
                    craftData.AddObtained(itemId, count);

                GuildTaskMgr::instance().CheckItemTask(itemId, count, trader, bot);
            }

            for (std::map<uint32, uint32>::iterator i = takenItemIds.begin(); i != takenItemIds.end(); ++i)
            {
                uint32 itemId = i->first;
                uint32 count = i->second;

                CraftData& craftData = AI_VALUE(CraftData&, "craft");
                // Local change: sold units no longer carry the order price
                auto const order = craftData.prices.find(itemId);
                if (order != craftData.prices.end())
                {
                    if (order->second.count > count)
                        order->second.count -= count;
                    else
                        craftData.prices.erase(order);
                }
                if (!craftData.IsEmpty() && craftData.itemId == itemId)
                    craftData.Crafted(count);
            }

            return true;
        }
    }
    else if (status == TRADE_STATUS_BEGIN_TRADE)
    {
        // Local change: stop the current walk; CanMove keeps the bot in place until the trade closes
        bot->GetMotionMaster()->Clear();
        bot->StopMoving();

        if (!bot->HasInArc(CAST_ANGLE_IN_FRONT, trader, sPlayerbotAIConfig.sightDistance))
            bot->SetFacingToObject(trader);

        BeginTrade();

        // Local change: fill an order whispered before the window was open (at most 10 s old)
        CraftData& craftData = AI_VALUE(CraftData&, "craft");
        if (!craftData.pendingOrder.empty())
        {
            std::string const order = craftData.pendingOrder;
            bool const fresh = time(nullptr) - craftData.pendingOrderTime <= 10 &&
                               craftData.pendingOrderTrader == trader->GetGUID();
            craftData.pendingOrder.clear();
            if (fresh)
                TradeAction(botAI).Execute(Event("trade", order));
        }

        return true;
    }
    return false;
}

void TradeStatusAction::BeginTrade()
{
    Player* trader = bot->GetTrader();
    if (!trader || (GET_PLAYERBOT_AI(trader) && !IsSelfBot(trader)))
        return;

    WorldPacket p;
    bot->GetSession()->HandleBeginTradeOpcode(p);

    ListItemsVisitor visitor;
    IterateItems(&visitor);

    botAI->TellMaster("=== Inventory ===");
    TellItems(visitor.items, visitor.soulbound);

    // Local change: what a random bot crafts on order, the best few per profession
    if (sPlayerbotAIConfig.randomBotCraftForPlayers && sRandomPlayerbotMgr.IsRandomBot(bot) && IsRealPlayer(trader))
    {
        std::vector<CraftableItem> const craftables = SetCraftAction::GetCraftableItems(bot);
        if (!craftables.empty())
        {
            botAI->TellMaster(PlayerbotTextMgr::instance().GetBotTextOrDefault(
                "craft_trade_header", "=== Crafting (whisper 'craft [item]') ===", {}));

            // ponytail: fixed top 10 per profession; add a 'craft list <name>' filter if players need the rest
            uint32 skill = 0;
            uint32 shown = 0;
            for (CraftableItem const& craftable : craftables)
            {
                if (craftable.skill != skill)
                {
                    skill = craftable.skill;
                    shown = 0;
                }

                if (++shown > 10)
                    continue;

                botAI->TellMaster(chat->FormatItem(craftable.item, craftable.count) + " - " +
                                  chat->formatMoney(craftable.price));
            }
        }
    }

    if (sRandomPlayerbotMgr.IsRandomBot(bot))
    {
        uint32 discount = sRandomPlayerbotMgr.GetTradeDiscount(bot, botAI->GetMaster());
        if (discount)
        {
            std::ostringstream out;
            out << "Discount up to: " << chat->formatMoney(discount);
            botAI->TellMaster(out);
        }
    }
}

void TradeStatusAction::CancelTrade()
{
    WorldPacket p;
    bot->GetSession()->HandleCancelTradeOpcode(p);
}

bool TradeStatusAction::CheckTrade()
{
    Player* trader = bot->GetTrader();
    if (!bot->GetTradeData() || !trader || !trader->GetTradeData())
        return false;

    if (!botAI->HasGameClientMaster() && GET_PLAYERBOT_AI(bot->GetTrader()) &&
        !IsSelfBot(bot->GetTrader()))
    {
        for (uint32 slot = 0; slot < TRADE_SLOT_TRADED_COUNT; ++slot)
        {
            Item* item = bot->GetTradeData()->GetItem((TradeSlots)slot);
            if (item)
                break;
        }
        bool isGettingItem = false;
        for (uint32 slot = 0; slot < TRADE_SLOT_TRADED_COUNT; ++slot)
        {
            Item* item = trader->GetTradeData()->GetItem((TradeSlots)slot);
            if (item)
            {
                isGettingItem = true;
                break;
            }
        }

        if (isGettingItem)
        {
            if (bot->GetGroup() && bot->GetGroup()->IsMember(bot->GetTrader()->GetGUID()) &&
                botAI->HasGameClientMaster())
            {
                botAI->TellMasterNoFacing(PlayerbotTextMgr::instance().GetBotTextOrDefault(
                    "trade_thank_you_player",
                    "Thank you %player",
                    {{"%player", chat->FormatWorldobject(bot->GetTrader())}}));
            }
            else
            {
                bot->Say(PlayerbotTextMgr::instance().GetBotTextOrDefault(
                             "trade_thank_you_player",
                             "Thank you %player",
                             {{"%player", chat->FormatWorldobject(bot->GetTrader())}}),
                         (bot->GetTeamId() == TEAM_ALLIANCE ? LANG_COMMON : LANG_ORCISH));
            }
        }
        return isGettingItem;
    }

    uint32 accountId = bot->GetSession()->GetAccountId();
    if (!sPlayerbotAIConfig.IsInRandomAccountList(accountId))
    {
        int32 botItemsMoney = CalculateCost(bot, true);
        int32 botMoney = bot->GetTradeData()->GetMoney() + botItemsMoney;
        int32 playerItemsMoney = CalculateCost(trader, false);
        int32 playerMoney = trader->GetTradeData()->GetMoney() + playerItemsMoney;
        if (playerMoney || botMoney)
            botAI->PlaySound(playerMoney < botMoney ? TEXT_EMOTE_SIGH : TEXT_EMOTE_THANK);

        return true;
    }

    int32 botItemsMoney = CalculateCost(bot, true);
    int32 botMoney = bot->GetTradeData()->GetMoney() + botItemsMoney;
    int32 playerItemsMoney = CalculateCost(trader, false);
    int32 playerMoney = trader->GetTradeData()->GetMoney() + playerItemsMoney;
    if (botItemsMoney > 0 && sPlayerbotAIConfig.enableRandomBotTrading == 2 &&
        (sRandomPlayerbotMgr.IsRandomBot(bot)|| sRandomPlayerbotMgr.IsAddclassBot(bot)))
    {
        bot->Whisper(PlayerbotTextMgr::instance().GetBotTextOrDefault(
                         "trade_selling_disabled", "Selling is disabled.", {}),
                     LANG_UNIVERSAL, trader);
        return false;
    }
    if (playerItemsMoney && sPlayerbotAIConfig.enableRandomBotTrading == 3 &&
        (sRandomPlayerbotMgr.IsRandomBot(bot)|| sRandomPlayerbotMgr.IsAddclassBot(bot)))
    {
        bot->Whisper(PlayerbotTextMgr::instance().GetBotTextOrDefault(
                         "trade_buying_disabled", "Buying is disabled.", {}),
                     LANG_UNIVERSAL, trader);
        return false;
    }
    for (uint32 slot = 0; slot < TRADE_SLOT_TRADED_COUNT; ++slot)
    {
        Item* item = bot->GetTradeData()->GetItem((TradeSlots)slot);
        if (item && !item->GetTemplate()->SellPrice && !item->GetTemplate()->IsConjuredConsumable())
        {
            std::ostringstream out;
            botAI->TellMaster(PlayerbotTextMgr::instance().GetBotTextOrDefault(
                "trade_item_not_for_sale",
                "%item - This is not for sale",
                {{"%item", chat->FormatItem(item->GetTemplate())}}));
            botAI->PlaySound(TEXT_EMOTE_NO);
            return false;
        }

        item = trader->GetTradeData()->GetItem((TradeSlots)slot);
        if (item)
        {
            std::ostringstream out;
            out << item->GetTemplate()->ItemId;
            ItemUsage usage = AI_VALUE2(ItemUsage, "item usage", out.str());
            if ((botMoney && !item->GetTemplate()->BuyPrice) || usage == ITEM_USAGE_NONE)
            {
                std::ostringstream out;
                botAI->TellMaster(PlayerbotTextMgr::instance().GetBotTextOrDefault(
                    "trade_item_not_needed",
                    "%item - I don't need this",
                    {{"%item", chat->FormatItem(item->GetTemplate())}}));
                botAI->PlaySound(TEXT_EMOTE_NO);
                return false;
            }
        }
    }

    if (!botMoney && !playerMoney)
        return true;

    if (!botItemsMoney && !playerItemsMoney)
    {
        botAI->TellError(PlayerbotTextMgr::instance().GetBotTextOrDefault(
            "trade_no_items_error", "There are no items to trade", {}));
        return false;
    }

    int32 discount = (int32)sRandomPlayerbotMgr.GetTradeDiscount(bot, trader);
    int32 delta = playerMoney - botMoney;
    int32 moneyDelta = (int32)trader->GetTradeData()->GetMoney() - (int32)bot->GetTradeData()->GetMoney();
    bool success = false;
    if (delta < 0)
    {
        if (delta + discount >= 0)
        {
            if (moneyDelta < 0)
            {
                botAI->TellError(PlayerbotTextMgr::instance().GetBotTextOrDefault(
                    "trade_discount_buy_only", "You can use discount to buy items only", {}));
                botAI->PlaySound(TEXT_EMOTE_NO);
                return false;
            }
            success = true;
        }
    }
    else
        success = true;

    if (success)
    {
        sRandomPlayerbotMgr.AddTradeDiscount(bot, trader, delta);
        switch (urand(0, 4))
        {
            case 0:
                botAI->TellMaster(PlayerbotTextMgr::instance().GetBotTextOrDefault(
                    "trade_success_pleasure", "A pleasure doing business with you", {}));
                break;
            case 1:
                botAI->TellMaster(PlayerbotTextMgr::instance().GetBotTextOrDefault(
                    "trade_success_fair_trade", "Fair trade", {}));
                break;
            case 2:
                botAI->TellMaster(PlayerbotTextMgr::instance().GetBotTextOrDefault(
                    "trade_success_thanks", "Thanks", {}));
                break;
            case 3:
                botAI->TellMaster(PlayerbotTextMgr::instance().GetBotTextOrDefault(
                    "trade_success_off_with_you", "Off with you", {}));
                break;
        }

        botAI->PlaySound(TEXT_EMOTE_THANK);
        return true;
    }

    // Local change: the total gold to put in, not the shortfall (players replaced their gold with the shortfall)
    uint32 const required = trader->GetTradeData()->GetMoney() - (delta + discount);
    botAI->TellMaster(PlayerbotTextMgr::instance().GetBotTextOrDefault(
        "trade_want_money_for_this",
        "I want %money for this",
        {{"%money", chat->formatMoney(required)}}));
    botAI->PlaySound(TEXT_EMOTE_NO);
    return false;
}

int32 TradeStatusAction::CalculateCost(Player* player, bool sell)
{
    Player* trader = bot->GetTrader();
    TradeData* data = player->GetTradeData();
    if (!data)
        return 0;

    uint32 sum = 0;
    std::map<uint32, uint32> orderedLeft;  // Local change: crafted-on-order units not yet priced in this trade
    for (auto const& [itemId, order] : AI_VALUE(CraftData&, "craft").prices)
        orderedLeft[itemId] = order.count;

    for (uint32 slot = 0; slot < TRADE_SLOT_TRADED_COUNT; ++slot)
    {
        Item* item = data->GetItem((TradeSlots)slot);
        if (!item)
            continue;

        ItemTemplate const* proto = item->GetTemplate();
        if (!proto)
            continue;

        if (proto->Quality < ITEM_QUALITY_NORMAL)
            return 0;

        CraftData& craftData = AI_VALUE(CraftData&, "craft");
        // Local change: crafted-on-order units at the order price, any other units of the stack as usual below
        uint32 count = item->GetCount();
        if (player == bot && sell)
        {
            auto const order = craftData.prices.find(proto->ItemId);
            if (order != craftData.prices.end())
            {
                uint32 const ordered = std::min(count, orderedLeft[proto->ItemId]);
                orderedLeft[proto->ItemId] -= ordered;
                sum += ordered * order->second.price;
                count -= ordered;
                if (!count)
                    continue;
            }
        }

        if (!craftData.IsEmpty())
        {
            if (player == trader && !sell && craftData.IsRequired(proto->ItemId))
                continue;

            if (player == bot && sell && craftData.itemId == proto->ItemId && craftData.IsFulfilled())
            {
                sum += item->GetCount() * SetCraftAction::GetCraftFee(craftData);
                continue;
            }
        }

        if (sell)
            sum += count * proto->SellPrice * sRandomPlayerbotMgr.GetSellMultiplier(bot);  // Local change: count

        else
            sum += count * proto->BuyPrice * sRandomPlayerbotMgr.GetBuyMultiplier(bot);  // Local change: count

    }

    return sum;
}
