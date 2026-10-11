-- Local change: upstream's 2026_10_05_00 gameobject texts use ids 1914-1927, which our craft order and shop menu
-- texts had taken (1914-1934; rpg_gather_no_profession_error 1935). Move ours to 30000+, away from upstream's range.
-- Runs before 2026_10_05_00 on both fresh and existing databases; matched by id and name, so it is idempotent.
UPDATE `ai_playerbot_texts` SET `id` = `id` - 1914 + 30000
WHERE `id` BETWEEN 1914 AND 1935 AND `name` IN (
    'craft_bags_full', 'craft_for_sale', 'craft_trade_header',
    'shop_cart_empty', 'shop_cart', 'shop_for_sale', 'shop_checkout', 'shop_clear_cart', 'shop_nothing',
    'shop_previous_page', 'shop_next_page', 'shop_my_levels', 'shop_all_levels', 'shop_back', 'shop_main_menu',
    'shop_how_many_stock', 'shop_how_many_crafts', 'shop_cart_full', 'shop_mail_subject', 'shop_trade_total',
    'shop_mailed', 'rpg_gather_no_profession_error'
);
