/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Local change: new file

#include "BotShopAction.h"

#include <algorithm>
#include <set>

#include "ChatHelper.h"
#include "CraftValue.h"
#include "DBCStores.h"
#include "GossipDef.h"
#include "Item.h"
#include "ItemUsageValue.h"
#include "ItemVisitors.h"
#include "Mail.h"
#include "ObjectAccessor.h"
#include "PlayerbotTextMgr.h"
#include "Playerbots.h"
#include "RandomPlayerbotMgr.h"
#include "ScriptedGossip.h"
#include "SetCraftAction.h"
#include "SpellInfo.h"
#include "TradeAction.h"
#include "World.h"

enum BotShopCommand : uint32
{
    SHOP_CMD_MAIN = 1,
    SHOP_CMD_SKILL,     // arg: profession skill or SHOP_FOR_SALE
    SHOP_CMD_CATEGORY,  // arg: category
    SHOP_CMD_PAGE,      // arg: page
    SHOP_CMD_LEVELS,
    SHOP_CMD_PICK,  // arg: item id; the amount comes from the text box
    SHOP_CMD_CHECKOUT,
    SHOP_CMD_CLEAR
};

// a category is a base plus the item's subclass, inventory type or class
enum BotShopCategory : uint32
{
    SHOP_CAT_ARMOR = 100,
    SHOP_CAT_WEAPON = 200,
    SHOP_CAT_GEM = 300,
    SHOP_CAT_CONSUMABLE = 400,
    SHOP_CAT_CLASS = 500,
    SHOP_CAT_SCROLL = 600,  // plus the inventory type it enchants, or a weapon offset below
    SHOP_SCROLL_WEAPON = 40,
    SHOP_SCROLL_TWO_HAND = 41,
    SHOP_SCROLL_STAFF = 42
};

static constexpr uint32 SHOP_CMD_SHIFT = 24;
static constexpr uint32 SHOP_PAGE_SIZE = 10;
static constexpr uint32 SHOP_LEVEL_WINDOW = 5;
static constexpr uint32 SHOP_MAX_STACKS = 30;  // the first 6 go in the trade, the rest in cash-on-delivery mails
static constexpr time_t SHOP_SESSION_TIME = 2 * MINUTE;  // idle; the client sends nothing on "Goodbye"
static constexpr time_t SHOP_CHECKOUT_TIME = MINUTE;     // to accept the checkout trade
static constexpr int32 SHOP_ONE_HAND_WEAPONS = (1 << ITEM_SUBCLASS_WEAPON_AXE) | (1 << ITEM_SUBCLASS_WEAPON_MACE) |
                                               (1 << ITEM_SUBCLASS_WEAPON_SWORD) | (1 << ITEM_SUBCLASS_WEAPON_FIST) |
                                               (1 << ITEM_SUBCLASS_WEAPON_DAGGER);

class ShopStockVisitor : public IterateItemsVisitor
{
public:
    bool Visit(Item* item) override
    {
        if (item->CanBeTraded())
            stock[item->GetEntry()] += item->GetCount();

        return true;
    }

    std::map<uint32, uint32> stock;
};

bool BotShopAction::Open(Player* customer)
{
    // the shop sells: not with EnableRandomBotTrading 2 (only buy)
    if (!sPlayerbotAIConfig.randomBotCraftForPlayers || sPlayerbotAIConfig.enableRandomBotTrading == 2 || !customer ||
        !IsRealPlayer(customer) || !sRandomPlayerbotMgr.IsRandomBot(bot))
        return false;

    // busy with another customer: a plain trade, with the chat list and item-link orders
    CraftData& data = AI_VALUE(CraftData&, "craft");
    if (InSession() && data.shopCustomer != customer->GetGUID())
        return false;

    data.shopCustomer = customer->GetGUID();
    data.shopSkill = 0;
    data.shopCategory = 0;
    data.shopPage = 0;
    data.shopCheckout = false;
    Show(customer);
    return true;
}

