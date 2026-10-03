/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "SetCraftAction.h"
#include "ChatHelper.h"
#include "CraftValue.h"
#include "Event.h"
#include "PlayerbotTextMgr.h"
#include "Playerbots.h"
#include "RandomPlayerbotMgr.h"
#include "TradeAction.h"

#include <algorithm>
#include <set>

// Local change: built once, thread-safe; the old static map was filled lazily from every map thread
SkillLineAbilityEntry const* SetCraftAction::GetSkillLine(uint32 spellId)
{
    static std::map<uint32, SkillLineAbilityEntry const*> const skillSpells = []
    {
        std::map<uint32, SkillLineAbilityEntry const*> spells;
        for (SkillLineAbilityEntry const* skillLine : sSkillLineAbilityStore)
            spells[skillLine->Spell] = skillLine;
        return spells;
    }();

    auto const itr = skillSpells.find(spellId);
    return itr != skillSpells.end() ? itr->second : nullptr;
}

// Local change
std::vector<CraftableItem> SetCraftAction::GetCraftableItems(Player* bot)
{
    static std::set<uint32> const manufacturing = {SKILL_ALCHEMY,         SKILL_BLACKSMITHING, SKILL_ENCHANTING,
                                                   SKILL_ENGINEERING,     SKILL_INSCRIPTION,   SKILL_JEWELCRAFTING,
                                                   SKILL_LEATHERWORKING,  SKILL_TAILORING};

    std::vector<CraftableItem> result;
    for (auto const& [spellId, playerSpell] : bot->GetSpellMap())
    {
        if (playerSpell->State == PLAYERSPELL_REMOVED || !playerSpell->Active)
            continue;

        SkillLineAbilityEntry const* skillLine = GetSkillLine(spellId);
        if (!skillLine || !manufacturing.contains(skillLine->SkillLine))
            continue;

        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!spellInfo)
            continue;

        for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        {
            // an enchant names the scroll it makes on a vellum (Spell::EffectEnchantItemPerm)
            bool const scroll = spellInfo->Effects[i].Effect == SPELL_EFFECT_ENCHANT_ITEM;
            if ((spellInfo->Effects[i].Effect != SPELL_EFFECT_CREATE_ITEM && !scroll) ||
                !spellInfo->Effects[i].ItemType)
                continue;

            // the trade refuses bound items and items without a sell price; scrolls have none but carry the order price
            ItemTemplate const* proto = sObjectMgr->GetItemTemplate(spellInfo->Effects[i].ItemType);
            // a grey item makes TradeStatusAction::CalculateCost price the whole trade at 0
            if (!proto || proto->Bonding == BIND_WHEN_PICKED_UP || (!proto->SellPrice && !scroll) ||
                proto->Quality < ITEM_QUALITY_NORMAL)
                continue;

            uint32 price = GetCraftFee(proto);
            for (uint32 x = 0; x < MAX_SPELL_REAGENTS; ++x)
            {
                if (spellInfo->Reagent[x] <= 0)
                    continue;

                if (ItemTemplate const* reagent = sObjectMgr->GetItemTemplate(spellInfo->Reagent[x]))
                    price += (reagent->BuyPrice ? reagent->BuyPrice : reagent->SellPrice) * spellInfo->ReagentCount[x];
            }

            uint32 const count = scroll ? 1 : std::max<int32>(1, spellInfo->Effects[i].CalcValue(bot));
            result.push_back({spellInfo, proto, skillLine->SkillLine, count, price, scroll});
            break;
        }
    }

    std::sort(result.begin(), result.end(), [](CraftableItem const& a, CraftableItem const& b)
              { return a.skill != b.skill ? a.skill < b.skill : a.item->ItemLevel > b.item->ItemLevel; });
    return result;
}

// Local change: make the item from nothing (the price covers the reagents) and put it in the trade window
bool SetCraftAction::CraftForTrader(std::string const& link)
{
    ItemIds itemIds = chat->parseItems(link);
    if (itemIds.empty())
        return false;

    uint32 const itemId = *itemIds.begin();
    std::vector<CraftableItem> const craftables = GetCraftableItems(bot);
    auto const craftable = std::find_if(craftables.begin(), craftables.end(),
                                        [itemId](CraftableItem const& c) { return c.item->ItemId == itemId; });
    if (craftable == craftables.end())
    {
        botAI->TellMaster(PlayerbotTextMgr::instance().GetBotTextOrDefault(
            "craft_cannot_craft", "I cannot craft this", {}));
        return false;
    }

    // Local change: optional craft count after the link ('craft [item] 20'), one stack of at most max stack size
    size_t const countPos = link.rfind(' ');
    uint32 crafts = countPos != std::string::npos ? std::max(1, atoi(link.substr(countPos + 1).c_str())) : 1;
    crafts = std::min(crafts, std::max<uint32>(1, craftable->item->GetMaxStackSize() / craftable->count));
    uint32 const total = crafts * craftable->count;

    // A free slot of its own: merged into a stack the bot already had, the whole stack would go into the trade
    uint8 bag = 0;
    uint8 slot = 0;
    ItemPosCountVec dest;
    Item* item = TradeAction::FindFreeSlot(bot, itemId, total, bag, slot) &&
                         bot->CanStoreNewItem(bag, slot, dest, itemId, total) == EQUIP_ERR_OK
                     ? bot->StoreNewItem(dest, itemId, true)
                     : nullptr;
    if (!item)
    {
        botAI->TellMaster(PlayerbotTextMgr::instance().GetBotTextOrDefault(
            "craft_bags_full", "My bags are full", {}));
        return false;
    }

    // StoreNewItem can merge into a stack the bot already had; only the new units carry the order price
    CraftData::OrderPrice& order = AI_VALUE(CraftData&, "craft").prices[itemId];
    order.price = (craftable->price + craftable->count - 1) / craftable->count;
    order.count += total;
    if (!TradeAction(botAI).TradeItem(item, -1))
        return false;

    botAI->TellMaster(PlayerbotTextMgr::instance().GetBotTextOrDefault(
        "craft_for_sale", "Crafted %item for %money",
        {{"%item", chat->FormatItem(craftable->item, total)},
         {"%money", chat->formatMoney(order.price * total)}}));  // what CheckTrade charges
    return true;
}

