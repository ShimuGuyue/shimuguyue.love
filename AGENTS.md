# AGENTS.md — shimuguyue.love

石木古月个人网站（[shimuguyue.love](https://shimuguyue.love)），`Vue3` 前端 / `C++` 后端 / `PostgreSQL` 数据库 / `Redis` 缓存。

## AI Agent 行为准则

- **要求冲突时停止执行**：多个要求不可调和时，AI 必须停止，明确指出冲突点并等待用户确认，不得自行选择或猜测。
- **用户修改优先**：用户手动修改的内容为最终权威。若 AI 编写的代码遭到修改，判断是细节微调还是重大重构：若是细节微调，则视为用户个性化修改，保留之；若是逻辑重构，终止当前任务并向用户提出确认，等待下一步指示。
- **环境安装权限**：当项目运行所需环境未下载时，AI 应停止任务，向用户指出缺失的环境及下载方式，待用户下载完成后再执行任务。
- **文件修改记录**：当新建/删除文件时，AI应将其记录到 `AGENTS.md`。
- **测试数据保留**：AI 在开发/测试过程中创建的数据库测试条目不需要删除，保留即可。

## 构建 / 运行

- **要求冲突时停止执行**：多个要求不可调和时，AI 必须停止，明确指出冲突点并等待用户确认，不得自行选择或猜测。
- **用户修改优先**：用户手动修改的内容为最终权威；AI 必须先读取当前文件内容再编辑，不得覆盖用户修改。
- **环境安装权限**：当项目运行所需环境未下载时，AI 应停止任务，向用户指出缺失的环境及下载方式，待用户下载完成后再执行任务。
- **文件修改记录**：当新建/删除文件时，AI应将其记录到 `AGENTS.md`。
- **测试数据保留**：AI 在开发/测试过程中创建的数据库测试条目不需要删除，保留即可。

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

### 数据库（`sql/`）

初始化脚本为 `create_*.sql`，创建仓库并赋予初始条目。初始化脚本可按任意顺序执行，约定不同脚本创建的仓库之间不得有前置依赖。

## 架构

```
前端 (Vue 3, Vite, 端口 5173)
  │  dev 时 Vite 代理 /api → localhost:8080
  │  /photo_wall → localhost:8080
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

- **博客**：双重存储 —— PostgreSQL 行 + `FILE_PATH/blogs/*/*.md` 文件（带 YAML frontmatter：标题、分类、标签、描述等），数据库中存储相对于 `FILE_PATH/blogs/` 的相对路径（不含 `.md` 后缀）。
- **照片墙**：文件存于 `FILE_PATH/photo_wall/`，元数据存于数据库，文件名与对应 `id` 同名。
- **关于我**：`tools/rebuild.sh` 在每次 `npm run build` 前调用 `tools/pull-readme.sh` 从 GitHub 拉取 README 仓库到 `$FILE_PATH/README`；前端构建时由 `client/vite.config.ts` 直接读取 `README.md`（缺失则构建报错）并以构建常量注入 `About.vue`。
- **友链**：`tools/rebuild.sh` 在每次 `npm run build` 前调用 `tools/pull-friend-links.sh` 从 Github 拉取数据仓库到 `FILE_PATH/friend_links/`；前端构建时读取 `meta.yaml`（id / title / url / description）并匹配 `${id}.*` 头像，经 `virtual:friend-links` 虚拟模块注入 `Friends.vue`。头像是纯静态产物：构建期交由 Vite 资源管线按内容哈希命名并复制进产物目录 `assets/<id>-<hash>.<ext>`，dev 时由 Vite 以 `/@fs/` 读取源文件。`meta.yaml` 内部格式约定参考 [shimuguyue.love-friend_links/meta.yaml](https://github.com/ShimuGuyue/shimuguyue.love-friend_links/blob/main/meta.yaml)。
- **认证**：Bearer token，存于 `sessions` 表，过期时间由环境变量 `SESSION_TTL_MINUTES` 控制（分钟），权限 JSON 序列化存库；前端到期自动退出登录。
- **缓存**：公开 GET 接口（分类 / 标签 / 博客列表与详情 / 图片）经 Redis 缓存，统一键前缀 `api-cache:`；博客 / 图片写接口成功后在事务提交后失效相关缓存，TTL 兜底。
- **配置**：`conf/.env`（环境变量）+ `conf/cache.yml`（公开 GET 接口缓存有效期）+ `conf/page_size.yml`（分页每页条数），由 `config::init()` 统一初始化并全部存入 ConfigMap（字符串键值），缺失或非法则 `exit(1)`。

## 关键环境变量

| 变量 | 用途 | 使用者 |
|---|---|---|
| `SERVER_HOST` | 监听地址 | server |
| `SERVER_PORT` | 监听端口 | server |
| `FRONTEND_ORIGIN` | CORS 允许的前端地址 | server |
| `FILE_PATH` | 文件根目录；服务端启动时创建/检测博客、照片墙、友链与 README 目录，`$FILE_PATH/README` 由 `pull-readme.sh`、`$FILE_PATH/friend_links` 由 `pull-friend-links.sh` 拉取，前端构建时读取 | server, tools, client (vite) |
| `FRIENDS_REPO` | 友链数据仓库地址（`pull-friend-links.sh` 拉取用，必填） | tools |
| `FIXED_SALT` | Argon2id 固定盐哈希盐值（32 位 hex = 16 字节） | server |
| `PGHOST` / `PGPORT` / `PGDATABASE` / `PGUSER` / `PGPASSWORD` | 数据库连接 | server |
| `DB_POOL_SIZE` | 数据库连接池大小（正整数，必填） | server |
| `SESSION_TTL_MINUTES` | 登录会话过期时间（分钟，正整数，必填） | server |
| `REDIS_HOST` / `REDIS_PORT` | Redis 地址与端口（必填） | server |
| `REDIS_PASSWORD` | Redis 密码（可选，空字符串表示无密码） | server, tools |
| `REDIS_POOL_SIZE` | Redis 连接池大小（正整数，必填） | server |
| `BUILD_DIR` | 前端构建输出目录（默认 `dist`） | client (vite) |

## 编码约定

### C++（C++23）

- **最小化异常**：允许最小范围内的 `try`/`catch`，如第三方库强制要求异常处理或工具函数等场景，但绝对禁止主动将异常 `throw` 到上层。
- **字符串安全**：返回值为字符串信息的，使用 `std::optional<std::string>` 区分空信息和其它信息。
- **Doxygen 注释**：`/** */` 风格，函数、类、命名空间均需标注。
- **`[[nodiscard]]`**：标注所有返回值不可丢弃的函数。
- **尾置返回类型**：`auto func() -> int`。
- **头文件单次包含：** 使用 `#pragma once` 而不是 `#ifndef ... #define ... #endif`。
- **代码块大括号包含**：循环语句的循环体必须加大括号；分支判断除非在最内层且所有分支均为单条语句，否则加大括号。
- **左大括号换行规范**：命名空间、类、函数、循环、分支、lambda 等的大括号换行。仅 `std::array` 等类型变量赋值不换行。
- **工具函数位置**：仅单文件使用的工具函数写入匿名命名空间，匿名命名空间写在有名命名空间之前，之间五行空白行分隔。
- **头文件导入顺序**：若为 .cpp 文件，首先导入对应的 .h 文件；接下来导入标准库，第三方库，最后是自定义头文件。三/四部分中间加空行。

### 前端

- **Vue**：`<script setup lang="ts">`，`<style scoped>` 默认。
- **TypeScript**：`@vue/tsconfig` 基座，`noUncheckedIndexedAccess` 开启。路径别名 `@/` 映射到 `src/`。
- **导入**：Node 内置模块用 `node:url` 格式；ES module（`"type": "module"`）。
- **网络请求**：无 API 封装模块，直接用 `fetch()`。认证 token 从 `useAuthStore()` 取出，拼接 `Authorization: Bearer <token>` 请求头。

## 目录速查

### **/** 根目录

| 路径 | 说明 |
|---|---|
| `AGENTS.md` | 项目规范与协作说明（本文档） |
| `README.md` | 项目说明文档 |
| `TODO.md` | 待办清单 |

### **.github/** Github 配置目录

| 路径 | 说明 |
|---|---|
| `.github/workflows/ci.yml` | CI：前端 type-check + 构建、后端 vcpkg + CMake 构建、PostgreSQL 冒烟测试 |
| `.github/workflows/deploy.yml` | CD：CI 通过后 SSH 到服务器执行 `tools/rebuild.sh` 自动部署 |

### **conf/** 配置文件目录

| 路径 | 说明 |
|---|---|
| `.env` / `.env.example` | 环境变量/模板 |
| `cache.yml` | 公开 GET 接口缓存有效期 |
| `page_size.yml` | 各筛选器分页每页条数（当前用于博客筛选页，前后端共同读取） |

### **client** 前端开发目录

| 路径 | 说明 |
|---|---|
| `client/package.json` | 前端依赖与 npm 脚本（dev / build / type-check / preview） |
| `client/package-lock.json` | 前端依赖锁定文件 |
| `client/index.html` | Vite 入口 HTML |
| `client/env.d.ts` | 环境变量与虚拟模块类型声明 |
| `client/vite.config.ts` | Vite 配置：dev 代理 `/api`、`/photo_wall` → localhost:8080，`BUILD_DIR` 输出目录；构建时直接读取 `$FILE_PATH/README/README.md` 注入 About 页、读取 `$FILE_PATH/friend_links/meta.yaml` 生成 `virtual:friend-links` 虚拟模块、读取 `conf/page_size.yml` 注入 `__BLOG_PAGE_SIZE__`；头像交由 Vite 资源管线按内容哈希命名，dev 期以 `/@fs/` 提供 `$FILE_PATH` 下的头像 |
| `client/tsconfig.json` | TS 总配置 |
| `client/tsconfig.app.json` | 应用代码 TS 配置 |
| `client/tsconfig.node.json` | 构建脚本 TS 配置 |
| `client/public/assets/favicon.png` | 站点图标 |
| `client/public/assets/note-background.png` | 博客背景图 |
| `client/src/main.ts` | 前端入口：挂载 App、注册 Pinia 与路由 |
| `client/src/App.vue` | 根组件：全局 CSS 变量（`:root` / `html.dark`） |
| `client/src/router/index.ts` | 13 条路由，`createWebHistory`，catch-all 参数用于博客路径 |
| `client/src/stores/auth.ts` | 认证状态（token、username），localStorage 持久化 |
| `client/src/stores/theme.ts` | 深色/浅色主题，toggle `html.dark` |
| `client/src/components/NavBar.vue` | 公共组件：导航栏、主题切换、用户入口 |
| `client/src/components/MarkdownPreview.vue` | 共享 Markdown 预览组件：封装 `MdPreview`，跟随暗色主题，标题 id 统一走 `md-editor-setup` 的 slug 规则 |
| `client/src/lib/md-editor-setup.ts` | md-editor-v3 全局配置：注入本地 highlight.js / katex 实例、`typographer: true` / `breaks: false`，并导出标题 slug 函数 |
| `client/src/views/Home.vue` | 主页：照片墙浏览、编辑、上传；右侧个人简介静态硬编码展示 |
| `client/src/views/Blogs.vue` | 博客列表页（分类/标签筛选、搜索） |
| `client/src/views/BlogDetail.vue` | 博客详情页（`MarkdownPreview` 渲染，目录来自 `getCatalog` 事件，保留滚动高亮与点击标题滚动） |
| `client/src/views/BlogEdit.vue` | 博客新建/编辑页（`MdEditor` 编辑器，图片/mermaid/echarts/prettier/全屏按钮禁用） |
| `client/src/views/About.vue` | 关于我页面（使用 `vite.config.ts` 构建期注入的 README 内容，`MarkdownPreview` 预览） |
| `client/src/views/Manage.vue` | 后台管理页 |
| `client/src/views/ProfileSection.vue` | 个人信息栏目页（路由 /manage/profile） |
| `client/src/views/UserManageSection.vue` | 用户管理栏目页（路由 /manage/users，需 manage:view 权限，保存编辑/创建用户需 manage:edit 权限） |
| `client/src/views/BlogManageSection.vue` | 博客管理栏目页（路由 /manage/blogs，需 manage:view 权限，表格展示全部博客的 file_path / title / category / tags） |
| `client/src/views/LoginKey.vue` | 密钥登录页 |
| `client/src/views/LoginPassword.vue` | 密码登录页 |
| `client/src/views/Projects.vue` | 项目页 |
| `client/src/views/Acknowledgments.vue` | 致谢页 |
| `client/src/views/Favorites.vue` | 收藏页 |
| `client/src/views/Friends.vue` | 友情链接页（条目由构建期 `virtual:friend-links` 静态注入，头像为 Vite 资源管线产出的内容哈希 URL，站点状态在浏览器端探测） |
| `client/src/assets/background.css` | 全局背景主题（粉色 × 紫色系） |
| `client/src/assets/background/block.css` | 块级组件共用背景与外观 |
| `client/src/assets/blog-layout.css` | 博客页布局共用样式 |
| `client/src/assets/blog/selector.css` | 博客筛选页样式（筛选栏 / 标签 / 搜索框） |
| `client/src/assets/blog/card.css` | 博客卡片样式 |
| `client/src/assets/button/login.css` | 登录页按钮样式（`.form-submit`） |
| `client/src/assets/button/manage.css` | 后台管理页按钮样式（`.manage-btn`） |
| `client/src/assets/button/function.css` | 通用功能按钮样式（`.func-btn` 单一样式，`--func-btn-*` 变量统管颜色/透明度/圆角/尺寸），照片墙编辑、便签编辑等复用 |
| `client/src/assets/markdown/` | Markdown 渲染样式（PinkFairy 主题），按类型拆分：`font.css`、`headings.css`、`divider.css`、`text.css`、`blockquote.css`、`lists.css`、`code.css`、`tables.css`、`images.css`、`tasks.css`、`alerts.css` |
| `client/src/assets/manage/font.css` | 后台管理页文本样式 |
| `client/src/assets/manage/table.css` | 后台管理页表格样式 |
| `client/src/assets/normal/color.css` | 颜色变量集中定义（基础色板 + `--pink-hot-rgb` + 半透明粉色 `--pink-<alpha>`） |
| `client/src/assets/normal/link.css` | 全局超链接统一样式（粉色 + 实线下划线，悬停侵蚀紫，与 Markdown 渲染一致；单一来源，`markdown/text.css` 不再重复定义） |

### **server/** 后端开发目录

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
| `server/src/config/config.cpp` / `.h` | 配置统一入口：对外提供 `config::config()` 只读访问 ConfigMap；初始化依次调用 `init_env()`、`init_cache()` 与 `init_page_size()`；约定所有配置文件都在 `conf/` 下，`config::find_conf_dir()` 向上查找并缓存该目录（进程内只查找一次），`config::find_config_file()` 在其内取具体配置文件 |
| `server/src/config/env.cpp` / `.h` | `conf/.env` 环境变量读取（`init_env()`，缺失则 `exit(1)`） |
| `server/src/config/config_map.cpp` / `.h` | 配置统一存储封装类 `ConfigMap`：`.env` 环境变量与 `cache.yml` / `page_size.yml` 的配置项都以字符串键值存入同一个 `unordered_map`（`operator[]` 只读、`set` 写入） |
| `server/src/config/cache.cpp` / `.h` | `conf/cache.yml` 缓存有效期配置加载：在 `conf/` 目录内取文件、yaml 解析校验（缺失或非法则 `exit(1)`），校验通过后以 `CACHE_TTL_*` 键写入 ConfigMap，读取方直接用 `config::config()["CACHE_TTL_*"]` |
| `server/src/config/page_size.cpp` / `.h` | `conf/page_size.yml` 分页每页条数配置加载：在 `conf/` 目录内取文件、yaml 解析校验（缺失或非法则 `exit(1)`），校验通过后以 `BLOGS_PAGESIZE` 键写入 ConfigMap，读取方直接用 `config::config()["BLOGS_PAGESIZE"]` |
| `server/src/auth/login.cpp` / `.h` | 密钥/密码登录、权限查询 |
| `server/src/auth/session.cpp` / `.h` | 会话 token 创建、验证、过期清理 |
| `server/src/auth/rate_limit.cpp` / `.h` | 登录频率限制 |
| `server/src/crypto/argon2id.cpp` / `.h` | Argon2id 密码哈希，随机盐 / 固定盐两种模式 |
| `server/src/doc/blog_queries.cpp` / `.h` | 博客（文档）数据库查询（博客文件路径来自 `FILE_PATH/blogs`） |
| `server/src/export/export_data.cpp` / `.h` | 后台数据导出 |
| `server/src/export/export_queries.cpp` / `.h` | 数据导出查询：各数据表读取为 JSON 数组 |
| `server/src/export/zip_writer.cpp` / `.h` | zip 打包工具（store 方式，无压缩） |
| `server/src/img/image_queries.cpp` / `.h` | 照片墙图片查询、上传、保存、删除 |
| `server/src/md/markdown_parser.cpp` / `.h` | Markdown YAML frontmatter 解析（用 yaml-cpp） |

### **sql/** 数据库表结构目录

| 路径 | 说明 |
|---|---|
| `sql/create_users.sql` | 用户表（users、permissions、user_permissions）+ 会话表（sessions） |
| `sql/create_blogs.sql` | 博客表（categories、tags、blogs、blog_tags） |
| `sql/create_images.sql` | 照片墙图片表（images） |

### **tools/** 自动化工具目录

| 路径 | 说明 |
|---|---|
| `tools/auto-sync-blogs.sh` | 博客 `.md` 自动同步脚本 |
| `tools/pull-readme.sh` | README 拉取脚本（由 `tools/rebuild.sh` 在 npm build 前调用；拉取到 `$FILE_PATH/README`，前端构建时直接读取） |
| `tools/pull-friend-links.sh` | 友链数据仓库拉取脚本（由 `tools/rebuild.sh` 在 npm build 前调用；从 `FRIENDS_REPO` 环境变量读取仓库地址并拉取到 `$FILE_PATH/friend_links`，前端构建时读取 `meta.yaml` 与头像） |
| `tools/rebuild.sh` | 一键重构脚本：git pull → 后端构建 → 重启服务 → 拉取 README/友链 → 前端构建（仅由用户在服务端调用，不在本地开发环境使用） |
| `tools/server-run.sh` | 服务端启动脚本 |

### **test/** 测试脚本目录

| 路径 | 说明 |
|---|---|
| `smoke-test.sh` | 后端冒烟测试（CI 与本地共用；使用 `conf/.env`、终止旧服务端并用临时进程；验证四个公开 GET 接口返回 200、Redis 缓存写入及空结果不缓存） |

## 注意事项

- `npm run build` 用 `run-p`（并行）执行 type-check 和 vite build，类型错误会导致构建不执行。
- 博客 URL 使用 `file_path` 作为 catch-all：`/blogs/:file_path(.*)`，`/blog-edit/:file_path(.*)`。
