# SPDX-FileCopyrightText: 2026 Yanghanrui666
#
# SPDX-License-Identifier: LGPL-3.0-or-later
"""包分析器：元信息归一化、依赖归属、迁移检查清单、linglong.yaml 建议字段。"""
from __future__ import annotations

import re
from urllib.parse import urlparse

from ..parsers.common import ParsedPackage, normalize_arch
from .rules import classify

_VERSION_SANITIZE_RE = re.compile(r"[^0-9.]")


def _normalize_version(raw: str) -> str:
    """归一化为玲珑建议的四段数字版本：5.02-1 -> 5.2.0.0。"""
    raw = (raw or "").strip().removeprefix("v").removeprefix("V")
    raw = raw.split("-")[0].split("+")[0].split("~")[0].split(":")[-1]
    raw = _VERSION_SANITIZE_RE.sub("", raw.replace("_", "."))
    parts = [p for p in raw.split(".") if p != ""]
    parts = [str(int(p)) if p.isdigit() else "0" for p in parts]
    while len(parts) < 4:
        parts.append("0")
    return ".".join(parts[:4])


def _sanitize_name(raw: str) -> str:
    name = re.sub(r"[^a-zA-Z0-9._+-]", "-", (raw or "app").strip().lower())
    name = re.sub(r"-+", "-", name).strip("-")
    return name or "app"


def _suggest_id(control: dict[str, str]) -> str:
    """优先从 Homepage 域名反推 id，否则用 org.linyaps.<name>。"""
    homepage = control.get("Homepage", "")
    app_name = _sanitize_name(control.get("Package", "") or "app")
    if homepage:
        try:
            url = urlparse(homepage)
            host = url.netloc.split(":")[0].removeprefix("www.")
            if host == "github.com":
                # https://github.com/<owner>/<repo> -> io.github.<owner>.<repo>
                segs = [s for s in url.path.strip("/").split("/") if s]
                if len(segs) >= 2:
                    return f"io.github.{_sanitize_name(segs[0])}.{_sanitize_name(segs[1])}"
            tlds = {"com", "cn", "org", "net", "io", "dev", "edu", "gov"}
            keep = [p for p in reversed(host.split(".")) if p and p not in tlds]
            if keep:
                return ".".join(reversed(keep)) + "." + app_name
        except Exception:
            pass
    return "org.linyaps." + app_name


def _describe_deps(parsed: ParsedPackage) -> list[tuple[str, str, str]]:
    """返回 [(name, constraint, level)]。"""
    seen: set[str] = set()
    result = []
    for name, constraint in parsed.dependencies:
        if name in seen:
            continue
        seen.add(name)
        result.append((name, constraint, classify(name)))
    return result


def _build_checklist(parsed: ParsedPackage, app_id: str, version: str) -> list[dict[str, str]]:
    checks: list[dict[str, str]] = []

    if parsed.desktops:
        icon = parsed.desktops[0].icon
        if icon.startswith("/"):
            checks.append(
                {
                    "key": "desktop-icon",
                    "title": "desktop 文件 Icon 绝对路径",
                    "status": "warn",
                    "detail": f"Icon={icon} 为绝对路径，容器内不可用；生成的构建脚本会自动改写为图标名。",
                }
            )
        else:
            checks.append(
                {
                    "key": "desktop",
                    "title": "desktop 启动文件",
                    "status": "pass",
                    "detail": f"检测到 {len(parsed.desktops)} 个 desktop 文件，符合 XDG 规范。",
                }
            )
    else:
        checks.append(
            {
                "key": "desktop",
                "title": "desktop 启动文件",
                "status": "fail",
                "detail": "未检测到 .desktop 文件；图形应用上架需要补齐（符合 Freedesktop XDG 规范）。",
            }
        )

    if parsed.icons:
        checks.append(
            {
                "key": "icons",
                "title": "应用图标",
                "status": "pass",
                "detail": f"检测到 {len(parsed.icons)} 个 hicolor 应用图标。",
            }
        )
        if not parsed.desktops:
            checks.append(
                {
                    "key": "icons-only",
                    "title": "图标未被 desktop 引用",
                    "status": "info",
                    "detail": "检测到图标但没有 desktop 文件，请确认应用入口方式。",
                }
            )
    else:
        checks.append(
            {
                "key": "icons",
                "title": "应用图标",
                "status": "warn",
                "detail": "未检测到 share/icons/hicolor 下的应用图标，建议补齐。",
            }
        )

    if parsed.binaries:
        checks.append(
            {
                "key": "binaries",
                "title": "可执行文件",
                "status": "pass",
                "detail": f"检测到 {len(parsed.binaries)} 个可执行文件，command 已指向包内真实路径。",
            }
        )
    else:
        checks.append(
            {
                "key": "binaries",
                "title": "可执行文件",
                "status": "fail",
                "detail": "未检测到可执行文件，无法确定启动命令，请人工确认。",
            }
        )

    checks.append(
        {
            "key": "prefix",
            "title": "安装前缀 ($PREFIX)",
            "status": "info",
            "detail": f"玲珑应用文件挂载于 /opt/apps/{app_id}/files，构建脚本已统一安装到 $PREFIX，"
            "无需修改原始包内的 /usr 路径。",
        }
    )

    if parsed.dependencies:
        bundled = [d for d in _describe_deps(parsed) if d[2] == "bundled"]
        checks.append(
            {
                "key": "deps",
                "title": "依赖处理",
                "status": "warn" if bundled else "pass",
                "detail": (
                    f"共 {len(parsed.dependencies)} 个依赖，其中 {len(bundled)} 个需随应用自带，"
                    "构建脚本会尝试在容器内收集；详见依赖分析面板。"
                    if bundled
                    else "全部依赖预计由 base/runtime 提供，无需额外处理。"
                ),
            }
        )

    checks.append(
        {
            "key": "version",
            "title": "版本号规范",
            "status": "pass",
            "detail": f"已归一化为四段数字版本 {version}（玲珑建议四位数字版本）。",
        }
    )
    return checks


