# 如意玲珑应用迁移助手（Linyaps Migration Assistant）

面向开放原子开源大赛玲珑赛题的 Web 工具：将 **deb / rpm / AppImage** 安装包快速迁移为
[如意玲珑（Linyaps）](https://linyaps.org.cn/) 应用工程。

上传安装包 → 自动解析元数据与依赖 → **依赖归属可视化分析** → 生成符合官方 1.14.x 规范的
`linglong.yaml` → 一键导出可直接 `ll-builder build` 的完整工程 zip。

## 核心特性

| 特性 | 说明 |
|---|---|
| 三格式解析 | deb（ar+tar 纯标准库）、rpm（lead/header/cpio 纯标准库）、AppImage（squashfs，需服务器有 unsquashfs 时完整解析） |
| 依赖归属分析 | 每个依赖自动划分三档：**base 覆盖**（org.deepin.base 提供）/ **runtime 覆盖**（org.deepin.runtime.dtk 提供）/ **需自带**（自动生成收集脚本），前端树形可视化 |
| linglong.yaml 生成 | 按官方 manifests 规范生成 version/package/command/base/runtime/sources/build；启动命令自动换算为 `/opt/apps/<id>/files/<包内路径>`；版本号自动归一化为四段数字；应用 id 从 Homepage 域名智能反推（如 `io.github.<user>.<repo>`） |
| 构建脚本生成 | deb 用 `dpkg-deb -x`（回退 ar+tar）安装到 `$PREFIX`；需自带依赖在容器内 `apt-get download` 自动收集；desktop 绝对路径图标自动改写 |
| 迁移检查清单 | XDG desktop 文件、hicolor 图标、可执行文件、版本规范等自动检查（通过/注意/待处理/说明） |
| 工程一键导出 | zip 含 linglong.yaml、README 构建指引、deps.txt、resources（原包 desktop/icon）、srcs/ 占位 |

## 架构

```
frontend/  Vue 3 + TypeScript + Vite + Element Plus + CodeMirror(yaml)
backend/   Python 3 + FastAPI
  app/parsers/     deb.py / rpm.py / appimage.py   零外部依赖静态解析
  app/analyzer/    rules_data.yaml 规则库 + 归属分析 + 检查清单
  app/generator/   linglong.yaml 生成 + 工程 zip 打包
  app/api/         FastAPI 路由（/api/analyze、/api/export）
```

## 快速开始

### 后端（端口 8000）

```bash
cd backend
python -m venv ../.venv
../.venv/Scripts/pip install -r requirements.txt   # Linux: ../.venv/bin/pip
../.venv/Scripts/python -m uvicorn app.main:app --port 8000
```

### 前端（端口 5173，已代理 /api 到 8000）

```bash
cd frontend
npm install
npm run dev
```

浏览器打开 http://localhost:5173，拖入 deb 包即可体验完整流程。

### 测试

```bash
cd backend
../.venv/Scripts/python -m pytest tests -q
```

测试使用真实样本包（`tests/fixtures/` 下的 sl deb/rpm）覆盖解析、依赖分类、yaml 生成与 API 全流程。

## 导出工程的使用（在 Linux/deepin 环境执行）

```bash
# 1. 将原始安装包放入 srcs/
# 2. 构建并验证
ll-builder build
ll-builder run --exec <可执行文件名>
ll-builder export        # 生成 .layer
ll-cli install *.layer
ll-cli run <appid>
```

## 说明

- 依赖归属规则库（`backend/app/analyzer/rules_data.yaml`）为启发式起点，可按构建结果持续扩充。
- 工具定位是「迁移提效助手」：输出分析与可直接构建的工程，实际构建结果取决于应用本身。
- 会话缓存存于内存（64 个），服务重启后需重新上传。
