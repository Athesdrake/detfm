import ast
import json
from pathlib import Path
from typing import Literal

import requests

type Side = Literal["clientbound", "serverbound"]
type Bound = Literal["main", "tribulle"]

BASE_URL = "https://github.com/friedkeenan/caseus/raw/refs/heads/main"

FILES: list[tuple[Side, Bound]] = [
    ("clientbound", "main"),
    ("serverbound", "main"),
    ("clientbound", "tribulle"),
    ("serverbound", "tribulle"),
]


class ClassPacketFinder(ast.NodeVisitor):
    """Search for classes nodes."""

    def __init__(self):
        self.classes: list[tuple[str, tuple[int, int] | int]] = []

    def visit_ClassDef(self, node):
        if len(node.bases) == 1 and isinstance((base := node.bases[0]), ast.Name):
            if "packet" in base.id.lower():
                if (pkt_id := PacketFinder().visit(node)) is not None:
                    self.classes.append((node.name, pkt_id))

        return self.generic_visit(node)


class PacketFinder(ast.NodeVisitor):
    """Search for the packet id inside classes"""

    def __init__(self):
        self.id: tuple[int, int] | int | None = None

    def visit(self, node):
        super().visit(node)
        return self.id

    def visit_Assign(self, node):
        if len(node.targets) != 1:
            return self.generic_visit(node)

        target = node.targets[0]
        if isinstance(target, ast.Name) and target.id == "id":
            self.id = self.eval(node.value)
        return self.generic_visit(node)

    def eval(self, node: ast.expr) -> tuple[int, int] | int | None:
        if isinstance(node, ast.Constant):
            return self.eval_constant(node)
        if isinstance(node, ast.Tuple):
            return self.eval_tuple(node)
        return None

    def eval_constant(self, node: ast.Constant) -> int | None:
        return node.value if isinstance(node.value, int) else None

    def eval_tuple(self, node: ast.Tuple) -> tuple[int, int] | None:
        values: list[int] = []
        for el in node.elts:
            if isinstance(el, ast.Constant):
                if (value := self.eval_constant(el)) is not None:
                    values.append(value)

        return tuple(values) if len(values) == 2 else None


def get_packets(side: Side, bound: Bound) -> dict[str, str]:
    with requests.get(f"{BASE_URL}/caseus/packets/{side}/{bound}.py") as r:
        mod = ast.parse(r.text)
        visitor = ClassPacketFinder()
        visitor.visit(mod)
        classes: dict[str, str] = {}
        for name, pkt_id in visitor.classes:
            if isinstance(pkt_id, tuple):
                pkt_id = (pkt_id[0] << 8) | pkt_id[1]

            classes[f"{pkt_id:0>4x}"] = name.removesuffix("Packet")

        return classes


def get_filename(side: Side, bound: Bound) -> str:
    if bound == "tribulle":
        return f"{bound}_{side}.json"
    return f"{side}.json"


def main():
    for side, bound in FILES:
        classes = get_packets(side, bound)
        filename = get_filename(side, bound)

        with open(Path(__file__).parent / filename, "w") as f:
            json.dump(classes, f, indent=4, sort_keys=True)


if __name__ == "__main__":
    main()
