# shimuguyue.love

## 项目介绍

这是一个**前端+后端+数据库+数据远程自动化同步/拉取**的个人网站项目，用于部署我的个人网站 [shimuguyue.love](https://shimuguyue.love)。

## 开始使用

项目测试环境为 `Ubuntu-24.04` 和 `Ubuntu-26.04`。

说明：以下出现所有 `${NAME}` 格式的变量为用户的**个性化配置**，而非环境变量。

### 克隆仓库

克隆仓库。

```bash
# 使用 SSH 克隆是为了可自动化拉取新更改并重新部署
# 如不需要服务器自动化更新，也可用 HTTP 克隆
git clone git@github.com:${USER_NAME}/${REPO_NAME}.git ${PROJECT_PATH}
cd ${PROJECT_PATH}
```

### 环境变量

复制项目根目录的 `.env.example` 为 `.env` 并填写。
文件类目录只配置一个根目录 `FILE_PATH`：博客文件位于 `$FILE_PATH/doc/blogs`、图片位于 `$FILE_PATH/image`、README 位于 `$FILE_PATH/README`，服务端启动时会自动创建这些目录。

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
    psql -f ./create_users.sql       # 角色表
    psql -f ./create_permissions.sql # 权限表 + 角色-权限关联表
    psql -f ./create_blogs.sql       # 博客表 + 分类表 + 标签表 + 博客-标签关联表
    psql -f ./create_sessions.sql    # session表
    psql -f ./create_images.sql      # 图片表
    psql -f ./create_profile.sql     # 个人简介表
    psql -f ./create_about.sql       # README页面内容表
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
    ```

+   构建并后台运行 `C++` 应用程序

    ```bash
    cmake -B build --preset default
    cmake --build build

    ../tools/server-run.sh start # 启动后端运行
    ../tools/server-run.sh stop  # 终止后端运行
    ```

### 项目配置

+   为自己创建超级管理员帐号

    首先在 `server/main.cpp` 添加以下代码并运行为自己的密钥/密码拿到哈希值

    ！拿到值后请立刻删除语句。
    ！固定盐哈希依赖环境变量 `FIXED_SALT`，请把语句放在 `config::init();` 之后。

    ```cpp
    std:cout << *crypto::Argon2id::hash_with_random_salt("${KEY}")     << std::endl; ///< 密钥哈希值
    std:cout << *crypto::Argon2id::hash_with_fixed_salt("${PASSWORD}") << std::endl; ///< 密码哈希值
    ```

    在数据库创建管理员用户。

    ```postgresql
    # 默认当前数据库表均为空，则对应条目的 id 可知

    # 添加管理员用户信息
    INSERT INTO users (key_hash, key_enabled, username, password_hash)
    VALUES (
        '${KEY_HASH}',      # 密钥哈希值
        'TRUE',             # 密钥可用状态，不暴露固定盐值情况下可以安全开启
        '${USERNAME}',      # 用户名
        '${PASSWORD_HASH}', # 密码哈希值
    ); # id 为 1

    # 添加权限列表
    INSERT INTO permissions (name) VALUES ('create'); #id 为 1
    INSERT INTO permissions (name) VALUES ('edit');   #id 为 2
    INSERT INTO permissions (name) VALUES ('drop');   #id 为 3

    # 为管理员授予权限
    INSERT INTO user_permissions (user_id, permission_id) VALUES (1, 1);
    INSERT INTO user_permissions (user_id, permission_id) VALUES (1, 2);
    INSERT INTO user_permissions (user_id, permission_id) VALUES (1, 3);
    ```

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
    cd ${FILE_PATH}/README
    git init
    git remote add origin ${REPO_URL}
    ```

+   设置定时拉取并同步到数据库（每天一次）

    ```bash
    chmod +x ./pull-readme.sh

    crontab -e
    0 4 * * * /bin/bash ${PROJECT_PATH}/tools/pull-readme.sh >> ${PROJECT_PATH}/tools/pull-readme.log 2>&1
    ```
