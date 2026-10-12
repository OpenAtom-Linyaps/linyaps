# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

import hashlib
import http.server
import io
import os
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
import threading
import unittest


SCRIPT = Path(sys.argv.pop(1)).resolve()
PAYLOAD = b"source content\n"
buffer = io.BytesIO()
with tarfile.open(fileobj=buffer, mode="w") as archive:
    entry = tarfile.TarInfo("source.txt")
    entry.size = len(PAYLOAD)
    archive.addfile(entry, io.BytesIO(PAYLOAD))
ARCHIVE = buffer.getvalue()
INVALID_ARCHIVE = b"not a tar archive\n"
DIGEST = hashlib.sha256(ARCHIVE).hexdigest()


class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        self.server.requests.append(self.path)
        if self.path == "/source.tar":
            content = ARCHIVE
        elif self.path == "/invalid.tar":
            content = INVALID_ARCHIVE
        else:
            self.send_error(404)
            return
        self.send_response(200)
        self.send_header("Content-Length", str(len(content)))
        self.end_headers()
        self.wfile.write(content)

    def log_message(self, *args):
        pass


class FetchArchiveSourceTest(unittest.TestCase):
    def setUp(self):
        self.workspace = tempfile.TemporaryDirectory(prefix="archive source test ")
        self.addCleanup(self.workspace.cleanup)
        self.root = Path(self.workspace.name)
        self.temporary = self.root / "download temp"
        self.temporary.mkdir()
        self.sentinel = self.temporary / "unrelated"
        self.sentinel.write_bytes(b"keep me")
        self.cache = self.root / "cache"
        self.cache.mkdir()
        self.output = self.root / "output"
        self.server = http.server.HTTPServer(("127.0.0.1", 0), Handler)
        self.server.requests = []
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.addCleanup(self.stop_server)

    def stop_server(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(timeout=5)

    def fetch(self, path="/source.tar", digest=DIGEST, relative_tmp=False):
        env = os.environ.copy()
        env["TMPDIR"] = self.temporary.name if relative_tmp else str(self.temporary)
        env["no_proxy"] = "127.0.0.1"
        env["NO_PROXY"] = "127.0.0.1"
        url = "http://127.0.0.1:{}{}".format(self.server.server_port, path)
        return subprocess.run(
            ["sh", str(SCRIPT), str(self.output), url, digest, str(self.cache)],
            cwd=str(self.root), env=env, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, universal_newlines=True, timeout=30,
        )

    def assert_download_cleaned(self):
        self.assertEqual(sorted(p.name for p in self.temporary.iterdir()), ["unrelated"])
        self.assertEqual(self.sentinel.read_bytes(), b"keep me")

    def test_digest_failure(self):
        result = self.fetch(digest="0" * 64)
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertIn("File SHA256 digest", result.stdout)
        self.assertFalse(self.output.exists())
        self.assert_download_cleaned()

    def test_http_failure(self):
        result = self.fetch(path="/missing.tar")
        self.assertEqual(result.returncode, 8, result.stderr)
        self.assertFalse(self.output.exists())
        self.assert_download_cleaned()

    def test_extraction_failure(self):
        digest = hashlib.sha256(INVALID_ARCHIVE).hexdigest()
        result = self.fetch(path="/invalid.tar", digest=digest)
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse((self.cache / ("archive_" + digest)).exists())
        self.assert_download_cleaned()

    def test_success_and_cache_hit(self):
        result = self.fetch()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.output / "source.txt").read_bytes(), PAYLOAD)
        self.assertEqual((self.cache / ("archive_" + DIGEST) / "source.txt").read_bytes(), PAYLOAD)
        self.assert_download_cleaned()
        self.server.requests.clear()
        result = self.fetch(path="/missing.tar")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.output / "source.txt").read_bytes(), PAYLOAD)
        self.assertEqual(self.server.requests, [])
        self.assert_download_cleaned()

    def test_relative_temporary_directory(self):
        result = self.fetch(digest="0" * 64, relative_tmp=True)
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assert_download_cleaned()


if __name__ == "__main__":
    unittest.main()
