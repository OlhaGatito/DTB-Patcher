from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

@dataclass
class Property:
    name: str
    value: str
    raw: str = ""
    line: int = 0

@dataclass
class Node:
    name: str
    path: str
    label: Optional[str] = None
    properties: dict[str, Property] = field(default_factory=dict)
    children: dict[str, "Node"] = field(default_factory=dict)

    def walk(self):
        yield self
        for child in self.children.values():
            yield from child.walk()

@dataclass
class Document:
    source: Path
    text: str
    root: Node

@dataclass
class Change:
    path: str
    kind: str
    detail: str
    category: str = "other"
    confidence: str = "medium"
