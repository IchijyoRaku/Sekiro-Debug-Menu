from __future__ import annotations

import subprocess
import sys
from pathlib import Path


ROOT_DIR = Path(__file__).resolve().parent.parent
SOLUTION_FILE = ROOT_DIR / "dinput8.sln"
BUILD_COMMAND = [
    "msbuild",
    str(SOLUTION_FILE),
    "/t:Build",
    "/p:Configuration=Release",
    "/p:Platform=x64",
    "/v:minimal",
]


def main() -> int:
    if not SOLUTION_FILE.exists():
        print(f"未找到解决方案文件: {SOLUTION_FILE}", file=sys.stderr)
        return 1

    print("执行构建命令:")
    print(" ".join(BUILD_COMMAND))
    print(f"工作目录: {ROOT_DIR}")

    completed = subprocess.run(BUILD_COMMAND, cwd=ROOT_DIR)
    return completed.returncode


if __name__ == "__main__":
    raise SystemExit(main())
