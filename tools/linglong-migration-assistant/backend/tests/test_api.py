# SPDX-FileCopyrightText: 2026 Yanghanrui666
#
# SPDX-License-Identifier: LGPL-3.0-or-later
"""API 集成测试：analyze -> export 全流程。"""
import io
import zipfile

from fastapi.testclient import TestClient

from app.main import app

from conftest import FIXTURES

client = TestClient(app)


def test_health():
    resp = client.get("/api/health")
    assert resp.status_code == 200


def test_analyze_and_export_roundtrip():
    with open(FIXTURES / "sl_5.02-1_amd64.deb", "rb") as fh:
        resp = client.post(
            "/api/analyze",
            files={"file": ("sl_5.02-1_amd64.deb", fh, "application/vnd.debian.binary-package")},
        )
    assert resp.status_code == 200
    data = resp.json()
    assert data["metadata"]["name"] == "sl"
    token = data["suggested"]["token"]
    assert data["suggested"]["yaml"].startswith("version:")

    export = client.post(
        "/api/export",
        json={"token": token, "yaml": data["suggested"]["yaml"]},
    )
    assert export.status_code == 200
    assert export.headers["content-type"].startswith("application/zip")
    with zipfile.ZipFile(io.BytesIO(export.content)) as zf:
        names = zf.namelist()
        assert any(n.endswith("linglong.yaml") for n in names)
        assert any(n.endswith("README.md") for n in names)
        assert any(n.endswith("srcs/README.txt") for n in names)


def test_rejects_invalid_format():
    resp = client.post(
        "/api/analyze",
        files={"file": ("fake.bin", io.BytesIO(b"not a package"), "application/octet-stream")},
    )
    assert resp.status_code == 400


def test_export_bad_yaml():
    with open(FIXTURES / "sl_5.02-1_amd64.deb", "rb") as fh:
        resp = client.post(
            "/api/analyze",
            files={"file": ("sl.deb", fh, "application/octet-stream")},
        )
    token = resp.json()["suggested"]["token"]
    bad = client.post("/api/export", json={"token": token, "yaml": "not: [valid"})
    assert bad.status_code == 400
    missing = client.post("/api/export", json={"token": "deadbeef", "yaml": "a: 1"})
    assert missing.status_code == 404