bool BotShopAction::InSession()
{
    CraftData& data = AI_VALUE(CraftData&, "craft");
    if (data.shopCustomer.IsEmpty())
        return false;

    Player* customer = ObjectAccessor::FindPlayer(data.shopCustomer);
    if (customer && time(nullptr) < data.shopExpire && bot->IsWithinDistInMap(customer, TRADE_DISTANCE))
        return true;

    // the client keeps the menu open when walking away; close it if it is still ours, and cancel a checkout trade
    // nobody answered (the core has no trade distance check); same map only: the same thread
    if (customer && customer->IsInMap(bot))
    {
        GossipMenu const& menu = customer->PlayerTalkClass->GetGossipMenu();
        if (menu.GetMenuId() == MENU_ID && menu.GetSenderGUID() == customer->GetGUID())
            CloseGossipMenuFor(customer);

        if (bot->GetTrader() == customer)
            bot->TradeCancel(true);
    }

    data.shopCustomer.Clear();
    data.shopCart.clear();
    data.shopCheckout = false;
    return false;
}

bool BotShopAction::InSession(PlayerbotAI* botAI)
{
    // cheap check first: CanMove asks this for every bot
    return sPlayerbotAIConfig.randomBotCraftForPlayers &&
           !botAI->GetAiObjectContext()->GetValue<CraftData&>("craft")->Get().shopCustomer.IsEmpty() &&
           BotShopAction(botAI).InSession();
}

void BotShopAction::OnSelect(Player* player, uint32 sender, uint32 action, std::string const& code)
{
    Player* shopBot =
        sPlayerbotAIConfig.randomBotCraftForPlayers ? ObjectAccessor::FindPlayerByLowGUID(sender) : nullptr;
    PlayerbotAI* shopBotAI = shopBot ? GET_PLAYERBOT_AI(shopBot) : nullptr;
    if (!shopBotAI)
    {
        CloseGossipMenuFor(player);
        return;
    }

    BotShopAction(shopBotAI).Select(player, action, code);
}

void BotShopAction::Select(Player* customer, uint32 action, std::string const& code)
{
    CraftData& data = AI_VALUE(CraftData&, "craft");
    if (!InSession() || data.shopCustomer != customer->GetGUID())
    {
        CloseGossipMenuFor(customer);
        return;
    }

    uint32 const arg = action & ((1 << SHOP_CMD_SHIFT) - 1);
    switch (action >> SHOP_CMD_SHIFT)
    {
        case SHOP_CMD_SKILL:
            data.shopSkill = arg;
            data.shopCategory = 0;
            data.shopPage = 0;
            break;
        case SHOP_CMD_CATEGORY:
            data.shopCategory = arg;
            data.shopPage = 0;
            break;
        case SHOP_CMD_PAGE:
            data.shopPage = arg;
            break;
        case SHOP_CMD_LEVELS:
            data.shopAllLevels = !data.shopAllLevels;
            data.shopPage = 0;
            break;
        case SHOP_CMD_PICK:
            AddToCart(customer, arg, code);
            break;
        case SHOP_CMD_CHECKOUT:
            Checkout(customer);
            return;
        case SHOP_CMD_CLEAR:
            data.shopCart.clear();
            [[fallthrough]];
        default:  // SHOP_CMD_MAIN
            data.shopSkill = 0;
            data.shopCategory = 0;
            data.shopPage = 0;
            break;
    }

    Show(customer);
}

