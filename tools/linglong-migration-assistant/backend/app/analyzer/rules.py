"""依赖归属规则加载与匹配。"""
from __future__ import annotations

from dataclasses import dataclass, field
from functools import lru_cache
from pathlib import Path

import yaml

RULES_PATH = Path(__file__).parent / "rules_data.yaml"


@dataclass
class LevelRules:
    exact: set[str] = field(default_factory=set)
    prefixes: list[str] = field(default_factory=list)


@dataclass
class Rules:
    base: LevelRules = field(default_factory=LevelRules)
    runtime: LevelRules = field(default_factory=LevelRules)


def _load_level(node: dict) -> LevelRules:
    return LevelRules(
        exact={str(x).lower() for x in node.get("exact", [])},
        prefixes=[str(x).lower() for x in node.get("prefixes", [])],
    )


@lru_cache(maxsize=1)
def load_rules() -> Rules:
    with open(RULES_PATH, "r", encoding="utf-8") as fh:
        raw = yaml.safe_load(fh) or {}
    return Rules(base=_load_level(raw.get("base", {}) or {}), runtime=_load_level(raw.get("runtime", {}) or {}))


def classify(name: str) -> str:
    """返回依赖归属：base | runtime | bundled。

    兼容两类名称：
    - deb 包名：libqt5core5a
    - RPM soname：libQt5Core.so.5（归一化为小写后匹配）
    """
    rules = load_rules()
    clean = name.strip().lower()
    if ".so" in clean:  # soname -> 库名（libQt5Core.so.5 -> libqt5core）
        clean = clean.split(".so", 1)[0]
    if not clean:
        return "bundled"
    for level in ("base", "runtime"):
        node: LevelRules = getattr(rules, level)
        if clean in node.exact:
            return level
        for prefix in node.prefixes:
            if clean.startswith(prefix):
                return level
    return "bundled"
