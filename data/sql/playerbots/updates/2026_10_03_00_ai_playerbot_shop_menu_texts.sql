-- #########################################################
-- Playerbots (Local change) - bot shop menu texts
-- Localized for all WotLK locales (koKR, frFR, deDE, zhCN,
-- zhTW, esES, esMX, ruRU)
-- #########################################################

DELETE FROM ai_playerbot_texts WHERE name IN (
    'shop_cart_empty',
    'shop_cart',
    'shop_for_sale',
    'shop_checkout',
    'shop_clear_cart',
    'shop_nothing',
    'shop_previous_page',
    'shop_next_page',
    'shop_my_levels',
    'shop_all_levels',
    'shop_back',
    'shop_main_menu',
    'shop_how_many_stock',
    'shop_how_many_crafts',
    'shop_cart_full',
    'shop_mail_subject',
    'shop_trade_total',
    'shop_mailed'
);
DELETE FROM ai_playerbot_texts_chance WHERE name IN (
    'shop_cart_empty',
    'shop_cart',
    'shop_for_sale',
    'shop_checkout',
    'shop_clear_cart',
    'shop_nothing',
    'shop_previous_page',
    'shop_next_page',
    'shop_my_levels',
    'shop_all_levels',
    'shop_back',
    'shop_main_menu',
    'shop_how_many_stock',
    'shop_how_many_crafts',
    'shop_cart_full',
    'shop_mail_subject',
    'shop_trade_total',
    'shop_mailed'
);

-- shop_cart_empty
INSERT INTO `ai_playerbot_texts`
    (`id`, `name`, `text`, `say_type`, `reply_type`,
     `text_loc1`, `text_loc2`, `text_loc3`, `text_loc4`,
     `text_loc5`, `text_loc6`, `text_loc7`, `text_loc8`)
VALUES (
    1917,
    'shop_cart_empty',
    '%bot''s shop - your cart is empty',
    0, 0,
    '%bot의 상점 - 장바구니가 비어 있습니다',
    'Boutique de %bot - votre panier est vide',
    'Laden von %bot - dein Warenkorb ist leer',
    '%bot的商店 - 购物车是空的',
    '%bot的商店 - 購物車是空的',
    'Tienda de %bot - tu carrito está vacío',
    'Tienda de %bot - tu carrito está vacío',
    'Лавка %bot - корзина пуста');

INSERT INTO ai_playerbot_texts_chance (name, probability) VALUES ('shop_cart_empty', 100);

-- shop_cart
INSERT INTO `ai_playerbot_texts`
    (`id`, `name`, `text`, `say_type`, `reply_type`,
     `text_loc1`, `text_loc2`, `text_loc3`, `text_loc4`,
     `text_loc5`, `text_loc6`, `text_loc7`, `text_loc8`)
VALUES (
    1918,
    'shop_cart',
    'Cart: %items - %money',
    0, 0,
    '장바구니: %items - %money',
    'Panier : %items - %money',
    'Warenkorb: %items - %money',
    '购物车：%items - %money',
    '購物車：%items - %money',
    'Carrito: %items - %money',
    'Carrito: %items - %money',
    'Корзина: %items - %money');

INSERT INTO ai_playerbot_texts_chance (name, probability) VALUES ('shop_cart', 100);

-- shop_for_sale
INSERT INTO `ai_playerbot_texts`
    (`id`, `name`, `text`, `say_type`, `reply_type`,
     `text_loc1`, `text_loc2`, `text_loc3`, `text_loc4`,
     `text_loc5`, `text_loc6`, `text_loc7`, `text_loc8`)
VALUES (
    1919,
    'shop_for_sale',
    'Items for sale',
    0, 0,
    '판매 물품',
    'Objets en vente',
    'Waren zum Verkauf',
    '出售的物品',
    '出售的物品',
    'Objetos a la venta',
    'Objetos a la venta',
    'Товары на продажу');

INSERT INTO ai_playerbot_texts_chance (name, probability) VALUES ('shop_for_sale', 100);

