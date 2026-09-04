"""deb 解析与依赖分类测试（使用真实样本包）。"""
import pytest

from app.analyzer.rules import classify
from app.parsers.common import detect_format
from app.parsers.deb import parse_deb

from conftest import FIXTURES

DEB = FIXTURES / "sl_5.02-1_amd64.deb"


@pytest.fixture(scope="module")
def parsed():
    return parse_deb(DEB.read_bytes())


def test_detect_format():
    assert detect_format(DEB.read_bytes()) == "deb"


def test_control_fields(parsed):
    assert parsed.control["Package"] == "sl"
    assert parsed.control["Version"].startswith("5.02")
    assert "Description" in parsed.control
    assert parsed.control["Homepage"].startswith("http")


def test_files_scanned(parsed):
    # sl 装在 usr/games/sl
    assert "usr/games/sl" in parsed.binaries
    assert parsed.file_count > 0
    assert parsed.total_size > 0


def test_desktop_absent_ok(parsed):
    # sl 是终端玩具，无 desktop 文件
    assert parsed.desktops == []


def test_parse_depends_variants():
    from app.parsers.deb import parse_depends

    deps = parse_depends("libc6 (>= 2.38), libcurl3-gnutls (= 8.11.0), zlib1g (>= 1:1.1.4)")
    names = [d[0] for d in deps]
    assert names == ["libc6", "libcurl3-gnutls", "zlib1g"]
    assert deps[0][1] == ">= 2.38"


def test_parse_control_multiline():
    from app.parsers.deb import parse_control

    fields = parse_control(
        "Package: x\nDescription: first line\n continuation line\n\nDepends: a\n"
    )
    assert fields["Description"] == "first line\ncontinuation line"
    assert fields["Depends"] == "a"


def test_classify_levels():
    assert classify("libc6") == "base"
    assert classify("libqt5core5a") == "runtime"
    assert classify("libdtkwidget5") == "runtime"
    assert classify("libnotexist-xyz9") == "bundled"
