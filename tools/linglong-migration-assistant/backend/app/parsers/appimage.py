# SPDX-FileCopyrightText: 2026 Yanghanrui666
#
# SPDX-License-Identifier: LGPL-3.0-or-later
"""AppImage 解析：尽力而为。

- 定位 ELF + squashfs 魔数（hsqs）
- 服务器有 unsquashfs 时完整提取 desktop/icon/bin
- 无 unsquashfs 时仅报告基本信息
"""
from __future__ import annotations

import shutil
import subprocess
import tempfile
from pathlib import Path

from .common import DesktopEntryData, ParsedPackage


def _squashfs_offset(data: bytes) -> int | None:
    pos = data.find(b"hsqs")
    return pos if pos > 0 else None


def parse_appimage(data: bytes, filename: str = "app.AppImage") -> ParsedPackage:
    pkg = ParsedPackage(package_format="appimage")
    name = Path(filename).stem
    pkg.control = {
        "Package": name,
        "Version": "",
        "Summary": f"AppImage 应用 {name}",
    }

    if data[:4] != b"\x7fELF":
        pkg.notes.append("文件不是 ELF 格式，可能不是 AppImage")
        return pkg

    offset = _squashfs_offset(data[: 4 * 1024 * 1024])
    if offset is None:
        pkg.notes.append("未定位到 squashfs 数据区（hsqs 魔数），仅提供基础信息")
        return pkg

    unsquashfs = shutil.which("unsquashfs")
    if unsquashfs is None:
        pkg.notes.append(
            "服务器未安装 unsquashfs，无法提取 AppImage 内部文件。"
            "在 Linux 上安装 squashfs-tools 后可获得完整分析。"
        )
        return pkg

    with tempfile.TemporaryDirectory(prefix="appimage_ma_") as tmp:
        img_path = Path(tmp) / "image.AppImage"
        sq_path = Path(tmp) / "squashfs-root"
        img_path.write_bytes(data[offset:])
        result = subprocess.run(
            [unsquashfs, "-d", str(sq_path), str(img_path)],
            capture_output=True,
            timeout=120,
        )
        if result.returncode != 0:
            pkg.notes.append(
                f"unsquashfs 提取失败：{result.stderr.decode('utf-8', 'replace')[:200]}"
            )
            return pkg

        for path in sorted(sq_path.rglob("*")):
            if not path.is_file():
                continue
            rel = path.relative_to(sq_path).as_posix()
            pkg.file_count += 1
            pkg.total_size += path.stat().st_size
            if rel.startswith("usr/bin/") or rel.startswith("bin/"):
                pkg.binaries.append(rel)
            elif rel.startswith(("usr/games/", "usr/sbin/", "sbin/")):
                pkg.binaries.append(rel)
            elif "share/icons/" in rel and "/apps/" in rel:
                pkg.icons.append(rel)
            elif rel.startswith("usr/share/applications/") and rel.endswith(".desktop"):
                try:
                    from .deb import parse_desktop

                    entry = parse_desktop(path.read_text("utf-8", "replace"))
                except Exception:
                    entry = {}
                pkg.desktops.append(
                    DesktopEntryData(
                        path=rel,
                        name=entry.get("Name", ""),
                        exec_=entry.get("Exec", ""),
                        icon=entry.get("Icon", ""),
                        wm_class=entry.get("StartupWMClass", ""),
                    )
                )
                try:
                    if path.stat().st_size <= 1024 * 1024:
                        pkg.resources[rel] = path.read_bytes()
                except OSError:
                    pass
    return pkg
