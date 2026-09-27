/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "GatherNodeMgr.h"

#include <algorithm>

#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "GameObject.h"
#include "GridDefines.h"
#include "GridTerrainData.h"
#include "Log.h"
#include "Map.h"
#include "MapMgr.h"
#include "Object.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "Random.h"
#include "SharedDefines.h"
#include "StringFormat.h"
#include "Timer.h"
#include "World.h"

namespace
{
bool GetGatherSkillFromTemplate(GameObjectTemplate const* goInfo, uint32& skillId, uint32& reqSkillValue)
{
    // Local change: fishing pools; Load sets their required skill from the zone
    if (goInfo && goInfo->type == GAMEOBJECT_TYPE_FISHINGHOLE)
    {
        skillId = SKILL_FISHING;
        reqSkillValue = 1;
        return true;
    }

    if (!goInfo || goInfo->type != GAMEOBJECT_TYPE_CHEST)
        return false;

    uint32 lockId = goInfo->GetLockId();
    if (!lockId)
        return false;

    LockEntry const* lockInfo = sLockStore.LookupEntry(lockId);
    if (!lockInfo)
        return false;

    for (uint8 j = 0; j < MAX_LOCK_CASE; ++j)  // Local change
    {
        if (lockInfo->Type[j] != LOCK_KEY_SKILL)
            continue;

        uint32 skill = SkillByLockType(LockType(lockInfo->Index[j]));
        if (skill == SKILL_HERBALISM || skill == SKILL_MINING)
        {
            skillId = skill;
            reqSkillValue = std::max(1u, lockInfo->Skill[j]);
            return true;
        }
    }

    return false;
}
}

void GatherNodeMgr::Load()
{
    uint32 oldMSTime = getMSTime();
    uint32 count = 0;

    // Loot ids with at least one freely lootable row. Quest-gated
    // "gathering" chests (Cactus Apple, Serpentbloom, ...) carry a
    // herbalism/mining lock but ALL their loot rows are QuestRequired, so
    // bots without the quest can't harvest them - and they never despawn,
    // which makes them permanent live-node bait. Note that filtering on
    // gameobject_questitem instead would be wrong: standard herbs carry a
    // conditional quest item (Root Sample) on top of their normal loot.
    std::unordered_set<uint32> openLootIds;
    if (QueryResult result =
            WorldDatabase.Query("SELECT DISTINCT Entry FROM gameobject_loot_template WHERE QuestRequired = 0"))
    {
        do
        {
            openLootIds.insert((*result)[0].Get<uint32>());
        } while (result->NextRow());
    }

    std::map<std::tuple<uint32, uint32, uint32>, std::vector<GatherNodeSpawn>> noZoneByGrid;  // Local change
    std::unordered_map<ObjectGuid::LowType, uint32> dbZoneIds;
    if (QueryResult result = WorldDatabase.Query("SELECT guid, zoneId FROM gameobject WHERE zoneId <> 0"))
    {
        do
        {
            dbZoneIds[(*result)[0].Get<uint32>()] = (*result)[1].Get<uint32>();
        } while (result->NextRow());
    }

    for (auto const& [spawnId, goData] : sObjectMgr->GetAllGOData())
    {
        // Instances are excluded: the state targets outdoor zone roaming.
        MapEntry const* mapEntry = sMapStore.LookupEntry(goData.mapid);
        if (!mapEntry || !mapEntry->IsContinent())
            continue;

        GameObjectTemplate const* goInfo = sObjectMgr->GetGameObjectTemplate(goData.id);

        uint32 skillId = 0;
        uint32 reqSkillValue = 0;
        if (!GetGatherSkillFromTemplate(goInfo, skillId, reqSkillValue))
            continue;

        if (!openLootIds.contains(goInfo->GetLootId()))
            continue;

        GatherNodeSpawn node;
        node.spawnId = spawnId;
        node.pos = WorldPosition(goData.mapid, goData.posX, goData.posY, goData.posZ);
        node.skillId = skillId;
        node.reqSkillValue = reqSkillValue;
        // Local change: spawns without a DB zoneId get it from their grid's terrain file below.
        // sMapMgr->GetZoneId would create the grid for good (terrain, vmap and mmap tiles), the startup memory
        // growth fixed in TravelMgr::PrepareDestinationCache.
        auto zoneItr = dbZoneIds.find(spawnId);
        if (zoneItr == dbZoneIds.end())
        {
            GridCoord const grid = Acore::ComputeGridCoord(goData.posX, goData.posY);
            noZoneByGrid[{goData.mapid, grid.x_coord, grid.y_coord}].push_back(std::move(node));
            continue;
        }

        AddNode(std::move(node), zoneItr->second);
        ++count;
    }

    // Local change: one terrain file load per grid, freed again
    for (auto& [gridKey, nodes] : noZoneByGrid)
    {
        auto const& [mapId, gridX, gridY] = gridKey;
        GridTerrainData terrain;
        if (terrain.Load(Acore::StringFormat("{}maps/{:03}{:02}{:02}.map", sWorld->GetDataPath(), mapId, gridX,
                                             gridY)) != TerrainMapDataReadResult::Success)
            continue;

        for (GatherNodeSpawn& node : nodes)
        {
            AreaTableEntry const* area =
                sAreaTableStore.LookupEntry(terrain.getArea(node.pos.GetPositionX(), node.pos.GetPositionY()));
            if (!area)
                continue;

            AddNode(std::move(node), area->zone ? area->zone : area->ID);
            ++count;
        }
    }

    LOG_INFO("playerbots", ">> Loaded {} gather node spawns in {} ms", count, GetMSTimeDiffToNow(oldMSTime));
}

