from pathlib import Path
import os, shutil, subprocess, sys

class DtcError(RuntimeError):
    pass

class DtcManager:
    def __init__(self, configured=None):
        self.configured = Path(configured) if configured else None

    def candidates(self):
        here = Path(__file__).resolve().parent.parent
        if self.configured: yield self.configured
        yield here / "tools" / "dtc.exe"
        yield Path(sys.executable).resolve().parent / "tools" / "dtc.exe"
        found = shutil.which("dtc")
        if found: yield Path(found)

    def find(self):
        for p in self.candidates():
            if p and p.is_file():
                return p
        raise DtcError("dtc.exe não encontrado. Coloque-o em tools\\dtc.exe ou informe o caminho nas configurações.")

    def run(self, args):
        dtc = str(self.find())
        cp = subprocess.run([dtc, *map(str,args)], text=True, capture_output=True, encoding="utf-8", errors="replace")
        if cp.returncode:
            raise DtcError((cp.stderr or cp.stdout).strip() or f"dtc retornou {cp.returncode}")
        return cp

    def version(self):
        return self.run(["--version"]).stdout.strip()

    def decompile(self, dtb, dts):
        dts = Path(dts); dts.parent.mkdir(parents=True, exist_ok=True)
        self.run(["-I","dtb","-O","dts","-o",dts,dtb])
        return dts

    def compile(self, dts, dtb):
        dtb = Path(dtb); dtb.parent.mkdir(parents=True, exist_ok=True)
        self.run(["-I","dts","-O","dtb","-o",dtb,dts])
        return dtb
