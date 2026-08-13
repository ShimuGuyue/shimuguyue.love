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

-- ============================================================
-- 初始数据：权限列表与默认管理员
-- ============================================================

-- 权限列表
INSERT INTO permissions (name) VALUES ('introduction:edit') ON CONFLICT (name) DO NOTHING;
INSERT INTO permissions (name) VALUES ('manage:view')       ON CONFLICT (name) DO NOTHING;
INSERT INTO permissions (name) VALUES ('manage:edit')       ON CONFLICT (name) DO NOTHING;
INSERT INTO permissions (name) VALUES ('blog:create')       ON CONFLICT (name) DO NOTHING;
INSERT INTO permissions (name) VALUES ('blog:edit')         ON CONFLICT (name) DO NOTHING;
INSERT INTO permissions (name) VALUES ('blog:delete')       ON CONFLICT (name) DO NOTHING;
INSERT INTO permissions (name) VALUES ('photo_wall:upload') ON CONFLICT (name) DO NOTHING;
INSERT INTO permissions (name) VALUES ('photo_wall:edit')   ON CONFLICT (name) DO NOTHING;
INSERT INTO permissions (name) VALUES ('photo_wall:delete') ON CONFLICT (name) DO NOTHING;

-- 默认管理员 root（用户名 / 密码见 README）
-- 密钥已停用：key_hash 为占位串的固定盐哈希，无实际登录用途
INSERT INTO users (enabled, key_hash, key_enabled, username, password_hash)
VALUES (
    TRUE,
    '0000000000000000000000000000000000000000000000000000000000000000', -- 占位密钥哈希（无意义，密钥不可用）
    FALSE,
    'root',
    '$argon2id$v=19$m=1048576,t=4,p=1$omPGLmkPgmE1OS9ZHGmQKQ$Br+7hgX33NVy5kOUzrY81tDqQ36+taiN05UV2mVIwJM' -- 密码 root 的随机盐哈希
)
ON CONFLICT (username) DO NOTHING;

-- 为 root 授予全部权限
INSERT INTO user_permissions (user_id, permission_id)
SELECT u.id, p.id
FROM users u
CROSS JOIN permissions p
WHERE u.username = 'root'
ON CONFLICT (user_id, permission_id) DO NOTHING;