void BotShopAction::Show(Player* customer)
{
    struct Entry
    {
        uint8 icon;
        std::string text;
        uint32 action;
        std::string box;  // a text box asking the amount
    };

    CraftData& data = AI_VALUE(CraftData&, "craft");
    data.shopExpire = time(nullptr) + SHOP_SESSION_TIME;

    std::vector<Entry> entries;
    if (!data.shopSkill)
    {
        entries.push_back({GOSSIP_ICON_VENDOR, Text("shop_for_sale", "Items for sale"),
                           (SHOP_CMD_SKILL << SHOP_CMD_SHIFT) | SHOP_FOR_SALE, ""});

        std::set<uint32> skills;
        for (CraftableItem const& craftable : SetCraftAction::GetCraftableItems(bot))
        {
            if (!skills.insert(craftable.skill).second)
                continue;

            if (SkillLineEntry const* skillLine = sSkillLineStore.LookupEntry(craftable.skill))
                entries.push_back({GOSSIP_ICON_TRAINER, skillLine->name[sWorld->GetDefaultDbcLocale()],
                                   (SHOP_CMD_SKILL << SHOP_CMD_SHIFT) | craftable.skill, ""});
        }

        if (!data.shopCart.empty())
        {
            entries.push_back(
                {GOSSIP_ICON_MONEY_BAG, Text("shop_checkout", "Checkout"), SHOP_CMD_CHECKOUT << SHOP_CMD_SHIFT, ""});
            entries.push_back(
                {GOSSIP_ICON_CHAT, Text("shop_clear_cart", "Clear cart"), SHOP_CMD_CLEAR << SHOP_CMD_SHIFT, ""});
        }
    }
    else if (data.shopSkill != SHOP_FOR_SALE && !data.shopCategory)
    {
        std::map<uint32, uint32> categories;  // category -> items shown in it
        for (ShopItem const& item : GetShopItems(customer, data.shopSkill))
            ++categories[item.category];

        for (auto const& [category, count] : categories)
            entries.push_back({GOSSIP_ICON_INTERACT_1, Acore::StringFormat("{} ({})", GetCategoryName(category), count),
                               (SHOP_CMD_CATEGORY << SHOP_CMD_SHIFT) | category, ""});
    }
    else
    {
        std::vector<ShopItem> items = GetShopItems(customer, data.shopSkill);
        std::erase_if(
            items, [&data](ShopItem const& item) { return data.shopCategory && item.category != data.shopCategory; });
        std::sort(items.begin(), items.end(),
                  [](ShopItem const& a, ShopItem const& b)
                  {
                      return a.proto->RequiredLevel != b.proto->RequiredLevel
                                 ? a.proto->RequiredLevel > b.proto->RequiredLevel
                                 : a.proto->Name1 < b.proto->Name1;
                  });

        for (ShopItem const& item : items)
        {
            std::string const name = GetItemName(customer, item.proto);
            std::string const price = chat->formatMoney(item.price);
            if (item.available)
                entries.push_back({GOSSIP_ICON_VENDOR, Acore::StringFormat("{} ({}) - {}", name, item.available, price),
                                   (SHOP_CMD_PICK << SHOP_CMD_SHIFT) | item.proto->ItemId,
                                   Text("shop_how_many_stock", "How many? %count in stock",
                                        {{"%count", std::to_string(item.available)}})});
            else
                entries.push_back({GOSSIP_ICON_VENDOR,
                                   item.count > 1 ? Acore::StringFormat("{} x{} - {}", name, item.count, price)
                                                  : Acore::StringFormat("{} - {}", name, price),
                                   (SHOP_CMD_PICK << SHOP_CMD_SHIFT) | item.proto->ItemId,
                                   Text("shop_how_many_crafts", "How many crafts? One craft makes %count",
                                        {{"%count", std::to_string(item.count)}})});
        }
    }

    if (data.shopPage * SHOP_PAGE_SIZE >= entries.size())
        data.shopPage = 0;

    size_t const first = data.shopPage * SHOP_PAGE_SIZE;
    size_t const last = std::min<size_t>(entries.size(), first + SHOP_PAGE_SIZE);
    uint32 const sender = bot->GetGUID().GetCounter();

    ClearGossipMenuFor(customer);
    customer->PlayerTalkClass->GetGossipMenu().SetMenuId(MENU_ID);
    AddGossipItemFor(customer, GOSSIP_ICON_MONEY_BAG, CartText(customer), sender, SHOP_CMD_MAIN << SHOP_CMD_SHIFT);
    for (size_t i = first; i < last; ++i)
    {
        if (entries[i].box.empty())
            AddGossipItemFor(customer, entries[i].icon, entries[i].text, sender, entries[i].action);
        else
            AddGossipItemFor(customer, entries[i].icon, entries[i].text, sender, entries[i].action, entries[i].box, 0,
                             true);
    }

    if (data.shopSkill)
    {
        if (entries.empty())
            AddGossipItemFor(customer, GOSSIP_ICON_CHAT, Text("shop_nothing", "Nothing for you here"), sender,
                             SHOP_CMD_MAIN << SHOP_CMD_SHIFT);

        if (data.shopPage)
            AddGossipItemFor(customer, GOSSIP_ICON_DOT, Text("shop_previous_page", "Previous page"), sender,
                             (SHOP_CMD_PAGE << SHOP_CMD_SHIFT) | (data.shopPage - 1));

        if (last < entries.size())
            AddGossipItemFor(customer, GOSSIP_ICON_DOT, Text("shop_next_page", "Next page"), sender,
                             (SHOP_CMD_PAGE << SHOP_CMD_SHIFT) | (data.shopPage + 1));

        AddGossipItemFor(customer, GOSSIP_ICON_DOT,
                         data.shopAllLevels ? Text("shop_my_levels", "Show items for my level")
                                            : Text("shop_all_levels", "Show all levels"),
                         sender, SHOP_CMD_LEVELS << SHOP_CMD_SHIFT);

        if (data.shopCategory)
            AddGossipItemFor(customer, GOSSIP_ICON_DOT, Text("shop_back", "Back"), sender,
                             (SHOP_CMD_SKILL << SHOP_CMD_SHIFT) | data.shopSkill);

        AddGossipItemFor(customer, GOSSIP_ICON_DOT, Text("shop_main_menu", "Main menu"), sender,
                         SHOP_CMD_MAIN << SHOP_CMD_SHIFT);
    }

    SendGossipMenuFor(customer, DEFAULT_GOSSIP_MESSAGE, customer->GetGUID());
}

