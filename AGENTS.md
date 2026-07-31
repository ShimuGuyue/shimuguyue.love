# AGENTS.md — shimuguyue.love

石木古月个人网站（[shimuguyue.love](https://shimuguyue.love)），Vue 3 前端 + C++ 后端 + PostgreSQL 数据库。

## 构建 / 运行

### 前端（`client/`）

```bash
cd client
npm install
npm run dev          # 开发服务器（热重载）—— 自动代理 /api 到 localhost:8080
npm run build        # 生产构建（先 type-check，再 vite build）
npm run type-check   # 仅类型检查
npm run build-only   # 跳过类型检查直接 vite build
npm run preview      # 预览生产构建
```

### 服务端（`server/`）

依赖 vcpkg（`libpqxx`、`libsodium`、`httplib`、`nlohmann-json`、`yaml-cpp`），需先设 `VCPKG_ROOT`。

```bash
cd server
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
  │  单 pqxx::connection，全局 std::mutex 串行化所有数据库操作
  ▼
PostgreSQL
```

- **博客**：双重存储 —— PostgreSQL 行 + `DOC_PATH/blogs/` 下 `.md` 文件（带 YAML frontmatter：标题、分类、标签、描述等），数据库中存储相对于 `DOC_PATH/blogs/` 的相对路径（不含 `.md` 后缀）。
- **图片**：文件存于 `IMAGE_PATH/`，元数据存于数据库，文件名与对应 `id` 同名。
- **认证**：Bearer token，存于 `sessions` 表，24 小时过期，权限 JSON 序列化存库。
- **配置**：所有配置从环境变量读取，缺失则 `exit(1)`。无配置文件。

## 关键环境变量

| 变量 | 用途 | 使用者 |
|---|---|---|
| `SERVER_HOST` | 监听地址 | server |
| `SERVER_PORT` | 监听端口 | server |
| `FRONTEND_ORIGIN` | CORS 允许的前端地址 | server |
| `PGHOST` / `PGPORT` / `PGDATABASE` / `PGUSER` / `PGPASSWORD` | 数据库连接 | server |
| `DOC_PATH` | 博客 `.md` 文件保存目录 | server |
| `IMAGE_PATH` | 图片文件保存目录 | server |
| `README_DIR` | 前端渲染 README 文件路径，供 `pull-readme.sh` 和 `/api/about/sync` 使用 | server, tools |
| `BUILD_DIR` | 前端构建输出目录（默认 `dist`） | client (vite) |

## 编码约定

### AI 行为准则

- **要求冲突时停止执行**：多个要求不可调和时，AI 必须停止，明确指出冲突点并等待用户确认，不得自行选择或猜测。
- **用户修改优先**：用户手动修改的内容为最终权威；AI 必须先读取当前文件内容再编辑，不得覆盖用户修改。

### C++（C++23）

- **禁止异常**：禁止 `throw`/`try`/`catch`，调用第三方库时在最外层统一捕获异常并转换为错误返回值。使用 `std::optional`、`std::expected`、返回值错误字符串代替。
- **Doxygen**：`/** */` 风格，函数、类、命名空间均需标注。
- **`[[nodiscard]]`**：标注所有返回值不可丢弃的函数。
- **尾置返回类型**：`auto func() -> int`。
- **`#pragma once`**。

### 前端

- **Vue**：`<script setup lang="ts">`，`<style scoped>` 默认。
- **TypeScript**：`@vue/tsconfig` 基座，`noUncheckedIndexedAccess` 开启。路径别名 `@/` 映射到 `src/`。
- **导入**：Node 内置模块用 `node:url` 格式；ES module（`"type": "module"`）。
- **CSS**：无 CSS 框架，全部自定义 CSS 变量（`App.vue` 中 `:root` / `html.dark`），共用样式文件在 `client/src/assets/`。
- **网络请求**：无 API 封装模块，直接用 `fetch()`。认证 token 从 `useAuthStore()` 取出，拼接 `Authorization: Bearer <token>` 请求头。

## 目录速查

| 路径 | 说明 |
|---|---|
| `client/src/router/index.ts` | 12 条路由，`createWebHistory`，catch-all 参数用于博客路径 |
| `client/src/stores/auth.ts` | 认证状态（token、username），localStorage 持久化 |
| `client/src/stores/theme.ts` | 深色/浅色主题，toggle `html.dark` |
| `client/src/components/NavBar.vue` | 唯一公共组件：导航栏、主题切换、用户入口 |
| `server/main.cpp` | 服务端入口：初始化 → 建立数据库连接 → 注册路由 → 监听 |
| `server/src/http/routes.cpp` | 所有 API 路由注册（~930 行），按 auth/blog/image/profile/about 分组 |
| `server/src/crypto/argon2id.cpp` | Argon2id 密码哈希，随机盐 / 固定盐两种模式 |
| `server/src/md/markdown_parser.cpp` | Markdown YAML frontmatter 解析（用 yaml-cpp） |
| `server/src/about/about_queries.cpp` | 关于我 页面内容数据库查询 |
| `sql/` | 数据库建表脚本，按 README 指定顺序执行 |
| `tools/` | 自动化脚本：博客自动同步、README 自动拉取 |
| `test/` | 测试用文件（非代码） |

## 注意事项

- `npm run build` 用 `run-p`（并行）执行 type-check 和 vite build，类型错误会导致构建不执行。
- 博客 URL 使用 `file_path` 作为 catch-all：`/blogs/:file_path(.*)`，`/blog-edit/:file_path(.*)`。