bool SetCraftAction::Execute(Event event)
{
    std::string const link = event.getParam();

    // Local change: a random bot crafts on order for the real player it trades with
    Player* trader = bot->GetTrader();
    if (sPlayerbotAIConfig.randomBotCraftForPlayers && trader && trader == event.getOwner() &&
        IsRealPlayer(trader) && sRandomPlayerbotMgr.IsRandomBot(bot) && link != "reset" && link != "?")
        return CraftForTrader(link);

    Player* master = GetMaster();
    if (!master)
        return false;

    CraftData& data = AI_VALUE(CraftData&, "craft");
    if (link == "reset")
    {
        data.Reset();
        botAI->TellMaster(PlayerbotTextMgr::instance().GetBotTextOrDefault(
            "craft_reset", "I will not craft anything", {}));
        return true;
    }

    if (link == "?")
    {
        TellCraft();
        return true;
    }

    ItemIds itemIds = chat->parseItems(link);
    if (itemIds.empty())
    {
        botAI->TellMaster(PlayerbotTextMgr::instance().GetBotTextOrDefault(
            "craft_usage", "Usage: 'craft [itemId]' or 'craft reset'", {}));
        return false;
    }

    uint32 itemId = *itemIds.begin();
    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
    if (!proto)
        return false;

    data.required.clear();
    data.obtained.clear();

    for (PlayerSpellMap::iterator itr = bot->GetSpellMap().begin(); itr != bot->GetSpellMap().end(); ++itr)
    {
        uint32 spellId = itr->first;

        if (itr->second->State == PLAYERSPELL_REMOVED || !itr->second->Active)
            continue;

        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!spellInfo)
            continue;

        SkillLineAbilityEntry const* skillLine = GetSkillLine(spellId);  // Local change
        if (skillLine != nullptr)
        {
            for (uint8 i = 0; i < 3; ++i)
            {
                if (spellInfo->Effects[i].Effect == SPELL_EFFECT_CREATE_ITEM &&
                    itemId == spellInfo->Effects[i].ItemType)
                {
                    for (uint32 x = 0; x < MAX_SPELL_REAGENTS; ++x)
                    {
                        if (spellInfo->Reagent[x] <= 0)
                            continue;

                        uint32 itemid = spellInfo->Reagent[x];
                        uint32 reagentsRequired = spellInfo->ReagentCount[x];
                        if (itemid)
                        {
                            data.required[itemid] = reagentsRequired;
                            data.obtained[itemid] = 0;
                        }
                    }
                }
            }
        }
    }

    if (data.required.empty())
    {
        botAI->TellMaster(PlayerbotTextMgr::instance().GetBotTextOrDefault(
            "craft_cannot_craft", "I cannot craft this", {}));
        return false;
    }

    data.itemId = itemId;

    TellCraft();
    return true;
}

void SetCraftAction::TellCraft()
{
    CraftData& data = AI_VALUE(CraftData&, "craft");
    if (data.IsEmpty())
    {
        botAI->TellMaster(PlayerbotTextMgr::instance().GetBotTextOrDefault(
            "craft_reset", "I will not craft anything", {}));
        return;
    }

    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(data.itemId);
    if (!proto)
        return;

    std::ostringstream reagentsOut;

    bool first = true;
    for (std::map<uint32, uint32>::iterator i = data.required.begin(); i != data.required.end(); ++i)
    {
        uint32 item = i->first;
        uint32 required = i->second;

        if (ItemTemplate const* reagent = sObjectMgr->GetItemTemplate(item))
        {
            if (first)
                first = false;
            else
                reagentsOut << ", ";

            reagentsOut << chat->FormatItem(reagent, required);

            uint32 given = data.obtained[item];
            if (given)
                reagentsOut << "|cffffff00(x" << given << " given)|r ";
        }
    }

    botAI->TellMaster(PlayerbotTextMgr::instance().GetBotTextOrDefault(
        "craft_summary",
        "I will craft %item using reagents: %reagents (craft fee: %money)",
        {{"%item", chat->FormatItem(proto)},
         {"%reagents", reagentsOut.str()},
         {"%money", chat->formatMoney(GetCraftFee(data))}}));
}

uint32 SetCraftAction::GetCraftFee(CraftData& data)
{
    if (data.IsEmpty())
        return 0;

    return GetCraftFee(sObjectMgr->GetItemTemplate(data.itemId));  // Local change
}

// Local change: split out of GetCraftFee(CraftData&)
uint32 SetCraftAction::GetCraftFee(ItemTemplate const* proto)
{
    if (!proto)
        return 0;

    uint32 level = std::max(proto->ItemLevel, proto->RequiredLevel);
    return level * level / 40;
}
