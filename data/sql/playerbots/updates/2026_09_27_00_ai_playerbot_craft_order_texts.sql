-- #########################################################
-- Playerbots (Local change) - craft on order texts
-- Localized for all WotLK locales (koKR, frFR, deDE, zhCN,
-- zhTW, esES, esMX, ruRU)
-- #########################################################

DELETE FROM ai_playerbot_texts WHERE name IN (
    'craft_bags_full',
    'craft_for_sale',
    'craft_trade_header'
);
DELETE FROM ai_playerbot_texts_chance WHERE name IN (
    'craft_bags_full',
    'craft_for_sale',
    'craft_trade_header'
);

-- craft_bags_full
INSERT INTO `ai_playerbot_texts`
    (`id`, `name`, `text`, `say_type`, `reply_type`,
     `text_loc1`, `text_loc2`, `text_loc3`, `text_loc4`,
     `text_loc5`, `text_loc6`, `text_loc7`, `text_loc8`)
VALUES (
    1914,
    'craft_bags_full',
    'My bags are full',
    0, 0,
    '가방이 가득 찼습니다',
    'Mes sacs sont pleins',
    'Meine Taschen sind voll',
    '我的背包满了',
    '我的背包滿了',
    'Mis bolsas están llenas',
    'Mis bolsas están llenas',
    'Мои сумки полны');

INSERT INTO ai_playerbot_texts_chance (name, probability) VALUES ('craft_bags_full', 100);

-- craft_for_sale
INSERT INTO `ai_playerbot_texts`
    (`id`, `name`, `text`, `say_type`, `reply_type`,
     `text_loc1`, `text_loc2`, `text_loc3`, `text_loc4`,
     `text_loc5`, `text_loc6`, `text_loc7`, `text_loc8`)
VALUES (
    1915,
    'craft_for_sale',
    'Crafted %item for %money',
    0, 0,
    '%item을(를) %money에 제작했습니다',
    'J''ai fabriqué %item pour %money',
    'Habe %item für %money hergestellt',
    '已制作 %item，价格 %money',
    '已製作 %item，價格 %money',
    'He fabricado %item por %money',
    'He fabricado %item por %money',
    'Изготовлено: %item за %money');

INSERT INTO ai_playerbot_texts_chance (name, probability) VALUES ('craft_for_sale', 100);

-- craft_trade_header
INSERT INTO `ai_playerbot_texts`
    (`id`, `name`, `text`, `say_type`, `reply_type`,
     `text_loc1`, `text_loc2`, `text_loc3`, `text_loc4`,
     `text_loc5`, `text_loc6`, `text_loc7`, `text_loc8`)
VALUES (
    1916,
    'craft_trade_header',
    '=== Crafting (whisper ''craft [item]'') ===',
    0, 0,
    '=== 제작 (귓속말: ''craft [item]'') ===',
    '=== Artisanat (chuchotez ''craft [item]'') ===',
    '=== Herstellung (flüstere ''craft [item]'') ===',
    '=== 制作（密语 ''craft [item]''）===',
    '=== 製作（密語 ''craft [item]''）===',
    '=== Fabricación (susurra ''craft [item]'') ===',
    '=== Fabricación (susurra ''craft [item]'') ===',
    '=== Изготовление (шепните ''craft [item]'') ===');

INSERT INTO ai_playerbot_texts_chance (name, probability) VALUES ('craft_trade_header', 100);
