import ctypes
import ctypes.wintypes
import json
import os
import subprocess
import threading
from pathlib import Path


for dll_dir in [
    r"D:\Environment\opencv\build\x64\vc15\bin",
    r"D:\Environment\jsoncpp\bin\Release",
]:
    if Path(dll_dir).exists():
        os.add_dll_directory(dll_dir)

try:
    import learnopencv_py
except ImportError:
    learnopencv_py = None


ROOT = Path(__file__).resolve().parent.parent
CONFIG_DIR = ROOT / "configs"
TASKS_DIR = ROOT / "tasks"


def load_json(path: Path, default):
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except Exception:
        return default


def save_json(path: Path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")


def enum_windows():
    user32 = ctypes.windll.user32
    rows = []
    proc_type = ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.c_void_p, ctypes.c_void_p)

    def cb(hwnd, _):
        if not user32.IsWindowVisible(hwnd):
            return True
        n = user32.GetWindowTextLengthW(hwnd)
        if n <= 0:
            return True
        title = ctypes.create_unicode_buffer(n + 1)
        cls = ctypes.create_unicode_buffer(256)
        user32.GetWindowTextW(hwnd, title, n + 1)
        user32.GetClassNameW(hwnd, cls, 256)
        rows.append({"hwnd": int(hwnd), "title": title.value, "class": cls.value})
        return True

    user32.EnumWindows(proc_type(cb), 0)
    return rows


def find_descendant_by_text_or_class(parent_hwnd, target):
    user32 = ctypes.windll.user32
    proc_type = ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.c_void_p, ctypes.c_void_p)
    found = ctypes.c_void_p(0)

    def cb(hwnd, _):
        n = user32.GetWindowTextLengthW(hwnd)
        title = ctypes.create_unicode_buffer(max(n + 1, 256))
        cls = ctypes.create_unicode_buffer(256)
        user32.GetWindowTextW(hwnd, title, len(title))
        user32.GetClassNameW(hwnd, cls, 256)
        if title.value == target or cls.value == target:
            found.value = int(hwnd)
            return False
        nested = find_descendant_by_text_or_class(int(hwnd), target)
        if nested:
            found.value = nested
            return False
        return True

    user32.EnumChildWindows(int(parent_hwnd), proc_type(cb), 0)
    return int(found.value or 0)


def enum_mumu_devices(target_class="MuMuNxDevice"):
    devices = []
    for row in enum_windows():
        child = find_descendant_by_text_or_class(row["hwnd"], target_class)
        if child:
            devices.append({**row, "target_hwnd": child, "target_class": target_class})
    return devices


def scan_adb_devices(adb_path):
    try:
        out = subprocess.check_output(
            [adb_path or "adb", "devices"],
            cwd=ROOT,
            text=True,
            encoding="utf-8",
            errors="ignore",
            timeout=5,
        )
    except Exception:
        return []
    devices = []
    for line in out.splitlines()[1:]:
        parts = line.split()
        if len(parts) >= 2 and parts[1] == "device":
            devices.append(parts[0])
    return devices


def adb_preview_png(adb_path, serial):
    cmd = [adb_path or "adb"]
    if serial:
        cmd += ["-s", serial]
    cmd += ["exec-out", "screencap", "-p"]
    try:
        data = subprocess.check_output(cmd, cwd=ROOT, timeout=5)
    except Exception:
        return None
    return data if data.startswith(b"\x89PNG") else None


class CppBridge:
    def __init__(self, root: Path, on_line, on_exit):
        self.root = root
        self.on_line = on_line
        self.on_exit = on_exit
        self.worker = None
        self.running = False

    def default_exe(self):
        candidates = [
            self.root / "x64/Debug/learnOpencv.exe",
            self.root / "x64/Release/learnOpencv.exe",
            self.root / "Debug/learnOpencv.exe",
            self.root / "Release/learnOpencv.exe",
        ]
        for path in candidates:
            if path.exists():
                return path
        return candidates[0]

    def is_running(self):
        return self.running and self.worker is not None and self.worker.is_alive()

    def start(self, exe_path: Path, task_name: str, config_path: Path):
        if self.is_running():
            raise RuntimeError("C++ 任务已在运行")
        if learnopencv_py is None:
            raise RuntimeError("learnopencv_py 未编译。请运行: python setup.py build_ext --inplace")

        self.running = True
        self.worker = threading.Thread(target=self._run_task, args=(task_name, str(config_path)), daemon=True)
        self.worker.start()

    def _run_task(self, task_name, config_path):
        code = 0
        try:
            learnopencv_py.run_task(task_name, config_path, self.on_line)
        except Exception as exc:
            code = 1
            self.on_line(f"C++ 绑定错误: {exc}")
        finally:
            self.running = False
            self.on_exit(code)

    def stop(self):
        if not self.is_running():
            return
        if learnopencv_py is not None:
            learnopencv_py.request_stop()
        self.on_line("已请求停止")
