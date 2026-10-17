# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

import hashlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


SCRIPT = Path(sys.argv.pop(1)).resolve()
DSC = b"Format: 3.0 (native)\nSource: example\nVersion: 1.0\n"
DIGEST = hashlib.sha256(DSC).hexdigest()


class FetchDscSourceTest(unittest.TestCase):
    def setUp(self):
        self.workspace = tempfile.TemporaryDirectory(prefix="dsc source test ")
        self.addCleanup(self.workspace.cleanup)
        self.root = Path(self.workspace.name)
        self.temporary = self.root / "download temp"
        self.temporary.mkdir()
        self.sentinel = self.temporary / "unrelated"
        self.sentinel.write_bytes(b"keep me")
        self.cache = self.root / "cache"
        self.output = self.root / "output"
        self.bin = self.root / "bin"
        self.bin.mkdir()
        self.write_tool(
            "dget",
            """#!/usr/bin/env python3
import os
from pathlib import Path
import sys
if os.environ.get("FAKE_DGET_FAIL"):
    sys.exit(42)
Path(sys.argv[-1].rsplit("/", 1)[-1]).write_bytes(%r)
""" % DSC,
        )
        self.write_tool(
            "dpkg-source",
            """#!/usr/bin/env python3
import os
from pathlib import Path
import sys
if os.environ.get("FAKE_DPKG_SOURCE_FAIL"):
    sys.exit(43)
destination = Path(sys.argv[-1])
destination.mkdir(parents=True)
(destination / "source.txt").write_text("source content\\n")
""",
        )

    def write_tool(self, name, content):
        path = self.bin / name
        path.write_text(content)
        path.chmod(0o755)

    def fetch(self, digest=DIGEST, relative_tmp=False, **environment):
        env = os.environ.copy()
        env.update(environment)
        env["PATH"] = str(self.bin) + os.pathsep + env["PATH"]
        env["TMPDIR"] = self.temporary.name if relative_tmp else str(self.temporary)
        return subprocess.run(
            [
                "sh",
                str(SCRIPT),
                str(self.output),
                "https://example.invalid/source.dsc",
                digest,
                str(self.cache),
            ],
            cwd=str(self.root),
            env=env,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            universal_newlines=True,
            timeout=30,
        )

    def assert_download_cleaned(self):
        self.assertEqual(sorted(path.name for path in self.temporary.iterdir()), ["unrelated"])
        self.assertEqual(self.sentinel.read_bytes(), b"keep me")

    def test_download_failure(self):
        result = self.fetch(FAKE_DGET_FAIL="1")
        self.assertEqual(result.returncode, 42, result.stderr)
        self.assert_download_cleaned()

    def test_digest_failure(self):
        result = self.fetch(digest="0" * 64)
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertIn("File SHA256 digest", result.stdout)
        self.assert_download_cleaned()

    def test_extraction_failure(self):
        result = self.fetch(FAKE_DPKG_SOURCE_FAIL="1")
        self.assertEqual(result.returncode, 43, result.stderr)
        self.assertFalse((self.cache / ("dsc_" + DIGEST)).exists())
        self.assert_download_cleaned()

    def test_success_and_cache_hit(self):
        result = self.fetch()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.output / "source.txt").read_text(), "source content\n")
        self.assert_download_cleaned()

        result = self.fetch(FAKE_DGET_FAIL="1")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.output / "source.txt").read_text(), "source content\n")
        self.assert_download_cleaned()

    def test_relative_temporary_directory(self):
        result = self.fetch(digest="0" * 64, relative_tmp=True)
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assert_download_cleaned()


if __name__ == "__main__":
    unittest.main()
