# SPDX-FileCopyrightText: 2026 Yanghanrui666
#
# SPDX-License-Identifier: LGPL-3.0-or-later
"""RPM 包解析：纯标准库实现（lead + header + cpio newc payload），零外部依赖。

RPM 文件结构：
  lead(96B, magic ed ab ee db)
  signature header（header 结构，8 字节对齐）
  main header（header 结构）
  payload：压缩 cpio (newc 格式)
"""
from __future__ import annotations

import bz2
import gzip
import io
import lzma

from .common import DesktopEntryData, ParsedPackage

RPM_LEAD_MAGIC = b"\xed\xab\xee\xdb"
HEADER_MAGIC = b"\x8e\xad\xe8"

# 常用 RPM tag
TAG_NAME = 1000
TAG_VERSION = 1001
TAG_RELEASE = 1002
TAG_SUMMARY = 1004
TAG_DESCRIPTION = 1005
TAG_LICENSE = 1014
TAG_PACKAGER = 1015
TAG_URL = 1020
TAG_ARCH = 1022
TAG_FILENAMES = 5000
TAG_REQUIRENAME = 1049
TAG_REQUIREVERSION = 1050

# 资源收集上限
MAX_RESOURCE_FILE_SIZE = 1 * 1024 * 1024
MAX_RESOURCE_TOTAL_SIZE = 8 * 1024 * 1024


def is_rpm(data: bytes) -> bool:
    return data[:4] == RPM_LEAD_MAGIC


def _parse_header(data: bytes, offset: int) -> tuple[dict[int, object], int]:
    """解析一个 header 段，返回 (tag->值, 下一段偏移)。"""
    if data[offset : offset + 3] != HEADER_MAGIC:
        raise ValueError("RPM header 魔数不匹配，文件可能损坏")
    index_count = int.from_bytes(data[offset + 8 : offset + 12], "big")
    data_size = int.from_bytes(data[offset + 12 : offset + 16], "big")
    index_base = offset + 16
    data_base = index_base + index_count * 16
    tags: dict[int, object] = {}

    for i in range(index_count):
        entry = index_base + i * 16
        tag = int.from_bytes(data[entry : entry + 4], "big")
        rtype = int.from_bytes(data[entry + 4 : entry + 8], "big")
        doff = int.from_bytes(data[entry + 8 : entry + 12], "big")
        count = int.from_bytes(data[entry + 12 : entry + 16], "big")
        pos = data_base + doff
        try:
            if rtype == 6:  # STRING
                end = data.index(b"\x00", pos)
                tags[tag] = data[pos:end].decode("utf-8", "replace")
            elif rtype in (8, 9):  # STRING_ARRAY / I18NSTRING
                items: list[str] = []
                cursor = pos
                for _ in range(count):
                    end = data.index(b"\x00", cursor)
                    items.append(data[cursor:end].decode("utf-8", "replace"))
                    cursor = end + 1
                tags[tag] = items
            elif rtype == 4:  # INT32
                tags[tag] = [
                    int.from_bytes(data[pos + j * 4 : pos + j * 4 + 4], "big")
                    for j in range(count)
                ]
            elif rtype == 3:  # INT16
                tags[tag] = [
                    int.from_bytes(data[pos + j * 2 : pos + j * 2 + 2], "big")
                    for j in range(count)
                ]
            elif rtype == 2:  # INT8
                tags[tag] = list(data[pos : pos + count])
        except (ValueError, IndexError):
            continue
    return tags, data_base + data_size


def _decompress_payload(data: bytes) -> bytes:
    if data[:2] == b"\x1f\x8b":
        return gzip.decompress(data)
    if data[:6] == b"\xfd7zXZ\x00":
        return lzma.decompress(data)
    if data[:3] == b"BZh":
        return bz2.decompress(data)
    if data[:4] == b"\x28\xb5\x2f\xfd":
        try:
            import zstandard
        except ImportError as exc:  # pragma: no cover
            raise RuntimeError(
                "检测到 zstd 压缩的 rpm 包，请在后端环境安装 zstandard：pip install zstandard"
            ) from exc
        return zstandard.ZstdDecompressor().stream_reader(io.BytesIO(data)).read()
    if data[:2] == b"\x5d\x00":  # lzma alone
        return lzma.decompress(data, format=lzma.FORMAT_ALONE)
    raise ValueError("无法识别的 RPM payload 压缩格式")


