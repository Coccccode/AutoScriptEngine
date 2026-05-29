import ctypes
import ctypes.wintypes
import json
import locale
import os
import queue
import subprocess
import threading
import time
import tkinter as tk
from datetime import datetime, timedelta
from pathlib import Path
from tkinter import filedialog, messagebox

import customtkinter as ctk

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


ROOT = Path(__file__).resolve().parent

ACTION_TEMPLATES = {
    "wait": {
        "action": "null",
        "delay": 1,
        "next": [],
    },
    "click_direct": {
        "action": "click",
        "recognization": "DirectHint",
        "x": 100,
        "y": 100,
        "afterDelay": 1,
        "next": [],
    },
    "click_template": {
        "action": "click",
        "recognization": "TemplateMatch",
        "template": "resource/images/example.png",
        "threshold": 0.89,
        "recognitionTimeoutMs": 10000,
        "recognitionIntervalMs": 500,
        "afterDelay": 1,
        "next": [],
    },
    "click_ocr": {
        "action": "click",
        "recognization": "OCR",
        "target": "target text",
        "ocrConfig": "config_chinese.txt",
        "minScore": 0.5,
        "recognitionTimeoutMs": 10000,
        "recognitionIntervalMs": 500,
        "afterDelay": 1,
        "next": [],
    },
    "detect_template": {
        "action": "detect",
        "recognization": "TemplateMatch",
        "template": "resource/images/example.png",
        "threshold": 0.89,
        "recognitionTimeoutMs": 10000,
        "recognitionIntervalMs": 500,
        "next": [],
    },
    "swipe": {
        "action": "swipe",
        "x1": 500,
        "y1": 700,
        "x2": 500,
        "y2": 300,
        "duration": 500,
        "next": [],
    },
    "ocr": {
        "action": "ocr",
        "recognization": "OCR",
        "target": "target text",
        "ocrConfig": "config_chinese.txt",
        "minScore": 0.5,
        "click": False,
        "recognitionTimeoutMs": 10000,
        "recognitionIntervalMs": 500,
        "next": [],
    },
}


def load_json(path: Path, default):
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except Exception:
        return default


def save_json(path: Path, data):
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


def capture_window_ppm(hwnd, max_width=520):
    user32 = ctypes.windll.user32
    gdi32 = ctypes.windll.gdi32

    rect = ctypes.wintypes.RECT()
    if not user32.GetWindowRect(hwnd, ctypes.byref(rect)):
        return None
    width = rect.right - rect.left
    height = rect.bottom - rect.top
    if width <= 0 or height <= 0:
        return None

    hdc = user32.GetDC(0)
    mem = gdi32.CreateCompatibleDC(hdc)
    bmp = gdi32.CreateCompatibleBitmap(hdc, width, height)
    old = gdi32.SelectObject(mem, bmp)
    gdi32.BitBlt(mem, 0, 0, width, height, hdc, rect.left, rect.top, 0x00CC0020)

    class BitmapInfoHeader(ctypes.Structure):
        _fields_ = [
            ("biSize", ctypes.c_uint32),
            ("biWidth", ctypes.c_int32),
            ("biHeight", ctypes.c_int32),
            ("biPlanes", ctypes.c_uint16),
            ("biBitCount", ctypes.c_uint16),
            ("biCompression", ctypes.c_uint32),
            ("biSizeImage", ctypes.c_uint32),
            ("biXPelsPerMeter", ctypes.c_int32),
            ("biYPelsPerMeter", ctypes.c_int32),
            ("biClrUsed", ctypes.c_uint32),
            ("biClrImportant", ctypes.c_uint32),
        ]

    header = BitmapInfoHeader(40, width, -height, 1, 32, 0, width * height * 4, 0, 0, 0, 0)
    buf = (ctypes.c_ubyte * (width * height * 4))()
    gdi32.GetDIBits(mem, bmp, 0, height, buf, ctypes.byref(header), 0)
    gdi32.SelectObject(mem, old)
    gdi32.DeleteObject(bmp)
    gdi32.DeleteDC(mem)
    user32.ReleaseDC(0, hdc)

    scale = min(1.0, max_width / width)
    out_w = max(1, int(width * scale))
    out_h = max(1, int(height * scale))
    src = bytes(buf)
    data = bytearray(f"P6\n{out_w} {out_h}\n255\n".encode("ascii"))
    for y in range(out_h):
        sy = int(y / scale)
        for x in range(out_w):
            sx = int(x / scale)
            i = (sy * width + sx) * 4
            b, g, r = src[i], src[i + 1], src[i + 2]
            data.extend([r, g, b])
    return bytes(data)


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


