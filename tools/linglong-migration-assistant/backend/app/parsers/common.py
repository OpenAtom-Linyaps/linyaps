# SPDX-FileCopyrightText: 2026 Yanghanrui666
#
# SPDX-License-Identifier: LGPL-3.0-or-later
"""解析层公共结构：统一的解析结果与格式探测。"""
from __future__ import annotations

from dataclasses import dataclass, field


@dataclass
class DesktopEntryData:
    path: str
    name: str = ""
    exec_: str = ""
    icon: str = ""
    wm_class: str = ""


@dataclass
class ParsedPackage:
    """三种包格式的统一解析结果。"""

    package_format: str  # deb | rpm | appimage
    control: dict[str, str] = field(default_factory=dict)
    binaries: list[str] = field(default_factory=list)
    libraries: list[str] = field(default_factory=list)
    icons: list[str] = field(default_factory=list)
    desktops: list[DesktopEntryData] = field(default_factory=list)
    dependencies: list[tuple[str, str]] = field(default_factory=list)  # (name, constraint)
    file_count: int = 0
    total_size: int = 0
    notes: list[str] = field(default_factory=list)
    resources: dict[str, bytes] = field(default_factory=dict)  # 导出用：desktop/icon 原始内容


def detect_format(data: bytes) -> str:
    """按魔数探测包格式。"""
    if data[:8] == b"!<arch>\n":
        return "deb"
    if data[:4] == b"\xed\xab\xee\xdb":
        return "rpm"
    if data[:4] == b"\x7fELF":
        return "appimage"
    raise ValueError(
        "无法识别的包格式：支持 deb / rpm / AppImage。"
        "请确认上传的是完整的软件包文件。"
    )


# 架构名归一化：原始架构 -> 玲珑架构
ARCH_MAP = {
    "amd64": "x86_64",
    "x86_64": "x86_64",
    "arm64": "arm64",
    "aarch64": "arm64",
    "i386": "x86_64",
    "i686": "x86_64",
    "mips64el": "mips64el",
    "loongarch64": "loongarch64",
    "riscv64": "riscv64",
    "sw_64": "sw_64",
}


def normalize_arch(raw: str) -> str:
    return ARCH_MAP.get(raw.strip().lower(), raw.strip().lower() or "x86_64")
