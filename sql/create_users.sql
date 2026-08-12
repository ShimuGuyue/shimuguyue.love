-- ============================================================
-- 用户表
-- ============================================================
-- key_hash:     Argon2id 哈希字符串，用于个人身份认证（唯一）
-- username:     用户名，最多 10 个半角字符，不重复，可为空
-- password_hash: Argon2id 密码哈希，无用户名时可为空
-- key_enabled:  key 是否可用，默认启用
-- ============================================================

CREATE TABLE users (
    id            SERIAL       PRIMARY KEY,
    enabled       BOOLEAN      NOT NULL DEFAULT TRUE,
    key_hash      TEXT         NOT NULL UNIQUE,
    key_enabled   BOOLEAN      NOT NULL DEFAULT TRUE,
    username      VARCHAR(10)  UNIQUE,
    password_hash TEXT
);

COMMENT ON TABLE  users IS '用户表';
COMMENT ON COLUMN users.id            IS '主键，自增';
COMMENT ON COLUMN users.enabled       IS '用户可用状态，FALSE 表示已停用（软删除）';
COMMENT ON COLUMN users.key_hash      IS 'Argon2id 哈希，个人身份认证 token';
COMMENT ON COLUMN users.key_enabled   IS '认证 key 是否可用';
COMMENT ON COLUMN users.username      IS '用户名，最多 10 个半角字符';
COMMENT ON COLUMN users.password_hash IS 'Argon2id 密码哈希';

-- ============================================================
-- 权限表
-- ============================================================
-- id:   自增主键
-- name: 权限名
-- ============================================================

CREATE TABLE permissions (
    id   SERIAL PRIMARY KEY,
    name TEXT   NOT NULL UNIQUE
);

COMMENT ON TABLE  permissions IS '权限表';
COMMENT ON COLUMN permissions.id   IS '主键，自增';
COMMENT ON COLUMN permissions.name IS '权限名';

-- ============================================================
-- 用户-权限关联表
-- ============================================================
-- user_id:       关联 users.id
-- permission_id: 关联 permissions.id
-- 按 user_id 优先、permission_id 次之排序
-- ============================================================

CREATE TABLE user_permissions (
    user_id       INT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    permission_id INT NOT NULL REFERENCES permissions(id) ON DELETE CASCADE,
    PRIMARY KEY (user_id, permission_id)
);

COMMENT ON TABLE  user_permissions IS '用户-权限关联表';
COMMENT ON COLUMN user_permissions.user_id       IS '关联 users.id';
COMMENT ON COLUMN user_permissions.permission_id IS '关联 permissions.id';
