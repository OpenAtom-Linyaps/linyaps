"""迁移工程 zip 打包导出。"""
from __future__ import annotations

import io
import zipfile

import yaml


def _readme(project_name: str, package_format: str, bundled_deps: list[str]) -> str:
    deps_note = (
        f"- 需自带的依赖（{len(bundled_deps)} 个）：{'、'.join(bundled_deps) or '无'}\n"
        "  构建脚本会在容器内通过 `apt-get download` 自动收集；离线环境请手动放入 srcs/ 并调整脚本。\n"
        if bundled_deps
        else "- 所有依赖预计由 base/runtime 提供，无需额外处理。\n"
    )
    return f"""# {project_name} 玲珑迁移工程

由「如意玲珑应用迁移助手」生成。

## 工程结构

```
.
├── linglong.yaml        # 玲珑构建配置（可在迁移助手中继续编辑）
├── README.md            # 本文件
├── deps.txt             # 需自带的依赖清单
├── resources/           # 从原包提取的 desktop/icon（供参考与人工修补）
└── srcs/                # 请将原始安装包放到这里
    └── README.txt
```

## 构建步骤

1. 将原始安装包（.{package_format}）复制到 `srcs/` 目录。
2. 在 Linux（deepin / UOS 等）环境安装玲珑构建工具：
   ```bash
   sudo apt install linglong-builder
   ```
3. 在本工程目录执行构建：
   ```bash
   ll-builder build
   ```
4. 容器内测试运行：
   ```bash
   ll-builder run --exec <可执行文件名>
   ```
5. 导出 layer 安装包：
   ```bash
   ll-builder export
   # 生成 <appid>_<version>_<arch>.layer
   ```
6. 安装验证：
   ```bash
   ll-cli install *.layer
   ll-cli run <appid>
   ```

## 依赖说明

{deps_note}
## 常见问题

- 构建首次运行需下载 base/runtime，耗时较长。
- 若提示缺少库，将对应 deb 放入 srcs/ 并在 build 段补充解压命令，或直接追加到 deps.txt 对应 apt-get download 列表。
- desktop 文件中的绝对路径图标已由构建脚本自动改写。
"""


def _srcs_readme(package_format: str) -> str:
    return f"请将原始 {package_format} 安装包复制到本目录（保持扩展名 .{package_format}）。\n"


def build_project_zip(
    yaml_content: str,
    package_format: str,
    bundled_deps: list[str],
    resources: dict[str, bytes],
) -> bytes:
    """打包迁移工程 zip，返回字节流。"""
    manifest = yaml.safe_load(yaml_content)
    app_id = manifest.get("package", {}).get("id", "migrated-app")
    project_name = app_id.split(".")[-1] or "migrated-app"

    buf = io.BytesIO()
    with zipfile.ZipFile(buf, "w", zipfile.ZIP_DEFLATED) as zf:
        zf.writestr(f"{project_name}/linglong.yaml", yaml_content)
        zf.writestr(
            f"{project_name}/README.md",
            _readme(project_name, package_format, bundled_deps),
        )
        zf.writestr(
            f"{project_name}/deps.txt",
            "\n".join(bundled_deps) + ("\n" if bundled_deps else ""),
        )
        zf.writestr(f"{project_name}/srcs/README.txt", _srcs_readme(package_format))
        for path, content in resources.items():
            zf.writestr(f"{project_name}/resources/{path}", content)
    return buf.getvalue()
