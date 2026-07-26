-- ============================================================
-- about 表：《关于我》页面 README 内容（单行，类似 profile）
-- ============================================================
-- id:      固定为 1
-- content: README.md 的 Markdown 原文
-- ============================================================

CREATE TABLE about (
    id      SERIAL PRIMARY KEY,
    content TEXT NOT NULL DEFAULT ''
);

COMMENT ON TABLE  about IS '《关于我》页面 README 内容';
COMMENT ON COLUMN about.id      IS '固定为 1';
COMMENT ON COLUMN about.content IS 'README.md Markdown 原文';

INSERT INTO about (id, content) VALUES (1, '');
