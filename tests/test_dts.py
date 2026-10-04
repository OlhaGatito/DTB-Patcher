import tempfile
from pathlib import Path
from dtb_patcher.dts import parse_dts, render

S='''/dts-v1/;\n/ {\n test-node {\n  foo = <1 2 3>;\n  status = "okay";\n };\n};\n'''
def test_parse_render():
    with tempfile.TemporaryDirectory() as d:
        p=Path(d)/"x.dts"; p.write_text(S,encoding="utf-8")
        doc=parse_dts(p)
        n=doc.root.children["test-node"]
        assert n.properties["foo"].value=="< 1 2 3 >"
        assert n.properties["status"].value=='" okay "'
        assert "test-node" in render(doc)
