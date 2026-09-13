-- ============================================================
-- 博客表
-- ============================================================

CREATE TABLE blogs (
    id           SERIAL  PRIMARY KEY,
    title        TEXT    NOT NULL,
    description  TEXT    NOT NULL,
    update_time  DATE    NOT NULL DEFAULT CURRENT_DATE,
    content      TEXT    NOT NULL,
    file_path    TEXT    NOT NULL UNIQUE
);

COMMENT ON TABLE  blogs IS '博客表';
COMMENT ON COLUMN blogs.id           IS '主键，自增';
COMMENT ON COLUMN blogs.title        IS '博客标题';
COMMENT ON COLUMN blogs.description  IS '描述';
COMMENT ON COLUMN blogs.update_time  IS '更新时间（精确到日）';
COMMENT ON COLUMN blogs.content      IS '博客正文';
COMMENT ON COLUMN blogs.file_path    IS '关联文件相对路径（$BLOG_PATH/file_path）';





-- ============================================================
-- 博客分类表
-- ============================================================

CREATE TABLE blog_categories (
    id   SERIAL PRIMARY KEY,
    name TEXT   NOT NULL UNIQUE
);

COMMENT ON TABLE  blog_categories IS '博客分类表';
COMMENT ON COLUMN blog_categories.id   IS '主键，自增';
COMMENT ON COLUMN blog_categories.name IS '分类名';

-- ============================================================
-- 博客-分类关联表（多对多）
-- ============================================================

CREATE TABLE blog_categories_relations (
    blog_id     INT NOT NULL REFERENCES blogs(id)           ON DELETE CASCADE,
    category_id INT NOT NULL REFERENCES blog_categories(id) ON DELETE CASCADE,
    PRIMARY KEY (blog_id, category_id)
);

COMMENT ON TABLE  blog_categories_relations IS '博客-分类关联表';
COMMENT ON COLUMN blog_categories_relations.blog_id     IS '关联 blogs.id';
COMMENT ON COLUMN blog_categories_relations.category_id IS '关联 blog_categories.id';

-- 按分类反查所属博客
CREATE INDEX idx_blog_categories_relations_category
    ON blog_categories_relations (category_id);





-- ============================================================
-- 博客标签表
-- ============================================================

CREATE TABLE blog_tags (
    id   SERIAL PRIMARY KEY,
    name TEXT   NOT NULL UNIQUE
);

COMMENT ON TABLE  blog_tags IS '博客标签表';
COMMENT ON COLUMN blog_tags.id   IS '主键，自增';
COMMENT ON COLUMN blog_tags.name IS '标签名';

-- ============================================================
-- 博客-标签关联表（多对多）
-- ============================================================

CREATE TABLE blog_tag_relations (
    blog_id INT NOT NULL REFERENCES blogs(id)     ON DELETE CASCADE,
    tag_id  INT NOT NULL REFERENCES blog_tags(id) ON DELETE CASCADE,
    PRIMARY KEY (blog_id, tag_id)
);

COMMENT ON TABLE  blog_tag_relations IS '博客-标签关联表';
COMMENT ON COLUMN blog_tag_relations.blog_id IS '关联 blogs.id';
COMMENT ON COLUMN blog_tag_relations.tag_id  IS '关联 blog_tags.id';

-- 按标签反查所属博客
CREATE INDEX idx_blog_tag_relations_tag
    ON blog_tag_relations (tag_id);
