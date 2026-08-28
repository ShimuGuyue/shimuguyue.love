# shimuguyue.love

## 项目介绍

这是一个**前端+后端+数据库+数据远程自动化同步/拉取**的个人网站项目，用于部署我的个人网站 [shimuguyue.love](https://shimuguyue.love)。

## 开始使用

项目测试环境为 `Ubuntu-24.04` 和 `Ubuntu-26.04`。

说明：以下出现所有 `${NAME}` 格式的变量为用户的**个性化配置**，而非环境变量。

### 克隆仓库

克隆仓库。

```bash
# 使用 SSH 克隆是为了使用 GitHub Action 自动化拉取新更改并重新部署
git clone git@github.com:${USER_NAME}/${REPO_NAME}.git ${PROJECT_PATH}
cd ${PROJECT_PATH}
```

### 环境变量

复制项目根目录的 `.env.example` 为 `.env` 并填写。
文件类目录只配置一个根目录 `FILE_PATH`：博客文件位于 `$FILE_PATH/doc/blogs`、图片位于 `$FILE_PATH/image`、README 位于 `$FILE_PATH/doc/README`，服务端启动时会自动创建这些目录。

### 前端

本条目下操作默认在 `${PROJECT_PATH}/client` 目录下执行。

+   安装 `Node.js` 和 `npm`

    ```bash
    curl -fsSL https://deb.nodesource.com/setup_22.x | bash -
    apt install -y nodejs
    ```

+   安装 `npm` 项目依赖

    ```bash
    npm install
    ```

+   构建生成版本，生成 `dist` 目录

    ```bash
    npm run build
    ```

### Nginx

+   安装 `nginx`

    ```bash
    apt install nginx
    ```

+   新建站点文件配置

    ```bash
    nano /etc/nginx/sites-available/${PROJECT_NAME}
    ```

+   启用站点配置

    ```bash
    ln -s /etc/nginx/sites-available/${PROJECT_NAME} /etc/nginx/ sites-enabled/
    nginx -t && systemctl reload nginx
    ```

### 数据库

本条目下操作默认在 `${PROJECT_PATH}/sql` 目录下执行。

+   安装 `postgresql`

    ```bash
    apt install postgresql
    ```

+   创建数据库和用户并设置权限

    ```bash
    sudo -u postgres psql
    ```

    ```postgresql
    CREATE USER ${PGUSER} WITH ENCRYPTED PASSWORD '${PGPASSWORD}';
    CREATE DATABASE ${PGDATABASE} OWNER ${PGUSER};
    GRANT ALL PRIVILEGES ON DATABASE ${PGDATABASE} TO ${PGUSER};
    \q
    ```

+   创建所需数据库表

    ```bash
    cd ${PROJECT_PATH}/sql/
    psql -f ./create_users.sql       # 角色表 + 权限表 + 角色-权限关联表
    psql -f ./create_blogs.sql       # 博客表 + 分类表 + 标签表 + 博客-标签关联表
    psql -f ./create_sessions.sql    # session表
    psql -f ./create_images.sql      # 图片表
    psql -f ./create_profile.sql     # 个人简介表
    psql -f ./create_about.sql       # README页面内容表
    ```

### Redis

各项接口的缓存有效期配置在项目根目录 `cache.yml`（单位：秒）。

+   安装 `redis-server`

    ```bash
    apt install redis-server
    systemctl enable --now redis-server
    ```

### 后端

本条目下操作默认在 `${PROJECT_PATH}/server` 目录下执行。

+   安装 `CMake`

    ```bash
    apt install cmake
    ```

+   安装 `vcpkg` 并设置环境变量

    ```bash
    git clone https://github.com/Microsoft/vcpkg.git ${VCPKG_ROOT}
    ${VCPKG_ROOT}/bootstrap-vcpkg.sh

    echo 'export VCPKG_ROOT=${VCPKG_ROOT}' >> ~/.bashrc
    echo 'export PATH="$PATH:$VCPKG_ROOT"' >> ~/.bashrc
    source ~/.bashrc
    ```

+   安装 `pkg-config`

    ```bash
    apt install pkg-config
    ```

+   使用 `vcpkg` 安装所需的库及库运行所需的包

    ```bash
    apt install bison flex autoconf autoconf-archive automake libtool

    vcpkg install libpqxx:x64-linux
    vcpkg install libsodium:x64-linux
    vcpkg install cpp-httplib:x64-linux
    vcpkg install nlohmann-json:x64-linux
    vcpkg install yaml-cpp:x64-linux
    vcpkg install spdlog
    vcpkg install redis-plus-plus:x64-linux
    ```

+  拉取 `libcpp-pg-pool`（连接池库，纯头文件，基于 `libpqxx`）

    ```bash
    git clone --depth 1 https://github.com/leventkaragol/libcpp-pg-pool ./third_party/libcpp-pg-pool
    ```

+   构建并后台运行 `C++` 应用程序

    ```bash
    cmake -B build --preset default  # 开发构建
    cmake -B build --preset release  # 发布构建（继承 default，启用 Release 优化）
    cmake --build build

    ../tools/server-run.sh start # 启动后端运行
    ../tools/server-run.sh stop  # 终止后端运行
    ```

### 项目配置

默认管理员账号由 `sql/create_users.sql` 自动创建：

- 用户名：`root`
- 密码：`root`

root 的密钥已停用（因固定盐需自行设定），仅支持密码登录；可在「后台管理 → 用户管理」中自行设置密钥并启用。

### 自动化（可选）

本条目下若代码块开头无 `cd` 指令，默认在 `${PROJECT_PATH}/tools` 目录下执行。

#### 博客自动同步

+   博客保存目录连接远程仓库并创建 `auto` 分支

    ```bash
    cd ${FILE_PATH}/doc/blogs
    git init
    git remote add origin ${REPO_URL}
    git checkout -b auto
    git commit --allow-empty -m "initial empty commit"
    git push -u origin auto
    ```

+   设置定时同步（每分钟一次）

    ```bash
    chmod +x ./auto-sync-blogs.sh

    crontab -e
    * * * * * /bin/bash ${PROJECT_PATH}/tools/auto-sync-blogs.sh >> ${PROJECT_PATH}/tools/auto-sync-blogs.log 2>&1
    ```

#### README内容自动拉取

+   创建目录连接 README 远程仓库

    ```bash
    cd ${FILE_PATH}/doc/README
    git init
    git remote add origin ${REPO_URL}
    ```

+   设置定时拉取并同步到数据库（每天一次）

    ```bash
    chmod +x ./pull-readme.sh

    crontab -e
    0 4 * * * /bin/bash ${PROJECT_PATH}/tools/pull-readme.sh >> ${PROJECT_PATH}/tools/pull-readme.log 2>&1
    ```

#### GitHub Actions 自动集成

仓库内置 GitHub Actions 工作流，push 或提交 PR 到 `main` 分支时自动执行：

+   `ci.yml` — 持续集成
    - 前端：`npm ci` → `npm run type-check` → `npm run build`
    - 后端：安装 vcpkg 依赖（含缓存）→ 以 `release` 预设配置并构建 CMake
    - 冒烟测试：启动 PostgreSQL 容器、创建数据表，运行服务端并验证公开 API

+   `deploy.yml` — 持续部署】
    - `main` 分支 CI 通过后自动执行，也可在 Actions 页面手动触发（`workflow_dispatch`）
    - 通过 SSH 在服务器上执行 `tools/rebuild.sh` 完成项目重建流程

在 GitHub 仓库 **Settings / Environments** 设置部署环境名为 `production`。

在 GitHub 仓库 **Settings / Secrets and variables / Actions** 中配置环境变量：

| 名称 | 类型 | 说明 |
|---|---|---|
| `DEPLOY_ENABLED` | Variable | 设为 `true` 开启自动部署；未设置或为其它值时不执行部署 |
| `DEPLOY_HOST` | Secret | 服务器 IP 或域名 |
| `DEPLOY_PORT` | Secret | SSH 端口 |
| `DEPLOY_USER` | Secret | SSH 登录用户名 |
| `DEPLOY_SSH_KEY` | Secret | SSH 私钥 |
| `DEPLOY_PATH` | Variable | 服务器上项目根目录路径 |
| `DEPLOY_HEALTH_URL` | Variable | 部署完成后的健康检查地址，例如生产环境域名 |

类型与 `deploy.yml` 中的引用方式一致：Variable 通过 `${{ vars.名称 }}` 读取，Secret 通过 `${{ secrets.名称 }}` 读取。
