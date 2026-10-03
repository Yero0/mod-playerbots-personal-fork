/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_CRAFTVALUE_H
#define PLAYERBOTS_CRAFTVALUE_H

#include "ObjectGuid.h"  // Local change
#include "Value.h"
#include <ctime>  // Local change
#include <map>
#include <string>  // Local change
#include <vector>  // Local change

class PlayerbotAI;

class CraftData
{
public:
    CraftData() : itemId(0) {}

    uint32 itemId;
    std::map<uint32, uint32> required, obtained;
    // Local change: crafted-on-order items by item id: price per item and how many units carry it
    struct OrderPrice
    {
        uint32 price{0};
        uint32 count{0};
    };
    std::map<uint32, OrderPrice> prices;
    // Local change: items advertised in chat, by item id: until when they are kept back from vendors
    std::map<uint32, time_t> advertised;
    // Local change: a trade command sent before the trade window was open, replayed when it opens
    std::string pendingOrder;
    time_t pendingOrderTime{0};
    ObjectGuid pendingOrderTrader;
    // Local change: shop menu (gossip) session with one real player, see BotShopAction
    struct ShopLine
    {
        uint32 itemId{0};
        uint32 amount{0};  // items
        uint32 price{0};   // per item
        bool crafted{false};
    };
    ObjectGuid shopCustomer;
    time_t shopExpire{0};
    std::vector<ShopLine> shopCart;
    uint32 shopSkill{0};     // list shown: 0 = main menu, a profession skill, or BotShopAction::SHOP_FOR_SALE
    uint32 shopCategory{0};  // 0 = the profession's categories
    uint32 shopPage{0};
    bool shopAllLevels{false};
    bool shopCheckout{false};  // fill the next trade window with the cart
    bool IsAdvertised(uint32 item) const
    {
        auto const itr = advertised.find(item);
        return itr != advertised.end() && itr->second > time(nullptr);
    }

    bool IsEmpty() { return itemId == 0; }
    void Reset() { itemId = 0; }
    bool IsRequired(uint32 item) { return required.find(item) != required.end(); }
    bool IsFulfilled()
    {
        for (std::map<uint32, uint32>::iterator i = required.begin(); i != required.end(); ++i)
        {
            uint32 item = i->first;
            if (obtained[item] < i->second)
                return false;
        }

        return true;
    }
    void AddObtained(uint32 itemId, uint32 count)
    {
        if (IsRequired(itemId))
        {
            obtained[itemId] += count;
        }
    }

    void Crafted(uint32 count)
    {
        for (std::map<uint32, uint32>::iterator i = required.begin(); i != required.end(); ++i)
        {
            uint32 item = i->first;
            if (obtained[item] >= required[item] * count)
            {
                obtained[item] -= required[item] * count;
            }
        }
    }
};

class CraftValue : public ManualSetValue<CraftData&>
{
public:
    CraftValue(PlayerbotAI* botAI, std::string const name = "craft") : ManualSetValue<CraftData&>(botAI, data, name) {}

private:
    CraftData data;
};

#endif