void BotShopAction::AddToCart(Player* customer, uint32 itemId, std::string const& code)
{
    int32 const picks = atoi(code.c_str());
    if (picks <= 0)
        return;

    CraftData& data = AI_VALUE(CraftData&, "craft");
    std::vector<ShopItem> const items = GetShopItems(customer, data.shopSkill);
    auto const item = std::find_if(items.begin(), items.end(),
                                   [itemId](ShopItem const& shopItem) { return shopItem.proto->ItemId == itemId; });
    if (item == items.end())
        return;

    bool const crafted = !item->available;
    auto line = std::find_if(data.shopCart.begin(), data.shopCart.end(), [itemId, crafted](CraftData::ShopLine const& l)
                             { return l.itemId == itemId && l.crafted == crafted; });
    uint32 const inCart = line != data.shopCart.end() ? line->amount : 0;

    // stacks of the other lines, then as many of these as still fit; whole crafts, never more than the bot has
    uint32 otherStacks = 0;
    for (CraftData::ShopLine const& other : data.shopCart)
    {
        if (line != data.shopCart.end() && &other == &*line)
            continue;

        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(other.itemId);
        uint32 const maxStack = proto ? std::max<uint32>(1, proto->GetMaxStackSize()) : 1;
        otherStacks += (other.amount + maxStack - 1) / maxStack;
    }

    uint32 const maxStack = std::max<uint32>(1, item->proto->GetMaxStackSize());
    uint32 amount = inCart + std::min<uint32>(picks, 1000) * item->count;
    amount = std::min(amount, (SHOP_MAX_STACKS - std::min(SHOP_MAX_STACKS, otherStacks)) * maxStack);
    amount -= amount % item->count;
    if (!crafted)
        amount = std::min(amount, item->available);

    if (amount <= inCart)
    {
        bot->Whisper(Text("shop_cart_full", "Your cart is full"), LANG_UNIVERSAL, customer);
        return;
    }

    if (line == data.shopCart.end())
    {
        data.shopCart.push_back({itemId, 0, 0, crafted});
        line = std::prev(data.shopCart.end());
    }

    line->amount = amount;
    line->price = (item->price + item->count - 1) / item->count;  // per item, as SetCraftAction::CraftForTrader
}

