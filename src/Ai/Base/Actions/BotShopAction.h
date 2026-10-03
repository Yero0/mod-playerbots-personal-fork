/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Local change: new file

#ifndef PLAYERBOTS_BOTSHOPACTION_H
#define PLAYERBOTS_BOTSHOPACTION_H

#include <map>
#include <string>
#include <vector>

#include "InventoryAction.h"

class Player;
class PlayerbotAI;

struct CraftableItem;
struct ItemTemplate;

// A random bot's shop (RandomBotCraftForPlayers) as a gossip menu sent to the real player trading with it: browse
// its crafts and items for sale, fill a cart, check out through a trade. Not registered: built where it is needed.
class BotShopAction : public InventoryAction
{
public:
    BotShopAction(PlayerbotAI* botAI) : InventoryAction(botAI, "bot shop") {}

    // the customer's player gossip menu; its sender is the bot's guid counter
    static constexpr uint32 MENU_ID = 0x424F5453;
    static constexpr uint32 SHOP_FOR_SALE = 0xFFFF;

    // a trade opened by a real player: start (or resume) the session and send the menu; false = plain trade
    bool Open(Player* customer);
    // a trade window with the customer is open after checkout: put the cart in, mail the rest cash on delivery
    void FillTrade();
    // the session keeps the bot in place; it ends (cart dropped) on timeout or when the customer leaves
    bool InSession();
    static bool InSession(PlayerbotAI* botAI);
    // the customer's gossip hooks
    static void OnSelect(Player* player, uint32 sender, uint32 action, std::string const& code);

private:
    struct ShopItem
    {
        ItemTemplate const* proto;
        uint32 count;      // items per pick: per craft, or 1 for items for sale
        uint32 price;      // per pick
        uint32 available;  // items for sale in the bags; 0 = crafted
        uint32 category;
    };

    void Select(Player* customer, uint32 action, std::string const& code);
    void Show(Player* customer);
    void AddToCart(Player* customer, uint32 itemId, std::string const& code);
    void Checkout(Player* customer);
    std::vector<ShopItem> GetShopItems(Player* customer, uint32 skill);
    std::string CartText(Player* customer);
    std::string Text(std::string const& name, std::string const& text,
                     std::map<std::string, std::string> const& placeholders = {});

    static bool IsShown(Player* customer, ItemTemplate const* proto, bool allLevels);
    static uint32 GetCategory(CraftableItem const& craftable);
    static std::string GetCategoryName(uint32 category);
    static std::string GetItemName(Player* customer, ItemTemplate const* proto);
};

#endif
