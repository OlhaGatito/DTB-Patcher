from __future__ import annotations

import tkinter as tk
from pathlib import Path
from tkinter import filedialog, messagebox, ttk


class DTBPatcherApp(tk.Tk):
    def __init__(self) -> None:
        super().__init__()
        self.title("DTB Patcher")
        self.geometry("1180x760")
        self.minsize(980, 650)

        self.source_path = tk.StringVar()
        self.destination_path = tk.StringVar()
        self.output_path = tk.StringVar()
        self.status = tk.StringVar(value="Ready")
        self.progress = tk.DoubleVar(value=0)

        self.categories = {
            "GPIO / Pin Control": tk.BooleanVar(value=True),
            "Buttons / Input": tk.BooleanVar(value=True),
            "Analog": tk.BooleanVar(value=True),
            "Audio": tk.BooleanVar(value=False),
            "Display": tk.BooleanVar(value=False),
            "Backlight": tk.BooleanVar(value=False),
            "Battery / Power": tk.BooleanVar(value=False),
            "Other / Advanced": tk.BooleanVar(value=False),
        }

        self._configure_style()
        self._build_ui()

    def _configure_style(self) -> None:
        self.configure(bg="#111318")
        style = ttk.Style(self)
        style.theme_use("clam")
        style.configure(".", background="#111318", foreground="#e8eaed", font=("Segoe UI", 10))
        style.configure("TFrame", background="#111318")
        style.configure("Card.TFrame", background="#191c22")
        style.configure("TLabel", background="#111318", foreground="#e8eaed")
        style.configure("Muted.TLabel", background="#191c22", foreground="#9aa1ad")
        style.configure("Title.TLabel", background="#111318", foreground="#ffffff", font=("Segoe UI Semibold", 22))
        style.configure("Subtitle.TLabel", background="#111318", foreground="#9aa1ad", font=("Segoe UI", 10))
        style.configure("CardTitle.TLabel", background="#191c22", foreground="#ffffff", font=("Segoe UI Semibold", 12))
        style.configure("TButton", padding=(12, 8))
        style.configure("Accent.TButton", padding=(16, 9), background="#4f7cff", foreground="#ffffff", borderwidth=0)
        style.map("Accent.TButton", background=[("active", "#6a90ff")])
        style.configure("TCheckbutton", background="#191c22", foreground="#e8eaed")
        style.map("TCheckbutton", background=[("active", "#191c22")])
        style.configure("Horizontal.TProgressbar", troughcolor="#252a33", background="#4f7cff", borderwidth=0, thickness=8)
        style.configure("TNotebook", background="#111318", borderwidth=0)
        style.configure("TNotebook.Tab", background="#191c22", foreground="#aeb5c2", padding=(16, 9))
        style.map("TNotebook.Tab", background=[("selected", "#252a33")], foreground=[("selected", "#ffffff")])

    def _build_ui(self) -> None:
        header = ttk.Frame(self)
        header.pack(fill="x", padx=28, pady=(24, 12))
        ttk.Label(header, text="DTB Patcher", style="Title.TLabel").pack(anchor="w")
        ttk.Label(header, text="Transfer selected device-tree components between DTBs", style="Subtitle.TLabel").pack(anchor="w", pady=(3, 0))
        self._build_files_section()
        self._build_main_section()
        self._build_footer()

    def _card(self, parent: ttk.Frame) -> ttk.Frame:
        return ttk.Frame(parent, style="Card.TFrame", padding=18)

    def _build_files_section(self) -> None:
        card = self._card(self)
        card.pack(fill="x", padx=28, pady=8)
        ttk.Label(card, text="DTB Files", style="CardTitle.TLabel").grid(row=0, column=0, columnspan=3, sticky="w", pady=(0, 14))
        self._file_row(card, 1, "Source DTB", self.source_path, self._choose_source)
        self._file_row(card, 2, "Destination DTB", self.destination_path, self._choose_destination)
        self._file_row(card, 3, "Output DTB", self.output_path, self._choose_output)
        card.columnconfigure(1, weight=1)

    def _file_row(self, parent, row, label, variable, command) -> None:
        ttk.Label(parent, text=label, width=17).grid(row=row, column=0, sticky="w", padx=(0, 12), pady=5)
        ttk.Entry(parent, textvariable=variable).grid(row=row, column=1, sticky="ew", pady=5)
        ttk.Button(parent, text="Browse…", command=command).grid(row=row, column=2, padx=(10, 0), pady=5)

    def _build_main_section(self) -> None:
        body = ttk.Frame(self)
        body.pack(fill="both", expand=True, padx=28, pady=8)
        body.columnconfigure(0, weight=1)
        body.columnconfigure(1, weight=1)
        body.rowconfigure(0, weight=1)

        left = self._card(body)
        left.grid(row=0, column=0, sticky="nsew", padx=(0, 5))
        ttk.Label(left, text="Transfer Components", style="CardTitle.TLabel").pack(anchor="w")
        ttk.Label(left, text="Only selected categories will be considered by the transfer engine.", style="Muted.TLabel").pack(anchor="w", pady=(3, 14))

        for name, variable in self.categories.items():
            ttk.Checkbutton(left, text=name, variable=variable).pack(anchor="w", pady=4)

        buttons = ttk.Frame(left, style="Card.TFrame")
        buttons.pack(fill="x", pady=(18, 0))
        ttk.Button(buttons, text="Select All", command=self._select_all).pack(side="left")
        ttk.Button(buttons, text="Clear All", command=self._clear_all).pack(side="left", padx=8)

        right = self._card(body)
        right.grid(row=0, column=1, sticky="nsew", padx=(5, 0))
        ttk.Label(right, text="Operation", style="CardTitle.TLabel").pack(anchor="w")

        notebook = ttk.Notebook(right)
        notebook.pack(fill="both", expand=True, pady=(12, 0))

        compare = ttk.Frame(notebook, style="Card.TFrame", padding=8)
        log = ttk.Frame(notebook, style="Card.TFrame", padding=8)
        notebook.add(compare, text="Compare")
        notebook.add(log, text="Log")
        self.notebook = notebook

        self.compare_text = tk.Text(compare, bg="#101217", fg="#cbd1dc", insertbackground="#ffffff", relief="flat", font=("Cascadia Mono", 9), wrap="none")
        self.compare_text.pack(fill="both", expand=True)
        self.compare_text.insert("1.0", "Load two DTBs to begin comparison.\n")
        self.compare_text.configure(state="disabled")

        self.log_text = tk.Text(log, bg="#101217", fg="#cbd1dc", insertbackground="#ffffff", relief="flat", font=("Cascadia Mono", 9), wrap="word")
        self.log_text.pack(fill="both", expand=True)
        self._log("DTB Patcher started.")
        self._log("Backend engine: not connected yet.")

    def _build_footer(self) -> None:
        footer = ttk.Frame(self)
        footer.pack(fill="x", padx=28, pady=(8, 24))
        ttk.Progressbar(footer, variable=self.progress, maximum=100, mode="determinate").pack(fill="x", pady=(0, 8))
        ttk.Label(footer, textvariable=self.status).pack(side="left")
        ttk.Button(footer, text="Compare DTBs", command=self._compare).pack(side="right", padx=(8, 0))
        ttk.Button(footer, text="Build Patched DTB", style="Accent.TButton", command=self._build).pack(side="right")

    def _choose_source(self) -> None:
        path = filedialog.askopenfilename(title="Select source DTB", filetypes=[("Device Tree Blob", "*.dtb"), ("All files", "*.*")])
        if path:
            self.source_path.set(path)
            self._log(f"Source: {path}")

    def _choose_destination(self) -> None:
        path = filedialog.askopenfilename(title="Select destination DTB", filetypes=[("Device Tree Blob", "*.dtb"), ("All files", "*.*")])
        if path:
            self.destination_path.set(path)
            self._log(f"Destination: {path}")

    def _choose_output(self) -> None:
        path = filedialog.asksaveasfilename(title="Save patched DTB as", defaultextension=".dtb", filetypes=[("Device Tree Blob", "*.dtb"), ("All files", "*.*")])
        if path:
            self.output_path.set(path)
            self._log(f"Output: {path}")

    def _select_all(self) -> None:
        for variable in self.categories.values():
            variable.set(True)

    def _clear_all(self) -> None:
        for variable in self.categories.values():
            variable.set(False)

    def _compare(self) -> None:
        if not self._validate_files(False):
            return
        self.status.set("Comparison engine will be connected next…")
        self.progress.set(0)
        self._log("COMPARE requested.")
        self._log("Backend comparison is intentionally not implemented in the GUI layer.")
        self.notebook.select(0)
        messagebox.showinfo("Backend pending", "The GUI is ready. The DTB parser/comparison backend will be connected next.")

    def _build(self) -> None:
        if not self._validate_files(True):
            return
        selected = [name for name, var in self.categories.items() if var.get()]
        if not selected:
            messagebox.showwarning("Nothing selected", "Select at least one component category.")
            return
        self.status.set("Patch engine will be connected next…")
        self.progress.set(0)
        self._log("BUILD requested.")
        self._log("Selected: " + ", ".join(selected))
        messagebox.showinfo("Backend pending", "The GUI is ready. The DTB transfer/compiler backend will be connected next.")

    def _validate_files(self, require_output: bool) -> bool:
        if not self.source_path.get() or not Path(self.source_path.get()).is_file():
            messagebox.showwarning("Source DTB", "Select an existing source DTB.")
            return False
        if not self.destination_path.get() or not Path(self.destination_path.get()).is_file():
            messagebox.showwarning("Destination DTB", "Select an existing destination DTB.")
            return False
        if require_output and not self.output_path.get():
            messagebox.showwarning("Output DTB", "Choose where the patched DTB should be saved.")
            return False
        return True

    def _log(self, message: str) -> None:
        self.log_text.configure(state="normal")
        self.log_text.insert("end", message + "\n")
        self.log_text.see("end")
        self.log_text.configure(state="disabled")


def main() -> None:
    DTBPatcherApp().mainloop()


if __name__ == "__main__":
    main()