void BotShopAction::Checkout(Player* customer)
{
    CloseGossipMenuFor(customer);
    CraftData& data = AI_VALUE(CraftData&, "craft");
    if (data.shopCart.empty())
        return;

    data.shopCheckout = true;
    data.shopExpire = time(nullptr) + SHOP_CHECKOUT_TIME;
    if (bot->GetTrader() == customer)
    {
        FillTrade();
        return;
    }

    if (bot->GetTrader() || customer->GetTrader())
    {
        data.shopCheckout = false;
        bot->Whisper(Text("trade_busy_now", "I'm kind of busy now"), LANG_UNIVERSAL, customer);
        return;
    }

    // TradeStatusAction calls FillTrade once the customer accepts and the window opens
    WorldPacket packet(CMSG_INITIATE_TRADE);
    packet << customer->GetGUID();
    bot->GetSession()->HandleInitiateTradeOpcode(packet);
}

void BotShopAction::FillTrade()
{
    CraftData& data = AI_VALUE(CraftData&, "craft");
    Player* customer = bot->GetTrader();
    if (!data.shopCheckout || !customer || customer->GetGUID() != data.shopCustomer || !bot->GetTradeData())
        return;

    struct MailItem
    {
        Item* item;
        uint32 count;
        uint32 price;
    };

    std::vector<MailItem> mailItems;
    uint32 freeSlots = 0;
    for (uint8 slot = 0; slot < TRADE_SLOT_TRADED_COUNT; ++slot)
        if (!bot->GetTradeData()->GetItem(TradeSlots(slot)))
            ++freeSlots;

    uint32 tradePrice = 0;
    TradeAction trade(botAI);
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

    // a bag item goes in the trade while slots are free, otherwise out of the bags into a cash-on-delivery mail
    auto const deliver = [&](Item* item, uint32 price)
    {
        uint32 const count = item->GetCount();
        if (freeSlots && trade.TradeItem(item, -1) && bot->GetTradeData()->HasItem(item->GetGUID()))
        {
            tradePrice += count * price;
            --freeSlots;
            return true;
        }

        bot->MoveItemFromInventory(item->GetBagSlot(), item->GetSlot(), true);
        item->DeleteFromInventoryDB(trans);
        if (item->GetState() == ITEM_UNCHANGED)  // as WorldSession::HandleSendMail
            item->FSetState(ITEM_CHANGED);

        item->SetOwnerGUID(customer->GetGUID());
        item->SaveToDB(trans);
        mailItems.push_back({item, count, count * price});
        return false;
    };

    for (CraftData::ShopLine const& line : data.shopCart)
    {
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(line.itemId);
        if (!proto)
            continue;

        if (line.crafted)
        {
            uint32 const maxStack = std::max<uint32>(1, proto->GetMaxStackSize());
            for (uint32 left = line.amount; left;)
            {
                uint32 const count = std::min(left, maxStack);
                left -= count;

                // made from nothing into a bag slot of its own, then into the trade at the order price
                uint8 bag = 0;
                uint8 slot = 0;
                ItemPosCountVec dest;
                Item* item = freeSlots && TradeAction::FindFreeSlot(bot, line.itemId, count, bag, slot) &&
                                     bot->CanStoreNewItem(bag, slot, dest, line.itemId, count) == EQUIP_ERR_OK
                                 ? bot->StoreNewItem(dest, line.itemId, true)
                                 : nullptr;
                if (item)
                {
                    if (deliver(item, line.price))
                    {
                        CraftData::OrderPrice& order = data.prices[line.itemId];
                        order.price = line.price;
                        order.count += count;
                    }

                    continue;
                }

                if (Item* mailed = Item::CreateItem(line.itemId, count, customer))
                {
                    mailed->SaveToDB(trans);
                    mailItems.push_back({mailed, count, count * line.price});
                }
            }

            continue;
        }

        // items for sale: the bot's own stacks, the last one split to the amount ordered
        FindItemByIdVisitor visitor(line.itemId);
        IterateItems(&visitor);
        uint32 left = line.amount;
        for (Item* item : visitor.GetResult())
        {
            if (!left)
                break;

            if (item->IsInTrade() || !item->CanBeTraded())
                continue;

            if (item->GetCount() > left)
            {
                uint8 bag = 0;
                uint8 slot = 0;
                if (!TradeAction::FindFreeSlot(bot, line.itemId, left, bag, slot))
                    break;

                bot->SplitItem(item->GetPos(), (uint16(bag) << 8) | slot, left);
                item = bot->GetItemByPos(bag, slot);
                if (!item)
                    break;
            }

            left -= std::min(left, item->GetCount());
            deliver(item, line.price);
        }
    }

    uint32 mailedCount = 0;
    uint32 mailedPrice = 0;
    for (size_t first = 0; first < mailItems.size(); first += MAX_MAIL_ITEMS)
    {
        MailDraft draft(Text("shop_mail_subject", "Your order from %bot", {{"%bot", bot->GetName()}}), "");
        uint32 cod = 0;
        for (size_t i = first; i < std::min<size_t>(mailItems.size(), first + MAX_MAIL_ITEMS); ++i)
        {
            draft.AddItem(mailItems[i].item);
            mailedCount += mailItems[i].count;
            cod += mailItems[i].price;
        }

        mailedPrice += cod;
        draft.AddCOD(cod);
        draft.SendMailTo(trans, MailReceiver(customer), MailSender(bot));
    }

    CharacterDatabase.CommitTransaction(trans);

    if (tradePrice)
        bot->Whisper(Text("shop_trade_total", "Put %money in the trade", {{"%money", chat->formatMoney(tradePrice)}}),
                     LANG_UNIVERSAL, customer);

    if (mailedCount)
        bot->Whisper(Text("shop_mailed", "%count more items come by mail, cash on delivery: %money",
                          {{"%count", std::to_string(mailedCount)}, {"%money", chat->formatMoney(mailedPrice)}}),
                     LANG_UNIVERSAL, customer);

    data.shopCart.clear();
    data.shopCustomer.Clear();
    data.shopCheckout = false;
}