class App(ctk.CTk):
    def __init__(self):
        super().__init__()
        ctk.set_appearance_mode("dark")
        ctk.set_default_color_theme("blue")

        self.title("脚本助手")
        self.geometry("1480x900")
        self.minsize(1180, 720)

        self.config_path = ROOT / "config.json"
        self.task_data = {}
        self.task_path = ROOT / "JieJieKa.json"
        self.windows = []
        self.mumu_devices = []
        self.connected_devices = {}
        self.device_candidate_var = ctk.StringVar(value="")
        self.log_queue = queue.Queue()
        self.preview_queue = queue.Queue()
        self.started_at = None
        self.scheduler_stop = threading.Event()
        self.preview_running = False
        self.preview_busy = False
        self.preview_image = None
        self.preview_status = ""
        self.bridge = CppBridge(ROOT, self.enqueue_log, self.on_cpp_exit)
        self.instances = {}
        self.active_instance_name = "实例1"
        self.instance_var = ctk.StringVar(value=self.active_instance_name)
        self.create_instance(self.active_instance_name, self.config_path, make_active=True)

        self.build_layout()
        self.load_config()
        self.refresh_tasks()
        self.refresh_windows()
        self.refresh_adb()
        self.after(150, self.drain_log_queue)
        self.after(50, self.drain_preview_queue)
        self.after(500, self.update_runtime)

    def card(self, parent, **grid):
        frame = ctk.CTkFrame(parent, corner_radius=10, fg_color="#151a25", border_width=1, border_color="#252b3a")
        frame.grid(**grid)
        return frame

    def create_instance(self, name, config_path=None, make_active=False):
        # 修复：强制使用纯英文作为配置文件名，防止 C++ 底层 ifstream 读取中文路径失败导致 JsonCpp 崩溃
        if config_path is None:
            import re
            safe = re.sub(r'[^a-zA-Z0-9_]', '', name) or f"inst_{len(self.instances)+1}"
            config_path = ROOT / f"config_{safe}.json"
        else:
            config_path = Path(config_path)

        if not config_path.exists():
            base = load_json(getattr(self, "config_path", ROOT / "config.json"), {})
            if not isinstance(base, dict):
                base = {}
            base["windowName"] = ""
            base["device"] = None
            save_json(config_path, base)
            
        bridge = CppBridge(ROOT, self.enqueue_log, self.on_cpp_exit)
        self.instances[name] = {
            "name": name,
            "config_path": config_path,
            "bridge": bridge,
            "device": None,
            "preview_running": False,
            "preview_busy": False,
            "preview_image": None,
            "candidate": "",
            # 新增：给每个实例一把专属的私有锁，彻底分离！
            "capture_lock": threading.Lock(), 
        }
        if make_active:
            self.active_instance_name = name
            self.config_path = config_path
            self.bridge = bridge
            if hasattr(self, "instance_var"):
                self.instance_var.set(name)
        if hasattr(self, "instance_tabs"):
            self.instance_tabs.configure(values=list(self.instances.keys()))

    def add_instance(self):
        index = len(self.instances) + 1
        while f"实例{index}" in self.instances:
            index += 1
        name = f"实例{index}"
        
        # 修复：明确给新实例分配纯英文的配置文件路径 (例如 config_instance_2.json)
        ascii_path = ROOT / f"config_instance_{index}.json"
        self.create_instance(name, ascii_path)
        self.switch_instance(name)

    def switch_instance(self, name):
        if name not in self.instances:
            return
        if hasattr(self, "log_text"):
            self.save_config()
        item = self.instances[name]
        self.active_instance_name = name
        self.config_path = item["config_path"]
        self.bridge = item["bridge"]
        self.instance_var.set(name)
        self.load_config()
        image = item.get("preview_image")
        if image is not None:
            self.preview_image = image
            self.preview_status = ""
            self.preview_label.configure(image=image, text="")
        elif hasattr(self, "preview_label"):
            self.show_preview_status(f"预览 {name}")
        if hasattr(self, "log_text"):
            self.log(f"已切换至 {name}")

    def active_instance(self):
        return self.instances[self.active_instance_name]

    def show_preview_status(self, text):
        if self.preview_status == text:
            return
        self.preview_status = text
        self.preview_image = None
        self.preview_label.configure(image=None, text=text)

    def build_layout(self):
        self.grid_columnconfigure(1, weight=1)
        self.grid_rowconfigure(0, weight=1)

        self.sidebar = ctk.CTkFrame(self, width=220, corner_radius=0, fg_color="#101521")
        self.sidebar.grid(row=0, column=0, sticky="nsew")
        self.sidebar.grid_propagate(False)
        self.build_sidebar()

        self.main = ctk.CTkFrame(self, corner_radius=0, fg_color="#0b0f18")
        self.main.grid(row=0, column=1, sticky="nsew")
        self.main.grid_columnconfigure(0, weight=1)
        self.main.grid_rowconfigure(1, weight=1)

        self.build_header()
        self.build_body()

    def build_sidebar(self):
        brand = ctk.CTkFrame(self.sidebar, fg_color="transparent")
        brand.pack(fill="x", padx=16, pady=(22, 18))
        ctk.CTkLabel(brand, text="脚本助手", font=ctk.CTkFont(size=18, weight="bold")).pack(anchor="w")
        ctk.CTkLabel(brand, text="v0.1.0", text_color="#8c95aa").pack(anchor="w", pady=(4, 0))

        self.nav_buttons = {}
        for name in ["脚本管理", "任务模板", "定时任务", "运行日志", "资源管理", "设置中心"]:
            btn = ctk.CTkButton(
                self.sidebar,
                text=name,
                height=42,
                anchor="w",
                fg_color="#5636e8" if name == "脚本管理" else "transparent",
                hover_color="#262d40",
                command=lambda n=name: self.select_nav(n),
            )
            btn.pack(fill="x", padx=14, pady=5)
            self.nav_buttons[name] = btn

        guide = ctk.CTkFrame(self.sidebar, corner_radius=10, fg_color="#171d2b")
        guide.pack(side="bottom", fill="x", padx=14, pady=18)
        ctk.CTkLabel(guide, text="脚本使用指南", font=ctk.CTkFont(size=14, weight="bold")).pack(anchor="w", padx=14, pady=(14, 2))
        ctk.CTkLabel(guide, text="运行前请先连接设备", text_color="#8c95aa").pack(anchor="w", padx=14)
        ctk.CTkButton(guide, text="保存配置", height=30, command=self.save_config).pack(anchor="w", padx=14, pady=14)

    def build_header(self):
        header = ctk.CTkFrame(self.main, fg_color="transparent")
        header.grid(row=0, column=0, sticky="ew", padx=18, pady=(18, 10))
        header.grid_columnconfigure(1, weight=1)
        ctk.CTkLabel(header, text="★ 我的脚本01", font=ctk.CTkFont(size=22, weight="bold")).grid(row=0, column=0, sticky="w")
        ctk.CTkLabel(header, text="帮助    交流群", text_color="#9aa3b5").grid(row=0, column=2, sticky="e")

        self.instance_tabs = ctk.CTkSegmentedButton(header, variable=self.instance_var, values=list(self.instances.keys()), command=self.switch_instance)
        self.instance_tabs.grid(row=1, column=0, columnspan=2, sticky="ew", pady=(14, 0))
        ctk.CTkButton(header, text="+ 新增实例", width=110, fg_color="#202635", command=self.add_instance).grid(row=1, column=2, sticky="e", pady=(14, 0))

    def build_body(self):
        body = ctk.CTkFrame(self.main, fg_color="transparent")
        body.grid(row=1, column=0, sticky="nsew", padx=18, pady=(0, 18))
        body.grid_columnconfigure(0, weight=1)
        body.grid_columnconfigure(1, weight=1)
        body.grid_columnconfigure(2, weight=1)
        body.grid_rowconfigure(1, weight=1)

        self.build_status_cards(body)
        self.build_task_panel(body)
        self.build_flow_panel(body)
        self.build_right_panel(body)

    def build_status_cards(self, parent):
        self.device_card = self.card(parent, row=0, column=0, sticky="nsew", padx=(0, 10), pady=(0, 14))
        self.device_card.grid_columnconfigure(1, weight=1)
        ctk.CTkLabel(self.device_card, text="设备控制", text_color="#cad2e4").grid(row=0, column=0, sticky="w", padx=16, pady=(14, 6))
        self.online_badge = ctk.CTkLabel(self.device_card, text="离线", fg_color="#4b5567", corner_radius=6, padx=8)
        self.online_badge.grid(row=0, column=1, sticky="w", pady=(14, 6))
        self.device_name_label = ctk.CTkLabel(self.device_card, text="未选择设备", font=ctk.CTkFont(size=15, weight="bold"))
        self.device_name_label.grid(row=1, column=0, columnspan=2, sticky="w", padx=16)
        self.device_candidate_menu = ctk.CTkOptionMenu(self.device_card, variable=self.device_candidate_var, values=["未找到MuMu设备"])
        self.device_candidate_menu.grid(row=2, column=0, columnspan=3, sticky="ew", padx=16, pady=(0, 6))
        ctk.CTkButton(self.device_card, text="刷新", width=90, fg_color="#202635", command=self.refresh_devices).grid(row=3, column=0, sticky="ew", padx=(16, 5), pady=(0, 14))
        ctk.CTkButton(self.device_card, text="连接设备", width=90, command=self.connect_selected_device).grid(row=3, column=1, sticky="ew", padx=5, pady=(0, 14))
        ctk.CTkButton(self.device_card, text="断开设备", width=90, fg_color="#202635", command=self.disconnect_selected_device).grid(row=3, column=2, sticky="ew", padx=(5, 16), pady=(0, 14))

        run_card = self.card(parent, row=0, column=1, sticky="nsew", padx=(0, 10), pady=(0, 14))
        run_card.grid_columnconfigure((0, 1), weight=1)
        ctk.CTkLabel(run_card, text="运行状态", text_color="#cad2e4").grid(row=0, column=0, sticky="w", padx=16, pady=(14, 4))
        self.status_badge = ctk.CTkLabel(run_card, text="已停止", fg_color="#252b3a", corner_radius=6, padx=8)
        self.status_badge.grid(row=0, column=1, sticky="w", pady=(14, 4))
        ctk.CTkLabel(run_card, text="运行时长", text_color="#8c95aa").grid(row=1, column=0, sticky="w", padx=16)
        ctk.CTkLabel(run_card, text="执行步骤", text_color="#8c95aa").grid(row=1, column=1, sticky="w", padx=16)
        self.runtime_label = ctk.CTkLabel(run_card, text="00:00:00", font=ctk.CTkFont(size=18))
        self.runtime_label.grid(row=2, column=0, sticky="w", padx=16, pady=(0, 14))
        self.step_count_label = ctk.CTkLabel(run_card, text="0 / 0", font=ctk.CTkFont(size=18))
        self.step_count_label.grid(row=2, column=1, sticky="w", padx=16, pady=(0, 14))

        action_card = self.card(parent, row=0, column=2, sticky="nsew", pady=(0, 14))
        self.start_btn = ctk.CTkButton(action_card, text="▶ 启动执行", height=40, fg_color="#5636e8", command=self.start_cpp)
        self.start_btn.pack(fill="x", padx=16, pady=(14, 8))
        self.stop_btn = ctk.CTkButton(action_card, text="■ 停止执行", height=34, fg_color="#202635", command=self.stop_cpp)
        self.stop_btn.pack(fill="x", padx=16, pady=4)
        ctk.CTkButton(action_card, text="💾 保存脚本", height=34, fg_color="#202635", command=self.save_task_file).pack(fill="x", padx=16, pady=(4, 14))

    def build_task_panel(self, parent):
        panel = self.card(parent, row=1, column=0, sticky="nsew", padx=(0, 4))
        panel.grid_rowconfigure(2, weight=1)
        ctk.CTkLabel(panel, text="任务列表", font=ctk.CTkFont(size=16, weight="bold")).grid(row=0, column=0, sticky="w", padx=16, pady=(16, 10))
        ctk.CTkButton(panel, text="+  添加任务", height=40, command=self.add_task_placeholder).grid(row=1, column=0, sticky="ew", padx=16, pady=(0, 10))
        self.task_list = ctk.CTkScrollableFrame(panel, fg_color="transparent")
        self.task_list.grid(row=2, column=0, sticky="nsew", padx=12, pady=(0, 16))

    def build_flow_panel(self, parent):
        panel = self.card(parent, row=1, column=1, sticky="nsew", padx=4)
        panel.grid_columnconfigure(0, weight=1)
        panel.grid_rowconfigure(2, weight=1)

        top = ctk.CTkFrame(panel, fg_color="transparent")
        top.grid(row=0, column=0, sticky="ew", padx=16, pady=(14, 8))
        top.grid_columnconfigure(1, weight=1)
        ctk.CTkLabel(top, text="任务流程", text_color="#7c5cff", font=ctk.CTkFont(size=15, weight="bold")).grid(row=0, column=0, sticky="w")
        ctk.CTkButton(top, text="导入流程", width=88, fg_color="#202635", command=self.open_task_file).grid(row=0, column=2, padx=6)
        ctk.CTkButton(top, text="导出流程", width=88, fg_color="#202635", command=self.save_task_file).grid(row=0, column=3)

        self.task_file_var = ctk.StringVar(value="JieJieKa.json")
        self.task_menu = ctk.CTkOptionMenu(panel, variable=self.task_file_var, values=["JieJieKa.json"], command=self.on_task_file_selected)
        self.task_menu.grid(row=1, column=0, sticky="ew", padx=16, pady=(0, 8))

        self.flow_list = ctk.CTkScrollableFrame(panel, fg_color="transparent")
        self.flow_list.grid(row=2, column=0, sticky="nsew", padx=14, pady=(0, 12))

        ctk.CTkButton(panel, text="+  添加步骤", height=42, fg_color="#202635", border_width=1, border_color="#3a4255", command=self.add_step_placeholder).grid(
            row=3, column=0, sticky="ew", padx=16, pady=(0, 16)
        )

    def build_right_panel(self, parent):
        panel = self.card(parent, row=1, column=2, sticky="nsew", padx=(4, 0))
        panel.grid_columnconfigure(0, weight=1)
        panel.grid_rowconfigure(5, weight=1)

        ctk.CTkLabel(panel, text="实时预览", font=ctk.CTkFont(size=16, weight="bold")).grid(row=0, column=0, sticky="w", padx=16, pady=(16, 8))
        self.preview = ctk.CTkFrame(panel, height=260, fg_color="#0b0f18", corner_radius=8)
        self.preview.grid(row=1, column=0, sticky="ew", padx=16)
        self.preview.grid_propagate(False)
        self.preview_label = ctk.CTkLabel(self.preview, text="预览区域\n后续可接入截图流", text_color="#667085")
        self.preview_label.place(relx=0.5, rely=0.5, anchor="center")

        controls = ctk.CTkFrame(panel, fg_color="transparent")
        controls.grid(row=2, column=0, sticky="ew", padx=16, pady=12)
        controls.grid_columnconfigure((0, 1, 2), weight=1)
        self.preview_fps = ctk.DoubleVar(value=2.0)
        ctk.CTkButton(controls, text="刷新截图", fg_color="#202635", command=self.preview_once).grid(row=0, column=0, sticky="ew", padx=(0, 6))
        ctk.CTkButton(controls, text="开启预览", fg_color="#202635", command=self.start_preview).grid(row=0, column=1, sticky="ew", padx=6)
        ctk.CTkButton(controls, text="停止预览", fg_color="#202635", command=self.stop_preview).grid(row=0, column=2, sticky="ew", padx=(6, 0))
        ctk.CTkLabel(controls, text="帧率(FPS)", text_color="#8c95aa").grid(row=1, column=0, sticky="w", pady=(10, 0))
        ctk.CTkEntry(controls, textvariable=self.preview_fps, width=80).grid(row=1, column=1, sticky="ew", padx=6, pady=(10, 0))

        self.build_settings(panel)
        self.build_log(panel)

    def build_settings(self, parent):
        settings = ctk.CTkFrame(parent, fg_color="#111723", corner_radius=8)
        settings.grid(row=3, column=0, sticky="ew", padx=16, pady=(0, 12))
        settings.grid_columnconfigure(1, weight=1)

        self.exe_path = ctk.StringVar(value=str(self.bridge.default_exe()))
        self.run_task_name = ctk.StringVar(value="JieJieKa")
        self.window_name = ctk.StringVar(value="")
        self.target_window_name = ctk.StringVar(value="")
        self.adb_path = ctk.StringVar(value="adb")
        self.device_serial = ctk.StringVar(value="")
        self.capture_backend = ctk.StringVar(value="wgc")

        fields = [
            ("任务名称", self.run_task_name),
            ("窗口标题", self.window_name),
            ("子窗口类名", self.target_window_name),
            ("ADB路径", self.adb_path),
            ("设备序列号", self.device_serial),
            ("EXE路径", self.exe_path),
        ]
        for row, (label, var) in enumerate(fields):
            ctk.CTkLabel(settings, text=label, text_color="#aab2c5").grid(row=row, column=0, sticky="w", padx=14, pady=7)
            ctk.CTkEntry(settings, textvariable=var).grid(row=row, column=1, sticky="ew", padx=(8, 14), pady=7)

        ctk.CTkButton(settings, text="保存配置", command=self.save_config).grid(row=len(fields), column=0, columnspan=2, sticky="ew", padx=14, pady=(8, 14))

    def build_log(self, parent):
        top = ctk.CTkFrame(parent, fg_color="transparent")
        top.grid(row=4, column=0, sticky="ew", padx=16, pady=(0, 8))
        top.grid_columnconfigure(0, weight=1)
        ctk.CTkLabel(top, text="运行日志", font=ctk.CTkFont(size=15, weight="bold")).grid(row=0, column=0, sticky="w")
        ctk.CTkButton(top, text="清空日志", width=68, fg_color="#202635", command=self.clear_log).grid(row=0, column=1, padx=6)
        ctk.CTkButton(top, text="保存日志", width=68, fg_color="#202635", command=self.save_log).grid(row=0, column=2)
        self.log_text = ctk.CTkTextbox(parent, height=160, fg_color="#0b0f18", text_color="#c8d0e0")
        self.log_text.grid(row=5, column=0, sticky="nsew", padx=16, pady=(0, 16))

    def select_nav(self, name):
        for key, btn in self.nav_buttons.items():
            btn.configure(fg_color="#5636e8" if key == name else "transparent")

    def device_label(self, device):
        return device.get("title") or f"HWND {device.get('hwnd', 0)}"

    def update_device_candidate_menu(self):
        values = [self.device_label(device) for device in self.mumu_devices] or ["未找到MuMu设备"]
        self.device_candidate_menu.configure(values=values)
        if values:
            self.device_candidate_var.set(values[0])

    def update_connected_menu(self):
        return

    def selected_candidate_device(self):
        selected = self.device_candidate_var.get()
        for device in self.mumu_devices:
            if self.device_label(device) == selected:
                return device
        return None

    def apply_device(self, device, save=True):
        if not device:
            return
        self.active_instance()["device"] = device
        self.window_name.set(device.get("title", ""))
        self.target_window_name.set(device.get("target_class", "MuMuNxDevice"))
        self.capture_backend.set("wgc")
        self.device_name_label.configure(text=f"{self.active_instance_name}: {self.device_label(device)}")
        self.online_badge.configure(text="已连接", fg_color="#168b45")
        if save:
            self.save_config()

    def connect_selected_device(self):
        device = self.selected_candidate_device()
        if not device:
            self.log("未选择 MuMuNxDevice 窗口")
            return
        self.apply_device(device)
        self.log(f"{self.active_instance_name} 已连接: {self.device_label(device)}")

    def disconnect_selected_device(self):
        self.active_instance()["device"] = None
        self.window_name.set("")
        self.device_name_label.configure(text=f"{self.active_instance_name}: 未选择设备")
        self.online_badge.configure(text="离线", fg_color="#4b5567")
        self.save_config()

    def load_config(self):
        cfg = load_json(self.config_path, {})
        self.capture_backend.set(cfg.get("captureBackend", "wgc"))
        self.window_name.set(cfg.get("windowName", "MuMu模拟器"))
        self.target_window_name.set(cfg.get("targetWindowName", "MuMuNxDevice"))
        self.adb_path.set(cfg.get("adbPath", "adb"))
        self.device_serial.set(cfg.get("deviceSerial", ""))
        device = cfg.get("device")
        if isinstance(device, dict) and device.get("title"):
            self.active_instance()["device"] = device
        self.update_device_label()

    def save_config(self):
        backend = self.capture_backend.get()
        cfg = load_json(self.config_path, {})
        cfg.update(
            {
                "windowType": 2,
                "controlType": 0 if backend == "adb" else 1,
                "captureBackend": backend,
                "windowName": self.window_name.get(),
                "targetWindowName": self.target_window_name.get(),
                "adbPath": self.adb_path.get(),
                "deviceSerial": self.device_serial.get(),
                "instanceName": self.active_instance_name,
                "device": self.active_instance().get("device"),
                "screenshotIntervalMs": cfg.get("screenshotIntervalMs", 1000),
            }
        )
        save_json(self.config_path, cfg)
        if hasattr(self, "log_text"):
            self.log(f"{self.config_path.name} 已保存")

    def refresh_tasks(self, select=None):
        files = sorted(p.name for p in ROOT.glob("*.json") if p.name.lower() != "config.json")
        if files:
            self.task_menu.configure(values=files)
            default = select if select in files else ("JieJieKa.json" if "JieJieKa.json" in files else files[0])
            self.load_task_file(ROOT / default)

    def on_task_file_selected(self, name):
        self.load_task_file(ROOT / name)

    def load_task_file(self, path: Path):
        self.task_path = path
        self.task_file_var.set(path.name)
        self.task_data = load_json(path, {})
        self.run_task_name.set(self.root_task_name())
        self.render_task_list()
        self.render_flow()

    def root_task_name(self):
        for name, value in self.task_data.items():
            if isinstance(value, dict) and value.get("type") == "task":
                return name
        return self.task_path.stem

    def root_steps(self):
        root = self.task_data.get(self.root_task_name(), {})
        steps = root.get("next", []) if isinstance(root, dict) else []
        if steps:
            return [str(name) for name in steps]
        return [name for name in self.task_data.keys() if name != self.root_task_name()]

    def set_root_steps(self, steps):
        root_name = self.root_task_name()
        if root_name not in self.task_data or not isinstance(self.task_data[root_name], dict):
            self.task_data[root_name] = {"type": "task"}
        self.task_data[root_name]["type"] = "task"
        self.task_data[root_name]["next"] = list(steps)

    def safe_name(self, value):
        text = "".join(ch if ch.isalnum() or ch in "_-" else "_" for ch in (value or "").strip())
        return text.strip("_")

    def safe_file_name(self, value):
        name = self.safe_name(Path(value).stem)
        return f"{name or 'CustomTask'}.json"

    def next_task_name(self):
        index = 1
        while (ROOT / f"CustomTask{index}.json").exists():
            index += 1
        return f"CustomTask{index}"

    def next_step_name(self):
        index = len(self.root_steps()) + 1
        while f"Step{index}" in self.task_data:
            index += 1
        return f"Step{index}"

    def render_task_list(self):
        for child in self.task_list.winfo_children():
            child.destroy()
        for name in ["探索副本", "御魂副本", "觉醒副本", "结界寄养"]:
            card = ctk.CTkFrame(self.task_list, fg_color="#151b29", corner_radius=8, border_width=1, border_color="#252b3a")
            card.pack(fill="x", padx=4, pady=5)
            ctk.CTkLabel(card, text=name, font=ctk.CTkFont(size=14, weight="bold")).pack(anchor="w", padx=12, pady=(10, 2))
            ctk.CTkLabel(card, text=f"{self.root_task_name()} - 自动任务", text_color="#8c95aa").pack(anchor="w", padx=12, pady=(0, 10))

    def render_flow(self):
        for child in self.flow_list.winfo_children():
            child.destroy()
        steps = self.root_steps()
        self.step_count_label.configure(text=f"0 / {len(steps)}")
        for index, step_name in enumerate(steps, start=1):
            step = self.task_data.get(step_name, {})
            row = ctk.CTkFrame(self.flow_list, fg_color="#151b29", corner_radius=8, border_width=1, border_color="#252b3a")
            row.pack(fill="x", padx=4, pady=6)
            row.grid_columnconfigure(1, weight=1)
            ctk.CTkLabel(row, text=str(index), width=28, fg_color="#20283a", corner_radius=6).grid(row=0, column=0, rowspan=2, padx=12, pady=14)
            ctk.CTkLabel(row, text=step_name, font=ctk.CTkFont(size=15, weight="bold")).grid(row=0, column=1, sticky="w", pady=(14, 2))
            desc = step.get("action", step.get("type", "step"))
            if "target" in step:
                desc += f" · {step['target']}"
            ctk.CTkLabel(row, text=desc, text_color="#8c95aa").grid(row=1, column=1, sticky="w", pady=(0, 14))
            tools = ctk.CTkFrame(row, fg_color="transparent")
            tools.grid(row=0, column=3, rowspan=2, padx=10)
            ctk.CTkButton(tools, text="上移", width=42, fg_color="#202635", command=lambda n=step_name: self.move_step(n, -1)).grid(row=0, column=0, padx=3)
            ctk.CTkButton(tools, text="下移", width=54, fg_color="#202635", command=lambda n=step_name: self.move_step(n, 1)).grid(row=0, column=1, padx=3)
            ctk.CTkButton(tools, text="删除", width=42, fg_color="#8a2d2d", command=lambda n=step_name: self.delete_step(n)).grid(row=0, column=2, padx=3)
            ctk.CTkButton(row, text="编辑", width=52, fg_color="#202635", command=lambda n=step_name: self.edit_step(n)).grid(row=0, column=2, rowspan=2, padx=12)

    def rebuild_step_links(self):
        steps = self.root_steps()
        for index, step_name in enumerate(steps):
            if step_name in self.task_data and isinstance(self.task_data[step_name], dict):
                self.task_data[step_name]["next"] = [steps[index + 1]] if index + 1 < len(steps) else []
        self.set_root_steps(steps)

    def move_step(self, step_name, delta):
        steps = self.root_steps()
        if step_name not in steps:
            return
        index = steps.index(step_name)
        new_index = index + delta
        if new_index < 0 or new_index >= len(steps):
            return
        steps[index], steps[new_index] = steps[new_index], steps[index]
        self.set_root_steps(steps)
        self.rebuild_step_links()
        self.render_flow()
        self.save_task_file()

    def delete_step(self, step_name):
        if step_name not in self.task_data:
            return
        if not messagebox.askyesno("删除步骤", f"确定要删除 {step_name} 吗？"):
            return
        steps = [name for name in self.root_steps() if name != step_name]
        self.task_data.pop(step_name, None)
        self.set_root_steps(steps)
        self.rebuild_step_links()
        self.render_flow()
        self.save_task_file()

    def open_task_file(self):
        path = filedialog.askopenfilename(initialdir=ROOT, filetypes=[("JSON文件", "*.json")])
        if path:
            self.load_task_file(Path(path))

    def save_task_file(self):
        save_json(self.task_path, self.task_data)
        self.log(f"{self.task_path.name} 已保存")

    def add_task_placeholder(self):
        dialog = ctk.CTkToplevel(self)
        dialog.title("创建自定义流程")
        dialog.geometry("420x230")
        dialog.transient(self)
        dialog.grab_set()

        name_var = ctk.StringVar(value=self.next_task_name())
        file_var = ctk.StringVar(value=f"{name_var.get()}.json")

        def sync_file(*_):
            file_var.set(f"{self.safe_name(name_var.get() or 'CustomTask')}.json")

        name_var.trace_add("write", sync_file)

        ctk.CTkLabel(dialog, text="任务名称").pack(anchor="w", padx=16, pady=(18, 4))
        ctk.CTkEntry(dialog, textvariable=name_var).pack(fill="x", padx=16)
        ctk.CTkLabel(dialog, text="JSON文件").pack(anchor="w", padx=16, pady=(12, 4))
        ctk.CTkEntry(dialog, textvariable=file_var).pack(fill="x", padx=16)

        def create():
            task_name = self.safe_name(name_var.get())
            file_name = self.safe_file_name(file_var.get() or f"{task_name}.json")
            if not task_name:
                messagebox.showerror("无效任务", "任务名称不能为空")
                return
            if Path(file_name).stem != task_name:
                messagebox.showerror("无效文件", "JSON文件名必须与任务名匹配，例如 CustomTask.json")
                return
            path = ROOT / file_name
            if path.exists():
                messagebox.showerror("文件已存在", f"{file_name} 已经存在")
                return
            self.task_data = {task_name: {"type": "task", "next": []}}
            self.task_path = path
            self.task_file_var.set(path.name)
            self.run_task_name.set(task_name)
            self.save_task_file()
            self.refresh_tasks(select=path.name)
            dialog.destroy()
            self.log(f"已创建自定义流程: {path.name}")

        ctk.CTkButton(dialog, text="创建", command=create).pack(fill="x", padx=16, pady=18)

    def add_step_placeholder(self):
        if not self.task_data:
            self.add_task_placeholder()
            return

        dialog = ctk.CTkToplevel(self)
        dialog.title("添加步骤")
        dialog.geometry("460x300")
        dialog.transient(self)
        dialog.grab_set()

        step_var = ctk.StringVar(value=self.next_step_name())
        action_var = ctk.StringVar(value="click_template")

        ctk.CTkLabel(dialog, text="步骤名称").pack(anchor="w", padx=16, pady=(18, 4))
        ctk.CTkEntry(dialog, textvariable=step_var).pack(fill="x", padx=16)
        ctk.CTkLabel(dialog, text="动作模板").pack(anchor="w", padx=16, pady=(12, 4))
        ctk.CTkOptionMenu(dialog, variable=action_var, values=list(ACTION_TEMPLATES.keys())).pack(fill="x", padx=16)
        ctk.CTkLabel(
            dialog,
            text="添加后，请使用“编辑”功能调整坐标、OCR文本、模板路径、识别区域、超时时间和下一步逻辑。",
            text_color="#8c95aa",
            wraplength=400,
            justify="left",
        ).pack(anchor="w", padx=16, pady=(14, 0))

        def create():
            step_name = self.safe_name(step_var.get())
            if not step_name:
                messagebox.showerror("无效步骤", "步骤名称不能为空")
                return
            if step_name in self.task_data:
                messagebox.showerror("步骤已存在", f"{step_name} 已经存在")
                return
            self.task_data[step_name] = json.loads(json.dumps(ACTION_TEMPLATES[action_var.get()], ensure_ascii=False))
            steps = self.root_steps()
            if steps:
                previous = steps[-1]
                if previous in self.task_data and isinstance(self.task_data[previous], dict):
                    self.task_data[previous]["next"] = [step_name]
            self.set_root_steps(steps + [step_name])
            self.render_task_list()
            self.render_flow()
            self.save_task_file()
            dialog.destroy()
            self.log(f"已添加步骤: {step_name}")

        ctk.CTkButton(dialog, text="添加步骤", command=create).pack(fill="x", padx=16, pady=18)

    def edit_step(self, step_name):
        step = self.task_data.get(step_name, {})
        dialog = ctk.CTkToplevel(self)
        dialog.title(step_name)
        dialog.geometry("620x520")
        text = ctk.CTkTextbox(dialog)
        text.pack(fill="both", expand=True, padx=12, pady=12)
        text.insert("1.0", json.dumps(step, ensure_ascii=False, indent=2))

        def apply():
            try:
                self.task_data[step_name] = json.loads(text.get("1.0", "end"))
            except json.JSONDecodeError as exc:
                messagebox.showerror("JSON 格式错误", str(exc))
                return
            self.render_flow()
            self.save_task_file()
            dialog.destroy()

        ctk.CTkButton(dialog, text="应用", command=apply).pack(fill="x", padx=12, pady=(0, 12))

    def refresh_windows(self):
        self.windows = enum_windows()
        self.mumu_devices = enum_mumu_devices("MuMuNxDevice")
        if hasattr(self, "device_candidate_menu"):
            self.update_device_candidate_menu()

    def refresh_adb(self):
        self.adb_devices = scan_adb_devices(self.adb_path.get())
        if self.adb_devices and not self.device_serial.get():
            self.device_serial.set(self.adb_devices[0])
        self.update_device_label()

    def refresh_devices(self):
        self.refresh_windows()
        self.refresh_adb()
        self.log("设备列表已刷新")

    def update_device_label(self):
        backend = self.capture_backend.get()
        device = self.active_instance().get("device") if self.instances else None
        if device:
            self.device_name_label.configure(text=f"{self.active_instance_name}: {self.device_label(device)}")
            self.online_badge.configure(text="已连接", fg_color="#168b45")
        elif backend == "adb" and self.device_serial.get():
            self.device_name_label.configure(text=f"ADB {self.device_serial.get()}")
            self.online_badge.configure(text="在线", fg_color="#168b45")
        elif self.window_name.get():
            self.device_name_label.configure(text=self.window_name.get())
            self.online_badge.configure(text="已配置", fg_color="#8a6f19")
        else:
            self.device_name_label.configure(text="未选择设备")
            self.online_badge.configure(text="离线", fg_color="#4b5567")

    def preview_once_legacy_unused(self, active_capture=True):
        try:
            self.save_config()
            if learnopencv_py is None:
                self.preview_label.configure(text="learnopencv_py 未编译")
                return
            max_width = max(320, self.preview.winfo_width() - 12)
            max_height = max(180, self.preview.winfo_height() - 12)
            data = learnopencv_py.capture_png(str(self.config_path), max_width, max_height)
            if not data:
                self.preview_label.configure(text="C++ 截图失败")
                return
            image = tk.PhotoImage(data=data)
            self.preview_image = image
            self.preview_label.configure(image=image, text="")
            return
            if self.capture_backend.get() == "adb":
                data = adb_preview_png(self.adb_path.get(), self.device_serial.get())
                if not data:
                    self.preview_label.configure(text="ADB 截图失败")
                    return
                image = tk.PhotoImage(data=data)
            else:
                hwnd = self.find_preview_hwnd()
                if not hwnd:
                    self.preview_label.configure(text="未找到窗口")
                    return
                data = capture_window_ppm(hwnd)
                if not data:
                    self.preview_label.configure(text="窗口截图失败")
                    return
                image = tk.PhotoImage(data=data)
            self.preview_image = image
            self.preview_label.configure(image=image, text="")
        except Exception as exc:
            self.preview_label.configure(text=f"预览失败: {exc}")

    def find_preview_hwnd(self):
        title = self.window_name.get()
        for row in self.windows:
            if row["title"] == title:
                return row["hwnd"]
        return 0

    def start_preview(self):
        self.save_config()
        item = self.active_instance()
        if self.capture_backend.get() != "adb" and not self.window_name.get():
            self.show_preview_status("设备未连接")
            self.log(f"{self.active_instance_name} 预览未能启动: 未连接设备")
            return
        if learnopencv_py is not None:
            learnopencv_py.start_capture(str(self.config_path), 30)
        item["preview_running"] = True
        self.preview_running = True
        self.log(f"{self.active_instance_name} 预览已启动，帧率 {self.preview_fps.get()} FPS")
        self.after(100, lambda name=self.active_instance_name: self.preview_tick(name))

    def stop_preview(self):
        item = self.active_instance()
        item["preview_running"] = False
        self.preview_running = False
        if learnopencv_py is not None and not item["bridge"].is_running():
            learnopencv_py.stop_capture(str(item["config_path"]))
        self.log(f"{self.active_instance_name} 预览已停止")

    def preview_tick(self, instance_name=None):
        instance_name = instance_name or self.active_instance_name
        item = self.instances.get(instance_name)
        if not item or not item.get("preview_running"):
            return
        self.preview_once(active_capture=not item["bridge"].is_running(), instance_name=instance_name)
        fps = max(0.2, min(30.0, float(self.preview_fps.get() or 1.0)))
        self.after(int(1000 / fps), lambda name=instance_name: self.preview_tick(name))

    def start_cpp(self):
        try:
            self.save_config()
            self.started_at = datetime.now()
            self.bridge.start(Path(self.exe_path.get()), self.run_task_name.get().strip() or "JieJieKa", self.config_path)
        except Exception as exc:
            messagebox.showerror("启动失败", str(exc))
            return
        self.status_badge.configure(text="运行中", fg_color="#168b45")
        self.start_btn.configure(state="disabled")
        self.log("C++ 任务已启动")

    def stop_cpp(self):
        self.bridge.stop()
        self.log("已请求停止")

    def on_cpp_exit(self, code):
        self.enqueue_log(f"C++ 任务已退出，状态码: {code}")
        self.after(0, self.mark_stopped)

    def mark_stopped(self):
        self.status_badge.configure(text="已停止", fg_color="#252b3a")
        self.start_btn.configure(state="normal")
        self.started_at = None

    def enqueue_log(self, text):
        self.log_queue.put(text)

    def drain_log_queue(self):
        while True:
            try:
                self.log(self.log_queue.get_nowait())
            except queue.Empty:
                break
        self.after(150, self.drain_log_queue)

    def log(self, text):
        ts = datetime.now().strftime("%H:%M:%S")
        self.log_text.insert("end", f"[{ts}] {text}\n")
        self.log_text.see("end")

    def clear_log(self):
        self.log_text.delete("1.0", "end")

    def save_log(self):
        path = filedialog.asksaveasfilename(
            initialdir=ROOT,
            defaultextension=".log",
            filetypes=[("日志文件", "*.log"), ("文本文件", "*.txt")],
        )
        if not path:
            return
        Path(path).write_text(self.log_text.get("1.0", "end"), encoding="utf-8")
        self.log(f"日志已保存至: {path}")

    def preview_once(self, active_capture=True, instance_name=None):
        instance_name = instance_name or self.active_instance_name
        item = self.instances.get(instance_name)
        if not item:
            return
        if item.get("preview_busy"):
            return
        if learnopencv_py is None:
            self.preview_label.configure(text="learnopencv_py 未编译")
            return

        item["preview_busy"] = True
        if instance_name == self.active_instance_name:
            self.preview_busy = True
        max_width = max(320, self.preview.winfo_width() - 12)
        max_height = max(180, self.preview.winfo_height() - 12)
        config_path = str(item["config_path"])

        def worker():
            try:
                # 修复：不再使用全局锁，只使用当前实例自己的私有锁
                if item["capture_lock"].acquire(timeout=2.0):
                    try:
                        data = learnopencv_py.latest_png_for(config_path, max_width, max_height, 3000)
                        if not data and active_capture:
                            data = learnopencv_py.capture_png(config_path, max_width, max_height)
                    finally:
                        item["capture_lock"].release()
                else:
                    data = None # 锁超时，丢弃这一帧

                self.preview_queue.put((instance_name, data, None))
            except Exception as exc:
                self.preview_queue.put((instance_name, None, exc))

        threading.Thread(target=worker, daemon=True).start()

    def drain_preview_queue(self):
        while True:
            try:
                queued = self.preview_queue.get_nowait()
            except queue.Empty:
                break
            if len(queued) == 3:
                instance_name, data, error = queued
            else:
                instance_name, data, error = self.active_instance_name, queued[0], queued[1]
            self.apply_preview_frame(instance_name, data, error)
        self.after(50, self.drain_preview_queue)

    def apply_preview_frame(self, instance_name, data, error):
        item = self.instances.get(instance_name)
        if not item:
            return
        try:
            if instance_name != self.active_instance_name and (error is not None or not data):
                return
            if error is not None:
                if instance_name == self.active_instance_name:
                    self.show_preview_status(f"预览失败: {error}")
                return
            if not data:
                if instance_name == self.active_instance_name:
                    self.show_preview_status("截图失败")
                return
            image = tk.PhotoImage(data=data)
            item["preview_image"] = image
            if instance_name == self.active_instance_name:
                self.preview_image = image
                self.preview_status = ""
                self.preview_label.configure(image=image, text="")
        finally:
            item["preview_busy"] = False
            if instance_name == self.active_instance_name:
                self.preview_busy = False

    def update_runtime(self):
        if self.started_at:
            delta = datetime.now() - self.started_at
            total = int(delta.total_seconds())
            h, rem = divmod(total, 3600)
            m, s = divmod(rem, 60)
            self.runtime_label.configure(text=f"{h:02d}:{m:02d}:{s:02d}")
        else:
            self.runtime_label.configure(text="00:00:00")
        self.after(500, self.update_runtime)


if __name__ == "__main__":
    App().mainloop()