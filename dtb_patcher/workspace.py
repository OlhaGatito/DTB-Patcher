from pathlib import Path
import json, os, re
from datetime import datetime

def documents_dir():
    if os.name=="nt":
        try:
            import ctypes
            from ctypes import wintypes
            p=ctypes.wintypes.PWSTR()
            # SHGetFolderPathW is widely available on supported Windows versions.
            buf=ctypes.create_unicode_buffer(32768)
            ctypes.windll.shell32.SHGetFolderPathW(None,5,None,0,buf)
            return Path(buf.value)
        except Exception: pass
    return Path.home()/"Documents"

class Workspace:
    def __init__(self, root=None):
        self.root=Path(root) if root else documents_dir()/"DTB-Patcher"
        self.new=self.root/"New dtb"; self.work=self.root/"Work"; self.backups=self.root/"Backups"
        self.projects=self.root/"Projects"; self.logs=self.root/"Logs"
        for p in (self.new,self.work,self.backups,self.projects,self.logs): p.mkdir(parents=True,exist_ok=True)
    def next_name(self):
        nums=[int(m.group(1)) for p in self.new.glob("patch-*.dtb") if (m:=re.match(r"patch-(\d+)\.dtb$",p.name,re.I))]
        return self.new/f"patch-{max(nums,default=0)+1:03d}.dtb"
    def report(self, data, stem):
        p=self.new/(stem+".json"); p.write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding="utf-8"); return p
