# shimuguyue.love

## 项目介绍

这是一个**前端+后端+数据库+数据远程自动化同步/拉取**的个人网站项目，用于部署我的个人网站 [shimuguyue.love](https://shimuguyue.love)。

## 开始使用

说明：以下出现所有 `${NAME}` 格式的变量为用户的**个性化配置**，而非环境变量。

### 克隆仓库

克隆仓库并进入目录

```bash
git clone git@github.com:${USER_NAME}/${REPO_NAME}.git ${PROJECT_PATH}

cd ${PROJECT_PATH}
```

### 前端

设置环境变量 `${FRONTEND_ORIGIN}`

```bash
echo 'export FRONTEND_ORIGIN=${FRONTEND_ORIGIN}' >> ~/.bashrc
source ~/.bashrc
```

安装 `Node.js` 和 `npm`

```bash
curl -fsSL https://deb.nodesource.com/setup_22.x | bash -
apt install -y nodejs
```

安装 `npm` 依赖

```bash
cd ${PROJECT_PATH}/client/
npm install
```

设置构建目录环境变量 `${BUILD_DIR_CLIENT}`

```bash
echo 'export BUILD_DIR=${BUILD_DIR_CLIENT}' >> ~/.bashrc
source ~/.bashrc
```

构建生成版本，生成 `dist` 目录

```bash
cd ${PROJECT_PATH}/client/
npm run build
```

### Nginx

安装 `nginx`

```bash
apt install nginx
```

新建站点文件配置

```bash
nano /etc/nginx/sites-available/${PROJECT_NAME}
```

启用站点配置

```bash
ln -s /etc/nginx/sites-available/${PROJECT_NAME} /etc/nginx/sites-enabled/
```

### 数据库

安装 `postgresql`

```bash
apt install postgresql
```

设置环境变量 `${PGPORT}`, `${PGHOST}`, `${PGPASSWORD}`, `${PGUSER}`, `${PGDATABASE}` 并创建对应用户和数据库

```bash
echo 'export SPGPORT=${SPGPORT}' >> ~/.bashrc
echo 'export PGHOST=${PGHOST}' >> ~/.bashrc
echo 'export PGUSER=${PGUSER}' >> ~/.bashrc
echo 'export PGPASSWORD=${PGPASSWORD}' >> ~/.bashrc
echo 'export PGDATABASE=${PGDATABASE}' >> ~/.bashrc
source ~/.bashrc
```

```postgresql
CREATE USER ${PGUSER} WITH ENCRYPTED PASSWORD ${PGPASSWORD};
CREATE DATABASE ${PGDATABASE} OWNER ${PGUSER};
GRANT ALL PRIVILEGES ON DATABASE ${PGDATABASE} TO ${PGUSER};
```

创建所需数据库表

```bash
cd ${PROJECT_PATH}/sql/
psql -f ./create_users.sql       # 角色表
psql -f ./create_permissions.sql # 权限表
psql -f ./create_blogs.sql       # 角色-权限关联表
psql -f ./create_sessions.sql    # session表
psql -f ./create_images.sql      # 图片表
psql -f ./create_profile.sql     # 个人简介表
```

### 后端

设置环境变量 `${SERVER_HOST}` 和 `${SERVER_PORT}`

```bash
echo 'export SERVER_HOST=${SERVER_HOST}' >> ~/.bashrc
echo 'export SERVER_PORT=${SERVER_PORT}' >> ~/.bashrc
source ~/.bashrc
```

安装 `CMake`

```bash
apt install cmake
```

安装 `vcpkg` 并添加到环境变量，设置环境变量 `${VCPKG_ROOT}`

```bash
cd ${LIB_PATH}
git clone https://github.com/Microsoft/vcpkg.git
./vcpkg/bootstrap-vcpkg.sh

echo 'export VCPKG_ROOT=${LIB_PATH}/vcpkg' >> ~/.bashrc
echo 'export PATH="$PATH:/$VCPKG_ROOT"' >> ~/.bashrc
source ~/.bashrc
```

安装 `pkg-config`

```bash
apt install pkg-config
```

使用 `vcpkg` 安装所需的库

```bash
apt install bison flex autoconf autoconf-archive automake libtool

vcpkg install libpqxx:x64-linux
vcpkg install libsodium:x64-linux
vcpkg install cpp-httplib:x64-linux
vcpkg install nlohmann-json:x64-linux
vcpkg install yaml-cpp:x64-linux
```

构建并运行 `C++` 应用程序

```bash
cd ${PROJECT_PATH}/server/
cmake -B ${BUILD_DIR_SERVER} --preset default
cmake --build ${BUILD_DIR_SERVER}
${BUILD_DIR_SERVER}/server
```

### 项目配置

设置“关于我”页面读取的 `README.md` 文件目录，并放置对应文件

```bash
echo 'export README_DIR=${README_DIR}' >> ~/.bashrc
```

设置文件保存目录环境变量

```bash
echo 'export DOC_PATH=${DOC_PATH}' >> ~/.bashrc     # 文档文件保存目录
echo 'export IMAGE_PATH=${IMAGE_PATH}' >> ~/.bashrc # 图片文件保存目录
source ~/.bashrc
```

为自己创建用户并赋予权限

```postgresql
# create
# drop
# edit
```

### 可选

#### 博客自动同步

博客保存目录连接远程仓库并创建 `auto` 分支

```bash
cd ${DOC_PATH}/blogs
git init
git remote add origin ${REPO_URL}
git checkout -b auto
git commit --allow-empty -m "initial empty commit"
git push -u origin auto
```

设置定时执行

```bash
chmod +x ${PROJECT_PATH}/tools/auto-sync-blogs.sh

crontab -e
* * * * * export DOC_PATH=${DOC_PATH} && /bin/bash ${PROJECT_PATH}/tools/auto-sync-blogs.sh >> ${PROJECT_PATH}/tools/auto-sync-blogs.log 2>&1
```

#### 关于我页面自动拉取Github

定时执行拉取

```bash
chmod +x ${PROJECT_PATH}/tools/pull-readme.sh

crontab -e
0 4 * * * export GITHUB_USER=${GITHUB_USER} README_DIR=${README_DIR} && /bin/bash ${PROJECT_PATH}/tools/pull-readme.sh >> ${PROJECT_PATH}/tools/pull-readme.log 2>&1
```

