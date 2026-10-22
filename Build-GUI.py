#!/usr/bin/env python3
"""Downhill Port - Windows build GUI (standard-library Tkinter; no game data included).

Launch with Build-GUI.cmd from the repository root. The app uses the project's
existing tools/split.py and port/tools/build_clean.py, not an alternate build.
All ISO/ELF/game-derived files stay on the user's computer.
"""
from __future__ import annotations

import hashlib
import os
import queue
import shutil
import subprocess
import sys
import threading
import time
import tkinter as tk
from pathlib import Path
from tkinter import filedialog, messagebox, scrolledtext, ttk

ROOT = Path(__file__).resolve().parents[1]
CLEAN = ROOT / "build" / "port" / "clean"
ISO_DIR = ROOT / "iso"
EXPECTED_SHA1 = "4e25a222ad8a475b5d26c2e928e90b5f2b7075eb"
PYTHON = sys.executable
TOOL_HINTS = {
    "clang": [Path(os.environ.get("ProgramFiles", "C:/Program Files")) / "LLVM/bin/clang.exe",
              Path.home() / "scoop/apps/llvm/current/bin/clang.exe"],
    "cmake": [Path(os.environ.get("ProgramFiles", "C:/Program Files")) / "CMake/bin/cmake.exe"],
    "7z": [Path(os.environ.get("ProgramFiles", "C:/Program Files")) / "7-Zip/7z.exe",
           Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)")) / "7-Zip/7z.exe"],
}
DOWNLOADS = {
    "Git": "https://git-scm.com/downloads",
    "LLVM": "https://github.com/llvm/llvm-project/releases",
    "CMake": "https://cmake.org/download/",
    "7-Zip": "https://www.7-zip.org/",
    "Visual Studio Build Tools": "https://visualstudio.microsoft.com/visual-cpp-build-tools/",
}


class Cancelled(Exception):
    pass


