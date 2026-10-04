from pathlib import Path
from datetime import datetime
from .dtc import DtcManager
from .dts import parse_dts, render
from .analysis import compare
from .transfer import apply
from .workspace import Workspace

class Engine:
    def __init__(self, workspace=None, dtc=None):
        self.ws=workspace or Workspace(); self.dtc=dtc or DtcManager()
    def analyze(self, donor, receiver):
        wd=self.ws.work
        dd=self.dtc.decompile(donor,wd/"donor.dts"); rd=self.dtc.decompile(receiver,wd/"receiver.dts")
        return parse_dts(dd),parse_dts(rd),compare(parse_dts(dd),parse_dts(rd))
    def build(self, donor, receiver, selected):
        d,r,changes=self.analyze(donor,receiver)
        selected_paths={x.path for x in selected}
        chosen=[x for x in changes if x.path in selected_paths]
        patched,_=apply(r,d,chosen)
        out=self.ws.next_name(); dts=self.ws.work/"patched.dts"
        dts.write_text(render(patched),encoding="utf-8")
        self.dtc.compile(dts,out)
        report={"timestamp":datetime.now().isoformat(),"donor":str(donor),"receiver":str(receiver),"output":str(out),
                "changes":[x.__dict__ for x in chosen],"dtc":self.dtc.version()}
        self.ws.report(report,out.stem)
        return out,report
