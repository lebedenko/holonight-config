#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Offline installed discovery and consumer cache regressions."""
import os
import re
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class DependencyTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='config dependencies with spaces ')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.source = self.root / 'source'
        shutil.copytree(ROOT, self.source, ignore=shutil.ignore_patterns(
            '.git', 'build', '.cache', '__pycache__', 'compile_commands.json'))
        self.empty = self.root / 'empty'
        self.empty.mkdir()
        self.env = dict(os.environ, PKG_CONFIG_PATH='', PKG_CONFIG_LIBDIR=str(self.empty))
        self.env.pop('HOLONIGHT_EXTERNAL_PREFIX', None)
        self.env.pop('HOLONIGHT_DEPENDENCY_PREFIX', None)

    def run_command(self, command, env=None):
        return subprocess.run(command, cwd=self.source, env=env or self.env,
                              text=True, capture_output=True)

    def test_missing_installed_toml_fails_direct_cmake(self):
        result = self.run_command(['cmake', '-S', '.', '-B', 'build/missing', '-G', 'Ninja',
                                   '-DPKG_CONFIG_USE_CMAKE_PREFIX_PATH=OFF'])
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('tomlplusplus', result.stdout + result.stderr)

    def test_normal_commands_do_not_prepare_or_acquire_missing_dependencies(self):
        for command in ('configure', 'build', 'test'):
            with self.subTest(command=command):
                result = self.run_command(['python3', 'tooling/workflow.py', command, '--preset', 'test'])
                self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertIn('tomlplusplus', result.stdout + result.stderr)
                self.assertFalse((self.source / 'build/deps').exists())
                self.assertFalse((self.source / 'build/external').exists())
                self.assertFalse((self.source / 'build/test/CMakeCache.txt').exists())

    def test_explicit_external_prefix_with_spaces_and_changed_identity_archive_cache(self):
        prefix = self.root / 'external prefix'
        pc_dir = prefix / 'lib/pkgconfig'
        pc_dir.mkdir(parents=True)
        installed = subprocess.check_output(['pkg-config', '--variable=pcfiledir', 'tomlplusplus'],
                                            text=True).strip()
        pc = pc_dir / 'tomlplusplus.pc'
        pc.write_bytes((Path(installed) / 'tomlplusplus.pc').read_bytes())
        env = dict(self.env, HOLONIGHT_EXTERNAL_PREFIX=str(prefix))
        command = ['python3', 'tooling/workflow.py', 'configure', '--preset', 'test']
        result = self.run_command(command, env)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        marker = self.source / 'build/test/previous-marker'
        marker.write_text('preserved')
        pc.write_text(re.sub(r'(?m)^Version:.*$', 'Version: 99.0.0', pc.read_text()))
        result = self.run_command(command, env)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual((self.source / 'build/test.previous-1/previous-marker').read_text(), 'preserved')
        self.assertFalse(marker.exists())
        self.assertFalse((self.source / 'build/deps').exists())
        self.assertFalse((self.source / 'build/external').exists())


if __name__ == '__main__':
    unittest.main()
