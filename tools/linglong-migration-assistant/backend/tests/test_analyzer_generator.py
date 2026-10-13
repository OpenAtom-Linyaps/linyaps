# SPDX-FileCopyrightText: 2026 Yanghanrui666
#
# SPDX-License-Identifier: LGPL-3.0-or-later
"""分析器与生成器测试。"""
import yaml

from app.analyzer.analyzer import analyze, _normalize_version
from app.generator.linglong_yaml import generate_yaml
from app.parsers.common import ParsedPackage


def test_normalize_version():
    assert _normalize_version("5.02-1+b1") == "5.2.0.0"
    assert _normalize_version("1.2.3") == "1.2.3.0"
    assert _normalize_version("v2.0") == "2.0.0.0"
    assert _normalize_version("") == "0.0.0.0"


def _make_parsed() -> ParsedPackage:
    return ParsedPackage(
        package_format="deb",
        control={
            "Package": "fakeapp",
            "Version": "1.2.3-4",
            "Description": "A fake app",
            "Homepage": "https://example.com/",
            "Architecture": "amd64",
        },
        binaries=["usr/bin/fakeapp"],
        desktops=[],
        dependencies=[("libc6", ">= 2.36"), ("libqt5core5a", ""), ("libweird9", "")],
    )


def test_analyze_structure():
    result = analyze(_make_parsed(), "fakeapp.deb")
    assert result["metadata"]["name"] == "fakeapp"
    assert result["metadata"]["version"] == "1.2.3.0"
    assert result["metadata"]["architecture"] == "x86_64"
    levels = {d["name"]: d["level"] for d in result["dependencies"]}
    assert levels["libc6"] == "base"
    assert levels["libqt5core5a"] == "runtime"
    assert levels["libweird9"] == "bundled"
    assert result["suggested"]["runtime"]  # 有 runtime 依赖
    assert result["suggested"]["command"].endswith("/bin/fakeapp")
    assert result["suggested"]["bundled_deps"] == ["libweird9"]


def test_generate_yaml_valid():
    result = analyze(_make_parsed(), "fakeapp.deb")
    text = generate_yaml(result["suggested"], result["dependencies"], "deb")
    data = yaml.safe_load(text)
    assert data["version"] == "1"
    assert data["package"]["kind"] == "app"
    assert data["package"]["id"] == "example.fakeapp"
    assert data["base"] == "org.deepin.base/23.1.0"
    assert data["runtime"] == "org.deepin.runtime.dtk/23.1.0"
    assert data["sources"][0]["kind"] == "local"
    assert data["command"][0].startswith("/opt/apps/")
    assert "dpkg-deb -x" in data["build"]
    assert "apt-get download libweird9" in data["build"]


def test_checklist_warns_missing_desktop():
    result = analyze(_make_parsed(), "fakeapp.deb")
    statuses = {c["key"]: c["status"] for c in result["checklist"]}
    assert statuses["desktop"] == "fail"
    assert statuses["binaries"] == "pass"
