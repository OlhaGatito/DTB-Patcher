import tkinter as tk
from tkinter import ttk,filedialog,messagebox
from pathlib import Path
from .engine import Engine
from .workspace import Workspace
from .dtc import DtcError

class App(tk.Tk):
    def __init__(self):
        super().__init__(); self.title("DTB-Patcher"); self.geometry("1100x720"); self.minsize(900,600)
        self.ws=Workspace(); self.engine=Engine(self.ws); self.donor=tk.StringVar(); self.receiver=tk.StringVar()
        self.items=[]; self.selected={}
        self._ui()
    def _ui(self):
        top=ttk.Frame(self,padding=12); top.pack(fill="x")
        ttk.Label(top,text="DTB PATCHER",font=("Segoe UI",18,"bold")).pack(side="left")
        f=ttk.Frame(self,padding=12); f.pack(fill="x")
        for title,var in (("DOADOR",self.donor),("RECEPTOR",self.receiver)):
            row=ttk.Frame(f); row.pack(fill="x",pady=5); ttk.Label(row,text=title,width=12).pack(side="left")
            ttk.Entry(row,textvariable=var).pack(side="left",fill="x",expand=True,padx=5)
            ttk.Button(row,text="Selecionar",command=lambda v=var:self.pick(v)).pack(side="left")
        buttons=ttk.Frame(f); buttons.pack(fill="x",pady=8)
        ttk.Button(buttons,text="⇄ Trocar",command=self.swap).pack(side="left",padx=3)
        ttk.Button(buttons,text="ANALISAR DTBs",command=self.analyze).pack(side="left",padx=3)
        ttk.Button(buttons,text="GERAR NOVO DTB",command=self.build).pack(side="left",padx=3)
        ttk.Button(buttons,text="Limpar",command=self.clear).pack(side="left",padx=3)
        body=ttk.Panedwindow(self,orient="vertical"); body.pack(fill="both",expand=True,padx=12,pady=5)
        tf=ttk.Frame(body); bf=ttk.Frame(body); body.add(tf,weight=4); body.add(bf,weight=1)
        self.tree=ttk.Treeview(tf,columns=("kind","category","detail"),show="tree headings",selectmode="extended")
        self.tree.heading("#0",text="Transferência"); self.tree.heading("kind",text="Tipo"); self.tree.heading("category",text="Categoria"); self.tree.heading("detail",text="Detalhe")
        self.tree.column("#0",width=360); self.tree.column("kind",width=130); self.tree.column("category",width=110); self.tree.column("detail",width=450)
        self.tree.pack(fill="both",expand=True)
        self.log=tk.Text(bf,height=8); self.log.pack(fill="both",expand=True)
        self.tree.bind("<ButtonRelease-1>",lambda e:self.toggle())
    def logx(self,s): self.log.insert("end",s+"\n"); self.log.see("end")
    def pick(self,var):
        p=filedialog.askopenfilename(filetypes=[("Device Tree Binary","*.dtb"),("Todos","*.*")])
        if p: var.set(p)
    def swap(self): self.donor.set(self.receiver.get()); self.receiver.set(self.donor.get()) if False else None
    def clear(self): self.tree.delete(*self.tree.get_children()); self.items=[]; self.log.delete("1.0","end")
    def analyze(self):
        if not Path(self.donor.get()).is_file() or not Path(self.receiver.get()).is_file():
            messagebox.showerror("DTB-Patcher","Selecione um DTB Doador e um DTB Receptor."); return
        try:
            _,_,changes=self.engine.analyze(self.donor.get(),self.receiver.get())
            self.clear()
            for i,c in enumerate(changes):
                self.items.append(c); self.tree.insert("", "end", iid=str(i), text=c.path,values=(c.kind,c.category,c.detail))
            self.logx(f"{len(changes)} diferenças encontradas.")
        except Exception as e: messagebox.showerror("Falha na análise",str(e))
    def toggle(self):
        pass
    def build(self):
        if not self.items: self.analyze()
        ids=self.tree.selection()
        chosen=[self.items[int(i)] for i in ids]
        if not chosen: messagebox.showwarning("DTB-Patcher","Selecione pelo menos uma transferência."); return
        try:
            out,report=self.engine.build(self.donor.get(),self.receiver.get(),chosen)
            self.logx(f"Gerado: {out}"); messagebox.showinfo("Concluído",f"Novo DTB criado em:\n{out}")
        except Exception as e: messagebox.showerror("Falha ao gerar",str(e))
def run(): App().mainloop()
