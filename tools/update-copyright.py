#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: GPL-3.0-or-later

"""
自动更新 SPDX-FileCopyrightText 中 UnionTech Software Technology Co., Ltd. 的版权年份。

规则：
  - 仅处理包含 "UnionTech Software Technology Co., Ltd." 的版权行
  - 2024         → 2024 - <当前年>
  - 2024-2025    → 2024 - <当前年>  （修正间距 + 更新年份）
  - 2024 - 2025  → 2024 - <当前年>  （更新年份）
  - 2024 - <当前年> → 不变           （已是最新）
  - 2024-<当前年> → 2024 - <当前年>  （仅修正间距）
  - <当前年>      → 不变             （单年份已是当前年）
  - 支持 git commit --amend / git rebase 的提交重写场景

用法：
    # 作为 git pre-commit hook（自动处理暂存文件）：
    python3 update-copyright.py

    # 安装为当前仓库的本地 hook：
    python3 update-copyright.py --install

    # 安装为全局 hook（对本机所有 git 仓库生效）：
    python3 update-copyright.py --install-global

    # 手动处理指定文件：
    python3 update-copyright.py path/to/file1 path/to/file2
"""

import os
import re
import shutil
import subprocess
import sys
from datetime import datetime
from pathlib import Path

CURRENT_YEAR: int = datetime.now().year
COMPANY: str = "UnionTech Software Technology Co., Ltd."
_POST_REWRITE_GUARD: str = "UPDATE_COPYRIGHT_SKIP_POST_REWRITE"

# 匹配格式：SPDX-FileCopyrightText: YEAR[<任意间距>-<任意间距>YEAR] UnionTech...
_COPYRIGHT_RE = re.compile(
    r"(SPDX-FileCopyrightText:\s+)"   # group 1: 前缀
    r"(\d{4})"                         # group 2: 起始年份
    r"(\s*-\s*\d{4})?"                 # group 3: 可选的年份范围（含分隔符）
    r"(\s+UnionTech Software Technology Co\., Ltd\.)"  # group 4: 公司名后缀
)

# ── 版权更新核心逻辑 ────────────────────────────────────────────────────────────

def _replace_year(match: re.Match) -> str:
    prefix: str = match.group(1)
    start_year: int = int(match.group(2))
    year_range: str | None = match.group(3)
    suffix: str = match.group(4)

    if year_range is None:
        if start_year == CURRENT_YEAR:
            return match.group(0)  # 单年且已是当前年，不改
        return f"{prefix}{start_year} - {CURRENT_YEAR}{suffix}"
    else:
        # 范围格式：统一规范化为 "起始年 - 当前年"
        return f"{prefix}{start_year} - {CURRENT_YEAR}{suffix}"


def update_copyright_in_file(file_path: str) -> bool:
    """更新单个文件的版权年份，返回 True 表示文件已修改。"""
    path = Path(file_path)
    if not path.is_file():
        return False
    try:
        content = path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return False

    if COMPANY not in content:
        return False

    new_content = _COPYRIGHT_RE.sub(_replace_year, content)
    if new_content == content:
        return False

    try:
        path.write_text(new_content, encoding="utf-8")
    except OSError as exc:
        print(f"[copyright] ERROR: 无法写入 {file_path}: {exc}", file=sys.stderr)
        return False
    return True


def get_staged_files() -> list[str]:
    """返回 git 暂存区中新增/修改的文件列表。"""
    result = subprocess.run(
        ["git", "diff", "--cached", "--name-only", "--diff-filter=ACM"],
        capture_output=True, text=True,
    )
    if result.returncode != 0:
        return []
    return [f for f in result.stdout.strip().splitlines() if f.strip()]


def get_commit_files(commit: str) -> list[str]:
    """返回指定提交中新增/修改的文件列表。"""
    result = subprocess.run(
        ["git", "diff-tree", "--root", "--no-commit-id", "--name-only", "-r",
         "--diff-filter=ACM", commit],
        capture_output=True, text=True,
    )
    if result.returncode != 0:
        return []
    return [f for f in result.stdout.strip().splitlines() if f.strip()]


def process_files(files: list[str], restage: bool = False) -> int:
    """处理文件列表，返回修改的文件数量。"""
    if not files:
        return 0
    modified: list[str] = []
    for file_path in files:
        if update_copyright_in_file(file_path):
            modified.append(file_path)
            print(f"[copyright] 已更新: {file_path}")
    if not modified:
        return 0
    if restage:
        result = subprocess.run(["git", "add", "--"] + modified)
        if result.returncode == 0:
            print(f"[copyright] 共更新 {len(modified)} 个文件并已重新暂存。")
        else:
            print("[copyright] 警告：部分文件未能重新暂存，请手动执行 git add。",
                  file=sys.stderr)
    else:
        print(f"[copyright] 共更新 {len(modified)} 个文件。")
    return len(modified)


def _parse_rewrite_pairs(stdin_text: str) -> list[tuple[str, str]]:
    """解析 post-rewrite stdin 中的 old/new commit 对。"""
    pairs: list[tuple[str, str]] = []
    for line in stdin_text.splitlines():
        parts = line.split()
        if len(parts) >= 2:
            pairs.append((parts[0], parts[1]))
    return pairs


def _get_rewritten_files(rewrite_type: str, stdin_text: str) -> list[str]:
    """返回 amend/rebase 重写后涉及的文件列表。"""
    if rewrite_type not in {"amend", "rebase"}:
        return []

    files: list[str] = []
    seen: set[str] = set()
    for _, new_commit in _parse_rewrite_pairs(stdin_text):
        for file_path in get_commit_files(new_commit):
            if file_path not in seen:
                seen.add(file_path)
                files.append(file_path)
    return files