std::vector<BotShopAction::ShopItem> BotShopAction::GetShopItems(Player* customer, uint32 skill)
{
    CraftData& data = AI_VALUE(CraftData&, "craft");
    std::vector<ShopItem> items;
    if (skill != SHOP_FOR_SALE)
    {
        for (CraftableItem const& craftable : SetCraftAction::GetCraftableItems(bot))
            if (craftable.skill == skill && IsShown(customer, craftable.item, data.shopAllLevels))
                items.push_back({craftable.item, craftable.count, craftable.price, 0, GetCategory(craftable)});

        return items;
    }

    // what the bot would sell to a vendor or advertise (SuggestTradeAction), priced as TradeStatusAction::CalculateCost
    ShopStockVisitor visitor;
    IterateItems(&visitor);
    for (auto const& [itemId, count] : visitor.stock)
    {
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
        if (!proto || !proto->SellPrice || proto->Quality < ITEM_QUALITY_NORMAL ||
            !IsShown(customer, proto, data.shopAllLevels))
            continue;

        ItemUsage const usage = AI_VALUE2(ItemUsage, "item usage", itemId);
        bool const advertised = proto->Class == ITEM_CLASS_TRADE_GOODS && proto->Bonding == NO_BIND;
        if (usage != ITEM_USAGE_VENDOR && usage != ITEM_USAGE_AH && !advertised)
            continue;

        uint32 const price = proto->SellPrice * sRandomPlayerbotMgr.GetSellMultiplier(bot);
        if (price)
            items.push_back({proto, 1, price, count, SHOP_CAT_CLASS + proto->Class});
    }

    return items;
}