def _iter_cpio_newc(data: bytes):
    """遍历 cpio newc 归档，yield (name, content)。"""
    pos = 0
    n = len(data)
    while pos + 110 <= n:
        magic = data[pos : pos + 6]
        if magic not in (b"070701", b"070702"):
            return
        fields = [int(data[pos + 6 + i * 8 : pos + 14 + i * 8], 16) for i in range(13)]
        filesize = fields[6]
        namesize = fields[11]
        name_start = pos + 110
        name = data[name_start : name_start + max(namesize - 1, 0)].decode(
            "utf-8", "replace"
        )
        if name == "TRAILER!!!":
            return
        data_start = name_start + namesize
        data_start += (-(110 + namesize)) % 4
        content = data[data_start : data_start + filesize]
        yield name, content
        pos = data_start + filesize + (filesize % 4 and 4 - filesize % 4 or 0)


def parse_rpm(data: bytes) -> ParsedPackage:
    if not is_rpm(data):
        raise ValueError("不是合法的 RPM 包（lead 魔数不匹配）")

    pkg = ParsedPackage(package_format="rpm")
    # 跳过 lead（96B）解析 signature header，其后按 8 字节对齐才是主 header
    tags, offset = _parse_header(data, 96)
    offset = (offset + 7) & ~7
    tags, offset = _parse_header(data, offset)
    payload = data[offset:]

    def _s(tag: int, default: str = "") -> str:
        value = tags.get(tag)
        if isinstance(value, str):
            return value
        if isinstance(value, list) and value:
            return str(value[0])
        return default

    pkg.control = {
        "Package": _s(TAG_NAME),
        "Version": _s(TAG_VERSION),
        "Release": _s(TAG_RELEASE),
        "Summary": _s(TAG_SUMMARY),
        "Description": _s(TAG_DESCRIPTION),
        "License": _s(TAG_LICENSE),
        "Maintainer": _s(TAG_PACKAGER),
        "Homepage": _s(TAG_URL),
        "Architecture": _s(TAG_ARCH),
    }

    requires = tags.get(TAG_REQUIRENAME)
    versions = tags.get(TAG_REQUIREVERSION, [])
    if isinstance(requires, list):
        import re

        for idx, name in enumerate(requires):
            if not name or name.startswith(("rpmlib(", "rtld(", "config(")):
                continue
            clean = re.sub(r"\([^)]*\)", "", name).strip()  # 去掉 (64bit) 等标志
            if not clean:
                continue
            constraint = versions[idx] if idx < len(versions) else ""
            pkg.dependencies.append((clean, constraint))

    try:
        raw = _decompress_payload(payload)
    except (ValueError, RuntimeError) as exc:
        pkg.notes.append(f"payload 解压失败（仅提供元信息）：{exc}")
        return pkg

    resource_total = 0
    for name, content in _iter_cpio_newc(raw):
        clean = name.lstrip("./")
        if not clean or clean.endswith("/"):
            continue
        pkg.file_count += 1
        pkg.total_size += len(content)
        if clean.startswith("usr/bin/") or clean.startswith("bin/"):
            pkg.binaries.append(clean)
        elif clean.startswith(("usr/games/", "usr/sbin/", "sbin/")):
            pkg.binaries.append(clean)
        elif clean.startswith("usr/lib/") or clean.startswith("lib/"):
            base = clean.rsplit("/", 1)[-1]
            if base.endswith(".so") or ".so." in base:
                pkg.libraries.append(clean)
        elif "share/icons/" in clean and "/apps/" in clean:
            pkg.icons.append(clean)
        elif clean.startswith("usr/share/applications/") and clean.endswith(".desktop"):
            entry = {}
            try:
                from .deb import parse_desktop

                entry = parse_desktop(content.decode("utf-8", "replace"))
            except Exception:
                entry = {}
            pkg.desktops.append(
                DesktopEntryData(
                    path=clean,
                    name=entry.get("Name", ""),
                    exec_=entry.get("Exec", ""),
                    icon=entry.get("Icon", ""),
                    wm_class=entry.get("StartupWMClass", ""),
                )
            )
            if (
                len(content) <= MAX_RESOURCE_FILE_SIZE
                and resource_total + len(content) <= MAX_RESOURCE_TOTAL_SIZE
            ):
                pkg.resources[clean] = content
                resource_total += len(content)
    return pkg
