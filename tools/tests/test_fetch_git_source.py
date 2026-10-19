# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

"""Offline regressions: python3 tools/tests/test_fetch_git_source.py."""

import os
from pathlib import Path
import subprocess
import tempfile
import unittest


class FetchGitSourceTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="linyaps-fetch-git-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.env = dict(os.environ, GIT_CONFIG_NOSYSTEM="1", GIT_CONFIG_GLOBAL=os.devnull,
                        GIT_CONFIG_COUNT="1", GIT_CONFIG_KEY_0="protocol.file.allow",
                        GIT_CONFIG_VALUE_0="always", GIT_SUBMODULES="true")
        self.source = self.root / "source"
        self.work = self.root / "work"
        self.script = Path(__file__).resolve().parents[2] / "misc/libexec/linglong/fetch-git-source"
        self.init_repo(self.source)
        (self.source / ".gitignore").write_text("generated/\nignored-link\n")
        (self.source / "tracked").write_text("original\n")
        self.commit(self.source)

    def git(self, repo, *args):
        return subprocess.check_output(["git", "-C", str(repo), *args],
                                       env=self.env, text=True, stderr=subprocess.STDOUT).strip()

    def init_repo(self, path):
        path.mkdir()
        self.git(path, "init", "-q")
        self.git(path, "config", "user.name", "Test")
        self.git(path, "config", "user.email", "test@example.invalid")

    def commit(self, path):
        self.git(path, "add", ".")
        self.git(path, "commit", "-qm", "fixture")

    def fetch(self, submodules=True):
        env = dict(self.env, GIT_SUBMODULES="true" if submodules else "")
        subprocess.run(["sh", str(self.script), str(self.work), str(self.source),
                        self.git(self.source, "rev-parse", "HEAD"), str(self.root / "cache")],
                       env=env, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    def test_reused_source_is_clean_without_removing_external_files(self):
        self.fetch()
        (self.work / "generated").mkdir()
        (self.work / "generated/stale.h").write_text("stale header")
        (self.work / "untracked").write_text("stale source")
        (self.work / "tracked").write_text("local modification")
        outside = self.root / "outside"
        outside.mkdir()
        (outside / "keep").write_text("must survive")
        (self.work / "ignored-link").symlink_to(outside, target_is_directory=True)
        self.fetch()
        self.assertFalse((self.work / "generated").exists())
        self.assertFalse((self.work / "untracked").exists())
        self.assertFalse((self.work / "ignored-link").is_symlink())
        self.assertEqual((self.work / "tracked").read_text(), "original\n")
        self.assertEqual((outside / "keep").read_text(), "must survive")
        self.assertTrue((self.work / ".git").is_dir())

    def test_ignored_artifacts_removed_with_submodules_disabled(self):
        self.fetch(submodules=False)
        (self.work / "generated").mkdir()
        (self.work / "generated/old.o").write_text("stale object")
        self.fetch(submodules=False)
        self.assertFalse((self.work / "generated").exists())

    def test_reused_submodule_is_clean(self):
        child = self.root / "child-source"
        self.init_repo(child)
        (child / ".gitignore").write_text("generated/\n")
        (child / "tracked").write_text("submodule original\n")
        self.commit(child)
        self.git(self.source, "submodule", "add", str(child), "child")
        self.commit(self.source)
        self.fetch()
        module = self.work / "child"
        (module / "generated").mkdir()
        (module / "generated/stale.h").write_text("stale header")
        (module / "tracked").write_text("modified submodule")
        (module / "untracked").write_text("untracked submodule file")
        self.fetch()
        self.assertFalse((module / "generated").exists())
        self.assertFalse((module / "untracked").exists())
        self.assertEqual((module / "tracked").read_text(), "submodule original\n")
        self.assertEqual(self.git(module, "status", "--porcelain", "--ignored"), "")


if __name__ == "__main__":
    unittest.main()