def analyze(parsed: ParsedPackage, filename: str) -> dict:
    control = parsed.control
    name = _sanitize_name(control.get("Package", "") or filename.rsplit(".", 1)[0])
    version = _normalize_version(control.get("Version", "") or "0")
    app_id = _suggest_id(control)

    deps = _describe_deps(parsed)
    arch_raw = control.get("Architecture", "")

    metadata = {
        "package_format": parsed.package_format,
        "name": name,
        "version": version,
        "description": control.get("Description", "") or control.get("Summary", ""),
        "architecture": normalize_arch(arch_raw) if arch_raw else "x86_64",
        "raw_arch": arch_raw,
        "maintainer": control.get("Maintainer", ""),
        "homepage": control.get("Homepage", ""),
        "license": control.get("License", ""),
        "raw": {k: v for k, v in control.items() if k not in ("Description",)},
    }

    desktop_entries = [
        {
            "path": d.path,
            "name": d.name,
            "exec": d.exec_,
            "icon": d.icon,
            "wm_class": d.wm_class,
        }
        for d in parsed.desktops
    ]
    files = {
        "binaries": parsed.binaries[:200],
        "desktop_entries": desktop_entries,
        "icons": parsed.icons[:100],
        "libraries": parsed.libraries[:200],
        "file_count": parsed.file_count,
        "total_size": parsed.total_size,
        "notes": parsed.notes,
    }

    dependencies = [
        {"name": n, "constraint": c, "level": lv} for n, c, lv in deps
    ]
    checklist = _build_checklist(parsed, app_id, version)

    # 启动命令：优先 desktop Exec 对应的可执行文件在包内的真实路径，
    # 否则取第一个可执行文件。deb/rpm 原样解压保留 usr/ 层级，
    # 容器内实际路径为 /opt/apps/<id>/files/<包内路径>。
    exec_cmd = ""
    if desktop_entries and desktop_entries[0]["exec"]:
        exec_cmd = desktop_entries[0]["exec"].split()[0]
        exec_cmd = exec_cmd.rsplit("/", 1)[-1]
    command = ""
    if parsed.package_format == "appimage":
        # AppImage 构建脚本会解压到 $PREFIX/app，入口为 AppRun
        command = f"/opt/apps/{app_id}/files/app/AppRun"
    elif parsed.binaries:
        binary_path = ""
        if exec_cmd:
            matches = [b for b in parsed.binaries if b.rsplit("/", 1)[-1] == exec_cmd]
            if matches:
                binary_path = matches[0]
        if not binary_path:
            binary_path = parsed.binaries[0]
        command = f"/opt/apps/{app_id}/files/{binary_path}"

    runtime_needed = any(d["level"] == "runtime" for d in dependencies)
    suggested = {
        "id": app_id,
        "name": name,
        "version": version,
        "description": control.get("Description", "") or control.get("Summary", ""),
        "command": command,
        "base": "org.deepin.base/23.1.0",
        "runtime": "org.deepin.runtime.dtk/23.1.0" if runtime_needed else "",
        "bundled_deps": [d["name"] for d in dependencies if d["level"] == "bundled"],
    }

    return {
        "metadata": metadata,
        "files": files,
        "dependencies": dependencies,
        "checklist": checklist,
        "suggested": suggested,
    }
