"""RPM 解析测试（使用真实样本包）。"""
import pytest

from app.parsers.common import detect_format
from app.parsers.rpm import parse_rpm

from conftest import FIXTURES

RPM = FIXTURES / "sl-5.02-1.el9.x86_64.rpm"


@pytest.fixture(scope="module")
def parsed():
    return parse_rpm(RPM.read_bytes())


def test_detect_format():
    assert detect_format(RPM.read_bytes()) == "rpm"


def test_header_fields(parsed):
    assert parsed.control["Package"] == "sl"
    assert parsed.control["Version"] == "5.02"
    assert parsed.control["Architecture"] == "x86_64"
    assert parsed.control["Description"]


def test_dependencies(parsed):
    # RPM 依赖以 soname 形式出现
    names = [n for n, _ in parsed.dependencies]
    assert "libncurses.so.6" in names
    assert "libtinfo.so.6" in names
    assert all(not n.endswith("(64bit)") for n in names)


def test_files_scanned(parsed):
    assert any(b.endswith("bin/sl") for b in parsed.binaries)
    assert parsed.file_count > 0