-- shop_checkout
INSERT INTO `ai_playerbot_texts`
    (`id`, `name`, `text`, `say_type`, `reply_type`,
     `text_loc1`, `text_loc2`, `text_loc3`, `text_loc4`,
     `text_loc5`, `text_loc6`, `text_loc7`, `text_loc8`)
VALUES (
    1920,
    'shop_checkout',
    'Checkout',
    0, 0,
    '결제',
    'Payer',
    'Zur Kasse',
    '结账',
    '結帳',
    'Pagar',
    'Pagar',
    'Оформить заказ');

INSERT INTO ai_playerbot_texts_chance (name, probability) VALUES ('shop_checkout', 100);

-- shop_clear_cart
INSERT INTO `ai_playerbot_texts`
    (`id`, `name`, `text`, `say_type`, `reply_type`,
     `text_loc1`, `text_loc2`, `text_loc3`, `text_loc4`,
     `text_loc5`, `text_loc6`, `text_loc7`, `text_loc8`)
VALUES (
    1921,
    'shop_clear_cart',
    'Clear cart',
    0, 0,
    '장바구니 비우기',
    'Vider le panier',
    'Warenkorb leeren',
    '清空购物车',
    '清空購物車',
    'Vaciar carrito',
    'Vaciar carrito',
    'Очистить корзину');

INSERT INTO ai_playerbot_texts_chance (name, probability) VALUES ('shop_clear_cart', 100);

-- shop_nothing
INSERT INTO `ai_playerbot_texts`
    (`id`, `name`, `text`, `say_type`, `reply_type`,
     `text_loc1`, `text_loc2`, `text_loc3`, `text_loc4`,
     `text_loc5`, `text_loc6`, `text_loc7`, `text_loc8`)
VALUES (
    1922,
    'shop_nothing',
    'Nothing for you here',
    0, 0,
    '여기에는 당신을 위한 것이 없습니다',
    'Rien pour vous ici',
    'Hier ist nichts für dich',
    '这里没有适合你的东西',
    '這裡沒有適合你的東西',
    'Aquí no hay nada para ti',
    'Aquí no hay nada para ti',
    'Здесь для вас ничего нет');

INSERT INTO ai_playerbot_texts_chance (name, probability) VALUES ('shop_nothing', 100);

-- shop_previous_page
INSERT INTO `ai_playerbot_texts`
    (`id`, `name`, `text`, `say_type`, `reply_type`,
     `text_loc1`, `text_loc2`, `text_loc3`, `text_loc4`,
     `text_loc5`, `text_loc6`, `text_loc7`, `text_loc8`)
VALUES (
    1923,
    'shop_previous_page',
    'Previous page',
    0, 0,
    '이전 페이지',
    'Page précédente',
    'Vorherige Seite',
    '上一页',
    '上一頁',
    'Página anterior',
    'Página anterior',
    'Предыдущая страница');

INSERT INTO ai_playerbot_texts_chance (name, probability) VALUES ('shop_previous_page', 100);

-- shop_next_page
INSERT INTO `ai_playerbot_texts`
    (`id`, `name`, `text`, `say_type`, `reply_type`,
     `text_loc1`, `text_loc2`, `text_loc3`, `text_loc4`,
     `text_loc5`, `text_loc6`, `text_loc7`, `text_loc8`)
VALUES (
    1924,
    'shop_next_page',
    'Next page',
    0, 0,
    '다음 페이지',
    'Page suivante',
    'Nächste Seite',
    '下一页',
    '下一頁',
    'Página siguiente',
    'Página siguiente',
    'Следующая страница');

INSERT INTO ai_playerbot_texts_chance (name, probability) VALUES ('shop_next_page', 100);

-- shop_my_levels
INSERT INTO `ai_playerbot_texts`
    (`id`, `name`, `text`, `say_type`, `reply_type`,
     `text_loc1`, `text_loc2`, `text_loc3`, `text_loc4`,
     `text_loc5`, `text_loc6`, `text_loc7`, `text_loc8`)
