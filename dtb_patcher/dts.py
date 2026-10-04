import re
from pathlib import Path
from .models import Document, Node, Property

TOKEN = re.compile(r'"(?:\\.|[^"\\])*"|/\\*.*?\\*/|//[^\\n]*|[{};:=<>\\[\\],&]|[^\\s{};:=<>\\[\\],&]+', re.S)

def strip_comments(s):
    s = re.sub(r'/\\*.*?\\*/', '', s, flags=re.S)
    return re.sub(r'//[^\\n]*', '', s)

def tokens(s):
    return TOKEN.findall(strip_comments(s))

def parse_dts(path):
    text = Path(path).read_text(encoding="utf-8", errors="replace")
    ts = tokens(text)
    i = 0
    root = Node("/", "/")
    stack = [root]
    labels = {}

    while i < len(ts):
        t = ts[i]
        if t in ("/dts-v1/;", "/dts-v1/", "/memreserve/"):
            while i < len(ts) and ts[i] != ";":
                i += 1
            i += 1
            continue

        label = None
        if i + 1 < len(ts) and ts[i + 1] == ":":
            label = t
            i += 2

        if i >= len(ts):
            break

        name = ts[i]
        i += 1

        if i < len(ts) and ts[i] == "{":
            if name == "/" and stack[-1] is root:
                i += 1
                continue

            parent = stack[-1]
            full = (parent.path.rstrip("/") + "/" + name).replace("//", "/")
            n = Node(name, full, label=label)
            parent.children[name] = n
            if label:
                labels[label] = n
            stack.append(n)
            i += 1
            continue

        val = []
        while i < len(ts) and ts[i] != ";":
            val.append(ts[i])
            i += 1
        if i < len(ts):
            i += 1
        stack[-1].properties[name] = Property(
            name, " ".join(val), raw=" ".join(val)
        )

        while len(stack) > 1 and i < len(ts) and ts[i] == "}":
            stack.pop()
            i += 1
            if i < len(ts) and ts[i] == ";":
                i += 1

    return Document(Path(path), text, root)

def render_value(v):
    return v

def render_node(n, level=0):
    ind = "\t" * level
    if n.path == "/":
        body = []
        for p in n.properties.values():
            body.append(ind + p.name + " = " + p.value + ";")
        for c in n.children.values():
            body.append(render_node(c, level))
        return "\n".join(body)

    label = (n.label + ": " if n.label else "")
    out = [ind + label + n.name + " {"]
    for p in n.properties.values():
        out.append("\t" * (level + 1) + p.name + " = " + p.value + ";")
    for c in n.children.values():
        out.append(render_node(c, level + 1))
    out.append(ind + "};")
    return "\n".join(out)

def render(doc):
    header = "/dts-v1/;\n\n/plugin/;\n\n" if "/plugin/" in doc.text else "/dts-v1/;\n\n"
    return header + "/ {\n" + render_node(doc.root, 1) + "\n};\n"