// Local change: fishing pools need the zone's fishing skill (the value the core checks when fishing)
void GatherNodeMgr::AddNode(GatherNodeSpawn&& node, uint32 zoneId)
{
    if (node.skillId == SKILL_FISHING)
        node.reqSkillValue = std::max<int32>(1, sObjectMgr->GetFishingBaseSkillLevel(zoneId));

    _nodes[node.pos.GetMapId()][zoneId].push_back(std::move(node));
}

bool GatherNodeMgr::IsUsable(PlayerbotAI* botAI, Player* bot, GatherNodeSpawn const& node, bool fishing)
{
    if ((node.skillId == SKILL_FISHING) != fishing)  // Local change
        return false;

    if (!botAI->HasSkill(SkillType(node.skillId)))
        return false;

    return bot->GetSkillValue(node.skillId) >= node.reqSkillValue;
}

std::vector<GatherNodeSpawn> const* GatherNodeMgr::GetZoneNodes(uint32 mapId, uint32 zoneId) const
{
    auto mapItr = _nodes.find(mapId);
    if (mapItr == _nodes.end())
        return nullptr;

    auto zoneItr = mapItr->second.find(zoneId);
    return zoneItr != mapItr->second.end() ? &zoneItr->second : nullptr;
}

bool GatherNodeMgr::HasUsableNodes(Player* bot, bool fishing)  // Local change: fishing
{
    std::vector<GatherNodeSpawn> const* nodes = GetZoneNodes(bot->GetMapId(), bot->GetZoneId());
    if (!nodes)
        return false;

    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    if (!botAI)
        return false;

    for (GatherNodeSpawn const& node : *nodes)
        if (IsUsable(botAI, bot, node, fishing))  // Local change
            return true;

    return false;
}

GatherNodeSpawn const* GatherNodeMgr::GetNextNode(Player* bot, std::unordered_set<ObjectGuid::LowType> const& visited,
                                                  bool fishing)  // Local change: fishing
{
    std::vector<GatherNodeSpawn> const* nodes = GetZoneNodes(bot->GetMapId(), bot->GetZoneId());
    if (!nodes)
        return nullptr;

    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    if (!botAI)
        return nullptr;

    Map* map = bot->GetMap();
    WorldPosition botPos(bot);

    std::vector<std::pair<float, GatherNodeSpawn const*>> candidates;
    for (GatherNodeSpawn const& node : *nodes)
    {
        if (visited.contains(node.spawnId) || !IsUsable(botAI, bot, node, fishing))  // Local change
            continue;

        // Don't route to spawn points we can already see to be empty.
        // Evaluated fresh on every selection so respawns come back into
        // the pool naturally.
        if (IsVerifiablyDown(map, node.pos, node.spawnId))
            continue;

        candidates.push_back({botPos.sqDistance(node.pos), &node});
    }

    if (candidates.empty())
        return nullptr;

    uint32 nearestCount = std::min<uint32>(4, candidates.size());
    std::partial_sort(candidates.begin(), candidates.begin() + nearestCount, candidates.end(),
                      [](auto const& a, auto const& b) { return a.first < b.first; });

    return candidates[urand(0, nearestCount - 1)].second;
}

GameObject* GatherNodeMgr::FindLiveNode(Map* map, ObjectGuid::LowType spawnId)
{
    auto bounds = map->GetGameObjectBySpawnIdStore().equal_range(spawnId);
    for (auto it = bounds.first; it != bounds.second; ++it)
        if (it->second->isSpawned() && it->second->GetGoState() == GO_STATE_READY)
            return it->second;

    return nullptr;
}

bool GatherNodeMgr::IsVerifiablyDown(Map* map, WorldPosition const& pos, ObjectGuid::LowType spawnId)
{
    // An unloaded grid means "unknown", not "not up".
    if (!map->IsGridLoaded(pos.GetPositionX(), pos.GetPositionY()))
        return false;

    return !FindLiveNode(map, spawnId);
}

GatherNodeSpawn const* GatherNodeMgr::GetNearestLiveNode(Player* bot,
                                                         std::unordered_set<ObjectGuid::LowType> const& visited,
                                                         float radius, bool fishing)  // Local change: fishing
{
    auto mapItr = _nodes.find(bot->GetMapId());
    if (mapItr == _nodes.end())
        return nullptr;

    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    if (!botAI)
        return nullptr;

    Map* map = bot->GetMap();
    WorldPosition botPos(bot);
    GatherNodeSpawn const* nearest = nullptr;
    float nearestDistSq = radius * radius;
    // All zone buckets of the map: this query is deliberately not
    // zone-scoped, and a radius this small prunes nearly everything on the
    // cheap distance check.
    for (auto const& [zoneId, nodes] : mapItr->second)
    {
        for (GatherNodeSpawn const& node : nodes)
        {
            float distSq = botPos.sqDistance(node.pos);
            if (distSq > nearestDistSq)
                continue;

            if (visited.contains(node.spawnId) || !IsUsable(botAI, bot, node, fishing))  // Local change
                continue;

            if (!map->IsGridLoaded(node.pos.GetPositionX(), node.pos.GetPositionY()))
                continue;

            if (!FindLiveNode(map, node.spawnId))
                continue;

            nearest = &node;
            nearestDistSq = distSq;
        }
    }

    return nearest;
}
