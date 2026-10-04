from pathlib import Path
import os, shutil, subprocess, sys
from urllib.request import Request, urlopen
from urllib.error import HTTPError, URLError

class DtcError(RuntimeError):
    pass

class DtcManager:
    BUNDLED_DTC_URL = "https://raw.githubusercontent.com/OlhaGatito/DTB-Patcher/main/tools/dtc.exe"

    def __init__(self, configured=None):
        self.configured = Path(configured) if configured else None

    def project_root(self):
        return Path(__file__).resolve().parent.parent

    def bundled_path(self):
        return self.project_root() / "tools" / "dtc.exe"

    def candidates(self):
        here = self.project_root()
        if self.configured: yield self.configured
        yield here / "tools" / "dtc.exe"
        yield Path(sys.executable).resolve().parent / "tools" / "dtc.exe"
        found = shutil.which("dtc")
        if found: yield Path(found)

    def find(self):
        for p in self.candidates():
            if p and p.is_file():
                return p
        raise DtcError("dtc.exe não encontrado. Use 'Baixar dependências' ou coloque-o em tools\\dtc.exe.")

    def download_bundled(self):
        target = self.bundled_path()
        target.parent.mkdir(parents=True, exist_ok=True)
        request = Request(self.BUNDLED_DTC_URL, headers={"User-Agent": "DTB-Patcher"})
        try:
            with urlopen(request, timeout=60) as response:
                data = response.read()
        except (HTTPError, URLError, TimeoutError) as e:
            raise DtcError(
                "Não foi possível baixar o DTC do repositório oficial do DTB-Patcher. "
                "Verifique a internet e tente novamente.\n\n"
                f"Detalhes: {e}"
            ) from e
        if len(data) < 4096 or data[:2] != b"MZ":
            raise DtcError("O arquivo baixado não parece ser um dtc.exe válido (assinatura PE MZ ausente).")
        tmp = target.with_suffix(".tmp")
        try:
            tmp.write_bytes(data)
            tmp.replace(target)
        except OSError as e:
            raise DtcError(f"Não foi possível gravar o DTC em {target}: {e}") from e
        return target

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
