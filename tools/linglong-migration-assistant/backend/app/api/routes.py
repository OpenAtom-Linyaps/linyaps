# SPDX-FileCopyrightText: 2026 Yanghanrui666
#
# SPDX-License-Identifier: LGPL-3.0-or-later
"""API 路由：分析、导出、会话缓存。"""
from __future__ import annotations

import uuid
from dataclasses import dataclass, field

from fastapi import APIRouter, File, HTTPException, UploadFile
from fastapi.responses import StreamingResponse

from ..analyzer.analyzer import analyze
from ..generator.linglong_yaml import generate_yaml
from ..generator.project import build_project_zip
from ..parsers import appimage as appimage_parser
from ..parsers import deb as deb_parser
from ..parsers import rpm as rpm_parser
from ..parsers.common import ParsedPackage, detect_format
from ..schemas import AnalysisResult, ExportRequest

router = APIRouter(prefix="/api")

MAX_UPLOAD_SIZE = 2 * 1024 * 1024 * 1024  # 2GB


@dataclass
class SessionData:
    analysis: dict
    resources: dict[str, bytes] = field(default_factory=dict)
    package_format: str = "deb"
    filename: str = ""


# 简单内存会话缓存（演示用途；生产可换 Redis）
_SESSIONS: dict[str, SessionData] = {}
_MAX_SESSIONS = 64


def _save_session(data: SessionData) -> str:
    if len(_SESSIONS) >= _MAX_SESSIONS:
        _SESSIONS.pop(next(iter(_SESSIONS)))
    token = uuid.uuid4().hex
    _SESSIONS[token] = data
    return token


@router.post("/analyze", response_model=AnalysisResult)
async def analyze_package(file: UploadFile = File(...)):
    raw = await file.read()
    if not raw:
        raise HTTPException(400, "上传文件为空")
    if len(raw) > MAX_UPLOAD_SIZE:
        raise HTTPException(413, "文件过大（上限 2GB）")

    try:
        fmt = detect_format(raw)
    except ValueError as exc:
        raise HTTPException(400, str(exc))

    try:
        if fmt == "deb":
            parsed: ParsedPackage = deb_parser.parse_deb(raw)
        elif fmt == "rpm":
            parsed = rpm_parser.parse_rpm(raw)
        else:
            parsed = appimage_parser.parse_appimage(raw, file.filename or "app.AppImage")
    except ValueError as exc:
        raise HTTPException(400, f"包解析失败：{exc}")
    except RuntimeError as exc:
        raise HTTPException(400, str(exc))

    result = analyze(parsed, file.filename or "")
    token = _save_session(
        SessionData(
            analysis=result,
            resources=parsed.resources,
            package_format=fmt,
            filename=file.filename or "",
        )
    )
    # 建议字段之外附带 yaml 初稿，便于前端直接进入编辑
    result["suggested"]["yaml"] = generate_yaml(
        result["suggested"], result["dependencies"], fmt
    )
    result["suggested"]["token"] = token
    return result


@router.post("/export")
async def export_project(req: ExportRequest):
    session = _SESSIONS.get(req.token)
    if session is None:
        raise HTTPException(404, "会话已过期，请重新上传并分析")

    import yaml as _yaml

    try:
        manifest = _yaml.safe_load(req.yaml)
        if not isinstance(manifest, dict) or "package" not in manifest:
            raise ValueError("缺少 package 字段")
    except Exception as exc:
        raise HTTPException(400, f"linglong.yaml 格式错误：{exc}")

    suggested = session.analysis.get("suggested", {})
    bundled = suggested.get("bundled_deps", [])
    zip_bytes = build_project_zip(
        req.yaml, session.package_format, bundled, session.resources
    )

    project_name = (manifest.get("package", {}) or {}).get("id", "migrated-app")
    filename = f"{project_name}-linglong-project.zip"

    import io

    return StreamingResponse(
        io.BytesIO(zip_bytes),
        media_type="application/zip",
        headers={"Content-Disposition": f'attachment; filename="{filename}"'},
    )


@router.get("/health")
async def health():
    return {"status": "ok"}
