# SPDX-FileCopyrightText: 2026 Yanghanrui666
#
# SPDX-License-Identifier: LGPL-3.0-or-later
"""Pydantic 数据模型定义。"""
from __future__ import annotations

from pydantic import BaseModel, Field


class PackageMetadata(BaseModel):
    """软件包元信息。"""

    package_format: str  # deb | rpm | appimage
    name: str
    version: str
    description: str = ""
    architecture: str = ""  # 归一化后的玲珑架构
    raw_arch: str = ""
    maintainer: str = ""
    homepage: str = ""
    license: str = ""
    raw: dict[str, str] = Field(default_factory=dict)


class DesktopEntry(BaseModel):
    name: str = ""
    exec_: str = Field(default="", alias="exec")
    icon: str = ""
    wm_class: str = ""
    path: str = ""  # 在包内的文件路径

    model_config = {"populate_by_name": True}


class DetectedFiles(BaseModel):
    binaries: list[str] = Field(default_factory=list)
    desktop_entries: list[DesktopEntry] = Field(default_factory=list)
    icons: list[str] = Field(default_factory=list)
    libraries: list[str] = Field(default_factory=list)
    file_count: int = 0
    total_size: int = 0
    notes: list[str] = Field(default_factory=list)  # 解析过程中的降级/警告说明


class Dependency(BaseModel):
    name: str
    level: str  # base | runtime | bundled
    constraint: str = ""


class CheckItem(BaseModel):
    key: str
    title: str
    status: str  # pass | warn | fail | info
    detail: str = ""


class AnalysisResult(BaseModel):
    metadata: PackageMetadata
    files: DetectedFiles
    dependencies: list[Dependency] = Field(default_factory=list)
    checklist: list[CheckItem] = Field(default_factory=list)
    suggested: dict = Field(default_factory=dict)  # 建议的 linglong.yaml 关键字段


class ExportRequest(BaseModel):
    token: str
    yaml: str
