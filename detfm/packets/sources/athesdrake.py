import json
from pathlib import Path
from typing import Iterator, Literal

import requests

GIST_HASH = "94dada5f4084e21e21077267467bc801"
GIST_URL = f"https://gist.githubusercontent.com/Athesdrake/{GIST_HASH}/raw/"
FILES = [
    "serverbound.json",
    "clientbound.json",
    "tribulle_clientbound.json",
    "tribulle_serverbound.json",
]


def find_key_base(keys: list[str]) -> Literal[0, 16]:
    if any(k.startswith("0") for k in keys if k != "0"):
        return 16
    return 0


def get_level(data: dict | str) -> int:
    if isinstance(data, str):
        return 0

    return max(get_level(value) + 1 for value in data.values())


def flatten(
    data: dict[str, dict[str, str]] | dict[str, str],
    base: Literal[0, 16],
    basekey: int = 0,
) -> Iterator[tuple[int, str]]:
    for key, value in data.items():
        if key == "name":
            continue

        key = basekey | int(key, base)
        if isinstance(value, dict):
            yield from (flatten(value, base, key << 8))
        else:
            yield key, value


def stringify(fmt: str, data: Iterator[tuple[int, str]]) -> dict[str, str]:
    return {fmt.format(k): v for k, v in data}


def get_packets(file: str) -> dict[str, str]:
    with requests.get(GIST_URL + file) as r:
        content = r.json()
        base = find_key_base(content.keys())
        return stringify("{:0>4x}", flatten(content, base))


def main():
    for file in FILES:
        content = get_packets(file)
        with open(Path(__file__).parent / file, "w") as f:
            json.dump(content, f, indent=4, sort_keys=True)


if __name__ == "__main__":
    main()