VALUES (
    1925,
    'shop_my_levels',
    'Show items for my level',
    0, 0,
    '내 레벨에 맞는 물품 보기',
    'Objets de mon niveau',
    'Waren für meine Stufe zeigen',
    '显示适合我等级的物品',
    '顯示適合我等級的物品',
    'Mostrar objetos de mi nivel',
    'Mostrar objetos de mi nivel',
    'Показать товары моего уровня');

INSERT INTO ai_playerbot_texts_chance (name, probability) VALUES ('shop_my_levels', 100);

-- shop_all_levels
INSERT INTO `ai_playerbot_texts`
    (`id`, `name`, `text`, `say_type`, `reply_type`,
     `text_loc1`, `text_loc2`, `text_loc3`, `text_loc4`,
     `text_loc5`, `text_loc6`, `text_loc7`, `text_loc8`)
VALUES (
    1926,
    'shop_all_levels',
    'Show all levels',
    0, 0,
    '모든 레벨 보기',
    'Tous les niveaux',
    'Alle Stufen zeigen',
    '显示所有等级',
    '顯示所有等級',
    'Mostrar todos los niveles',
    'Mostrar todos los niveles',
    'Показать все уровни');

INSERT INTO ai_playerbot_texts_chance (name, probability) VALUES ('shop_all_levels', 100);

-- shop_back
INSERT INTO `ai_playerbot_texts`
    (`id`, `name`, `text`, `say_type`, `reply_type`,
     `text_loc1`, `text_loc2`, `text_loc3`, `text_loc4`,
     `text_loc5`, `text_loc6`, `text_loc7`, `text_loc8`)
VALUES (
    1927,
    'shop_back',
    'Back',
    0, 0,
    '뒤로',
    'Retour',
    'Zurück',
    '返回',
    '返回',
    'Volver',
    'Volver',
    'Назад');

INSERT INTO ai_playerbot_texts_chance (name, probability) VALUES ('shop_back', 100);

-- shop_main_menu
INSERT INTO `ai_playerbot_texts`
    (`id`, `name`, `text`, `say_type`, `reply_type`,
     `text_loc1`, `text_loc2`, `text_loc3`, `text_loc4`,
     `text_loc5`, `text_loc6`, `text_loc7`, `text_loc8`)
VALUES (
    1928,
    'shop_main_menu',
    'Main menu',
    0, 0,
    '메인 메뉴',
    'Menu principal',
    'Hauptmenü',
    '主菜单',
    '主選單',
    'Menú principal',
    'Menú principal',
    'Главное меню');

INSERT INTO ai_playerbot_texts_chance (name, probability) VALUES ('shop_main_menu', 100);

-- shop_how_many_stock
INSERT INTO `ai_playerbot_texts`
    (`id`, `name`, `text`, `say_type`, `reply_type`,
     `text_loc1`, `text_loc2`, `text_loc3`, `text_loc4`,
     `text_loc5`, `text_loc6`, `text_loc7`, `text_loc8`)
VALUES (
    1929,
    'shop_how_many_stock',
    'How many? %count in stock',
    0, 0,
    '몇 개? 재고 %count개',
    'Combien ? %count en stock',
    'Wie viele? %count auf Lager',
    '要多少？库存 %count',
    '要多少？庫存 %count',
    '¿Cuántos? %count en existencia',
    '¿Cuántos? %count en existencia',
    'Сколько? В наличии: %count');

INSERT INTO ai_playerbot_texts_chance (name, probability) VALUES ('shop_how_many_stock', 100);

-- shop_how_many_crafts
INSERT INTO `ai_playerbot_texts`
    (`id`, `name`, `text`, `say_type`, `reply_type`,
     `text_loc1`, `text_loc2`, `text_loc3`, `text_loc4`,
     `text_loc5`, `text_loc6`, `text_loc7`, `text_loc8`)
