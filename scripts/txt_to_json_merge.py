import json
import re
from pathlib import Path

TXT_PATH = Path("text/menu_text.txt")
JSON_PATH = Path("text/zh-cn.json")

BRACKET_PATTERN = re.compile(r"\[(.*?)]")


def extract_keys(txt_path: Path) -> list[str]:
    keys: list[str] = []
    for raw_line in txt_path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line:
            continue

        match = BRACKET_PATTERN.search(line)
        if match:
            key = match.group(1).strip()
        else:
            key = line.strip()

        if key and key not in keys:
            keys.append(key)
    return keys


def load_existing_json(json_path: Path) -> dict[str, str]:
    if not json_path.exists():
        return {}

    content = json_path.read_text(encoding="utf-8")
    content = re.sub(r",(\s*[}\]])", r"\1", content)
    return json.loads(content)


def merge_keys(existing: dict[str, str], keys: list[str]) -> dict[str, str]:
    merged = dict(existing)
    for key in keys:
        if key not in merged:
            merged[key] = ""
    return merged


def main() -> None:
    keys = extract_keys(TXT_PATH)
    existing = load_existing_json(JSON_PATH)
    merged = merge_keys(existing, keys)
    JSON_PATH.write_text(
        json.dumps(merged, ensure_ascii=False, indent=4) + "\n",
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
