-- ============================================================
-- 友情链接表
-- ============================================================
-- id:          自增主键
-- name:        站点名称
-- url:         站点链接（唯一，友链条目以站点链接区分）
-- description: 站点描述
-- 头像文件约定：存放于 FILE_PATH/image/friend_avatars/<id>.<ext>，文件名与友链 id 同名。
-- ============================================================

CREATE TABLE friends (
    id          SERIAL  PRIMARY KEY,
    name        TEXT    NOT NULL,
    url         TEXT    NOT NULL UNIQUE,
    description TEXT    NOT NULL DEFAULT ''
);

COMMENT ON TABLE  friends IS '友情链接表';
COMMENT ON COLUMN friends.id          IS '自增主键';
COMMENT ON COLUMN friends.name        IS '站点名称';
COMMENT ON COLUMN friends.url         IS '站点链接（唯一，友链条目以站点链接区分）';
COMMENT ON COLUMN friends.description IS '站点描述';
