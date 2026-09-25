#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2026 3219378872
#
# SPDX-License-Identifier: LGPL-3.0-or-later

"""Exercise the real file fetcher and wget against a loopback HTTP server."""

import hashlib
import http.server
import os
from pathlib import Path
import subprocess
import tempfile
import threading
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "libexec/linglong/fetch-file-source"
CONTENT = b"#!/bin/sh\necho original\n"
DIGEST = hashlib.sha256(CONTENT).hexdigest()


class RequestHandler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        self.server.requests += 1
        if self.path != "/source":
            self.send_error(404)
            return
        content = self.server.source.read_bytes()
        self.send_response(200)
        self.send_header("Content-Length", str(len(content)))
        self.end_headers()
        self.wfile.write(content)

    def log_message(self, *_):
        pass


class FileSourceTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="fetch file source ")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.cache = self.root / "cache dir"
        self.cache.mkdir()
        self.cached = self.cache / ("file_" + DIGEST)
        self.output = self.root / "output file"
        self.web = self.root / "web"
        self.web.mkdir()
        (self.web / "source").write_bytes(CONTENT)
        self.server = http.server.HTTPServer(("127.0.0.1", 0), RequestHandler)
        self.server.source = self.web / "source"
        self.server.requests = 0
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.addCleanup(self.stop_server)
        self.url = "http://127.0.0.1:{}/source".format(self.server.server_port)

    def stop_server(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join()

    def fetch(self, output=None, url=None, success=True):
        # Ensure CI's proxy environment cannot redirect the local HTTP fixture.
        env = os.environ.copy()
        env.update(no_proxy="127.0.0.1", NO_PROXY="127.0.0.1")
        result = subprocess.run(
            ["/bin/sh", str(SCRIPT), str(output or self.output),
             url or self.url, DIGEST, str(self.cache)],
            env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            universal_newlines=True, timeout=10,
        )
        if success:
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        return result

    def assert_isolated(self, output):
        self.assertEqual(output.read_bytes(), CONTENT)
        self.assertFalse(os.path.samefile(output, self.cached))
        self.cached.chmod(0o644)
        output.write_bytes(b"project-local edit\n")
        output.chmod(0o600)
        self.assertEqual(self.cached.read_bytes(), CONTENT)
        self.assertNotEqual(output.stat().st_mode, self.cached.stat().st_mode)

    def test_cold_fetch_does_not_share_cache_writes(self):
        self.fetch()
        self.assert_isolated(self.output)
        self.output.unlink()
        self.fetch()
        self.assertEqual(self.output.read_bytes(), CONTENT)
        self.assertEqual(self.server.requests, 1)

    def test_warm_fetch_does_not_share_cache_writes(self):
        self.cached.write_bytes(CONTENT)
        self.fetch()
        self.assert_isolated(self.output)
        self.assertEqual(self.server.requests, 0)

    def test_cache_writes_do_not_change_output(self):
        self.fetch()
        self.cached.write_bytes(b"changed cache\n")
        self.assertEqual(self.output.read_bytes(), CONTENT)

    def test_corrupted_cache_is_replaced_with_verified_download(self):
        self.cached.write_bytes(b"old build modification\n")
        self.fetch()
        self.assertEqual(self.cached.read_bytes(), CONTENT)
        self.assertEqual(self.output.read_bytes(), CONTENT)
        self.assertEqual(self.server.requests, 1)

    def test_corrupted_legacy_output_hardlink_is_replaced(self):
        self.cached.write_bytes(b"old build modification\n")
        os.link(self.cached, self.output)
        self.fetch()
        self.assert_isolated(self.output)
        self.assertEqual(self.server.requests, 1)

    def test_valid_cache_works_without_remote(self):
        self.cached.write_bytes(CONTENT)
        self.fetch(url=self.url + "-missing")
        self.assertEqual(self.output.read_bytes(), CONTENT)
        self.assertEqual(self.server.requests, 0)

    def test_corrupted_cache_with_failed_download_fails(self):
        self.cached.write_bytes(b"old build modification\n")
        self.fetch(url=self.url + "-missing", success=False)
        self.assertFalse(self.output.exists())
        self.assertFalse(self.cached.exists())
        self.assertEqual(self.server.requests, 1)

    def test_failed_download_does_not_publish_cache_or_output(self):
        self.fetch(url=self.url + "-missing", success=False)
        self.assertFalse(self.cached.exists())
        self.assertFalse(self.output.exists())

    def test_wrong_digest_does_not_publish_cache_or_output(self):
        (self.web / "source").write_bytes(b"wrong download\n")
        self.fetch(success=False)
        self.assertFalse(self.cached.exists())
        self.assertFalse(self.output.exists())

    def test_failed_output_copy_returns_failure_and_keeps_verified_cache(self):
        self.fetch(output=self.root / "missing-parent" / "output", success=False)
        self.assertEqual(self.cached.read_bytes(), CONTENT)

    def test_existing_output_hardlink_is_replaced(self):
        self.cached.write_bytes(CONTENT)
        os.link(self.cached, self.output)
        self.fetch()
        self.assert_isolated(self.output)

    def test_cross_filesystem_output_uses_copy_fallback(self):
        # Reflinks cannot cross filesystems; tmpfs supplies a real fallback path.
        if not os.access("/dev/shm", os.W_OK):
            self.skipTest("a writable /dev/shm is unavailable")
        if os.stat("/dev/shm").st_dev == self.cache.stat().st_dev:
            self.skipTest("/dev/shm and cache are on the same filesystem")
        with tempfile.TemporaryDirectory(prefix="fetch-file-", dir="/dev/shm") as temp:
            output = Path(temp) / "output"
            self.fetch(output=output)
            self.assert_isolated(output)


if __name__ == "__main__":
    unittest.main(verbosity=2)
