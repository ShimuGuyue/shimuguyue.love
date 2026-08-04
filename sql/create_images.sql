-- ============================================================
-- 照片墙图片表
-- ============================================================
-- id:          自增主键
-- path:        图片相对路径（相对于 FILE_PATH/image，如 "home/photo.jpg"）
-- description: 图片简介
-- scale:       缩放比例（默认 1.0）
-- rotation:    旋转角度（度，默认 0.0）
-- pos_x:       水平放置坐标（百分比，0-100，默认 50）
-- pos_y:       垂直放置坐标（百分比，0-100，默认 50）
-- ============================================================

CREATE TABLE images (
    id          SERIAL  PRIMARY KEY,
    path        TEXT    NOT NULL UNIQUE,
    description TEXT    NOT NULL DEFAULT '',
    scale       REAL    NOT NULL DEFAULT 1.0,
    rotation    REAL    NOT NULL DEFAULT 0.0,
    pos_x       REAL    NOT NULL DEFAULT 50.0,
    pos_y       REAL    NOT NULL DEFAULT 50.0,
    z           INTEGER NOT NULL DEFAULT 0
);

COMMENT ON TABLE  images IS '照片墙图片表';
COMMENT ON COLUMN images.id          IS '自增主键';
COMMENT ON COLUMN images.path        IS '图片相对路径';
COMMENT ON COLUMN images.description IS '图片简介';
COMMENT ON COLUMN images.scale       IS '缩放比例';
COMMENT ON COLUMN images.rotation    IS '旋转角度（度）';
COMMENT ON COLUMN images.pos_x       IS '水平坐标（百分比 0-100）';
COMMENT ON COLUMN images.pos_y       IS '垂直坐标（百分比 0-100）';
COMMENT ON COLUMN images.z           IS '图片层级';
