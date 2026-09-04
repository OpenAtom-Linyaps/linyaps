import sys
from pathlib import Path

# 让 tests 可以直接 import app 包
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

FIXTURES = Path(__file__).parent / "fixtures"