class Builder(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Downhill Port • Construtor Windows")
        self.geometry("880x680")
        self.minsize(740, 570)
        self.configure(bg="#111827")
        self.events = queue.Queue()
        self.worker = None
        self.process = None
        self.cancel_requested = threading.Event()
        self.closing = False
        self.stage = "idle"
        self.mode = ""
        self.start_time = 0.0
        self.percent = 0
        self.link_start = 0.0
        self.last_update = 0.0
        self.exe_path = None
        self.env = os.environ.copy()
        self.configure_theme()
        self.build_ui()
        self.check_existing()
        self.after(250, self.drain_events)
        self.protocol("WM_DELETE_WINDOW", self.on_close)

    def configure_theme(self):
        style = ttk.Style(self)
        style.theme_use("clam")
        style.configure("TFrame", background="#111827")
        style.configure("TLabel", background="#111827", foreground="#dbeafe", font=("Segoe UI", 10))
        style.configure("Title.TLabel", font=("Segoe UI Semibold", 21), foreground="#f8fafc")
        style.configure("Muted.TLabel", foreground="#94a3b8")
        style.configure("TButton", font=("Segoe UI", 10), padding=(12, 8))
        style.configure("Accent.TButton", background="#2563eb", foreground="#ffffff")
        style.map("Accent.TButton", background=[("active", "#1d4ed8"), ("disabled", "#475569")])
        style.configure("TProgressbar", thickness=20, background="#60a5fa", troughcolor="#334155")

    def build_ui(self):
        frame = ttk.Frame(self, padding=22)
        frame.pack(fill="both", expand=True)
        ttk.Label(frame, text="DOWNHILL PORT", style="Title.TLabel").pack(anchor="w")
        ttk.Label(frame, text="Construa a versão nativa do Windows sem precisar digitar comandos.",
                  style="Muted.TLabel").pack(anchor="w", pady=(2, 15))
        self.iso_label = tk.StringVar(value="Verificando ISO...")
        ttk.Label(frame, textvariable=self.iso_label, wraplength=800).pack(anchor="w", pady=(0, 7))
        top = ttk.Frame(frame)
        top.pack(fill="x", pady=(0, 12))
        self.iso_button = ttk.Button(top, text="Selecionar ISO", command=self.choose_iso)
        self.iso_button.pack(side="left", padx=(0, 8))
        ttk.Button(top, text="Abrir pasta ISO", command=lambda: self.open_folder(ISO_DIR)).pack(side="left", padx=(0, 8))
        ttk.Button(top, text="Verificar ferramentas", command=self.show_tools).pack(side="left")
        controls = ttk.Frame(frame)
        controls.pack(fill="x", pady=(0, 13))
        self.full_button = ttk.Button(controls, text="▶ Build completo", style="Accent.TButton",
                                      command=lambda: self.start("full"))
        self.full_button.pack(side="left", padx=(0, 8))
        self.resume_button = ttk.Button(controls, text="↻ Continuar linkagem", command=lambda: self.start("resume"))
        self.resume_button.pack(side="left", padx=(0, 8))
        self.cancel_button = ttk.Button(controls, text="■ Cancelar", command=self.cancel, state="disabled")
        self.cancel_button.pack(side="left", padx=(0, 8))
        self.open_exe_button = ttk.Button(controls, text="Abrir pasta do jogo", command=self.open_exe, state="disabled")
        self.open_exe_button.pack(side="right")
        self.status = tk.StringVar(value="Pronto.")
        ttk.Label(frame, textvariable=self.status, wraplength=800).pack(anchor="w", pady=(0, 6))
        self.bar = ttk.Progressbar(frame, orient="horizontal", mode="determinate", maximum=100)
        self.bar.pack(fill="x", pady=(0, 7))
        self.progress_label = tk.StringVar(value="0%")
        ttk.Label(frame, textvariable=self.progress_label, style="Muted.TLabel").pack(anchor="w")
        loghead = ttk.Frame(frame)
        loghead.pack(fill="x", pady=(15, 7))
        ttk.Label(loghead, text="Registro da execução").pack(side="left")
        ttk.Button(loghead, text="Abrir logs", command=lambda: self.open_folder(CLEAN / "logs")).pack(side="right")
        self.log_widget = scrolledtext.ScrolledText(frame, bg="#0b1220", fg="#dbeafe",
                       insertbackground="#f8fafc", font=("Consolas", 9), relief="flat", height=16,
                       wrap="word", padx=11, pady=9, state="disabled")
        self.log_widget.pack(fill="both", expand=True)
        ttk.Label(frame,
                  text="Progresso por etapas. A linkagem pode demorar e não fornece uma porcentagem exata. "
                       "O aplicativo nunca envia a ISO para a internet.",
                  style="Muted.TLabel", wraplength=800).pack(anchor="w", pady=(9, 0))

    def log(self, line):
        self.log_widget.configure(state="normal")
        self.log_widget.insert("end", str(line).rstrip() + "\n")
        if int(self.log_widget.index("end-1c").split(".")[0]) > 1800:
            self.log_widget.delete("1.0", "400.0")
        self.log_widget.see("end")
        self.log_widget.configure(state="disabled")

    def send(self, kind, *values):
        self.events.put((kind, *values))

    def drain_events(self):
        try:
            while True:
                kind, *args = self.events.get_nowait()
                if kind == "log":
                    self.log(args[0])
                elif kind == "status":
                    self.status.set(args[0])
                elif kind == "progress":
                    self.set_percent(args[0], args[1])
                elif kind == "stage":
                    self.stage = args[0]
                    if args[0] == "link":
                        self.link_start = time.monotonic()
                        self.bar.configure(mode="indeterminate")
                        self.bar.start(12)
                        self.progress_label.set("70%+ • Linkagem sem porcentagem exata")
                    elif self.bar.cget("mode") == "indeterminate":
                        self.bar.stop()
                        self.bar.configure(mode="determinate")
                elif kind == "done":
                    self.finish(*args)
        except queue.Empty:
            pass
        if self.worker and self.worker.is_alive():
            self.poll_build_progress()
        self.after(800, self.drain_events)

    def set_percent(self, number, label):
        self.percent = max(0, min(100, int(number)))
        if self.bar.cget("mode") == "indeterminate":
            self.bar.stop()
            self.bar.configure(mode="determinate")
        self.bar["value"] = self.percent
        self.progress_label.set(f"{self.percent}% • {label}")

    def poll_build_progress(self):
        if self.stage == "compile" and self.mode == "full":
            obj_dir = CLEAN / "tree" / "build" / "port" / "obj"
            source_root = ROOT / "src"
            now = time.monotonic()
            if now - self.last_update >= 2:
                self.last_update = now
                try:
                    # Counting objects is an estimate: generated filenames may differ
                    # from source paths, so cap at 65 until the script confirms success.
                    source_count = sum(1 for f in source_root.rglob("*") if f.suffix in (".c", ".cpp"))
                    done = sum(1 for _ in obj_dir.rglob("*.o")) if obj_dir.is_dir() else 0
                    if source_count and done:
                        pct = min(65, 25 + int(40 * min(done, source_count) / source_count))
                        if pct >= self.percent:
                            self.set_percent(pct, f"Compilação inicial: {done}/{source_count} objetos aprox.")
                except OSError:
                    pass
        elif self.stage == "link":
            now = time.monotonic()
            if now - self.last_update >= 3:
                self.last_update = now
                obj_dir = CLEAN / "tree" / "build" / "port" / "recomp"
                try:
                    count = sum(1 for _ in obj_dir.glob("*.c.o")) if obj_dir.exists() else 0
                    elapsed = int(now - self.link_start)
                    self.status.set(f"Linkagem ativa • {elapsed}s nesta etapa • {count} objetos recompilados encontrados")
                except OSError:
                    pass

    def tool(self, names, env=None):
        env = self.env if env is None else env
        if isinstance(names, str):
            names = (names,)
        for name in names:
            executable = shutil.which(name, path=env.get("PATH"))
            if executable:
                return executable
            for candidate in TOOL_HINTS.get(name, ()):
                if candidate.is_file():
                    env["PATH"] = str(candidate.parent) + os.pathsep + env.get("PATH", "")
                    return str(candidate)
        return None

    def check_existing(self):
        isos = list(ISO_DIR.glob("*.iso")) if ISO_DIR.exists() else []
        elf = ISO_DIR / "SCUS_971.77"
        if len(isos) == 1:
            self.iso_label.set("ISO detectada: " + isos[0].name)
        elif elf.exists():
            self.iso_label.set("Executável PS2 extraído: SCUS_971.77")
        elif len(isos) > 1:
            self.iso_label.set("Há mais de uma ISO na pasta iso/. Deixe somente a USA.")
        else:
            self.iso_label.set("Nenhuma ISO encontrada. Selecione sua cópia USA (SCUS-97177).")
        self.find_exe()

    def find_exe(self):
        manifests = sorted((p for p in CLEAN.glob("*/manifest.json") if p.parent.name != "tree"),
                           key=lambda p: p.stat().st_mtime, reverse=True)
        for manifest in manifests:
            exe = manifest.parent / "dhd.exe"
            if exe.is_file():
                self.exe_path = exe
                self.open_exe_button.configure(state="normal")
                return exe
        return None

    def choose_iso(self):
        if self.worker and self.worker.is_alive():
            return
        filename = filedialog.askopenfilename(title="Selecione sua ISO USA de Downhill Domination",
                    filetypes=[("Imagens ISO", "*.iso"), ("Todos os arquivos", "*.*")])
        if not filename:
            return
        source = Path(filename)
        if source.suffix.lower() != ".iso":
            messagebox.showerror("ISO inválida", "Selecione um arquivo .iso.")
            return
        if source.parent.resolve() == ISO_DIR.resolve():
            self.iso_label.set("ISO selecionada: " + source.name)
            return
        if list(ISO_DIR.glob("*.iso")):
            messagebox.showwarning("ISO já existente",
                                   "Remova a ISO antiga da pasta iso/ antes de selecionar outra.")
            return
        if not messagebox.askyesno("Copiar ISO", f"Copiar {source.name} para a pasta iso/? "
                                   "O arquivo pode ter cerca de 2 GB."):
            return
        self.pending_iso = source
        self.start("copy")

    def show_tools(self):
        env = os.environ.copy()
        values = [
            ("Python", f"{sys.version_info.major}.{sys.version_info.minor}"),
            ("Git", self.tool("git", env)),
            ("Clang", self.tool("clang", env)),
            ("CMake", self.tool("cmake", env)),
            ("7-Zip", self.tool(("7z", "7za", "7zz"), env)),
        ]
        msg = "\n".join(f"{name}: {value or 'NÃO ENCONTRADO'}" for name, value in values)
        msg += "\n\nTambém é necessário Visual Studio C++ Build Tools (x86 + Windows SDK)."
        if sys.version_info < (3, 12):
            msg += "\n\nRecomendado: Python 3.12+ (a versão atual pode funcionar)."
        messagebox.showinfo("Ferramentas", msg)
        self.log(msg)

    def start(self, mode):
        if self.worker and self.worker.is_alive():
            messagebox.showwarning("Build em andamento", "Já existe uma tarefa ativa nesta janela.")
            return
        if mode == "resume" and not any((CLEAN / "tree" / "build" / "port" / "obj").rglob("*.o")):
            messagebox.showwarning("Build completo necessário",
                      "Ainda não há objetos compilados. Use 'Build completo' primeiro.")
            return
        if mode != "copy" and not messagebox.askokcancel(
            "Iniciar build",
            "Não deixe outra compilação rodando em outro terminal do VS Code.\n\n"
            "Iniciar agora?"):
            return
        self.cancel_requested.clear()
        self.mode = mode
        self.stage = "setup"
        self.start_time = time.monotonic()
        self.last_update = 0
        self.percent = 0
        self.set_percent(0, "Iniciando")
        self.full_button.configure(state="disabled")
        self.resume_button.configure(state="disabled")
        self.iso_button.configure(state="disabled")
        self.cancel_button.configure(state="normal")
        self.worker = threading.Thread(target=self.run_worker, args=(mode,), daemon=True)
        self.worker.start()

    def check_cancel(self):
        if self.cancel_requested.is_set():
            raise Cancelled("Operação cancelada pelo usuário.")

    def run_worker(self, mode):
        ok = False
        message = ""
        try:
            if mode == "copy":
                self.copy_iso()
                ok = True
                message = "ISO copiada com sucesso."
            else:
                self.prepare_and_build(mode)
                ok = True
                message = "dhd.exe gerado com sucesso!"
        except Cancelled as exc:
            message = str(exc)
        except Exception as exc:
            message = f"{type(exc).__name__}: {exc}"
            self.send("log", "ERRO: " + message)
        self.send("done", ok, message, mode)

    def copy_iso(self):
        src = self.pending_iso
        ISO_DIR.mkdir(parents=True, exist_ok=True)
        dest = ISO_DIR / src.name
        part = ISO_DIR / (src.name + ".part")
        size = src.stat().st_size
        copied = 0
        self.send("status", "Copiando ISO para a pasta local...")
        try:
            with src.open("rb") as fsrc, part.open("wb") as fdst:
                while True:
                    self.check_cancel()
                    buf = fsrc.read(4 * 1024 * 1024)
                    if not buf:
                        break
                    fdst.write(buf)
                    copied += len(buf)
                    self.send("progress", min(99, int(copied * 100 / max(size, 1))), "Copiando ISO")
            part.replace(dest)
        finally:
            if part.exists():
                part.unlink()

    def run_command(self, cmd, phase, env=None):
        self.check_cancel()
        self.send("status", phase)
        self.send("log", "\n> " + subprocess.list2cmdline([str(c) for c in cmd]))
        (ROOT / "build" / "gui").mkdir(parents=True, exist_ok=True)
        with (ROOT / "build" / "gui" / "build.log").open("a", encoding="utf-8") as logfile:
            creationflags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
            proc = subprocess.Popen([str(c) for c in cmd], cwd=ROOT, env=env or self.env,
                     stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                     text=True, encoding="utf-8", errors="replace", bufsize=1,
                     creationflags=creationflags)
            self.process = proc
            try:
                for line in proc.stdout:
                    self.check_cancel()
                    logfile.write(line)
                    logfile.flush()
                    self.send("log", line.rstrip("\r\n"))
                    if line.startswith("port/tools/build.py") and "exit 0" in line:
                        self.send("stage", "link")
                rc = proc.wait()
                self.check_cancel()
                if rc:
                    raise RuntimeError(f"Falha ({rc}): {phase}. Consulte build/port/clean/logs/ e build/gui/build.log.")
            finally:
                if proc.poll() is None:
                    self.stop_process(proc)
                self.process = None

    def stop_process(self, proc):
        if proc.poll() is not None:
            return
        if os.name == "nt":
            subprocess.run(["taskkill", "/PID", str(proc.pid), "/T", "/F"],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False,
                           creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        else:
            proc.terminate()
        try:
            proc.wait(timeout=6)
        except subprocess.TimeoutExpired:
            proc.kill()

    def prepare_and_build(self, mode):
        self.env = os.environ.copy()
        for program in ("git", "clang", "cmake"):
            if not self.tool(program):
                raise RuntimeError(f"Ferramenta ausente: {program}. Veja 'Verificar ferramentas'.")
        for program in ("llvm-nm", "llvm-objcopy", "llvm-rc", "llvm-cxxfilt", "lld-link"):
            if not self.tool(program):
                raise RuntimeError(f"LLVM incompleto: {program} não encontrado.")
        self.send("progress", 5, "Ferramentas verificadas")
        ISO_DIR.mkdir(parents=True, exist_ok=True)
        elf = ISO_DIR / "SCUS_971.77"
        if not elf.exists():
            isos = [p for p in ISO_DIR.iterdir() if p.is_file() and p.suffix.lower() == ".iso"]
            if len(isos) != 1:
                raise RuntimeError("Deixe uma única ISO USA na pasta iso/ ou selecione a ISO na interface.")
            seven_zip = self.tool(("7z", "7za", "7zz"))
            if not seven_zip:
                raise RuntimeError("7-Zip não encontrado. Instale 7-Zip para extrair SCUS_971.77.")
            self.run_command([seven_zip, "e", str(isos[0]), "SCUS_971.77",
                              "-o" + str(ISO_DIR), "-y"], "Extraindo executável do PS2...")
        self.check_cancel()
        self.send("status", "Conferindo SHA-1 do executável PS2...")
        digest = hashlib.sha1()
        with elf.open("rb") as f:
            for block in iter(lambda: f.read(1 << 20), b""):
                self.check_cancel()
                digest.update(block)
        if digest.hexdigest() != EXPECTED_SHA1:
            raise RuntimeError("SCUS_971.77 não corresponde à edição USA esperada (SHA-1 diferente).")
        self.send("progress", 12, "ISO validada")
        if mode == "full":
            self.run_command([PYTHON, "-m", "pip", "install", "-r", "requirements.txt"],
                             "Verificando dependências Python...")
            self.send("progress", 19, "Dependências prontas")
        if not (ROOT / "asm").is_dir() or not (ROOT / "assets").is_dir():
            self.run_command([PYTHON, "-u", "tools/split.py"], "Separando dados da sua ISO...")
        else:
            self.send("log", "asm/ e assets/ encontrados: etapa de extração lógica reaproveitada.")
        self.send("progress", 25 if mode == "full" else 65, "Dados do jogo prontos")
        self.check_cancel()
        if mode == "full":
            self.send("stage", "compile")
            cmd = [PYTHON, "-u", "port/tools/build_clean.py"]
        else:
            self.send("stage", "link")
            cmd = [PYTHON, "-u", "port/tools/build_clean.py", "--skip-compile"]
        self.run_command(cmd, "Compilando e vinculando o jogo..." if mode == "full" else
                         "Continuando a linkagem...", env=self.env)
        self.check_cancel()
        # A clean build prints the final exe location only after a successful link/copy.
        if not any(CLEAN.glob("*/manifest.json")):
            raise RuntimeError("O build encerrou sem manifesto. Confira build/port/clean/logs/.")
        self.send("progress", 100, "Build concluído")

    def finish(self, success, message, mode):
        self.cancel_button.configure(state="disabled")
        self.full_button.configure(state="normal")
        self.resume_button.configure(state="normal")
        self.iso_button.configure(state="normal")
        self.process = None
        if self.bar.cget("mode") == "indeterminate":
            self.bar.stop()
            self.bar.configure(mode="determinate")
        if success:
            if mode == "copy":
                self.status.set(message)
                self.set_percent(100, "ISO copiada")
            else:
                exe = self.find_exe()
                self.status.set(message if exe else "O script terminou, mas não foi encontrado dhd.exe.")
                self.set_percent(100, "Finalizado")
                if exe:
                    self.log("Executável: " + str(exe))
        else:
            self.status.set(message)
            self.log(message)
        self.check_existing()
        if self.closing:
            self.destroy()

    def cancel(self):
        if self.worker and self.worker.is_alive():
            if messagebox.askyesno("Cancelar", "Encerrar a compilação em andamento?\n"
                                    "Objetos parcialmente compilados podem ser reaproveitados "
                                    "apenas quando o build permitir."):
                self.cancel_requested.set()
                self.status.set("Cancelando e encerrando processos filhos...")
                proc = self.process
                if proc and proc.poll() is None:
                    threading.Thread(target=self.stop_process, args=(proc,), daemon=True).start()

    def open_folder(self, directory):
        directory.mkdir(parents=True, exist_ok=True)
        if os.name == "nt":
            os.startfile(str(directory))
        else:
            messagebox.showinfo("Pasta", str(directory))

    def open_exe(self):
        exe = self.find_exe()
        if exe:
            self.open_folder(exe.parent)
        else:
            messagebox.showinfo("Executável", "Nenhum dhd.exe finalizado foi encontrado.")

    def on_close(self):
        if self.worker and self.worker.is_alive():
            self.cancel()
            if self.cancel_requested.is_set():
                self.closing = True
        else:
            self.destroy()


if __name__ == "__main__":
    Builder().mainloop()
