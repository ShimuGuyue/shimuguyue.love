-- ============================================================
-- 友情链接表
-- ============================================================
-- id:          自增主键
-- name:        站点名称（同时约定为友链头像文件名，存放于FILE_PATH/image/friend_avatars/<name>.<ext>）
-- url:         站点链接
-- description: 站点描述
-- ============================================================

CREATE TABLE friends (
    id          SERIAL  PRIMARY KEY,
    name        TEXT    NOT NULL UNIQUE,
    url         TEXT    NOT NULL,
    description TEXT    NOT NULL DEFAULT ''
);

COMMENT ON TABLE  friends IS '友情链接表';
COMMENT ON COLUMN friends.id          IS '自增主键';
COMMENT ON COLUMN friends.name        IS '站点名称（同时约定为图片文件名）';
COMMENT ON COLUMN friends.url         IS '站点链接';
COMMENT ON COLUMN friends.description IS '站点描述';