std::string BotShopAction::CartText(Player* customer)
{
    CraftData& data = AI_VALUE(CraftData&, "craft");
    if (data.shopCart.empty())
        return Text("shop_cart_empty", "%bot's shop - your cart is empty", {{"%bot", bot->GetName()}});

    std::string items;
    uint32 total = 0;
    for (size_t i = 0; i < data.shopCart.size(); ++i)
    {
        CraftData::ShopLine const& line = data.shopCart[i];
        total += line.amount * line.price;
        if (i == 3)
            items += ", ...";

        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(line.itemId);
        if (i < 3 && proto)
            items += Acore::StringFormat("{}{}x {}", i ? ", " : "", line.amount, GetItemName(customer, proto));
    }

    return Text("shop_cart", "Cart: %items - %money", {{"%items", items}, {"%money", chat->formatMoney(total)}});
}

std::string BotShopAction::Text(std::string const& name, std::string const& text,
                                std::map<std::string, std::string> const& placeholders)
{
    return PlayerbotTextMgr::instance().GetBotTextOrDefault(name, text, placeholders);
}

bool BotShopAction::IsShown(Player* customer, ItemTemplate const* proto, bool allLevels)
{
    InventoryResult const result = customer->CanUseItem(proto);
    if (result != EQUIP_ERR_OK && (!allLevels || result != EQUIP_ERR_CANT_EQUIP_LEVEL_I))
        return false;

    // armor and weapon proficiency (CanUseItem leaves it to CanEquipItem)
    if (uint32 const skill = proto->GetSkill(); skill && !customer->GetSkillValue(skill))
        return false;

    return allLevels || !proto->RequiredLevel || proto->RequiredLevel + SHOP_LEVEL_WINDOW >= customer->GetLevel();
}

uint32 BotShopAction::GetCategory(CraftableItem const& craftable)
{
    ItemTemplate const* proto = craftable.item;
    if (craftable.scroll)
    {
        SpellInfo const* spell = craftable.spell;
        if (spell->EquippedItemClass == ITEM_CLASS_WEAPON)
        {
            if (spell->EquippedItemSubClassMask == (1 << ITEM_SUBCLASS_WEAPON_STAFF))
                return SHOP_CAT_SCROLL + SHOP_SCROLL_STAFF;

            return SHOP_CAT_SCROLL + ((spell->EquippedItemSubClassMask & SHOP_ONE_HAND_WEAPONS) ? SHOP_SCROLL_WEAPON
                                                                                                : SHOP_SCROLL_TWO_HAND);
        }

        for (uint32 type = INVTYPE_HEAD; type < MAX_INVTYPE; ++type)
            if (spell->EquippedItemInventoryTypeMask & (1 << type))
                return SHOP_CAT_SCROLL + type;

        return SHOP_CAT_SCROLL + INVTYPE_SHIELD;  // shield enchants name the subclass, not a slot
    }

    switch (proto->Class)
    {
        case ITEM_CLASS_ARMOR:
            return SHOP_CAT_ARMOR + (proto->InventoryType == INVTYPE_ROBE ? INVTYPE_CHEST : proto->InventoryType);
        case ITEM_CLASS_WEAPON:
            return SHOP_CAT_WEAPON + proto->SubClass;
        case ITEM_CLASS_GEM:
            return SHOP_CAT_GEM + proto->SubClass;
        case ITEM_CLASS_CONSUMABLE:
            return SHOP_CAT_CONSUMABLE + proto->SubClass;
        default:
            return SHOP_CAT_CLASS + proto->Class;
    }
}