VALUES (
    1930,
    'shop_how_many_crafts',
    'How many crafts? One craft makes %count',
    0, 0,
    '몇 번 제작할까요? 1회 제작 시 %count개',
    'Combien de fabrications ? Une fabrication donne %count',
    'Wie oft herstellen? Einmal ergibt %count',
    '制作几次？每次制作 %count 个',
    '製作幾次？每次製作 %count 個',
    '¿Cuántas fabricaciones? Una da %count',
    '¿Cuántas fabricaciones? Una da %count',
    'Сколько раз изготовить? За раз выходит %count');

INSERT INTO ai_playerbot_texts_chance (name, probability) VALUES ('shop_how_many_crafts', 100);

-- shop_cart_full
INSERT INTO `ai_playerbot_texts`
    (`id`, `name`, `text`, `say_type`, `reply_type`,
     `text_loc1`, `text_loc2`, `text_loc3`, `text_loc4`,
     `text_loc5`, `text_loc6`, `text_loc7`, `text_loc8`)
VALUES (
    1931,
    'shop_cart_full',
    'Your cart is full',
    0, 0,
    '장바구니가 가득 찼습니다',
    'Votre panier est plein',
    'Dein Warenkorb ist voll',
    '你的购物车已满',
    '你的購物車已滿',
    'Tu carrito está lleno',
    'Tu carrito está lleno',
    'Ваша корзина заполнена');

INSERT INTO ai_playerbot_texts_chance (name, probability) VALUES ('shop_cart_full', 100);

-- shop_mail_subject
INSERT INTO `ai_playerbot_texts`
    (`id`, `name`, `text`, `say_type`, `reply_type`,
     `text_loc1`, `text_loc2`, `text_loc3`, `text_loc4`,
     `text_loc5`, `text_loc6`, `text_loc7`, `text_loc8`)
VALUES (
    1932,
    'shop_mail_subject',
    'Your order from %bot',
    0, 0,
    '%bot의 주문품',
    'Votre commande de %bot',
    'Deine Bestellung von %bot',
    '来自%bot的订单',
    '來自%bot的訂單',
    'Tu pedido de %bot',
    'Tu pedido de %bot',
    'Ваш заказ от %bot');

INSERT INTO ai_playerbot_texts_chance (name, probability) VALUES ('shop_mail_subject', 100);

-- shop_trade_total
INSERT INTO `ai_playerbot_texts`
    (`id`, `name`, `text`, `say_type`, `reply_type`,
     `text_loc1`, `text_loc2`, `text_loc3`, `text_loc4`,
     `text_loc5`, `text_loc6`, `text_loc7`, `text_loc8`)
VALUES (
    1933,
    'shop_trade_total',
    'Put %money in the trade',
    0, 0,
    '거래 창에 %money을(를) 넣어 주세요',
    'Mettez %money dans l''échange',
    'Lege %money in den Handel',
    '请在交易中放入 %money',
    '請在交易中放入 %money',
    'Pon %money en el comercio',
    'Pon %money en el comercio',
    'Положите %money в обмен');

INSERT INTO ai_playerbot_texts_chance (name, probability) VALUES ('shop_trade_total', 100);

-- shop_mailed
INSERT INTO `ai_playerbot_texts`
    (`id`, `name`, `text`, `say_type`, `reply_type`,
     `text_loc1`, `text_loc2`, `text_loc3`, `text_loc4`,
     `text_loc5`, `text_loc6`, `text_loc7`, `text_loc8`)
VALUES (
    1934,
    'shop_mailed',
    '%count more items come by mail, cash on delivery: %money',
    0, 0,
    '나머지 %count개는 우편(대금 상환)으로 보냅니다: %money',
    '%count autres objets arrivent par courrier, contre remboursement : %money',
    '%count weitere Gegenstände kommen per Post, per Nachnahme: %money',
    '另外 %count 件物品将通过邮件寄出，货到付款：%money',
    '另外 %count 件物品將透過郵件寄出，貨到付款：%money',
    '%count objetos más llegan por correo, contra reembolso: %money',
    '%count objetos más llegan por correo, contra reembolso: %money',
    'Ещё %count шт. придут почтой, наложенным платежом: %money');

INSERT INTO ai_playerbot_texts_chance (name, probability) VALUES ('shop_mailed', 100);
