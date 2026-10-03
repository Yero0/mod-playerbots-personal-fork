-- Local change: upstream's 2026_09_13 text uses id 1913, which our rpg_gather text had taken.
-- Dated before it so a database that already has ours moves it first; no-op on a new one.
UPDATE `ai_playerbot_texts` SET `id` = 1935 WHERE `id` = 1913 AND `name` = 'rpg_gather_no_profession_error';
