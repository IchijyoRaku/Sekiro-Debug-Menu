from pathlib import Path
import shutil
import sys

SOURCE_DLL = (Path(__file__).resolve().parent.parent / "x64" / "Release" / "dinput8.dll").resolve()
TARGET_DIR = Path(r"D:\steam\steamapps\common\Sekiro")
TARGET_DLL = TARGET_DIR / "dinput8.dll"


def main() -> int:
    print(f"[INFO] Source: {SOURCE_DLL}")
    print(f"[INFO] Target: {TARGET_DLL}")

    if not SOURCE_DLL.exists():
        print("[ERROR] Source DLL not found.")
        return 1

    if not TARGET_DIR.exists():
        print("[ERROR] Target directory not found.")
        return 1

    shutil.copy2(SOURCE_DLL, TARGET_DLL)
    print("[OK] Deployed dinput8.dll to Sekiro directory.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
