from dtb_patcher.workspace import Workspace
def test_numbering(tmp_path):
    w=Workspace(tmp_path)
    assert w.next_name().name=="patch-001.dtb"
    (w.new/"patch-001.dtb").write_bytes(b"x")
    assert w.next_name().name=="patch-002.dtb"
