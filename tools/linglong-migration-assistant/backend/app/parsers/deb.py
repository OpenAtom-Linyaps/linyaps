"""deb 包解析：纯标准库实现（ar 归档 + tar），零外部依赖。

结构参考 Debian .deb 规范：
  <ar 归档>
    debian-binary
    control.tar.{gz|xz|zst|bz2}   -> ./control 等
    data.tar.{gz|xz|zst|bz2}      -> ./usr/... 实际文件
"""
from __future__ import annotations

import bz2
import gzip
import io
import lzma
import tarfile

from .common import DesktopEntryData, ParsedPackage

AR_MAGIC = b"!<arch>\n"
AR_HEADER_SIZE = 60

# 资源收集上限，防止超包占用过多内存
MAX_RESOURCE_FILE_SIZE = 1 * 1024 * 1024
MAX_RESOURCE_TOTAL_SIZE = 8 * 1024 * 1024


def is_deb(data: bytes) -> bool:
    return data[:8] == AR_MAGIC


def _iter_ar_members(data: bytes):
    pos = 8
    n = len(data)
    while pos + AR_HEADER_SIZE <= n:
        header = data[pos : pos + AR_HEADER_SIZE]
        name = header[:16].decode("ascii", "replace").rstrip()
        try:
            size = int(header[48:58].decode("ascii").strip())
        except ValueError:
            break
        body = data[pos + AR_HEADER_SIZE : pos + AR_HEADER_SIZE + size]
        yield name.rstrip("/"), body
        pos += AR_HEADER_SIZE + size
        if pos & 1:  # ar 成员按 2 字节对齐
            pos += 1


def _decompress(data: bytes) -> bytes:
    if data[:6] == b"\xfd7zXZ\x00":
        return lzma.decompress(data)
    if data[:2] == b"\x1f\x8b":
        return gzip.decompress(data)
    if data[:3] == b"BZh":
        return bz2.decompress(data)
    if data[:4] == b"\x28\xb5\x2f\xfd":
        try:
            import zstandard
        except ImportError as exc:  # pragma: no cover
            raise RuntimeError(
                "检测到 zstd 压缩的 deb 包，请在后端环境安装 zstandard：pip install zstandard"
            ) from exc
        return zstandard.ZstdDecompressor().stream_reader(io.BytesIO(data)).read()
    return data


def _open_tar(raw: bytes) -> tarfile.TarFile:
    return tarfile.open(fileobj=io.BytesIO(_decompress(raw)), mode="r:")


def parse_control(text: str) -> dict[str, str]:
    """解析 Debian control 文件（Key: Value，续行以空格开头）。"""
    fields: dict[str, str] = {}
    current: str | None = None
    for line in text.splitlines():
        if not line.strip():
            current = None
            continue
        if line[:1] in (" ", "\t") and current:
            fields[current] += "\n" + line.strip()
        elif ":" in line:
            key, _, value = line.partition(":")
            current = key.strip()
            fields[current] = value.strip()
    return fields


def parse_depends(value: str) -> list[tuple[str, str]]:
    """解析 Depends 字段：'libc6 (>= 2.38), libfoo | libbar'。"""
    result: list[tuple[str, str]] = []
    if not value:
        return result
    for group in value.split(","):
        group = group.strip()
        if not group:
            continue
        # 只取第一候选（alternatives 的第一个通常为主依赖）
        first = group.split("|")[0].strip()
        name = first
        constraint = ""
        if first.startswith("("):  # 形如 "(>= 2.38)" 无名字，跳过
            continue
        if "(" in first:
            name, _, rest = first.partition("(")
            name = name.strip()
            constraint = rest.rstrip(")").strip()
        name = name.split("[")[0].strip()  # 去掉架构限定
        if name:
            result.append((name, constraint))
    return result


def parse_desktop(text: str) -> dict[str, str]:
    """提取 [Desktop Entry] 段的键值。"""
    entry: dict[str, str] = {}
    in_main = False
    for line in text.splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("["):
            in_main = line == "[Desktop Entry]"
            continue
        if in_main and "=" in line:
            key, _, value = line.partition("=")
            entry.setdefault(key.strip(), value.strip())
    return entry


def parse_deb(data: bytes) -> ParsedPackage:
    if not is_deb(data):
        raise ValueError("不是合法的 deb 包（ar 魔数不匹配）")

    control_raw: bytes | None = None
    data_raw: bytes | None = None
    for name, body in _iter_ar_members(data):
        if name == "debian-binary":
            continue
        if name.startswith("control.tar"):
            control_raw = body
        elif name.startswith("data.tar"):
            data_raw = body

    if control_raw is None:
        raise ValueError("deb 包缺少 control.tar 成员，文件可能不完整")

    pkg = ParsedPackage(package_format="deb")

    with _open_tar(control_raw) as tf:
        control_text: str | None = None
        for m in tf.getmembers():
            if m.name.lstrip("./") == "control":
                fh = tf.extractfile(m)
                control_text = fh.read().decode("utf-8", "replace") if fh else None
                break
        if control_text is None:
            raise ValueError("control.tar 中未找到 control 文件")
        pkg.control = parse_control(control_text)

    for key in ("Depends", "Pre-Depends"):
        pkg.dependencies.extend(parse_depends(pkg.control.get(key, "")))

    if data_raw is None:
        pkg.notes.append("deb 包未包含 data.tar，无法分析应用文件")
        return pkg

    resource_total = 0
    with _open_tar(data_raw) as tf:
        for m in tf.getmembers():
            name = m.name.lstrip("./")
            if not name or name.endswith("/"):
                continue
            pkg.file_count += 1
            pkg.total_size += m.size
            if name.startswith("usr/bin/") or name.startswith("bin/"):
                pkg.binaries.append(name)
            elif name.startswith(("usr/games/", "usr/sbin/", "sbin/")):
                pkg.binaries.append(name)
            elif name.startswith("usr/lib/") or name.startswith("lib/"):
                base = name.rsplit("/", 1)[-1]
                if base.endswith(".so") or ".so." in base:
                    pkg.libraries.append(name)
            elif "share/icons/" in name and "/apps/" in name:
                pkg.icons.append(name)
            elif name.startswith("usr/share/applications/") and name.endswith(".desktop"):
                fh = tf.extractfile(m)
                content = fh.read() if fh else b""
                entry = parse_desktop(content.decode("utf-8", "replace"))
                pkg.desktops.append(
                    DesktopEntryData(
                        path=name,
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
                    pkg.resources[name] = content
                    resource_total += len(content)
    return pkg
