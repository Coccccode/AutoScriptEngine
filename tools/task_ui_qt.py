import queue
import sys
import threading
from datetime import datetime
from pathlib import Path

from PySide6.QtCore import QTimer, Qt
from PySide6.QtGui import QAction, QImage, QPixmap
from PySide6.QtWidgets import (
    QApplication,
    QComboBox,
    QDialog,
    QDialogButtonBox,
    QFormLayout,
    QGridLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QListWidget,
    QListWidgetItem,
    QMainWindow,
    QMessageBox,
    QPushButton,
    QPlainTextEdit,
    QSplitter,
    QStatusBar,
    QTabBar,
    QVBoxLayout,
    QWidget,
)

from tools.ui_backend import (
    CONFIG_DIR,
    ROOT,
    TASKS_DIR,
    CppBridge,
    adb_preview_png,
    enum_mumu_devices,
    learnopencv_py,
    load_json,
    save_json,
    scan_adb_devices,
)


def task_info_text(task_data):
    info = task_data.get("Info")
    if isinstance(info, str):
        return info.strip()
    if isinstance(info, dict):
        title = str(info.get("title", "")).strip()
        message = str(info.get("message", info.get("text", ""))).strip()
        if title and message:
            return f"{title}: {message}"
        return title or message
    return ""


def root_task_name(task_data, fallback):
    for key, value in task_data.items():
        if isinstance(value, dict) and value.get("type") == "task":
            return key
    return fallback