// ponytail: English category names (the client's); move them to GetBotTextOrDefault if other locales need them
std::string BotShopAction::GetCategoryName(uint32 category)
{
    static char const* const slots[MAX_INVTYPE] = {
        "Other", "Head",     "Neck",   "Shoulder", "Shirt",   "Chest",     "Waist",    "Legs",
        "Feet",  "Wrist",    "Hands",  "Finger",   "Trinket", "One-Hand",  "Shield",   "Ranged",
        "Back",  "Two-Hand", "Bag",    "Tabard",   "Chest",   "Main Hand", "Off Hand", "Held In Off-hand",
        "Ammo",  "Thrown",   "Ranged", "Quiver",   "Relic"};
    static char const* const weapons[MAX_ITEM_SUBCLASS_WEAPON] = {"One-Handed Axes",
                                                                  "Two-Handed Axes",
                                                                  "Bows",
                                                                  "Guns",
                                                                  "One-Handed Maces",
                                                                  "Two-Handed Maces",
                                                                  "Polearms",
                                                                  "One-Handed Swords",
                                                                  "Two-Handed Swords",
                                                                  "Weapons",
                                                                  "Staves",
                                                                  "Exotic",
                                                                  "Exotic",
                                                                  "Fist Weapons",
                                                                  "Miscellaneous",
                                                                  "Daggers",
                                                                  "Thrown",
                                                                  "Spears",
                                                                  "Crossbows",
                                                                  "Wands",
                                                                  "Fishing Poles"};
    static char const* const gems[MAX_ITEM_SUBCLASS_GEM] = {"Red Gems",    "Blue Gems",   "Yellow Gems",
                                                            "Purple Gems", "Green Gems",  "Orange Gems",
                                                            "Meta Gems",   "Simple Gems", "Prismatic Gems"};
    static char const* const consumables[MAX_ITEM_SUBCLASS_CONSUMABLE] = {
        "Consumables",  "Potions",           "Elixirs",  "Flasks", "Scrolls",
        "Food & Drink", "Item Enhancements", "Bandages", "Other"};
    static char const* const classes[MAX_ITEM_CLASS] = {
        "Consumables", "Bags",  "Weapons", "Gems",  "Armor", "Reagents",  "Projectiles",   "Trade Goods", "Generic",
        "Recipes",     "Money", "Quivers", "Quest", "Keys",  "Permanent", "Miscellaneous", "Glyphs"};

    uint32 const sub = category % 100;
    if (category >= SHOP_CAT_SCROLL)
    {
        std::string const target = sub == SHOP_SCROLL_WEAPON     ? "Weapon"
                                   : sub == SHOP_SCROLL_TWO_HAND ? "Two-Hand"
                                   : sub == SHOP_SCROLL_STAFF    ? "Staff"
                                   : sub < MAX_INVTYPE           ? slots[sub]
                                                                 : "Other";
        return "Scrolls - " + target;
    }

    if (category >= SHOP_CAT_CLASS)
        return sub < MAX_ITEM_CLASS ? classes[sub] : "Other";

    if (category >= SHOP_CAT_CONSUMABLE)
        return sub < MAX_ITEM_SUBCLASS_CONSUMABLE ? consumables[sub] : "Other";

    if (category >= SHOP_CAT_GEM)
        return sub < MAX_ITEM_SUBCLASS_GEM ? gems[sub] : "Other";

    if (category >= SHOP_CAT_WEAPON)
        return sub < MAX_ITEM_SUBCLASS_WEAPON ? weapons[sub] : "Other";

    return sub < MAX_INVTYPE ? slots[sub] : "Other";
}

std::string BotShopAction::GetItemName(Player* customer, ItemTemplate const* proto)
{
    std::string name = proto->Name1;
    if (ItemLocale const* locale = sObjectMgr->GetItemLocale(proto->ItemId))
        ObjectMgr::GetLocaleString(locale->Name, customer->GetSession()->GetSessionDbLocaleIndex(), name);

    // common items stay in the default gossip color: white is unreadable on the parchment
    if (proto->Quality < ITEM_QUALITY_UNCOMMON)
        return name;

    return Acore::StringFormat("|c{:08x}{}|r", ItemQualityColors[proto->Quality], name);
}
