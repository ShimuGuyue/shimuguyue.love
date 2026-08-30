# AGENTS.md — shimuguyue.love

石木古月个人网站（[shimuguyue.love](https://shimuguyue.love)），Vue 3 前端 + C++ 后端 + PostgreSQL 数据库。

## 构建 / 运行

### 前端（`client/`）

```bash
cd client/
npm install
npm run dev          # 开发服务器（热重载）—— 自动代理 /api 到 localhost:8080
npm run build        # 生产构建（先 type-check，再 vite build）
npm run type-check   # 仅类型检查
npm run build-only   # 跳过类型检查直接 vite build
npm run preview      # 预览生产构建
```

### 服务端（`server/`）

依赖 vcpkg（`libpqxx`、`libsodium`、`httplib`、`nlohmann-json`、`yaml-cpp`、`spdlog`、`redis-plus-plus`），需先设 `VCPKG_ROOT`。

```bash
cd server/
cmake -B build --preset default
cmake --build build
./build/server
```

### 数据库

初始化脚本在 `sql/` 下，用 `psql -f <脚本>` 按顺序执行（见 README）。

## 架构

```
前端 (Vue 3, Vite, 端口 5173)
  │  dev 时 Vite 代理 /api → localhost:8080
  │  /image/home → localhost:8080
  ▼
服务端 (C++23, httplib, 端口由 SERVER_PORT 决定)
  │  连接池（libcpp-pg-pool / lklibs::PgPool，DB_POOL_SIZE 条常驻连接），
  │  db::with_db() 并发获取独占连接，无空闲时阻塞等待
  │  公开 GET 接口 cache-aside 缓存（redis++，REDIS_POOL_SIZE 条连接），
  │  未命中时直查数据库，200 成功响应写缓存；写接口成功后失效相关缓存
  ▼
PostgreSQL

Redis（缓存层，可随时丢弃；故障时仅记日志并降级直查数据库）
```

- **博客**：双重存储 —— PostgreSQL 行 + `FILE_PATH/doc/blogs/*/*.md` 文件（带 YAML frontmatter：标题、分类、标签、描述等），数据库中存储相对于 `FILE_PATH/doc/blogs/` 的相对路径（不含 `.md` 后缀）。
- **图片**：文件存于 `FILE_PATH/image/`，元数据存于数据库，文件名与对应 `id` 同名。
- **认证**：Bearer token，存于 `sessions` 表，过期时间由环境变量 `SESSION_TTL_MINUTES` 控制（分钟），权限 JSON 序列化存库；前端到期自动退出登录。
- **缓存**：公开 GET 接口（分类 / 标签 / 博客列表与详情 / 图片 / 关于我 / 个人介绍）经 Redis 缓存，统一键前缀 `api-cache:`；博客 / 图片 / 个人介绍写接口成功后在事务提交后失效相关缓存，`/api/about` 由 `tools/pull-readme.sh` 尽力而为失效、TTL 兜底。
- **配置**：`conf/.env`（环境变量）+ `conf/cache.yml`（公开 GET 接口缓存有效期），由 `config::init()` 统一初始化，缺失或非法则 `exit(1)`。

## 关键环境变量

| 变量 | 用途 | 使用者 |
|---|---|---|
| `SERVER_HOST` | 监听地址 | server |
| `SERVER_PORT` | 监听端口 | server |
| `FRONTEND_ORIGIN` | CORS 允许的前端地址 | server |
| `FILE_PATH` | 文件根目录；启动时统一创建/检测博客（`FILE_PATH/doc/blogs`）、图片（`FILE_PATH/image`）、README（`FILE_PATH/doc/README`）目录 | server, tools |
| `FIXED_SALT` | Argon2id 固定盐哈希盐值（32 位 hex = 16 字节） | server |
| `PGHOST` / `PGPORT` / `PGDATABASE` / `PGUSER` / `PGPASSWORD` | 数据库连接 | server |
| `DB_POOL_SIZE` | 数据库连接池大小（正整数，必填） | server |
| `SESSION_TTL_MINUTES` | 登录会话过期时间（分钟，正整数，必填） | server |
| `REDIS_HOST` / `REDIS_PORT` | Redis 地址与端口（必填） | server |
| `REDIS_PASSWORD` | Redis 密码（可选，空字符串表示无密码） | server, tools |
| `REDIS_POOL_SIZE` | Redis 连接池大小（正整数，必填） | server |
| `BUILD_DIR` | 前端构建输出目录（默认 `dist`） | client (vite) |

## 编码约定

### AI 行为准则

- **要求冲突时停止执行**：多个要求不可调和时，AI 必须停止，明确指出冲突点并等待用户确认，不得自行选择或猜测。
- **用户修改优先**：用户手动修改的内容为最终权威；AI 必须先读取当前文件内容再编辑，不得覆盖用户修改。
- **环境安装权限**：当项目运行所需环境未下载时，AI 应停止任务，向用户指出缺失的环境及下载方式，待用户下载完成后再执行任务。
- **文件修改记录**：当新建/删除文件时，AI应将其记录到 `AGENTS.md`。
- **测试数据保留**：AI 在开发/测试过程中创建的数据库测试条目不需要删除，保留即可。

### C++（C++23）

- **禁止异常**：禁止 `throw`/`try`/`catch`，调用第三方库时在最外层统一捕获异常并转换为错误返回值。使用 `std::optional`、`std::expected`、返回值错误字符串代替。
- **Doxygen**：`/** */` 风格，函数、类、命名空间均需标注。
- **`[[nodiscard]]`**：标注所有返回值不可丢弃的函数。
- **尾置返回类型**：`auto func() -> int`。
- 头文件 `#pragma once`。
- 循环语句 `for`、`while` 循环体加大括号。
- 命名空间、类、函数、循环、分支、lambda 等的大括号换行。变量赋值不换行。
- 匿名命名空间写在有名命名空间之前。之间五行空白行分隔。
- 缩进：命名空间内类和变量不缩进。函数进行缩进。

### 前端

- **Vue**：`<script setup lang="ts">`，`<style scoped>` 默认。
- **TypeScript**：`@vue/tsconfig` 基座，`noUncheckedIndexedAccess` 开启。路径别名 `@/` 映射到 `src/`。
- **导入**：Node 内置模块用 `node:url` 格式；ES module（`"type": "module"`）。
- **网络请求**：无 API 封装模块，直接用 `fetch()`。认证 token 从 `useAuthStore()` 取出，拼接 `Authorization: Bearer <token>` 请求头。

## 目录速查

**根目录**

| 路径 | 说明 |
|---|---|
| `AGENTS.md` | 项目规范与协作说明（本文档） |
| `README.md` | 项目说明文档 |
| `TODO.md` | 待办清单 |
| `conf/` | 配置文件目录：`.env`（环境变量，gitignore）、`cache.yml`（公开 GET 接口缓存有效期）、`.env.example`（模板）；由 `config::init_env()` / `config::init_cache()` 从项目目录向上查找并校验 |
| `.github/workflows/ci.yml` | CI：前端 type-check + 构建、后端 vcpkg + CMake 构建、PostgreSQL 冒烟测试 |
| `.github/workflows/deploy.yml` | CD：CI 通过后 SSH 到服务器执行 `tools/rebuild.sh` 自动部署 |

**client/**

| 路径 | 说明 |
|---|---|
| `client/package.json` | 前端依赖与 npm 脚本（dev / build / type-check / preview） |
| `client/package-lock.json` | 前端依赖锁定文件 |
| `client/index.html` | Vite 入口 HTML |
| `client/env.d.ts` | 环境变量类型声明 |
| `client/vite.config.ts` | Vite 配置：dev 代理 `/api`、`/image/home` → localhost:8080，`BUILD_DIR` 输出目录 |
| `client/tsconfig.json` | TS 总配置 |
| `client/tsconfig.app.json` | 应用代码 TS 配置 |
| `client/tsconfig.node.json` | 构建脚本 TS 配置 |
| `client/public/assets/favicon.png` | 站点图标 |
| `client/public/assets/note-background.png` | 博客背景图 |
| `client/src/main.ts` | 前端入口：挂载 App、注册 Pinia 与路由 |
| `client/src/App.vue` | 根组件：全局 CSS 变量（`:root` / `html.dark`） |
| `client/src/router/index.ts` | 14 条路由，`createWebHistory`，catch-all 参数用于博客路径 |
| `client/src/stores/auth.ts` | 认证状态（token、username），localStorage 持久化 |
| `client/src/stores/theme.ts` | 深色/浅色主题，toggle `html.dark` |
| `client/src/components/NavBar.vue` | 公共组件：导航栏、主题切换、用户入口 |
| `client/src/components/MarkdownPreview.vue` | 共享 Markdown 预览组件：封装 `MdPreview`，跟随暗色主题，标题 id 统一走 `md-editor-setup` 的 slug 规则 |
| `client/src/lib/md-editor-setup.ts` | md-editor-v3 全局配置：注入本地 highlight.js / katex 实例、`typographer: true` / `breaks: false`，并导出标题 slug 函数 |
| `client/src/views/Home.vue` | 主页：照片墙浏览、编辑、上传 |
| `client/src/views/Blogs.vue` | 博客列表页（分类/标签筛选、搜索） |
| `client/src/views/BlogDetail.vue` | 博客详情页（`MarkdownPreview` 渲染，目录来自 `getCatalog` 事件，保留滚动高亮与点击标题滚动） |
| `client/src/views/BlogEdit.vue` | 博客新建/编辑页（`MdEditor` 编辑器，图片/mermaid/echarts/prettier/全屏按钮禁用） |
| `client/src/views/About.vue` | 关于我页面（渲染 README，`MarkdownPreview` 预览） |
| `client/src/views/Manage.vue` | 后台管理页 |
| `client/src/views/ProfileSection.vue` | 个人信息栏目页（路由 /manage/profile） |
| `client/src/views/UserManageSection.vue` | 用户管理栏目页（路由 /manage/users，需 manage:view 权限，保存编辑/创建用户需 manage:edit 权限） |
| `client/src/views/BlogManageSection.vue` | 博客管理栏目页（路由 /manage/blogs，需 manage:view 权限，表格展示全部博客的 file_path / title / category / tags） |
| `client/src/views/FriendManageSection.vue` | 友链管理栏目页（路由 /manage/friends，需 manage:view 权限；支持新建/编辑友链、按行上传 512×512 头像，友链条目以站点链接 url 区分，每页 9 条分页展示） |
| `client/src/views/LoginKey.vue` | 密钥登录页 |
| `client/src/views/LoginPassword.vue` | 密码登录页 |
| `client/src/views/Projects.vue` | 项目页 |
| `client/src/views/Acknowledgments.vue` | 致谢页 |
| `client/src/views/Favorites.vue` | 收藏页 |
| `client/src/views/Friends.vue` | 友情链接页 |
| `client/src/assets/background.css` | 全局背景主题（粉色 × 紫色系） |
| `client/src/assets/background/block.css` | 块级组件共用背景与外观 |
| `client/src/assets/blog-layout.css` | 博客页布局共用样式 |
| `client/src/assets/blog/selector.css` | 博客筛选页样式（筛选栏 / 标签 / 搜索框） |
| `client/src/assets/blog/card.css` | 博客卡片样式 |
| `client/src/assets/button/login.css` | 登录页按钮样式（`.form-submit`） |
| `client/src/assets/button/manage.css` | 后台管理页按钮样式（`.manage-btn`） |
| `client/src/assets/button/function.css` | 通用功能按钮样式（`.func-btn` 单一样式，`--func-btn-*` 变量统管颜色/透明度/圆角/尺寸），照片墙编辑、个人介绍编辑、便签编辑等复用 |
| `client/src/assets/markdown/` | Markdown 渲染样式（PinkFairy 主题），按类型拆分：`font.css`、`headings.css`、`divider.css`、`text.css`、`blockquote.css`、`lists.css`、`code.css`、`tables.css`、`images.css`、`tasks.css`、`alerts.css` |
| `client/src/assets/manage/font.css` | 后台管理页文本样式 |
| `client/src/assets/manage/table.css` | 后台管理页表格样式 |
| `client/src/assets/normal/color.css` | 颜色变量集中定义（基础色板 + `--pink-hot-rgb` + 半透明粉色 `--pink-<alpha>`） |

**server/**

| 路径 | 说明 |
|---|---|
| `server/main.cpp` | 服务端入口：初始化 → 建立数据库连接池 → 注册路由 → 监听 |
| `server/CMakeLists.txt` | CMake 构建配置（源文件列表、vcpkg 依赖） |
| `server/CMakePresets.json` | CMake 预设（default / release，release 继承 default 并设置 `CMAKE_BUILD_TYPE=Release`） |
| `server/third_party/libcpp-pg-pool/` | 从 GitHub 拉取的连接池库（MIT 协议，纯头文件，基于 libpqxx） |
| `server/src/http/routes.cpp` | API 路由注册（~180 行），统一调用 handlers 中的处理函数 |
| `server/src/http/routes.h` | HTTP 服务配置声明（`FRONTEND_ORIGIN` / `SERVER_HOST` / `SERVER_PORT`、`setup_routes`） |
| `server/src/http/handlers.cpp` / `.h` | 全部 API 路由处理函数（业务逻辑），由 routes.cpp 统一注册调用 |
| `server/src/cache/cache.cpp` / `.h` | Redis 公开接口缓存（基于 redis++ / redis-plus-plus）：初始化（PING 校验）、get / set / del、按前缀 SCAN+DEL 失效、统一键构造 `api-cache:`，get 记录缓存命中/未命中日志、set 记录写缓存日志 |
| `server/src/db/connection.cpp` / `.h` | 数据库连接池初始化 + 表检查 |
| `server/src/db/connection_pool.cpp` / `.h` | 连接池实现：基于 `lklibs::PgPool` 的薄封装，`db::with_db()` 并发获取独占连接，无空闲时阻塞等待 |
| `server/src/config/config.cpp` / `.h` | 配置统一初始化入口：依次调用 `init_env()` 与 `init_cache()`；共享 `config::find_config_file()` 向上查找 `conf/` 配置文件 |
| `server/src/config/env.cpp` / `.h` | `conf/.env` 环境变量读取（`init_env()`，缺失则 `exit(1)`） |
| `server/src/config/env_map.cpp` / `.h` | 环境变量存储封装类（内部 `unordered_map`，只读 `operator[]`） |
| `server/src/config/cache.cpp` / `.h` | `conf/cache.yml` 缓存有效期配置加载：向上查找文件、yaml 解析校验（缺失或非法则 `exit(1)`），`config::cache_ttl()` 只读访问 |
| `server/src/auth/login.cpp` / `.h` | 密钥/密码登录、权限查询 |
| `server/src/auth/session.cpp` / `.h` | 会话 token 创建、验证、过期清理 |
| `server/src/auth/rate_limit.cpp` / `.h` | 登录频率限制 |
| `server/src/crypto/argon2id.cpp` / `.h` | Argon2id 密码哈希，随机盐 / 固定盐两种模式 |
| `server/src/doc/blog_queries.cpp` / `.h` | 博客（文档）数据库查询（博客文件路径来自 `FILE_PATH/doc`） |
| `server/src/export/export_data.cpp` / `.h` | 后台数据导出 |
| `server/src/export/export_queries.cpp` / `.h` | 数据导出查询：各数据表读取为 JSON 数组 |
| `server/src/export/zip_writer.cpp` / `.h` | zip 打包工具（store 方式，无压缩） |
| `server/src/img/image_queries.cpp` / `.h` | 照片墙图片查询、上传、保存、删除 |
| `server/src/friend/friend_queries.cpp` / `.h` | 友情链接数据库查询（友链条目以站点链接 url 区分；按友链 id 匹配 `friend_avatars/<id>.<ext>` 图片；`update_friend` 按 old_url 定位并更新名称/链接/描述；`upload_avatar` 上传/替换 512×512 头像） |
| `server/src/profile/profile_queries.cpp` / `.h` | 个人介绍查询、更新 |
| `server/src/about/about_queries.cpp` / `.h` | 关于我 README 内容数据库查询 |
| `server/src/md/markdown_parser.cpp` / `.h` | Markdown YAML frontmatter 解析（用 yaml-cpp） |

**sql/ 与 tools/**

| 路径 | 说明 |
|---|---|
| `sql/create_users.sql` | 用户表（users、permissions、user_permissions）+ 会话表（sessions） |
| `sql/create_blogs.sql` | 博客表（categories、tags、blogs、blog_tags） |
| `sql/create_images.sql` | 照片墙图片表（images） |
| `sql/create_profile.sql` | 个人介绍表（profile，单行） |
| `sql/create_about.sql` | 关于我内容表（about，单行） |
| `sql/create_friends.sql` | 友情链接表（friends：name / url / description，url 唯一） |
| `sql/migrate_friends_url_unique.sql` | 友链表迁移：移除 name 唯一约束、为 url 加唯一约束（友链条目改按站点链接区分） |
| `tools/auto-sync-blogs.sh` | 博客 `.md` 自动同步脚本 |
| `tools/pull-readme.sh` | README 自动拉取脚本（psql 同步成功后重建 `/api/about` 缓存：先失效旧键，再请求接口回源） |
| `tools/migrate-friend-avatars.sh` | 友链头像迁移脚本：将 `friend_avatars` 下按站点名命名的头像文件重命名为 `<友链id>.<扩展名>`（配合 `sql/migrate_friends_url_unique.sql` 使用） |
| `tools/rebuild.sh` | 一键重构脚本：前端构建 → 后端构建 → 重启服务（仅由用户在服务端调用，不在本地开发环境使用） |
| `tools/server-run.sh` | 服务端启动脚本 |
| `tools/server-run.log` | 服务端运行日志（运行产物） |
| `test/` | 测试脚本：`smoke-test.sh` 后端冒烟测试（CI 与本地共用；使用 `conf/.env`、终止旧服务端并用临时进程；写接口测试用专用账号 `smoke_test` 幂等创建并授权 `introduction:edit`，测试前后备份/恢复 profile） |

## 注意事项

- `npm run build` 用 `run-p`（并行）执行 type-check 和 vite build，类型错误会导致构建不执行。
- 博客 URL 使用 `file_path` 作为 catch-all：`/blogs/:file_path(.*)`，`/blog-edit/:file_path(.*)`。