class SettingsDialog(QDialog):
    def __init__(self, config_data, parent=None):
        super().__init__(parent)
        self.setWindowTitle("设置")
        self.resize(520, 320)

        layout = QVBoxLayout(self)
        form = QFormLayout()
        layout.addLayout(form)

        self.run_task_name = QLineEdit(config_data.get("runTaskName", "JieJieKa"))
        self.window_name = QLineEdit(config_data.get("windowName", "MuMu模拟器"))
        self.target_window_name = QLineEdit(config_data.get("targetWindowName", "MuMuNxDevice"))
        self.adb_path = QLineEdit(config_data.get("adbPath", "adb"))
        self.device_serial = QLineEdit(config_data.get("deviceSerial", ""))
        self.exe_path = QLineEdit(config_data.get("exePath", ""))
        self.capture_backend = QComboBox()
        self.capture_backend.addItems(["wgc", "adb"])
        backend = config_data.get("captureBackend", "wgc")
        self.capture_backend.setCurrentText(backend if backend in ("wgc", "adb") else "wgc")

        form.addRow("任务名称", self.run_task_name)
        form.addRow("窗口标题", self.window_name)
        form.addRow("子窗口类名", self.target_window_name)
        form.addRow("ADB 路径", self.adb_path)
        form.addRow("设备序列号", self.device_serial)
        form.addRow("EXE 路径", self.exe_path)
        form.addRow("采集方式", self.capture_backend)

        buttons = QDialogButtonBox(QDialogButtonBox.Save | QDialogButtonBox.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

    def values(self):
        return {
            "runTaskName": self.run_task_name.text().strip() or "JieJieKa",
            "windowName": self.window_name.text().strip(),
            "targetWindowName": self.target_window_name.text().strip() or "MuMuNxDevice",
            "adbPath": self.adb_path.text().strip() or "adb",
            "deviceSerial": self.device_serial.text().strip(),
            "exePath": self.exe_path.text().strip(),
            "captureBackend": self.capture_backend.currentText(),
        }


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("脚本助手")
        self.resize(1480, 920)

        self.instances = {}
        self.active_instance_name = "实例1"
        self.preview_queue = queue.Queue()
        self.log_queue = queue.Queue()
        self.mumu_devices = []
        self.adb_devices = []
        self.started_at = None

        self._build_ui()
        self._create_instance(self.active_instance_name, CONFIG_DIR / "config.json")
        self.instance_tabs.addTab(self.active_instance_name)
        self.instance_tabs.setCurrentIndex(0)

        self.queue_timer = QTimer(self)
        self.queue_timer.timeout.connect(self._drain_queues)
        self.queue_timer.start(50)

        self.runtime_timer = QTimer(self)
        self.runtime_timer.timeout.connect(self._update_runtime)
        self.runtime_timer.start(500)

        QTimer.singleShot(0, self._finish_startup)

    def _build_ui(self):
        central = QWidget(self)
        self.setCentralWidget(central)
        root = QVBoxLayout(central)
        root.setContentsMargins(12, 12, 12, 12)
        root.setSpacing(12)

        header = QHBoxLayout()
        title = QLabel("脚本助手")
        title.setStyleSheet("font-size: 24px; font-weight: 700;")
        header.addWidget(title)
        header.addStretch(1)

        self.settings_btn = QPushButton("设置")
        self.settings_btn.clicked.connect(self._open_settings)
        header.addWidget(self.settings_btn)

        self.add_instance_btn = QPushButton("新增实例")
        self.add_instance_btn.clicked.connect(self._add_instance)
        header.addWidget(self.add_instance_btn)
        root.addLayout(header)

        self.instance_tabs = QTabBar()
        self.instance_tabs.setExpanding(False)
        self.instance_tabs.currentChanged.connect(self._on_instance_tab_changed)
        root.addWidget(self.instance_tabs)

        body = QSplitter(Qt.Horizontal)
        body.setChildrenCollapsible(False)
        root.addWidget(body, 1)

        left = QWidget()
        left_layout = QVBoxLayout(left)
        left_layout.setContentsMargins(0, 0, 0, 0)
        left_layout.setSpacing(12)
        body.addWidget(left)

        right = QSplitter(Qt.Vertical)
        right.setChildrenCollapsible(False)
        body.addWidget(right)
        body.setStretchFactor(0, 0)
        body.setStretchFactor(1, 1)
        body.setSizes([420, 980])

        self.device_group = QGroupBox("设备控制")
        dg = QGridLayout(self.device_group)
        self.device_status = QLabel("离线")
        self.device_name = QLabel("未选择设备")
        self.device_candidate = QComboBox()
        self.device_candidate.currentTextChanged.connect(self._remember_candidate)
        self.refresh_devices_btn = QPushButton("刷新设备")
        self.refresh_devices_btn.clicked.connect(self._refresh_devices)
        self.toggle_device_btn = QPushButton("连接设备")
        self.toggle_device_btn.clicked.connect(self._toggle_device)
        dg.addWidget(self.device_status, 0, 0)
        dg.addWidget(self.device_name, 1, 0, 1, 2)
        dg.addWidget(self.device_candidate, 2, 0, 1, 2)
        dg.addWidget(self.refresh_devices_btn, 3, 0)
        dg.addWidget(self.toggle_device_btn, 3, 1)
        left_layout.addWidget(self.device_group)

        self.task_group = QGroupBox("任务与运行")
        tg = QFormLayout(self.task_group)
        self.task_combo = QComboBox()
        self.task_combo.currentTextChanged.connect(self._on_task_selected)
        self.start_btn = QPushButton("启动执行")
        self.start_btn.clicked.connect(self._start_cpp)
        self.stop_btn = QPushButton("停止执行")
        self.stop_btn.clicked.connect(self._stop_cpp)
        run_bar = QHBoxLayout()
        run_bar.addWidget(self.start_btn)
        run_bar.addWidget(self.stop_btn)
        tg.addRow("任务文件", self.task_combo)
        tg.addRow(run_bar)
        left_layout.addWidget(self.task_group)

        self.task_steps_group = QGroupBox("任务步骤")
        steps_layout = QVBoxLayout(self.task_steps_group)
        self.task_steps = QListWidget()
        steps_layout.addWidget(self.task_steps)
        left_layout.addWidget(self.task_steps_group, 1)

        self.preview_group = QGroupBox("实时预览")
        self.preview_group.setMinimumHeight(360)
        preview_container = QWidget()
        pg = QVBoxLayout(preview_container)
        pg.setContentsMargins(0, 0, 0, 0)
        self.preview_label = QLabel("预览区域")
        self.preview_label.setAlignment(Qt.AlignCenter)
        self.preview_label.setMinimumHeight(360)
        self.preview_label.setStyleSheet("background:#0b0f18; color:#667085; border:1px solid #252b3a;")
        pg.addWidget(self.preview_label, 1)
        preview_bar = QHBoxLayout()
        self.preview_once_btn = QPushButton("刷新截图")
        self.preview_once_btn.clicked.connect(self._preview_once)
        self.preview_toggle_btn = QPushButton("开启预览")
        self.preview_toggle_btn.clicked.connect(self._toggle_preview)
        preview_bar.addWidget(self.preview_once_btn)
        preview_bar.addWidget(self.preview_toggle_btn)
        pg.addLayout(preview_bar)
        group_layout = QVBoxLayout(self.preview_group)
        group_layout.addWidget(preview_container)
        right.addWidget(self.preview_group)

        self.log_group = QGroupBox("日志")
        self.log_group.setMinimumHeight(240)
        log_container = QWidget()
        lg = QVBoxLayout(log_container)
        lg.setContentsMargins(0, 0, 0, 0)
        self.log_text = QPlainTextEdit()
        self.log_text.setReadOnly(True)
        lg.addWidget(self.log_text)
        log_group_layout = QVBoxLayout(self.log_group)
        log_group_layout.addWidget(log_container)
        right.addWidget(self.log_group)
        right.setStretchFactor(0, 3)
        right.setStretchFactor(1, 2)
        right.setSizes([520, 320])

        self.status_bar = QStatusBar()
        self.setStatusBar(self.status_bar)
        self.runtime_label = QLabel("00:00:00")
        self.status_bar.addPermanentWidget(self.runtime_label)

        save_action = QAction("保存配置", self)
        save_action.triggered.connect(self._save_active_config)
        self.menuBar().addAction(save_action)

        settings_action = QAction("设置", self)
        settings_action.triggered.connect(self._open_settings)
        self.menuBar().addAction(settings_action)

    def _create_instance(self, name, config_path: Path):
        if not config_path.exists():
            base = load_json(CONFIG_DIR / "config.json", {})
            if not isinstance(base, dict):
                base = {}
            base["windowName"] = ""
            base["device"] = None
            base["taskFile"] = base.get("taskFile", "JieJieKa.json")
            save_json(config_path, base)
        bridge = CppBridge(ROOT, self._enqueue_log, self._on_cpp_exit)
        self.instances[name] = {
            "name": name,
            "config_path": config_path,
            "bridge": bridge,
            "device": None,
            "device_candidate": "",
            "preview_running": False,
            "preview_busy": False,
            "preview_image": None,
            "capture_lock": threading.Lock(),
            "task_file": "",
            "task_data": {},
        }

    def _finish_startup(self):
        self._load_active_config()
        self._refresh_tasks()
        self._refresh_devices()
        self._sync_instance_ui()

    def _add_instance(self):
        index = len(self.instances) + 1
        while f"实例{index}" in self.instances:
            index += 1
        name = f"实例{index}"
        config_path = CONFIG_DIR / f"config_instance_{index}.json"
        self._create_instance(name, config_path)
        self.instance_tabs.addTab(name)
        self.instance_tabs.setCurrentIndex(self.instance_tabs.count() - 1)

    def _active_instance(self):
        return self.instances[self.active_instance_name]

    def _active_config(self):
        return load_json(self._active_instance()["config_path"], {})

    def _device_label(self, device):
        title = device.get("title") or "未命名窗口"
        hwnd = int(device.get("hwnd", 0) or 0)
        target_hwnd = int(device.get("target_hwnd", 0) or 0)
        return f"{title} | hwnd={hwnd} | target={target_hwnd}"

    def _on_instance_tab_changed(self, index):
        if index < 0:
            return
        name = self.instance_tabs.tabText(index)
        if name not in self.instances:
            return
        self._save_active_config()
        self.active_instance_name = name
        self._load_active_config()
        self._sync_instance_ui()

    def _sync_instance_ui(self):
        item = self._active_instance()
        self._sync_device_candidate_menu()
        self._sync_device_button()
        self._sync_preview_button()
        self._sync_task_ui()

        image = item.get("preview_image")
        if image is None:
            self.preview_label.setText(f"预览 {self.active_instance_name}")
            self.preview_label.setPixmap(QPixmap())
        else:
            self._set_preview_bytes(image)

    def _sync_task_ui(self):
        item = self._active_instance()
        task_file = item.get("task_file", "")
        if task_file:
            self.task_combo.blockSignals(True)
            if self.task_combo.findText(task_file) >= 0:
                self.task_combo.setCurrentText(task_file)
            self.task_combo.blockSignals(False)
        self._render_task_steps()

    def _render_task_steps(self):
        item = self._active_instance()
        task_data = item.get("task_data", {})
        self.task_steps.clear()
        info = task_info_text(task_data)
        if info:
            info_item = QListWidgetItem(f"Info: {info}")
            self.task_steps.addItem(info_item)
        root_name = root_task_name(task_data, Path(item.get("task_file") or "task").stem)
        if root_name and root_name in task_data:
            self.task_steps.addItem(QListWidgetItem(f"Root: {root_name}"))
        for key, value in task_data.items():
            if key == "Info" or not isinstance(value, dict):
                continue
            if value.get("type") == "task":
                continue
            action = value.get("action", "null")
            suffix = ""
            if action in ("task", "runTask", "callTask"):
                nested_file = value.get("file", value.get("taskFile", value.get("json", "")))
                nested_task = value.get("task", value.get("taskName", value.get("name", "")))
                suffix = f" -> {nested_file}:{nested_task}" if nested_file else ""
            elif value.get("target"):
                suffix = f" -> {value.get('target')}"
            elif value.get("template"):
                suffix = f" -> {Path(value.get('template')).name}"
            self.task_steps.addItem(QListWidgetItem(f"{key} [{action}]{suffix}"))

    def _sync_device_candidate_menu(self):
        selected = self._active_instance().get("device_candidate", "")
        values = [self._device_label(device) for device in self.mumu_devices] or ["未找到MuMu设备"]
        self.device_candidate.blockSignals(True)
        self.device_candidate.clear()
        self.device_candidate.addItems(values)
        if selected in values:
            self.device_candidate.setCurrentText(selected)
        elif values:
            self.device_candidate.setCurrentIndex(0)
            self._active_instance()["device_candidate"] = values[0]
        self.device_candidate.blockSignals(False)

    def _sync_device_button(self):
        connected = self._active_instance().get("device") is not None
        self.toggle_device_btn.setText("断开设备" if connected else "连接设备")

    def _sync_preview_button(self):
        self.preview_toggle_btn.setText("停止预览" if self._active_instance()["preview_running"] else "开启预览")

    def _remember_candidate(self, text):
        self._active_instance()["device_candidate"] = text

    def _selected_candidate_device(self):
        selected = self.device_candidate.currentText()
        self._active_instance()["device_candidate"] = selected
        for device in self.mumu_devices:
            if self._device_label(device) == selected:
                return device
        return None

    def _refresh_windows(self):
        self.mumu_devices = enum_mumu_devices("MuMuNxDevice")
        self._sync_device_candidate_menu()

    def _refresh_adb(self):
        cfg = self._active_config()
        self.adb_devices = scan_adb_devices(cfg.get("adbPath", "adb"))

    def _refresh_devices(self):
        self._refresh_windows()
        self._refresh_adb()

    def _load_active_config(self):
        cfg = self._active_config()
        item = self._active_instance()
        device = cfg.get("device")
        item["device"] = device if isinstance(device, dict) and device.get("title") else None
        item["task_file"] = cfg.get("taskFile", item.get("task_file") or "JieJieKa.json")

        if item["device"] is not None:
            self.device_status.setText("已连接")
            self.device_name.setText(f"{self.active_instance_name}: {self._device_label(item['device'])}")
        elif cfg.get("captureBackend", "wgc") == "adb" and cfg.get("deviceSerial", ""):
            self.device_status.setText("在线")
            self.device_name.setText(f"ADB {cfg.get('deviceSerial', '')}")
        else:
            self.device_status.setText("离线")
            self.device_name.setText("未选择设备")

    def _save_active_config(self):
        if not self.instances:
            return
        item = self._active_instance()
        cfg = self._active_config()
        cfg.update(
            {
                "windowType": 2,
                "controlType": 0 if cfg.get("captureBackend", "wgc") == "adb" else 1,
                "instanceName": self.active_instance_name,
                "device": item.get("device"),
                "screenshotIntervalMs": cfg.get("screenshotIntervalMs", 1000),
                "taskFile": self.task_combo.currentText(),
                "runTaskName": cfg.get("runTaskName", root_task_name(item.get("task_data", {}), "JieJieKa")),
                "exePath": cfg.get("exePath", str(CppBridge(ROOT, lambda _x: None, lambda _x: None).default_exe())),
            }
        )
        save_json(item["config_path"], cfg)

    def _save_settings(self, values):
        item = self._active_instance()
        cfg = self._active_config()
        cfg.update(values)
        cfg["controlType"] = 0 if values["captureBackend"] == "adb" else 1
        cfg["device"] = item.get("device")
        save_json(item["config_path"], cfg)
        self._load_active_config()

    def _open_settings(self):
        dialog = SettingsDialog(self._active_config(), self)
        if dialog.exec() != QDialog.Accepted:
            return
        self._save_settings(dialog.values())
        self._append_log(f"{self.active_instance_name} 设置已保存")

    def _apply_device(self, device):
        item = self._active_instance()
        item["device"] = device
        cfg = self._active_config()
        cfg.update(
            {
                "windowName": device.get("title", ""),
                "targetWindowName": device.get("target_class", "MuMuNxDevice"),
                "captureBackend": "wgc",
                "controlType": 1,
                "device": device,
            }
        )
        save_json(item["config_path"], cfg)
        self.device_status.setText("已连接")
        self.device_name.setText(f"{self.active_instance_name}: {self._device_label(device)}")
        self._sync_device_button()

    def _toggle_device(self):
        item = self._active_instance()
        if item.get("device") is not None:
            item["device"] = None
            cfg = self._active_config()
            cfg["device"] = None
            save_json(item["config_path"], cfg)
            self.device_status.setText("离线")
            self.device_name.setText(f"{self.active_instance_name}: 未选择设备")
            self._sync_device_button()
            self._save_active_config()
            return

        device = self._selected_candidate_device()
        if not device:
            QMessageBox.warning(self, "未选择设备", "请选择一个 MuMu 设备")
            return
        self._apply_device(device)
        self._append_log(f"{self.active_instance_name} 已连接: {self._device_label(device)}")

    def _refresh_tasks(self):
        files = sorted(p.name for p in TASKS_DIR.glob("*.json"))
        self.task_combo.blockSignals(True)
        self.task_combo.clear()
        self.task_combo.addItems(files)
        selected = self._active_instance().get("task_file") or "JieJieKa.json"
        if selected in files:
            self.task_combo.setCurrentText(selected)
        elif "JieJieKa.json" in files:
            self.task_combo.setCurrentText("JieJieKa.json")
        elif files:
            self.task_combo.setCurrentIndex(0)
        self.task_combo.blockSignals(False)
        if self.task_combo.currentText():
            self._on_task_selected(self.task_combo.currentText())

    def _on_task_selected(self, name):
        if not name:
            return
        path = TASKS_DIR / name
        task_data = load_json(path, {})
        item = self._active_instance()
        item["task_file"] = name
        item["task_data"] = task_data if isinstance(task_data, dict) else {}
        task_name = root_task_name(item["task_data"], path.stem)
        cfg = self._active_config()
        if not cfg.get("runTaskName"):
            cfg["runTaskName"] = task_name
            save_json(item["config_path"], cfg)
        self._render_task_steps()
        self._save_active_config()

        info = task_info_text(item["task_data"])
        if info:
            self._append_log(f"{name}: {info}")

    def _capture_preview_bytes(self, item, max_width=1280, max_height=720):
        config_path = str(item["config_path"])
        if learnopencv_py is not None:
            data = learnopencv_py.latest_png_for(config_path, max_width, max_height, 3000)
            if data:
                return data
            return learnopencv_py.capture_png(config_path, max_width, max_height)

        cfg = load_json(item["config_path"], {})
        if cfg.get("captureBackend", "wgc") == "adb":
            return adb_preview_png(cfg.get("adbPath", "adb"), cfg.get("deviceSerial", ""))
        return None

    def _toggle_preview(self):
        item = self._active_instance()
        if item["preview_running"]:
            item["preview_running"] = False
            if learnopencv_py is not None and not item["bridge"].is_running():
                learnopencv_py.stop_capture(str(item["config_path"]))
            self._append_log(f"{self.active_instance_name} 预览已停止")
            self._sync_preview_button()
            return

        cfg = self._active_config()
        self._save_active_config()
        if cfg.get("captureBackend", "wgc") != "adb" and not cfg.get("windowName", ""):
            self.preview_label.setText("设备未连接")
            return
        if learnopencv_py is not None:
            learnopencv_py.start_capture(str(item["config_path"]), 30)
        item["preview_running"] = True
        self._sync_preview_button()
        self._append_log(f"{self.active_instance_name} 预览已启动")
        self._schedule_preview_tick(self.active_instance_name)

    def _schedule_preview_tick(self, instance_name):
        item = self.instances.get(instance_name)
        if not item or not item["preview_running"]:
            return
        self._preview_once(instance_name)
        QTimer.singleShot(500, lambda name=instance_name: self._schedule_preview_tick(name))

    def _preview_once(self, instance_name=None):
        name = instance_name or self.active_instance_name
        item = self.instances.get(name)
        if not item or item["preview_busy"]:
            return
        item["preview_busy"] = True

        def worker():
            data = None
            error = None
            try:
                with item["capture_lock"]:
                    data = self._capture_preview_bytes(item)
            except Exception as exc:
                error = exc
            self.preview_queue.put((name, data, error))

        threading.Thread(target=worker, daemon=True).start()

    def _set_preview_bytes(self, data):
        image = QImage.fromData(data)
        if image.isNull():
            self.preview_label.setText("截图失败")
            self.preview_label.setPixmap(QPixmap())
            return
        pixmap = QPixmap.fromImage(image)
        scaled = pixmap.scaled(self.preview_label.size(), Qt.KeepAspectRatio, Qt.SmoothTransformation)
        self.preview_label.setPixmap(scaled)
        self.preview_label.setText("")

    def _enqueue_log(self, text):
        self.log_queue.put(text)

    def _append_log(self, text):
        ts = datetime.now().strftime("%H:%M:%S")
        self.log_text.appendPlainText(f"[{ts}] {text}")

    def _on_cpp_exit(self, code):
        self.log_queue.put(f"C++ 任务已退出，状态码: {code}")
        self.started_at = None

    def _start_cpp(self):
        try:
            self._save_active_config()
            item = self._active_instance()
            info = task_info_text(item.get("task_data", {}))
            if info:
                self._append_log(f"任务提示: {info}")
            self.started_at = datetime.now()
            item["bridge"].start(
                Path(self._active_config().get("exePath", str(CppBridge(ROOT, lambda _x: None, lambda _x: None).default_exe()))),
                self._active_config().get("runTaskName", root_task_name(item.get("task_data", {}), "JieJieKa")),
                item["config_path"],
            )
            self._append_log("C++ 任务已启动")
        except Exception as exc:
            QMessageBox.critical(self, "启动失败", str(exc))

    def _stop_cpp(self):
        self._active_instance()["bridge"].stop()
        self._append_log("已请求停止")

    def _drain_queues(self):
        while True:
            try:
                name, data, error = self.preview_queue.get_nowait()
            except queue.Empty:
                break
            item = self.instances.get(name)
            if item is None:
                continue
            item["preview_busy"] = False
            if error is None and data:
                item["preview_image"] = data
                if name == self.active_instance_name:
                    self._set_preview_bytes(data)
            elif name == self.active_instance_name:
                self.preview_label.setText(f"预览失败: {error}" if error else "截图失败")
                self.preview_label.setPixmap(QPixmap())

        while True:
            try:
                text = self.log_queue.get_nowait()
            except queue.Empty:
                break
            self._append_log(text)

    def _update_runtime(self):
        if self.started_at is None:
            self.runtime_label.setText("00:00:00")
            return
        delta = datetime.now() - self.started_at
        total = int(delta.total_seconds())
        h, rem = divmod(total, 3600)
        m, s = divmod(rem, 60)
        self.runtime_label.setText(f"{h:02d}:{m:02d}:{s:02d}")

    def closeEvent(self, event):
        for item in self.instances.values():
            if item["preview_running"] and learnopencv_py is not None and not item["bridge"].is_running():
                learnopencv_py.stop_capture(str(item["config_path"]))
        super().closeEvent(event)


def main():
    app = QApplication(sys.argv)
    app.setApplicationName("脚本助手")
    window = MainWindow()
    window.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
