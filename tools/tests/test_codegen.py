#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Hanabi9249
#
# SPDX-License-Identifier: LGPL-3.0-or-later

"""Run with QUICKTYPE pointing to a real quicktype executable and Bash on PATH."""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


class CodegenTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.bash = os.environ.get("BASH", shutil.which("bash"))
        cls.quicktype = os.environ.get("QUICKTYPE", shutil.which("quicktype"))
        if not cls.bash or not cls.quicktype:
            raise unittest.SkipTest("Bash and real quicktype are required")
        cls.repo = Path(__file__).resolve().parents[2]
        script = (cls.repo / "tools/codegen.sh").read_text()
        cls.functions = script[script.index("quicktype() {"):script.index("# yq should")]
        version = subprocess.run(
            [cls.bash, "-c", 'exec "$QUICKTYPE" --version'],
            env={**os.environ, "QUICKTYPE": cls.quicktype},
            text=True, capture_output=True, check=True,
        )
        print(version.stdout.strip())

    def generate(self, schema, dependencies=None):
        with tempfile.TemporaryDirectory(prefix="codegen-") as directory:
            root = Path(directory)
            (root / "schema.json").write_text(schema)
            for name, content in (dependencies or {}).items():
                (root / name).write_bytes(content)
            (root / "run.sh").write_text(
                'set -e\nset -o pipefail\nrepoRoot="$PWD"\n'
                + self.functions
                + '\ngenerate schema.json CDI "linglong::cdi::types" include generated\n'
                + 'echo reached-after-generate\n'
            )
            result = subprocess.run(
                [self.bash, "run.sh"], cwd=root,
                env={**os.environ, "QUICKTYPE": self.quicktype},
                text=True, capture_output=True,
            )
            headers = {
                path.name: path.read_text()
                for path in (root / "include/generated").glob("*.hpp")
            }
            print(f"generator caller exit={result.returncode}; headers={sorted(headers)}")
            if result.stderr:
                print(result.stderr.strip())
            return result, headers

    def test_existing_schema_with_missing_reference_fails(self):
        schema = (self.repo / "libs/cdi/schema.json").read_text()
        result, headers = self.generate(schema)
        self.assertNotEqual(result.returncode, 0, result.stderr)
        self.assertNotIn("reached-after-generate", result.stdout)
        self.assertFalse(headers)
        self.assertIn("defs.json", result.stderr)

    def test_malformed_existing_schema_fails(self):
        result, headers = self.generate("{")
        self.assertNotEqual(result.returncode, 0, result.stderr)
        self.assertNotIn("reached-after-generate", result.stdout)
        self.assertFalse(headers)
        self.assertIn("Error:", result.stderr)

    def test_real_cdi_schema_generates_headers(self):
        result, headers = self.generate(
            (self.repo / "libs/cdi/schema.json").read_text(),
            {"defs.json": (self.repo / "libs/cdi/defs.json").read_bytes()},
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("reached-after-generate", result.stdout)
        self.assertIn("Cdi.hpp", headers)
        self.assertIn("Generators.hpp", headers)
        self.assertIn('#include "generated/Cdi.hpp"', headers["Generators.hpp"])
        self.assertTrue(headers["Generators.hpp"].endswith("// clang-format on\n"))


if __name__ == "__main__":
    unittest.main(verbosity=2)