def handle_post_rewrite(rewrite_type: str, stdin_text: str) -> int:
    """处理 git commit --amend / git rebase 这类提交重写场景。"""
    if os.environ.get(_POST_REWRITE_GUARD) == "1":
        return 0

    files = _get_rewritten_files(rewrite_type, stdin_text)
    if not files:
        return 0

    modified_count = process_files(files, restage=True)
    if modified_count == 0:
        return 0

    env = os.environ.copy()
    env[_POST_REWRITE_GUARD] = "1"
    result = subprocess.run(
        ["git", "commit", "--amend", "--no-edit", "--no-verify"],
        env=env,
    )
    if result.returncode != 0:
        print("[copyright] 警告：自动 amend 失败，请手动执行 "
              "'git commit --amend --no-edit'。",
              file=sys.stderr)
        return 1

    print(f"[copyright] 已处理 git {rewrite_type} 触发的版权年份更新。")
    return modified_count

# ── 安装逻辑 ───────────────────────────────────────────────────────────────────

_HOOK_TEMPLATES = {
    "pre-commit": """\
#!/bin/sh
# pre-commit hook: 自动更新 UnionTech 版权年份
exec python3 "{script}" "$@"
""",
    "post-rewrite": """\
#!/bin/sh
# post-rewrite hook: 处理 amend / rebase 后的版权年份更新
exec python3 "{script}" --post-rewrite "$@"
""",
}


def _write_hook(hook_path: Path, script_path: Path, hook_name: str) -> None:
    """将 hook 写入 hook_path，指向 script_path。"""
    if hook_path.exists() and not _is_our_hook(hook_path):
        backup = hook_path.with_suffix(".bak")
        shutil.copy2(hook_path, backup)
        print(f"[copyright] 原 hook 已备份至 {backup}")
    hook_path.write_text(_HOOK_TEMPLATES[hook_name].format(script=script_path))
    hook_path.chmod(0o755)
    print(f"[copyright] ✓ 已写入 {hook_path}")


def _is_our_hook(hook_path: Path) -> bool:
    """判断现有 hook 是否由本脚本安装。"""
    try:
        return "update-copyright" in hook_path.read_text()
    except OSError:
        return False


def install_local() -> None:
    """安装为当前仓库的本地 pre-commit hook。"""
    result = subprocess.run(
        ["git", "rev-parse", "--git-dir"],
        capture_output=True, text=True,
    )
    if result.returncode != 0:
        print("[copyright] 错误：当前目录不在 git 仓库内。", file=sys.stderr)
        sys.exit(1)

    git_dir = Path(result.stdout.strip())
    hooks_dir = git_dir / "hooks"
    hooks_dir.mkdir(exist_ok=True)

    script_path = Path(__file__).resolve()
    _write_hook(hooks_dir / "pre-commit", script_path, "pre-commit")
    _write_hook(hooks_dir / "post-rewrite", script_path, "post-rewrite")
    print("[copyright] 本地 hook 安装完成，仅对当前仓库生效。")


def install_global() -> None:
    """安装为全局 git hook，对本机所有 git 仓库生效。"""
    # 1. 将本脚本复制到 ~/.local/share/git-hooks/
    dest_dir = Path.home() / ".local" / "share" / "git-hooks"
    dest_dir.mkdir(parents=True, exist_ok=True)
    script_dest = dest_dir / "update-copyright.py"
    shutil.copy2(Path(__file__).resolve(), script_dest)
    script_dest.chmod(0o755)
    print(f"[copyright] 脚本已复制至 {script_dest}")

    # 2. 将 pre-commit / post-rewrite hook 写入全局 hooks 目录
    global_hooks_dir = Path.home() / ".config" / "git" / "hooks"
    global_hooks_dir.mkdir(parents=True, exist_ok=True)
    _write_hook(global_hooks_dir / "pre-commit", script_dest, "pre-commit")
    _write_hook(global_hooks_dir / "post-rewrite", script_dest, "post-rewrite")

    # 3. 配置 git 全局 hooksPath
    current = subprocess.run(
        ["git", "config", "--global", "core.hooksPath"],
        capture_output=True, text=True,
    ).stdout.strip()

    if current and current != str(global_hooks_dir):
        print(f"[copyright] 警告：core.hooksPath 已设置为 {current}，将覆盖。",
              file=sys.stderr)

    subprocess.run(
        ["git", "config", "--global", "core.hooksPath", str(global_hooks_dir)],
        check=True,
    )
    print(f"[copyright] ✓ git config --global core.hooksPath = {global_hooks_dir}")
    print("[copyright] 全局 hook 安装完成，对本机所有 git 仓库生效。")

# ── 入口 ───────────────────────────────────────────────────────────────────────

def main() -> None:
    args = sys.argv[1:]

    if args == ["--install"]:
        install_local()
    elif args == ["--install-global"]:
        install_global()
    elif args[:1] == ["--post-rewrite"]:
        rewrite_type = args[1] if len(args) > 1 else ""
        stdin_text = sys.stdin.read()
        handle_post_rewrite(rewrite_type, stdin_text)
    elif args and not args[0].startswith("-"):
        # 手动模式：处理命令行指定的文件
        process_files(args, restage=False)
    else:
        # Hook 模式：处理暂存区文件并重新暂存
        staged = get_staged_files()
        process_files(staged, restage=True)


if __name__ == "__main__":
    main()
