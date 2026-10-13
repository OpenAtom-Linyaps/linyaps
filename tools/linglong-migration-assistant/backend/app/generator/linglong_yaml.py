# SPDX-FileCopyrightText: 2026 Yanghanrui666
#
# SPDX-License-Identifier: LGPL-3.0-or-later
"""linglong.yaml 生成器。

遵循如意玲珑 1.14.x 官方规范（https://linyaps.org.cn/guide/building/manifests.html）：
  version / package / command / base / runtime / sources / build
"""
from __future__ import annotations

import io

import yaml


class _Literal(str):
    """多行文本用块标量（|）表示。"""


class _Dumper(yaml.SafeDumper):
    pass


_Dumper.add_representer(_Literal, lambda d, s: d.represent_scalar("tag:yaml.org,2002:str", s, style="|"))


def _build_script_deb(bundled_deps: list[str]) -> str:
    script = """set -e
# ===== 安装主包（deb）到 $PREFIX =====
PKG_FILE=$(ls /project/linglong/sources/srcs/*.deb 2>/dev/null | head -n 1)
if [ -z "$PKG_FILE" ]; then
  echo "错误：请在工程 srcs/ 目录放置原始 deb 包后重新构建" >&2
  exit 1
fi
mkdir -p ${PREFIX}
if command -v dpkg-deb >/dev/null 2>&1; then
  dpkg-deb -x "$PKG_FILE" ${PREFIX}
else
  mkdir -p /tmp/main-pkg && cd /tmp/main-pkg
  ar x "$PKG_FILE"
  tar -xf data.tar.* -C ${PREFIX}
fi
"""
    if bundled_deps:
        deps = " ".join(bundled_deps)
        script += f"""
# ===== 收集需自带的依赖（共 {len(bundled_deps)} 个）=====
mkdir -p /tmp/linglong-deps && cd /tmp/linglong-deps
apt-get download {deps} || echo "警告：apt-get download 失败，请在联网环境构建或手动放置依赖 deb"
for d in *.deb; do
  if command -v dpkg-deb >/dev/null 2>&1; then dpkg-deb -x "$d" ${{PREFIX}}; else ar x "$d" && tar -xf data.tar.* -C ${{PREFIX}}; fi
done
"""
    script += """
# ===== 规范化 desktop 图标路径（绝对路径 -> 图标名）=====
if ls ${PREFIX}/share/applications/*.desktop >/dev/null 2>&1; then
  sed -i 's|^Icon=/usr/.*/\\([^/]*\\)$|Icon=\\1|' ${PREFIX}/share/applications/*.desktop || true
fi
"""
    return script


def _build_script_rpm(bundled_deps: list[str]) -> str:
    script = """set -e
# ===== 安装主包（rpm）到 $PREFIX =====
PKG_FILE=$(ls /project/linglong/sources/srcs/*.rpm 2>/dev/null | head -n 1)
if [ -z "$PKG_FILE" ]; then
  echo "错误：请在工程 srcs/ 目录放置原始 rpm 包后重新构建" >&2
  exit 1
fi
mkdir -p ${PREFIX}
mkdir -p /tmp/main-pkg && cd /tmp/main-pkg
rpm2cpio "$PKG_FILE" | cpio -idm -D ${PREFIX} 2>/dev/null \\
  || { rpm2cpio "$PKG_FILE" > payload.cpio && cpio -idm -D ${PREFIX} < payload.cpio; }
"""
    script += """
# ===== 规范化 desktop 图标路径（绝对路径 -> 图标名）=====
if ls ${PREFIX}/usr/share/applications/*.desktop >/dev/null 2>&1; then
  sed -i 's|^Icon=/usr/.*/\\([^/]*\\)$|Icon=\\1|' ${PREFIX}/usr/share/applications/*.desktop || true
fi
"""
    return script


def _build_script_appimage() -> str:
    script = """set -e
# ===== 提取 AppImage 到 $PREFIX/app =====
PKG_FILE=$(ls /project/linglong/sources/srcs/*.AppImage 2>/dev/null | head -n 1)
if [ -z "$PKG_FILE" ]; then
  echo "错误：请在工程 srcs/ 目录放置 AppImage 文件后重新构建" >&2
  exit 1
fi
chmod +x "$PKG_FILE"
mkdir -p ${PREFIX}
cd ${PREFIX}
"$PKG_FILE" --appimage-extract >/dev/null
mv squashfs-root app
# 将 desktop/icon 暴露到标准 XDG 目录
mkdir -p ${PREFIX}/share/applications ${PREFIX}/share/icons
cp -r app/usr/share/applications/*.desktop ${PREFIX}/share/applications/ 2>/dev/null || true
cp -r app/usr/share/icons/* ${PREFIX}/share/icons/ 2>/dev/null || true
"""
    return script


def build_manifest_dict(suggested: dict, dependencies: list[dict], package_format: str) -> dict:
    """根据分析建议构建 linglong.yaml 的字典结构。"""
    app_id = suggested["id"]
    manifest: dict = {
        "version": "1",
        "package": {
            "id": app_id,
            "name": suggested["name"],
            "version": suggested["version"],
            "kind": "app",
            "description": suggested.get("description", "Migrated application."),
        },
        "command": [suggested["command"]] if suggested.get("command") else [],
        "base": suggested.get("base") or "org.deepin.base/23.1.0",
    }
    if suggested.get("runtime"):
        manifest["runtime"] = suggested["runtime"]
    manifest["sources"] = [
        {"kind": "local", "name": "srcs", "directory": "./srcs"}
    ]

    bundled = [d["name"] for d in dependencies if d["level"] == "bundled"]
    if package_format == "deb":
        build = _build_script_deb(bundled)
    elif package_format == "rpm":
        build = _build_script_rpm(bundled)
    else:
        build = _build_script_appimage()
    manifest["build"] = _Literal(build)
    return manifest


def render_yaml(manifest: dict) -> str:
    return yaml.dump(
        manifest,
        Dumper=_Dumper,
        sort_keys=False,
        allow_unicode=True,
        default_flow_style=False,
        width=4096,
    )


def generate_yaml(suggested: dict, dependencies: list[dict], package_format: str) -> str:
    return render_yaml(build_manifest_dict(suggested, dependencies, package_format))
